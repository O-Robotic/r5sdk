//=============================================================================//
//
// Purpose: VScript System
//
//=============================================================================//
#include "core/stdafx.h"
#include "tier0/frametask.h"
#include "tier0/fasttimer.h"
#include "tier1/cvar.h"
#include "languages/squirrel_re/vsquirrel.h"
#include "languages/squirrel_re/include/squirrel.h"
#include "vscript/vscript.h"
#include "game/shared/vscript_shared.h"
#include "pluginsystem/modsystem.h"
#include "languages/squirrel_re/include/sqtable.h"
#include "languages/squirrel_re/include/sqfuncproto.h"
#include "languages/squirrel_re/include/sqarray.h"
#include "languages/squirrel_re/include/sqclosure.h"
#include "tier1/fmtstr.h"

static const char* s_scriptContextNames[] = { "SERVER", "CLIENT", "UI" };

//---------------------------------------------------------------------------------
// Since we parse and append mod scripts to the base script list, we must defer the
// deallocation of the RSON buffer until after the pre-compile job has finished, as
// the script list holds pointers to the strings inside the RSON object!
//---------------------------------------------------------------------------------
struct ScriptModPrecompileListDeferred_s
{
	void Reset()
	{
		for (int i = 0; i < MAX_MODS_TO_LOAD; i++)
		{
			RSON::Node_t* const modRson = rson[i];

			if (!modRson)
				continue; // Nothing to free.

			RSON_Free(modRson, AlignedMemAlloc());
			AlignedMemAlloc()->Free(modRson);

			rson[i] = nullptr;
		}
	}

	RSON::Node_t* rson[ MAX_MODS_TO_LOAD ];
};

static ScriptModPrecompileListDeferred_s s_scriptModPrecompileListDeferred[(int)SQCONTEXT::COUNT];
static bool s_scriptModListAppended[(int)SQCONTEXT::COUNT];

//---------------------------------------------------------------------------------
// Purpose: Returns the script VM pointer by context
// Input  : context - 
//---------------------------------------------------------------------------------
CSquirrelVM* Script_GetScriptHandle(const SQCONTEXT context)
{
	switch (context)
	{
	case SQCONTEXT::SERVER:
		return g_pServerScript;
	case SQCONTEXT::CLIENT:
		return g_pClientScript;
	case SQCONTEXT::UI:
		return g_pUIScript;
	NO_DEFAULT
	}
}

//---------------------------------------------------------------------------------
// Purpose: loads the script list, listing scripts to be compiled.
// Input  : *rsonfile - 
//---------------------------------------------------------------------------------
RSON::Node_t* Script_LoadScriptList(const SQChar* rsonfile)
{
	Msg(eDLL_T::ENGINE, "Loading script list: '%s'\n", rsonfile);
	return v_Script_LoadScriptList(rsonfile);
}

