#ifndef JOBTHREAD_H
#define JOBTHREAD_H

#define JT_JOB_GROUP_BASE_ID 0x3000

#define JT_THREAD_SLOTS 28
#define JT_WAIT_ARMED 0xAA
#define JT_WAIT_NOTIFIED 0xBB
#define JT_EPOCH_MASK 0xFFFFFFFF00000000ull


typedef uint32_t JobID_t;
typedef uint8_t JobTypeID_t;
typedef uint32_t JobAffinity_t;

typedef bool(*JobHelpCallback_t)(__int64, _DWORD*, __int64, _QWORD*);

enum JobPriority_e : uint32_t
{
};

struct JobFifoLock_s
{
	int id;
	int depth;
	short tls[64];
};

struct JobContext_s
{
	void* callbackArg;  // Argument to job callback function.
	void* callbackFunc; // Job callback function.
	JobTypeID_t jobTypeId;
	bool field_11;
	__int16 field_12;
	int field_14;
	JobID_t jobId;
	int field_1C;
	int field_20;
	int field_24; // Bit fields?
	__int64 field_28;
	__int64 unknownMask;
	__int64 unknownInt;
};

typedef struct JobUserData_s
{
	JobUserData_s(int32_t si)
	{
		data.sint = si;
	}
	JobUserData_s(uint32_t ui)
	{
		data.uint = ui;
	}
	JobUserData_s(int64_t si)
	{
		data.sint = si;
	}
	JobUserData_s(uint64_t ui)
	{
		data.uint = ui;
	}
	JobUserData_s(double sa)
	{
		data.scal = sa;
	}
	JobUserData_s(void* pt)
	{
		data.ptr = pt;
	}

	union
	{
		int64_t sint;
		uint64_t uint;
		double scal;
		void* ptr;
	} data;
} JobUserData_t;

typedef bool ( *JobStopCallback_t )( JobUserData_t pArg );

struct JT_ThreadState_t
{
	uint64_t		m_PreferredJobTypes;
	uint64_t		m_PermittedJobTypes;
	volatile LONG64 m_SavedParkState;
	JobTypeID_t		m_CurrentJobType;
	uint8_t			m_pad019[3];
	uint16_t		m_SlotCacheHead;
	uint16_t		m_SlotCacheTail;
	uint64_t		m_SlotCache[4];
};

struct JobParkPredicate_t
{
	JobStopCallback_t pfnPredicate;
	JobUserData_t pArg;
};

struct JT_JobContext_t
{
	void	*pArgs;
	void	*pfnCallback;
	uint8_t	 nJobType;
	uint8_t	 nFlags;
	uint16_t unused12;
	int32_t	 refCount;
	uint32_t ownerJobId;
	uint32_t unused1C;
	uint64_t depState;
	uint64_t unused28;
	uint64_t progressState;
	uint64_t nItemStride;
};

struct JT_JobType_t
{
	void	   *m_pfnEntry;
	void	   *m_pfnExitHook;
	void	   *m_pfnEnterHook;
	uint64_t	m_nRequiredStackBytes;
	const char *pszName;
	uint32_t	m_nPriority;
	uint32_t	m_nItemStride;
	uint32_t	m_nChunkFractionQ16;
	uint8_t		m_nRedirectJobType;
	uint8_t		m_gap35[11];
	uint64_t	m_QueueState;
	uint8_t		m_unused48[56];
};

struct JT_JobDeps_t
{
	int32_t pendingDeps;
	int32_t journal[3];
};

struct JobSystem_t
{
	int				   m_nThreadCount;
	int				   m_nExternalThreadCount;
	int				   m_NextWorkerThreadSlot;
	volatile LONG	   m_bDisableThreadParking;
	uint64_t		   m_AllocatedJobTypesBitMask;
	uint8_t			   m_JobTypeIdFamilyBase[13];
	uint8_t			   m_gap025[7];
	uint8_t			   m_JobTypeIdCounters[13];
	uint8_t			   m_gap039[7];
	uint64_t		   m_SlotPoolCursor;
	uint8_t			   m_gap048[40];
	void			  *m_pfnThreadIDAllocFunc;
	uint64_t		   m_ParkedThreadMask;
	JobParkPredicate_t m_ParkedPredicates[JT_THREAD_SLOTS];
	uint8_t			   m_gap240[64];
	uint8_t			   m_JobTypePending[64];
	uint64_t		   m_JobTypeHead[64];
	uint64_t		   m_JobTypeRing[64][32];
	JT_JobType_t	   m_registeredJobTypes[64];
	JT_JobContext_t	   m_jobCtxs[2048];
	JT_JobDeps_t	   m_jobDeps[2048];
};

static_assert( offsetof( JobSystem_t, m_NextWorkerThreadSlot ) == 0x08 );
static_assert( offsetof( JobSystem_t, m_bDisableThreadParking ) == 0x0C );
static_assert( offsetof( JobSystem_t, m_ParkedThreadMask ) == 0x78 );
static_assert( offsetof( JobSystem_t, m_ParkedPredicates ) == 0x80 );

