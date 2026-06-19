//=============================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// server.cpp: implementation of the CServer class.
//
/////////////////////////////////////////////////////////////////////////////////
#include "core/stdafx.h"
#include "common/protocol.h"
#include "tier0/frametask.h"
#include "tier1/cvar.h"
#include "tier1/strtools.h"
#include "engine/server/sv_main.h"
#include "engine/server/server.h"
#include "networksystem/pylon.h"
#include "networksystem/bansystem.h"
#include "ebisusdk/EbisuSDK.h"
#include "public/edict.h"
#include "pluginsystem/pluginsystem.h"
#include "game/server/gameinterface.h"

#include "networksystem/hostmanager.h"
#include "jwt/include/decode.h"
#include "mbedtls/include/mbedtls/sha256.h"

//---------------------------------------------------------------------------------
// Console variables
//---------------------------------------------------------------------------------
ConVar sv_showconnecting("sv_showconnecting", "1", FCVAR_RELEASE, "Logs information about the connecting client to the console");

ConVar sv_globalBanlist("sv_globalBanlist", "1", FCVAR_RELEASE, "Determines whether or not to use the global banned list.", false, 0.f, false, 0.f, "0 = Disable, 1 = Enable.");
ConVar sv_banlistRefreshRate("sv_banlistRefreshRate", "30.0", FCVAR_DEVELOPMENTONLY, "Banned list refresh rate (seconds).", true, 1.f, false, 0.f);

static ConVar sv_validatePersonaName("sv_validatePersonaName", "1", FCVAR_RELEASE, "Validate the client's textual persona name on connect.");
static ConVar sv_minPersonaNameLength("sv_minPersonaNameLength", "4", FCVAR_RELEASE, "The minimum length of the client's textual persona name.", true, 0.f, false, 0.f);
static ConVar sv_maxPersonaNameLength("sv_maxPersonaNameLength", "16", FCVAR_RELEASE, "The maximum length of the client's textual persona name.", true, 0.f, false, 0.f);

//---------------------------------------------------------------------------------
// Purpose: Gets the number of human players on the server
// Output : int
//---------------------------------------------------------------------------------
int CServer::GetNumHumanPlayers(void) const
{
	int nHumans = 0;
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		const CClient* const pClient = g_pServer->GetClient(i);

		if (pClient->IsHumanPlayer())
			nHumans++;
	}

	return nHumans;
}

//---------------------------------------------------------------------------------
// Purpose: Gets the number of fake clients on the server
// Output : int
//---------------------------------------------------------------------------------
int CServer::GetNumFakeClients(void) const
{
	int nBots = 0;
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		const CClient* const pClient = g_pServer->GetClient(i);

		if (pClient->IsConnected() && pClient->IsFakeClient())
			nBots++;
	}

	return nBots;
}

//---------------------------------------------------------------------------------
// Purpose: Gets the number of clients on the server
// Output : int
//---------------------------------------------------------------------------------
int CServer::GetNumClients(void) const
{
	int nClients = 0;
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		const CClient* const pClient = g_pServer->GetClient(i);

		if (pClient->IsConnected())
			nClients++;
	}

	return nClients;
}

//---------------------------------------------------------------------------------
// Purpose: Rejects connection request and sends back a message
// Input  : iSocket - 
//			*pChallenge - 
//			*szMessage - 
//---------------------------------------------------------------------------------
void CServer::RejectConnection(int iSocket, netadr_t* pNetAdr, const char* szMessage)
{
	CServer__RejectConnection(this, iSocket, pNetAdr, szMessage);
}

