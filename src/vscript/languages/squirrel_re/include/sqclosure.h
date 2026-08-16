/*	see copyright notice in squirrel.h */
#ifndef _SQCLOSURE_H_
#define _SQCLOSURE_H_

struct SQClosure : public SQCollectable
{
public:
	SQObjectPtr			 m_unkObj1;
	SQObjectPtr			 _function;
	SQObjectPtrVec _outervalues;
};
static_assert(offsetof(SQClosure, _function) == 0x50);
typedef SQInteger( __fastcall* SQFUNCTION )( HSQUIRRELVM );

struct SQNativeClosure : SQCollectable
{
	bool		m_unkBool;
	bool		_nparamscheck;
	char		m_gap0003[6];
	SQObjectPtrVec	_SomeTypesVec;
	SQObjectPtrVec m_unkVec2;
	SQFUNCTION	_function;
	SQObjectPtr _name;
	SQObjectPtr _notsure;
};


#endif //_SQCLOSURE_H_