struct JT_WorkerThreadControlInfo_t
{
	SRWLOCK			   m_Lock;
	CONDITION_VARIABLE m_ConditionalVariable;

	// Padding out to one cache line maybe???
	char m_gap016[48];
};

inline JT_WorkerThreadControlInfo_t *g_JTWorkerThreadControlInfos;

struct alignas( 64 ) WakeWordContainer
{
	volatile LONG word;
};

inline volatile WakeWordContainer s_WakeWords[JT_THREAD_SLOTS]{};

typedef void ( *IdleCallback )();

// Array size = 2048*sizeof(JobContext_s)
inline JobContext_s* job_JT_Context = nullptr;
inline JobSystem_t*	 g_pJobSystem;
inline IdleCallback *g_pJT_IdleCallbacks;
inline JT_ThreadState_t *g_JT_ThreadStates;

// low 32 bits are a thread bit mask
// high 32 is a wake epoch
inline volatile LONG64 *g_pJT_ThreadParkState;

extern bool JT_IsJobDone(const JobID_t jobId);
extern JobID_t JTGuts_AddJob(JobTypeID_t jobTypeId, JobID_t jobId, void* callbackFunc, void* callbackArg);
extern JobID_t JT_GetCurrentJob();

void JT_HelpWithJobTypesOrSleep( JobStopCallback_t pfnStop, JobUserData_t userData, const __int64 preferred,
									const __int64 permitted, volatile LONG64 *const pWaitWord, const char publish );
void JT_WakeThreadsByMask( uint64_t mask );

inline void(*JT_ParallelCall)(void);
inline void*(*JT_HelpWithAnything)(bool bShouldLoadPak);

inline void ( *JT_WakeWorkersForJob )( uint8_t jobType, uint64_t nWantedThreads );
inline void ( *v_JT_WakeThreadsByMask )( uint64_t mask );

inline bool ( *JT_HelpWithJobTypes )( JobStopCallback_t, JobUserData_t userData, __int64 a3, __int64 a4 );
inline void ( *v_JT_HelpWithJobTypesOrSleep )( JobStopCallback_t callback, JobUserData_t userData, const __int64 preferred,
												  const __int64 permitted, volatile LONG64 *const pWaitWord, const char publish );

inline __int64 ( *JT_WaitForJobAndOnlyHelpWithJobTypes )( JobID_t, uint64_t unkMask1, uint64_t unkMask2 );

inline bool(*JT_AcquireFifoLock)(struct JobFifoLock_s* pFifo);
inline void(*JT_ReleaseFifoLock)(struct JobFifoLock_s* pFifo);

inline JobID_t(*JT_BeginJobGroup)(const JobID_t jobId);
inline void(*JT_EndJobGroup)(const JobID_t jobId);

inline unsigned int (*JT_AllocateJob)(); // Returns an index to the 'job_JT_Context' array
inline JobID_t(*JTGuts_AddJob_Internal)(JobTypeID_t jobTypeId, JobID_t jobId, void* callbackfunc, void* callbackArg, int jobIndex, JobContext_s* context);