//---------------------------------------------------------------------------------
// Purpose: Initializes a CSVClient for a new net connection. This will only be called
//			once for a player each game, not once for each level change.
// Input  : *pServer - 
//			*pChallenge - 
// Output : pointer to client instance on success, nullptr on failure
//---------------------------------------------------------------------------------
CClient* CServer::ConnectClient(CServer* pServer, user_creds_s* pChallenge)
{
	if (pServer->m_State < server_state_t::ss_active)
		return nullptr;

	char* pszPersonaName = pChallenge->personaName;
	NucleusID_t nNucleusID = pChallenge->personaId;

	const bool bEnableLogging = sv_showconnecting.GetBool();
	const int nPort = int(ntohs(pChallenge->netAdr.GetPort()));

	char szAddresBuffer[128];
	const char* pszAddresBuffer = nullptr;

	if (bEnableLogging)
	{
		// Render the client address once.
		pChallenge->netAdr.ToString(szAddresBuffer, sizeof(szAddresBuffer), true);
		pszAddresBuffer = szAddresBuffer;

		Msg(eDLL_T::SERVER, "Processing connectionless challenge for '[%s]:%i' ('%llu')\n",
			pszAddresBuffer, nPort, nNucleusID);
	}

	bool bValidName = false;

	if (VALID_CHARSTAR(pszPersonaName) &&
		V_IsValidUTF8(pszPersonaName))
	{
		if (sv_validatePersonaName.GetBool() && 
			!IsValidPersonaName(pszPersonaName, sv_minPersonaNameLength.GetInt(), sv_maxPersonaNameLength.GetInt()))
		{
			bValidName = false;
		}
		else
		{
			bValidName = true;
		}
	}

	// Only proceed connection if the client's name is valid and UTF-8 encoded.
	if (!bValidName)
	{
		pServer->RejectConnection(pServer->m_Socket, &pChallenge->netAdr, "#Valve_Reject_Invalid_Name");

		if (bEnableLogging)
		{
			Warning(eDLL_T::SERVER, "Connection rejected for '[%s]:%i' ('%llu' has an invalid name!)\n",
				pszAddresBuffer, nPort, nNucleusID);
		}

		return nullptr;
	}

	if (g_BanSystem.IsBanned(&pChallenge->netAdr, nNucleusID))
	{
		pServer->RejectConnection(pServer->m_Socket, &pChallenge->netAdr, "#Valve_Reject_Banned");

		if (bEnableLogging)
		{
			Warning(eDLL_T::SERVER, "Connection rejected for '[%s]:%i' ('%llu' is banned from this server!)\n",
				pszAddresBuffer, nPort, nNucleusID);
		}

		return nullptr;
	}

	CClient* const pClient = CServer__ConnectClient(pServer, pChallenge);

	for (auto& callback : !PluginSystem()->GetConnectClientCallbacks())
	{
		if (!callback.Function()(pServer, pClient, pChallenge))
		{
			pClient->Disconnect(REP_MARK_BAD, "#Valve_Reject_Banned");
			return nullptr;
		}
	}

	if (pClient && sv_globalBanlist.GetBool())
	{
		if (!pClient->GetNetChan()->GetRemoteAddress().IsLoopback())
		{
			if (!pszAddresBuffer)
			{
				pChallenge->netAdr.ToString(szAddresBuffer, sizeof(szAddresBuffer), true);
				pszAddresBuffer = szAddresBuffer;
			}

			const string addressBufferCopy(pszAddresBuffer);
			const string personaNameCopy(pszPersonaName);

			std::thread th(SV_CheckForBanAndDisconnect, pClient, addressBufferCopy, nNucleusID, personaNameCopy, nPort);
			th.detach();
		}
	}

	return pClient;
}

//---------------------------------------------------------------------------------
// Purpose: Sends netmessage to all active clients
// Input  : *msg       -
//          onlyActive - 
//          reliable   - 
//---------------------------------------------------------------------------------
void CServer::BroadcastMessage(CNetMessage* const msg, const bool onlyActive, const bool reliable)
{
	CServer__BroadcastMessage(this, msg, onlyActive, reliable);
}

//---------------------------------------------------------------------------------
// Purpose: Runs the server frame
// Input  : *pServer - 
//---------------------------------------------------------------------------------
void CServer::RunFrame(CServer* pServer)
{
	CServer__RunFrame(pServer);
}

bool CServer::SpawnServer(CServer* pServer, const char* pszMapName, const char* pszMapGroupName)
{
	const bool bSpawnResult = CServer__SpawnServer(pServer, pszMapName, pszMapGroupName);
	if (bSpawnResult)
	{
		if (::IsDedicated() || (V_strcmp(pszMapName, "mp_lobby") != 0 && V_strcmp(pszMapName, "mp_npe") != 0))
		{
			CClient::CheckMSForNewAuthKey();
		}
	}
	return bSpawnResult;
}