CUtlString sq_printobjvalue(SQObjectPtr* obj)
{
	CUtlString string;


    switch ( obj->_type )
	{
	case OT_NULL:
	{
		string = "null";
		break;
	}
	case OT_INTEGER:
	{
		string.Format( "%d", _integer( *obj ) );
		break;
	}
	case OT_FLOAT:
	{
		string.Format( "%f", _float( *obj ) );
		break;
	}
	case OT_BOOL:
	{
		string = _bool( *obj ) ? "true" : "false";
		break;
	}
	case OT_STRING:
	{
		string = _stringval( *obj );
		break;
	}
	case OT_TABLE:
	{
		SQTable* pTable = _table( *obj );

		string.Append( "--Table Start--\n" );

		SQ_FOR_EACH_TABLE( pTable, tableIt )
		{
			auto&			 hashNode = pTable->_nodes[tableIt];

            if ( hashNode.key._type == OT_NULL && hashNode.val._type == OT_NULL )
				continue;

			const CUtlString key	  = sq_printobjvalue( &hashNode.key );
			const CUtlString val	  = sq_printobjvalue( &hashNode.val );

			string.Append( Format( "%s: %s\n", key.String(), val.String() ).c_str() );
		}

		string.Append( "--Table Start--" );
		break;
	}
	case OT_ARRAY:
	{
		SQArray* pArray = _array( *obj );

		string.Append( "--Array Start--\n" );

		for ( int i = 0; i < pArray->Size(); i++ )
		{
			SQObjectPtr& obj = pArray->_values[i];
			string.Append( sq_printobjvalue( &obj ) );
			string.Append( '\n' );
		}

		string.Append( "--Array End--" );
		break;
	}
	case OT_USERDATA:
	{
		string = "userdata";
		break;
	}
	case OT_CLOSURE:
	{
		SQClosure* pClosure = _closure( *obj );

        string.Append( "--Closure Start--\n" );
        string.Append( Format("Function: %s\n", sq_printobjvalue( &pClosure->_function ).String()).c_str());
		string.Append( "--Closure End--" );
		break;
	}
	case OT_NATIVECLOSURE:
    {
		SQNativeClosure* pNativeClosure = _nativeclosure( *obj );
        string.Append( "--NativeClosure Start--\n" );

        string.Append( "_nparamscheck: " );
		string.Append( pNativeClosure->_nparamscheck ? "true" : "false" );
		string.Append( "\n" );

        string.Append( Format("Name: %s\n", sq_printobjvalue( &pNativeClosure->_name ).String() ).c_str());
        string.Append( Format("Func Addr: 0x%x\n", pNativeClosure->_function ).c_str());

        string.Append( "--NativeClosure End--" );
		break;
    }
	case OT_FUNCPROTO:
    {
		SQFunctionProto* pFuncProto = _funcproto( *obj );

        string.Append( "--FuncProto Start--\n" );

        CUtlString sourceName = sq_printobjvalue( &pFuncProto->_sourcename );
		CUtlString name		  = sq_printobjvalue( &pFuncProto->_name );

        string.Append( Format( "Source Name: %s\n", sourceName.String() ).c_str());
		string.Append( Format( "Name: %s\n", name.String() ).c_str() );

        string.Append( "--FuncProto End--" );
		break;
    }
	case OT_UNIMPLEMENTED:
    {
		SQFunctionProtoUnimplemented* pUnImplFunc = _unimplemented( *obj );

        string.Append( "--UnimplFunc Start--\n" );

        CUtlString sourceName = sq_printobjvalue( &pUnImplFunc->_sourcename );
		SQInteger  lineNum	  = pUnImplFunc->_linenum;

        string.Append( Format( "SourceName %s\nLineNum: %d\n", sourceName.String(), lineNum ).c_str() );

        string.Append( "--UnimplFunc End--" );
		break;
    }
	case OT_STRUCTDEF:
    {
		SQStructDef* pStructDef = (SQStructDef*)obj->_unVal.pUserPointer;

        CUtlString name = sq_printobjvalue( &pStructDef->_name );
        
        string.Append( "--StructDef Start--\n" );

        string.Append( Format( "Name %s\nHashVal %d\n", name.String(), pStructDef->_HashValMaybe ).c_str() );

        string.Append( "--StructDef End--" );
        break;
    }
    default: 
        string.Append(Format("Unhandled Type %s", IdType2Name(obj->_type)).c_str());
        break;
    }

    return string;
}

