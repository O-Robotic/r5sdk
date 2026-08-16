/*	see copyright notice in squirrel.h */
#ifndef _SQFUNCPROTO_H_
#define _SQFUNCPROTO_H_

#include "sqobject.h"

struct SQLineInfo
{
	SQInteger _line;
	SQInteger _op;
};

struct SQInstruction
{
	int		 op;
	int		 _arg1;
	int		 _arg0;
	uint16_t _arg2;
	uint16_t _arg3;
};

struct SQFunctionProto : public SQCollectable
{
	void*		  _unalignedMemAddr;
	SQObjectPtr	  _sourcename;
	SQObjectPtr	  _name;
	SQObjectPtr*  _unkObj1;
	SQObjectPtr	  _unkObj;
	SQInteger	  _lineNum;
	SQInteger	  _stacksize;
	char		  m_gap0088[16];
	SQInteger	  _nlineinfos;
	char		  m_gap9C[4];
	SQLineInfo*	  _lineinfos;
	char		  m_gap0A8[16];
	SQInteger	  _nparameters;
	char		  m_gap00BC[96];
	SQInteger	  _ninstructions;
	SQInstruction _instructions[1];
};
static_assert(offsetof(SQFunctionProto, _sourcename) == 0x48);

#endif // _SQFUNCPROTO_H_
