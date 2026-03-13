#pragma once

DLL_EXPORT void SDK_Init();
DLL_EXPORT void SDK_Shutdown();

void Systems_Init();
void Systems_Shutdown();

void Winsock_Startup();
void Winsock_Shutdown();
void DirtySDK_Startup();
void DirtySDK_Shutdown();
void QuerySystemInfo();

void DetourInit();
void DetourAddress();
void DetourRegister();

extern bool g_bSdkInitialized;
extern bool g_bSdkShutdownInitiatedFromConsoleHandler;

inline void* (*v_memmove)(void* pDest, const void* pSrc, size_t nBytes);
inline int (*v_memcmp)(const void* pMem1, const void* pMem2, size_t nBytes);
inline void* (*v_memset)(void* pDest, int val, size_t nBytes);

inline int (*v_strcmp)(const char* pStr, const char* pStr1);
inline char* (*v_strdup)(const char* pStr);
inline const char* (*v_strchr)(const char* pStr, int val);

class VMemOverride : public IDetour
{
	virtual void GetAdr(void) const
	{
		LogFunAdr("memmove", v_memmove);
		LogFunAdr("memcmp", v_memcmp);
		LogFunAdr("memset", v_memset);

		LogFunAdr("strcmp", v_strcmp);
		LogFunAdr("strdup", v_strdup);
		LogFunAdr("strchr", v_strchr);

	}
	virtual void GetFun(void) const
	{
		Module_FindPattern(g_GameDll, "4C 8B D9 4C 8B D2 49 83 F8").GetPtr(v_memmove);
		Module_FindPattern(g_GameDll, "48 2B D1 49 83 F8").GetPtr(v_memcmp);
		Module_FindPattern(g_GameDll, "4C 8B D9 0F B6 D2").GetPtr(v_memset);

		Module_FindPattern(g_GameDll, "48 2B D1 F6 C1").GetPtr(v_strcmp);
		Module_FindPattern(g_GameDll, "40 53 48 83 EC ?? 48 8B D9 48 85 C9 75 ?? 33 C0").GetPtr(v_strdup);
		Module_FindPattern(g_GameDll, "48 83 EC ?? 0F B6 C2").GetPtr(v_strchr);
	}
	virtual void GetVar(void) const {}
	virtual void GetCon(void) const {}
	virtual void Detour(const bool bAttach) const
	{
		DetourSetup(&v_memmove, &memmove, bAttach);
		DetourSetup(&v_memcmp, &memcmp, bAttach);
		DetourSetup(&v_memset, &memset, bAttach);

		DetourSetup(&v_strcmp, &strcmp, bAttach);
		DetourSetup(&v_strdup, &strdup, bAttach);
		DetourSetup(&v_strchr, &strchr, bAttach);
	}
};