//---------------------------------------------------------------------------------
// Purpose: loads script files listed in the script list, to be compiled.
// Input  : *s - 
//			*path - 
//			*name - 
//			flags - 
//---------------------------------------------------------------------------------
SQBool Script_LoadScriptFile(CSquirrelVM* const s, const SQChar* path, const SQChar* name, SQInteger flags)
{
	///////////////////////////////////////////////////////////////////////////////
	SQBool bRes = v_Script_LoadScriptFile(s, path, name, flags);


    SQSharedState* pSharedState = s->GetVM()->_sharedstate;
	(void)pSharedState;

    auto& unkVec = pSharedState->m_unkVecOfSQSharedState_scope_state_maybe;


    for (unsigned int i = 0; i < unkVec.size(); i++)
    {
        SQSharedState_scope_state_maybe* scopeState = &unkVec[i];
		
        
        SQTable* pConstValuesTable = _table( scopeState->_ConstTable );
		SQTable* pConstUserDataTable = _table( scopeState->_ConstTableUserDatas );

        SQTable* pFileLevelDefsMaybe = _table( scopeState->_fileLocalVarNameKeysWithStrangeIndexVal );
		SQTable* pFunctionHolders	 = _table( scopeState->_fileFunctionsTable );

        SQTable* pStructDefs = _table( scopeState->_fileStructDefs );
		SQTable* pTypedefs	 = _table( scopeState->_typeDefs );

        SQTable* pUnkStructDefTable = _table( scopeState->_inlineStructs );

        
        //Msg( eDLL_T::ENGINE, "------ConstValuesTable Start------\n" );
		//Msg( eDLL_T::ENGINE, "%s\n", sq_printobjvalue( &scopeState->_ConstTable ).String() );
        //Msg( eDLL_T::ENGINE, "------ConstValuesTable End------\n" );
        

        //Msg( eDLL_T::ENGINE, "------ConstUserDataTable Start------\n" );
		//Msg( eDLL_T::ENGINE, "%s\n", sq_printobjvalue( &scopeState->_ConstTableUserDatas ).String() );
		//Msg( eDLL_T::ENGINE, "------ConstUserDataTable End------\n" );
        
        //Msg( eDLL_T::ENGINE, "------StructDefs Start------\n" );
		//Msg( eDLL_T::ENGINE, "%s\n", sq_printobjvalue( &scopeState->_fileStructDefs ).String() );
		//Msg( eDLL_T::ENGINE, "------StructDefs End------\n" );


        //Msg( eDLL_T::ENGINE, "------Typedefs Start------\n" );
		//Msg( eDLL_T::ENGINE, "%s\n", sq_printobjvalue( &scopeState->_typeDefs ).String() );
		//Msg( eDLL_T::ENGINE, "------Typedefs End------\n" );

        Msg( eDLL_T::ENGINE, "------unkStructDefTable Start------\n" );
		Msg( eDLL_T::ENGINE, "%s\n", sq_printobjvalue( &scopeState->_inlineStructs ).String() );
		Msg( eDLL_T::ENGINE, "------unkStructDefTable End------\n" );

        /*
        Msg( eDLL_T::ENGINE, "------FileLevelDefsMaybe Start------\n" );
		SQ_FOR_EACH_TABLE( pFileLevelDefsMaybe, iTableItr )
		{
			auto& node = pFileLevelDefsMaybe->_nodes[iTableItr];

			if ( node.key._type == OT_NULL && node.val._type == OT_NULL )
				continue;

			if ( node.key._type != OT_STRING || node.val._type != OT_INTEGER )
				Assert( 0 );

			Msg( eDLL_T::ENGINE, "Key: %s Value %d\n", _stringval( node.key ), _integer(node.val) );
		}
		Msg( eDLL_T::ENGINE, "------FileLevelDefsMaybe End------\n" );
        */

        
        //Msg( eDLL_T::ENGINE, "------FunctionHolders Start------\n" );
		//Msg( eDLL_T::ENGINE, "%s\n", sq_printobjvalue( &scopeState->_fileFunctionsTable ).String() );
        /*
        SQ_FOR_EACH_TABLE( pFunctionHolders, iTableItr )
		{
			auto& node = pFunctionHolders->_nodes[iTableItr];

			if ( node.key._type == OT_NULL && node.val._type == OT_NULL )
				continue;

			if ( node.key._type != OT_STRING )
				Assert( 0 );

            if ( node.val._type == OT_UNIMPLEMENTED )
			{
				SQFunctionProtoUnimplemented* unimplFunc = (SQFunctionProtoUnimplemented*)node.val._unVal.pFunctionProto;
				Msg( eDLL_T::ENGINE, "Impl Key: %s SourceName %s LineNum %d\n", _stringval( node.key ), _string(unimplFunc->_sourcename), unimplFunc->_linenum );
            }
			else if ( node.val._type == OT_FUNCPROTO )
			{
				SQFunctionProto* funcProto = (SQFunctionProto*)node.val._unVal.pFunctionProto;
				Msg( eDLL_T::ENGINE, "Unimpl Key: %s SourceName %s LineNum %d\n", _stringval( node.key ), _stringval(funcProto->_sourcename), funcProto->_lineNum);
            }
			else
				Assert( 0 );
		}
        */

		//Msg( eDLL_T::ENGINE, "------FunctionHolders End------\n" );







        
    }


    return bRes;
}

