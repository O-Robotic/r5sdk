#pragma once

#define INTERFACEVERSION_PLUGINSYSTEM "VPluginSystem003"

class CServer;

struct PluginOperation_s
{
	enum class PluginCommand_e : int16_t
	{
		PLUGIN_INSTALL_CALLBACK = 0,
		PLUGIN_REMOVE_CALLBACK
	};

	enum class PluginCallback_e : int16_t
	{
		// !! - WARNING: if any existing values are changed, you must increment INTERFACEVERSION_PLUGINSYSTEM - !! 

		CModAppSystemGroup_Create  = 0,
		CModAppSystemGroup_Destroy = 1,
		CServer_ConnectClient      = 2,
		OnReceivedChatMessage      = 3,

		OnRegisterSharedScriptFunctions = 4,
		OnRegisterServerScriptFunctions = 5,
		OnRegisterClientScriptFunctions = 6,
		OnRegisterUIScriptFunctions     = 7,
		CServer_RunFrame                = 8,
	};

	PluginCommand_e commandId;
	PluginCallback_e callbackId;
	const char* name;
	void* function;
};

abstract_class IPluginSystem
{
public:
	virtual void* RunOperation(PluginOperation_s* const help) = 0;
	virtual void CoreMsgV(LogType_t logType, LogLevel_t logLevel, eDLL_T context,
		const char* pszLogger, const char* pszFormat, va_list args, const UINT exitCode, const char* pszUptimeOverride) = 0;
};