///////////////////////////////////////////////////////////////////////////////
class VJobThread : public IDetour
{
	virtual void GetAdr(void) const
	{
		LogFunAdr("JT_ParallelCall", JT_ParallelCall);

        LogFunAdr( "JT_WakeWorkersForJob", JT_WakeWorkersForJob );
		LogFunAdr( "v_JT_WakeThreadsByMask", v_JT_WakeThreadsByMask );
		
        LogFunAdr("JT_HelpWithAnything", JT_HelpWithAnything);
		LogFunAdr("JT_HelpWithJobTypes", JT_HelpWithJobTypes);
		LogFunAdr("JT_HelpWithJobTypesOrSleep", v_JT_HelpWithJobTypesOrSleep);
		LogFunAdr("JT_WaitForJobAndOnlyHelpWithJobTypes", JT_WaitForJobAndOnlyHelpWithJobTypes);

		LogFunAdr("JT_AcquireFifoLock", JT_AcquireFifoLock);
		LogFunAdr("JT_ReleaseFifoLock", JT_ReleaseFifoLock);

		LogFunAdr("JT_BeginJobGroup", JT_BeginJobGroup);
		LogFunAdr("JT_EndJobGroup", JT_EndJobGroup);

		LogFunAdr("JT_AllocateJob", JT_AllocateJob);
		LogFunAdr("JTGuts_AddJob_Internal", JTGuts_AddJob_Internal);
		LogVarAdr("job_JT_Context", job_JT_Context);
		LogVarAdr("g_JobSystem", g_pJobSystem);
		LogVarAdr("g_JT_IdleCallbacks", g_pJT_IdleCallbacks);
		LogVarAdr( "g_JT_ThreadParkState", (void*)g_pJT_ThreadParkState );
		LogVarAdr( "g_JT_ThreadStates", (void *)g_JT_ThreadStates );
		LogVarAdr( "g_JTWorkerThreadControlInfos", g_JTWorkerThreadControlInfos );

	}
	virtual void GetFun(void) const
	{
		Module_FindPattern(g_GameDll, "48 8B C4 48 89 58 08 48 89 78 10 55 48 8D 68 A1 48 81 EC ?? ?? ?? ?? 0F 29 70 E8 48 8D 1D ?? ?? ?? ??").GetPtr(JT_ParallelCall);
		Module_FindPattern(g_GameDll, "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC 30 80 3D ?? ?? ?? ?? ??").GetPtr(JT_HelpWithAnything);
		Module_FindPattern(g_GameDll, "48 89 5C 24 ?? 4C 89 4C 24 ?? 4C 89 44 24 ?? 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 60").GetPtr(JT_HelpWithJobTypes);
		Module_FindPattern(g_GameDll, "4C 89 4C 24 ?? 4C 89 44 24 ?? 48 89 54 24 ?? 48 89 4C 24 ?? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ??").GetPtr(v_JT_HelpWithJobTypesOrSleep);
		Module_FindPattern(g_GameDll, "48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 56 48 83 EC 30 8B F9").GetPtr(JT_WaitForJobAndOnlyHelpWithJobTypes);
		Module_FindPattern(g_GameDll, "48 83 EC 08 65 48 8B 04 25 ?? ?? ?? ?? 4C 8B C1").GetPtr(JT_AcquireFifoLock);
		Module_FindPattern(g_GameDll, "48 83 EC 28 44 8B 11").GetPtr(JT_ReleaseFifoLock);
		Module_FindPattern(g_GameDll, "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 56 48 83 EC 30 65 48 8B 04 25 ?? ?? ?? ?? BA ?? ?? ?? ??").GetPtr(JT_AllocateJob);
		Module_FindPattern(g_GameDll, "40 53 48 83 EC ?? 8B D9 E8 ?? ?? ?? ?? 44 8B C0").GetPtr(JT_BeginJobGroup);
		Module_FindPattern(g_GameDll, "8B D1 48 8D 05 ?? ?? ?? ?? 81 E2 ?? ?? ?? ?? 48 C1 E2 06 48 03 D0 E9 ?? ?? ?? ??").GetPtr(JT_EndJobGroup);
		Module_FindPattern(g_GameDll, "48 89 74 24 ? 57 48 83 EC 20 0F B6 F1").GetPtr(JTGuts_AddJob_Internal);

        Module_FindPattern( g_GameDll, "48 89 5C 24 ?? 56 48 83 EC ?? 48 8B D9 48 89 7C 24" ).GetPtr( v_JT_WakeThreadsByMask );
		Module_FindPattern( g_GameDll, "40 53 48 83 EC ?? 41 BB ?? ?? ?? ?? 4C 8B D2" ).GetPtr( JT_WakeWorkersForJob );
	}
	virtual void GetVar(void) const
	{
		CMemory(JT_EndJobGroup).FindPattern("48 8D").ResolveRelativeAddressSelf(3, 7).GetPtr(job_JT_Context);

        Module_FindPattern( g_GameDll, "48 8B C4 48 89 58 ?? 48 89 70 ?? 57 41 54 41 55 41 56 41 57 48 81 EC ?? ?? ?? ?? 0F 29 70" ).OffsetSelf( 0x7E ).FindPatternSelf( "89 05 E4 14" ).ResolveRelativeAddressSelf( 0x2, 0x6 ).GetPtr(g_pJobSystem);
	    
        CMemory( v_JT_HelpWithJobTypesOrSleep )
			.OffsetSelf( 0x100 )
			.FindPatternSelf( "48 8B 05" )
			.ResolveRelativeAddressSelf( 0x3, 0x7 )
			.GetPtr( g_pJT_ThreadParkState );
        
        CMemory( JT_WakeWorkersForJob )
			.FindPatternSelf( "48 8D 1D" )
			.ResolveRelativeAddressSelf( 0x3, 0x7 )
			.GetPtr( g_JT_ThreadStates );

        Module_FindPattern( g_GameDll, "40 53 55 48 83 EC ?? F0 48 83 0D" /*RenderThread*/ )
			.FindPatternSelf( "48 89 05 72" )
			.ResolveRelativeAddressSelf( 0x3, 0x7 )
			.OffsetSelf( -8 )
			.GetPtr( g_pJT_IdleCallbacks );

        CMemory( v_JT_WakeThreadsByMask )
			.FindPattern( "48 8D 35" )
			.ResolveRelativeAddressSelf( 0x3, 0x7 )
			.GetPtr( g_JTWorkerThreadControlInfos );
    }
	virtual void GetCon(void) const { }
	virtual void Detour(const bool bAttach) const;
};
///////////////////////////////////////////////////////////////////////////////

#endif // JOBTHREAD_H