//---------------------------------------------------------------------------------
// Purpose: appends listed mod script into an already existing script array
// Input  : context - 
//			*scriptArray - 
//			*pScriptCount - 
//---------------------------------------------------------------------------------
static void Script_AppendModScriptList(const SQCONTEXT context, char** const scriptArray, int* const pScriptCount)
{
	ModSystem()->LockModList();
	CSquirrelVM* const s = Script_GetScriptHandle(context);

	FOR_EACH_VEC(ModSystem()->GetModList(), i)
	{
		CModSystem::ModInstance_t* const mod = ModSystem()->GetModList()[i];
		mod->hasPrecompiledScripts = false;

		if (!mod->IsEnabled())
			continue;

		// If its already loaded, use that one. This func can be called twice
		// from CSquirrelVM::PrecompileServerScripts() and we don't want to
		// load the rson again.
		RSON::Node_t* modRson = s_scriptModPrecompileListDeferred[(int)context].rson[i];

		if (!modRson)
		{
			bool parseFailure;
			modRson = mod->LoadScriptCompileList(&parseFailure);

			if (parseFailure)
			{
				Error(s->GetNativeContext(), 0,
					"%s: Failed to parse RSON file \"%s\"\n",
					__FUNCTION__, mod->GetScriptCompileListPath().Get());
			}

			if (!modRson)
				continue; // Just continue, this mod doesn't contain a compile list.

			s_scriptModPrecompileListDeferred[(int)context].rson[i] = modRson;
		}

		s->SetAsCompiler(modRson);

		const CUtlString currentScriptList = mod->GetScriptCompileListPath();
		const char* const pCurrentScriptList = currentScriptList.String();

		char* modScriptPaths[MAX_SCRIPT_FILES_TO_LOAD];
		int modScriptCount = 0;

		v_Script_ParseScriptList(context, pCurrentScriptList, modRson, modScriptPaths, &modScriptCount,
			// We check on the main `scriptArray` now because `scriptArray` was
			// already checked on `precompiledScriptArray` on the previous call.
			scriptArray, *pScriptCount);

		if (modScriptCount > 0)
		{
			const int newScriptCount = *pScriptCount + modScriptCount;

			// Make sure we didn't exceed it!
			if (newScriptCount > MAX_SCRIPT_FILES_TO_LOAD)
			{
				Error(s->GetNativeContext(), 0,
					"%s: Out of room appending scripts from mod '%s'(\"%s\"); max is MAX_SCRIPT_FILES_TO_LOAD = %d, got %d\n",
					__FUNCTION__, mod->name.String(), mod->id.String(), MAX_SCRIPT_FILES_TO_LOAD, newScriptCount);

				break;
			}

			// Append it.
			memcpy(&scriptArray[*pScriptCount], modScriptPaths, modScriptCount * sizeof(char*));
			*pScriptCount = newScriptCount;

			// Only set this when everything was successful, this is so that
			// SharedScript_ModSystem_RunCallbacks() doesn't do unnecessary
			// work for mods that don't have scripts at all.
			mod->hasPrecompiledScripts = true;
		}
	}

	ModSystem()->UnlockModList();
}