bool CServer::HandleC2SAuthentication( char* pszToken, size_t nTokenLength, const char* const pszPersonaName, uint64_t platformUserID, netadr_t* pAdr ) 
{
#define ERROR_AND_RETURN( fmt, ... )                                                   \
	do                                                                                 \
	{                                                                                  \
		CServer__RejectConnection( this, m_Socket, pAdr, fmt, ##__VA_ARGS__ ); \
		if ( claims )                                                                  \
		{                                                                              \
			l8w8jwt_free_claims( claims, numClaims );                                  \
		}                                                                              \
		return false;                                                                  \
	} while ( 0 )\


    l8w8jwt_claim* claims	 = nullptr;
	size_t		   numClaims = 0;

	struct l8w8jwt_decoding_params params;
	l8w8jwt_decoding_params_init( &params );

	params.alg = L8W8JWT_ALG_RS256;

	params.jwt		  = pszToken;
	params.jwt_length = nTokenLength;

	std::shared_lock lock( s_jwtPublicKeyMutex );
	params.verification_key		   = (unsigned char*)JWT_PUBLIC_KEY.c_str();
	params.verification_key_length = JWT_PUBLIC_KEY.size();

	params.validate_exp			 = sv_onlineAuthValidateExpiry.GetBool();
	params.exp_tolerance_seconds = (uint8_t)sv_onlineAuthExpiryTolerance.GetInt();

	params.validate_iat			 = sv_onlineAuthValidateIssuedAt.GetBool();
	params.iat_tolerance_seconds = (uint8_t)sv_onlineAuthIssuedAtTolerance.GetInt();

	enum l8w8jwt_validation_result validation_result;
	const int					   r = l8w8jwt_decode( &params, &validation_result, &claims, &numClaims );

	if ( r != L8W8JWT_SUCCESS )
		ERROR_AND_RETURN( "Code %i", r );

	if ( validation_result != L8W8JWT_VALID )
	{
		char reasonBuffer[64];
		l8w8jwt_get_validation_result_desc( validation_result, reasonBuffer, sizeof( reasonBuffer ) );

		ERROR_AND_RETURN( "%s", reasonBuffer );
	}

	bool foundSessionId = false;
	for ( size_t i = 0; i < numClaims; ++i )
	{
		const l8w8jwt_claim& claim = claims[i];

		// session id
		if ( !strcmp( claim.key, "sessionId" ) )
		{
			const char* const sessionId = claim.value;
			const CNetAdr&	  hostIP	= g_ServerHostManager.GetHostIP();

			char	  newId[256];
			const int idLen = snprintf( newId, sizeof( newId ), "%llu-%s-%s", platformUserID, pszPersonaName, hostIP.ToString() );

			if ( idLen < 0 )
				ERROR_AND_RETURN( "Session ID stitching failed" );

			uint8_t sessionHash[32]; // hash decoded from JWT token
			V_hextobinary( sessionId, claim.value_length, sessionHash, sizeof( sessionHash ) );

			uint8_t	  oobHash[32]; // hash of data collected from out of band packet
			const int shRet = mbedtls_sha256( (const uint8_t*)newId, idLen, oobHash, NULL );

			if ( shRet != NULL )
				ERROR_AND_RETURN( "Session ID hashing failed" );

			if ( memcmp( oobHash, sessionHash, sizeof( sessionHash ) ) != 0 )
				ERROR_AND_RETURN( "Token is not authorized for the connecting client" );

			foundSessionId = true;
		}
	}

	if ( !foundSessionId )
		ERROR_AND_RETURN( "No session ID" );

	l8w8jwt_free_claims( claims, numClaims );
	return true;
#undef ERROR_AND_RETURN
}

void CServer::VProcessC2SConnect(CServer* thisp, bf_read* pBuff, netadr_t* pAdr)
{
	const bool bClientHasAuthInfo = pBuff->ReadOneBit();

    if (sv_onlineAuthEnable.GetBool() && !pAdr->IsLoopback())
    {
        if (!bClientHasAuthInfo)
        {
			CServer__RejectConnection( thisp, thisp->m_Socket, pAdr, "Missing Authentication Info" );
			return;
        }

        int nTokenLength;
		char szAuthToken[1024];

        if (!pBuff->ReadString(szAuthToken, sizeof(szAuthToken), false, &nTokenLength))
        {
			CServer__RejectConnection( thisp, thisp->m_Socket, pAdr, "Oversized JWT Token" );
			return;
        }

        if ( nTokenLength == 0 || szAuthToken[0] == '\0' )
		{
			CServer__RejectConnection( thisp, thisp->m_Socket, pAdr, "Missing Token" );
			return;
		}

        const ssize_t nBufferPos = pBuff->GetNumBitsRead();

        // Skip past unneeded data
        // netProtocolVersion = 32 bits
        // gameVersion = 32 bits
        // challenge = 32 bits
        // reservation = 32 bits
        // platformID = 8 bits
		pBuff->SeekRelative( 136 );

        char		   szPersonaName[64];
        const uint64_t platformUserID = static_cast<uint64_t>(pBuff->ReadLongLong());

        pBuff->ReadString( szPersonaName, sizeof( szPersonaName ) );

        //Make sure to seek back to the start of the buffer so the main process func doesnt fail
		pBuff->Seek( nBufferPos );

        if (!thisp->HandleC2SAuthentication(szAuthToken, static_cast<size_t>(nTokenLength), szPersonaName, platformUserID, pAdr))
			return;
    }
    else if (bClientHasAuthInfo)
    {
        if (!pBuff->SkipString())
        {
			CServer__RejectConnection( thisp, thisp->m_Socket, pAdr, "Malformed C2S_CONNECT packet" );
			return;
        }
    }

#undef ERROR_AND_RETURN
    CServer__ProcessC2SConnect( thisp, pBuff, pAdr );
}

///////////////////////////////////////////////////////////////////////////////
void VServer::Detour(const bool bAttach) const
{
	DetourSetup(&CServer__SpawnServer, &CServer::SpawnServer, bAttach);
	DetourSetup(&CServer__RunFrame, &CServer::RunFrame, bAttach);
	DetourSetup(&CServer__ConnectClient, &CServer::ConnectClient, bAttach);
	DetourSetup( &CServer__ProcessC2SConnect, &CServer::VProcessC2SConnect, bAttach );
}

///////////////////////////////////////////////////////////////////////////////
CServer* g_pServer = nullptr;
CClientExtended CServer::sm_ClientsExtended[MAX_PLAYERS];
