#ifndef SQSTATE_H
#define SQSTATE_H
#include "squirrel.h"
#include "sqobject.h"
//#include "sqcompiler.h"

struct SQCompiler;
class CSquirrelVM;

struct RefTable {
	struct RefNode {
		SQObjectPtr obj;
		SQUnsignedInteger refs;
		struct RefNode* next;
	};
	void AddRef(SQObject& obj);
	SQBool Release(SQObject& obj);

	RefTable::RefNode* Get(SQObject& obj, SQHash& mainpos, RefNode** prev, bool add);
private:
	SQUnsignedInteger _numofslots;
	SQUnsignedInteger _slotused;
	RefNode* _nodes;
	RefNode* _freelist;
	RefNode** _buckets;
	SQInteger		  _context;
};

struct StringTable
{
	char*	  _strings;
	SQInteger gap008;
	SQInteger _numofslots;
	SQInteger gap012;
	SQInteger _context;
};

struct SQFunctionProtoUnimplemented : SQRefCounted
{
	SQObjectPtr _sourcename;
	SQInteger	_linenum;
	SQObjectPtr m_unkObj2;
	uint64_t	m_unkQWORD050;
	SQObjectPtr m_unkObj4;
	bool		m_unkBool;
	bool		m_unkBool2;
	char		m_gap06A[6];
	sqvector<SQObjectPtr>	m_unkVec;
	SQInteger	m_unkInt;
	SQInteger	m_unkInt2;
	SQInteger	m_unkInt3;
	SQInteger	m_unkInt4;
};

struct SQStructDef : SQCollectable
{
	SQObjectPtr			 _name;
	SQObjectPtrVec m_unkObjVecMaybe;
	SQObjectPtr			 _properties_maybe;
	SQObjectPtrVec		 m_unkVec1;
	SQInteger			 _HashValMaybe;
	char				 m_pad001[12];
};


struct SQSharedState_scope_state_maybe
{
	//SQObjectPtr _unkTables[7];
	
    //_ConstTable seems to hold string keys with the names of constants in a file, the value is the type and value of the set constant
    //_ConstTableUserDatas has at least some of the same keys as _ConstTable but with userdata values

    //_fileLocalVarNameKeysWithStrangeIndexVal has string values of file level definitions with int values, not sure

    //_unkTable4 seems to hold string keys of named functions with unimplemented function vals
    //_fileStructDefs string keys with struct names, structdef values
    //_typeDefs string keys with typedef names, int values
    //_inlineStructs int keys with structdef values, no idea what these are
    
    
	SQObjectPtr _ConstTable;
	SQObjectPtr _ConstTableUserDatas;
	SQObjectPtr _fileLocalVarNameKeysWithStrangeIndexVal;
	SQObjectPtr _fileFunctionsTable;
	SQObjectPtr _fileStructDefs;
	SQObjectPtr _typeDefs;
	SQObjectPtr _inlineStructs;
    
};

struct SQSharedState_wtaf_do_you_do
{
	SQObjectPtr m_UnkTypeArray[16383];
	char		m_gap3FFF0[32];
};

#pragma pack(push, 1)
struct SQSharedState
{
	_BYTE							 gap0[16416];
	void*							 _metamethods;
	SQObjectPtr						 _ConstTable;
	void*							 _systemstrings;
	char							 gap5[8];
	StringTable*					 _stringtable;
	RefTable						 _refs_table;
	_BYTE							 gap0002[16];
	SQObjectPtr						 _constructoridx;
	char							 m_gap4098[24];
	sqvector<SQObjectPtr>			 _unkVecSomethingClassRelated;
	char							 m_gap40C8[24];
	SQTable*						 _codeFuncs;
	char							 m_gap40E0[8];
	SQTable*						 _constTable;
	char							 m_gap9[8];
	SQTable*						 _anotherConstTable;
	char							 gap4070[8];
	SQTable*						 _unkGlobalsTable;
	_BYTE							 gap4118[8];
	SQTable*						 _globalfunctions;
	_BYTE							 gap4128[56];
	SQTable*						 m_SomethingToDoWithPerSourceFileStateScopeThingYes;
	sqvector <SQSharedState_scope_state_maybe> m_unkVecOfSQSharedState_scope_state_maybe;
	SQSharedState_scope_state_maybe* m_unkVecOfSQSharedState_scope_state_maybe_Top;
	SQTable*						 _inlineStructs;
	char							 m_gap4188[8];
	SQTable*						 _typeDefs;
	SQTable*						 _fileStructDefs;
	SQTable*						 _ConstTableUserDatas;
	SQTable*						 _unkSomethingTypeCompiler;
	SQTable*						 _maybeUnanmedStructsNotSure;
	char							 gap41B8[8];
	SQTable*						 _entityFuncs;
	_BYTE							 gap8[32];
	SQTable*						 _structuretable_maybe;
	char							 gap41F0[16];
	SQSharedState_wtaf_do_you_do*	 m_monstrousStruct;
	void*							 _types_maybe;
	SQInteger						 _nNumTypes;
	SQInteger						 _numFunctionTypes;
	void*							 m_FunctionRefTypeDataBuf_maybe;
	void*							 m_FunctionRefTypeDataBuf_maybe_end;
	char							 gap6[8];
	SQCompiler*						 _compiler;
	uint8_t							 m_gap4240[16];
	SQVM*							 _unkVM;
	class SQDbgServer*					 m_pDbgServer;
	char							 m_gap4260[8];
	void*							 m_CurrentExpressionNodePtr;
	void*							 m_ExpressionNodeBufferEndPtr;
	char							 m_gap4278[16];
	SQCollectable*					 _gc_chain;
	uint8_t							 gap2[112];
	SQTable*						 _floattable_orsomething;
	char							 gap4308[112];
	bool							 _somethingsetjmp;
	char							 m_gap7[7];
	void*							 _compilererrorhandler;
	void*							 _printfunc;
	SQInteger( __fastcall * _randomintfunc )( SQInteger max );
	void*		 _unkFunc;
	uint8_t		 gap4390[16];
	char		 _isdeveloper;
	SQChar		 _contextname[8];
	char		 gap43b9[111];
	CSquirrelVM* _scriptvm;
	SQInteger	 _context;
	char		 pad_4434[4];
	int			 _internal_error;
	_BYTE		 gap443C[4];
	char*		 _scratchpad;
	int			 _scratchpadsize;


    SQCompiler* GetCompiler()
	{
		return _compiler;
	}

	CSquirrelVM* GetScriptVM()
	{
		return _scriptvm;
	}
};

#pragma pack(pop)
//static_assert(offsetof(SQSharedState, _compiler) == 0x4238);
//static_assert(offsetof(SQSharedState, _printfunc) == 0x4388);

struct SQBufState
{
	const SQChar* buf;
	const SQChar* bufTail;
	const SQChar* bufPos;

	SQBufState(const SQChar* code)
	{
		buf = code;
		bufTail = code + strlen(code);
		bufPos = code;
	}
};
#endif // SQSTATE_H