//---------------------------------------------------------------------------------
// Purpose: parses rson data to get an array of scripts to compile 
// Input  : context - 
//			*scriptListPath - 
//			*rson - 
//			*scriptArray - 
//			*pScriptCount - 
//			**precompiledScriptArray - 
//			precompiledScriptCount - 
//---------------------------------------------------------------------------------
bool Script_ParseScriptList(SQCONTEXT context, const char* scriptListPath,
	RSON::Node_t* rson, char** scriptArray, int* pScriptCount,
	char** precompiledScriptArray, int precompiledScriptCount)
{
	v_Script_ParseScriptList(context, scriptListPath, rson, scriptArray,
		pScriptCount, precompiledScriptArray, precompiledScriptCount);

	if (ModSystem()->IsEnabled() && !s_scriptModListAppended[(int)context])
	{
		Script_AppendModScriptList(context, scriptArray, pScriptCount);
		s_scriptModListAppended[(int)context] = true;
	}

	// always returns true internally, and code never checks return value,
	// so just do the same here.
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: precompiles scripts for the given VM
// Input  : *vm
//---------------------------------------------------------------------------------
SQBool Script_PrecompileScripts(CSquirrelVM* vm)
{
	SQCONTEXT context = vm->GetContext();
	Msg(eDLL_T(context), "Starting script compiler...\n");

	CFastTimer timer;
	timer.Start();

	SQBool result = false;
	s_scriptModListAppended[(int)context] = false;

	switch (context)
	{
	case SQCONTEXT::SERVER:
	{
		result = v_Script_PrecompileServerScripts(vm);
		break;
	}
	case SQCONTEXT::CLIENT:
	case SQCONTEXT::UI:
	{
		result = v_Script_PrecompileClientScripts(vm);
		break;
	}
	}

	s_scriptModPrecompileListDeferred[(int)context].Reset();
	timer.End();

	Msg(eDLL_T(context), "Script compiler finished in %lf seconds\n", timer.GetDuration().GetSeconds());
	return result;
}

SQBool Script_PrecompileServerScripts(CSquirrelVM* vm)
{
	return Script_PrecompileScripts(g_pServerScript);
}

SQBool Script_PrecompileClientScripts(CSquirrelVM* vm)
{
	return Script_PrecompileScripts(vm);
}

//---------------------------------------------------------------------------------
// Purpose: Compiles and executes input code on target VM by context
// Input  : *code - 
//			context - 
//---------------------------------------------------------------------------------
void Script_Execute(const SQChar* code, const SQCONTEXT context)
{
	Assert(context != SQCONTEXT::NONE);
	Assert(ThreadInMainOrServerFrameThread());

	CSquirrelVM* s = Script_GetScriptHandle(context);
	const char* const contextName = s_scriptContextNames[(int)context];

	if (!s)
	{
		Error(eDLL_T::ENGINE, NO_ERROR, "Attempted to run %s script with no handle to VM\n", contextName);
		return;
	}

	HSQUIRRELVM v = s->GetVM();

	if (!v)
	{
		Error(eDLL_T::ENGINE, NO_ERROR, "Attempted to run %s script while VM isn't initialized\n", contextName);
		return;
	}

	if (!s->Run(code))
	{
		Error(eDLL_T::ENGINE, NO_ERROR, "Failed to run %s script \"%s\"\n", contextName, code);
		return;
	}
}

//---------------------------------------------------------------------------------
void VScript::Detour(const bool bAttach) const
{
	DetourSetup(&v_Script_LoadScriptList, &Script_LoadScriptList, bAttach);
	DetourSetup(&v_Script_LoadScriptFile, &Script_LoadScriptFile, bAttach);
	DetourSetup(&v_Script_ParseScriptList, &Script_ParseScriptList, bAttach);
	DetourSetup(&v_Script_PrecompileServerScripts, &Script_PrecompileServerScripts, bAttach);
	DetourSetup(&v_Script_PrecompileClientScripts, &Script_PrecompileClientScripts, bAttach);
}
