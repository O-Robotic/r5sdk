#pragma once
#include "engine/cdll_int.h"

class CEngineClient : public IVEngineClient
{
public:
	void SetRestrictServerCommands(bool bRestrict);
	bool GetRestrictServerCommands() const;
	void SetRestrictClientCommands(bool bRestrict);
	bool GetRestrictClientCommands() const;
	int GetLocalPlayer(); // Local player index.

	// Hook statics:
	static void _ClientCmd(CEngineClient* thisptr, const char* const szCmdString);
	static bool _IsValidPacket(CEngineClient* thisptr, int flow, uint8_t nFrameNumber);
	static float _GetPacketTime(CEngineClient* thisptr, int flow, uint8_t nFrameNumber);
	static int _GetPacketSize(CEngineClient* thisptr, int flow, uint8_t nFrameNumber);
};

/* ==== CVENGINECLIENT ================================================================================================================================================== */
///////////////////////////////////////////////////////////////////////////////
inline void(*CEngineClient__ClientCmd)(CEngineClient* thisptr, const char* const szCmdString);

inline bool(*CEngineClient__IsValidPacket)(CEngineClient* thisptr, int flow, uint8_t nFrameNumber);
inline float(*CEngineClient__GetPacketTime)(CEngineClient* thisptr, int flow, uint8_t nFrameNumber);
inline int(*CEngineClient__GetPacketSize)(CEngineClient* thisptr, int flow, uint8_t nFrameNumber);

inline CMemory g_pEngineClientVFTable = nullptr;
inline CEngineClient* g_pEngineClient = nullptr;

///////////////////////////////////////////////////////////////////////////////
class HVEngineClient : public IDetour
{
	virtual void GetAdr(void) const
	{
		LogConAdr("CEngineClient::`vftable'", (void*)g_pEngineClientVFTable.GetPtr());
		LogFunAdr("CEngineClient::ClientCmd", CEngineClient__ClientCmd);
		LogFunAdr("CEngineClient::IsValidPacket", CEngineClient__IsValidPacket);
		LogFunAdr("CEngineClient::GetPacketTime", CEngineClient__GetPacketTime);
		LogFunAdr("CEngineClient::GetPacketSize", CEngineClient__GetPacketSize);
	}
	virtual void GetFun(void) const
	{
		Module_FindPattern(g_GameDll, "40 53 48 83 EC 20 80 3D ?? ?? ?? ?? ?? 48 8B DA 74 0C").GetPtr(CEngineClient__ClientCmd);
		Module_FindPattern(g_GameDll, "4C 8B 0D ?? ?? ?? ?? 4D 85 C9 74 ?? 41 83 E0 ?? 48 63 C2 4D 03 C0 48 69 C8 ?? ?? ?? ?? 4B 8D 04 C1 0F B6 84 01").GetPtr(CEngineClient__IsValidPacket);
		Module_FindPattern(g_GameDll, "4C 8B 0D ?? ?? ?? ?? 4D 85 C9 74 ?? 41 83 E0 ?? 48 63 C2 4D 03 C0 48 69 C8 ?? ?? ?? ?? 4B 8D 04 C1 F3 0F 10 84 01").GetPtr(CEngineClient__GetPacketTime);
		Module_FindPattern(g_GameDll, "4B 8D 04 C1 8B 84 01").GetPtr(CEngineClient__GetPacketSize);
	}
	virtual void GetVar(void) const { }
	virtual void GetCon(void) const 
	{
		g_pEngineClientVFTable = g_GameDll.GetVirtualMethodTable(".?AVCEngineClient@@");
		g_pEngineClient = (CEngineClient*)&g_pEngineClientVFTable; // CEngineClient is iface only (doesn't have members).
	}
	virtual void Detour(const bool bAttach) const;
};
///////////////////////////////////////////////////////////////////////////////
