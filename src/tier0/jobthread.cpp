
#include "engine/host_cmd.h"
#include "tier0/jobthread.h"

static bool bUseWaitOnAddress = true;

bool JT_IsJobDone(const JobID_t jobId)
{
	return (job_JT_Context[jobId & 0xFFF].field_24 ^ jobId) & 0xFFFFC000;
}

// TODO: confirm this actually returns a JobID_t and not some other int
JobID_t JTGuts_AddJob(JobTypeID_t jobTypeId, JobID_t jobId, void* callbackFunc, void* callbackArg)
{
	const unsigned int jobIndex = JT_AllocateJob();
	return JTGuts_AddJob_Internal(jobTypeId, jobId, callbackFunc, callbackArg, jobIndex, &job_JT_Context[jobIndex]);
}

JobID_t JT_GetCurrentJob()
{
	return *(_DWORD*)(*(_QWORD*)CModule::GetThreadEnvironmentBlock()->ThreadLocalStoragePointer + 12i64);
}

uint32_t JT_GetCurrentThreadID()
{
	return *(_DWORD *)( *(_QWORD *)CModule::GetThreadEnvironmentBlock()->ThreadLocalStoragePointer + 8 );
}

static void JT_WakeThreadsByMask( uint64_t mask )
{
    while (mask)
    {
		DWORD bitIdx;
		_BitScanForward64( &bitIdx, mask );
		mask &= mask - 1;
		
        if (bUseWaitOnAddress)
        {
			volatile LONG *const pWakeWork = &s_WakeWords[bitIdx].word;
			if ( InterlockedExchange( pWakeWork, JT_WAIT_NOTIFIED ) == JT_WAIT_ARMED )
				WakeByAddressSingle( (PVOID *)pWakeWork );
        }
        else
        {
			JT_WorkerThreadControlInfo_t &control_Info = g_JTWorkerThreadControlInfos[bitIdx];
			AcquireSRWLockExclusive( &control_Info.m_Lock );
			WakeConditionVariable( &control_Info.m_ConditionalVariable );
            ReleaseSRWLockExclusive( &control_Info.m_Lock );
        }  
    }
}

static void JT_CheckParkPredicates( uint32_t threadID )
{
	if ( g_pJobSystem->m_ParkedThreadMask == 0 )
		return;

	uint64_t threadsToCheck = ReadAcquire64( (LONG64*)&g_pJobSystem->m_ParkedThreadMask ) & ~( 1ull << threadID );
	if ( threadsToCheck == 0 )
		return;

	uint64_t		   threadsReadyToWake = 0;

    while (threadsToCheck)
    {
		DWORD bitIdx;
		_BitScanForward64( &bitIdx, threadsToCheck );
		threadsToCheck &= threadsToCheck - 1;

        JobParkPredicate_t predicate = {0,0};
        _InterlockedCompareExchange128( (volatile LONG64 *)&g_pJobSystem->m_ParkedPredicates[bitIdx], ( (LONG64 *)( &predicate ) )[1],
										( (LONG64 *)( &predicate ) )[0], (LONG64 *)&predicate );

        if ( predicate.pfnPredicate && predicate.pfnPredicate( predicate.pArg ) )
			threadsReadyToWake |= ( 1ull << bitIdx );
    }

    threadsReadyToWake &= ReadAcquire64( (LONG64 *)&g_pJobSystem->m_ParkedThreadMask );
	if ( threadsReadyToWake )
		JT_WakeThreadsByMask( threadsReadyToWake & InterlockedAnd64( (volatile LONG64*)g_pJT_ThreadParkState, ~threadsReadyToWake ) );
}

static JobParkPredicate_t JT_PublishPredicate( uint32_t threadID, JobStopCallback_t pFunc, JobUserData_t pArg )
{
	JobParkPredicate_t prev { pFunc, pArg };
	while ( !_InterlockedCompareExchange128( (volatile LONG64*)&g_pJobSystem->m_ParkedPredicates[threadID], *(LONG64*)&pArg, (LONG64)pFunc,
											 (LONG64*)&prev ) )
	{}
	_InterlockedOr64( (volatile LONG64*)&g_pJobSystem->m_ParkedThreadMask, 1ull << threadID );
	return prev;
}

static void JT_UnpublishPredicate( uint32_t threadID, JobParkPredicate_t prev )
{
	JobParkPredicate_t buf = prev;
	while ( !_InterlockedCompareExchange128( (volatile LONG64*)&g_pJobSystem->m_ParkedPredicates[threadID], ( (LONG64*)&prev )[1],
											 ( (LONG64*)&prev )[0], (LONG64*)&buf ) )
	{}

	if ( !prev.pfnPredicate )
		_InterlockedAnd64( (volatile LONG64*)&g_pJobSystem->m_ParkedThreadMask, ~( 1ull << threadID ) );
}

static void JT_WaitForWork(uint32_t threadID, const DWORD timeout)
{
	volatile LONG *wakeWord = &s_WakeWords[threadID].word;

    for (;;)
    {
		const LONG prev = InterlockedCompareExchange( wakeWord, JT_WAIT_ARMED, 0 );
		if ( prev == JT_WAIT_NOTIFIED )
			break;

        LONG expected = JT_WAIT_ARMED;
		if ( WaitOnAddress( wakeWord, (void *)&expected, sizeof( LONG ), timeout ) )
			continue;

        break;
    }

    if ( bUseWaitOnAddress )
        InterlockedExchange( wakeWord, 0 );
}

void JT_HelpWithJobTypesOrSleep( JobStopCallback_t pfnStop, JobUserData_t userData, const __int64 preferredJobMask,
									const __int64 permittedJobMask, volatile LONG64 *const pWaitWord, const char bPublishPredicate )
{
	const unsigned slot = JT_GetCurrentThreadID();
	const uint64_t bit	= uint64_t{ 1 } << slot;

	const auto jobDone = [&] { return pfnStop && pfnStop( userData ); };
	const auto helpWithJobTypes = [&] { return JT_HelpWithJobTypes( pfnStop, userData, preferredJobMask, permittedJobMask ); };

	JT_CheckParkPredicates( slot );
	if ( jobDone() || helpWithJobTypes() )
		return;

	JobParkPredicate_t previous{ 0, 0 };
	if ( bPublishPredicate )
		previous = JT_PublishPredicate( slot, pfnStop, userData );

	volatile LONG64* pThreadSavedParkState  = &g_JT_ThreadStates[slot].m_SavedParkState;
	volatile LONG* pbThreadParkingDisabled  = &g_pJobSystem->m_bDisableThreadParking;

	while ( !ReadAcquire( pbThreadParkingDisabled ) )
	{
		JT_CheckParkPredicates( slot );
		if ( pWaitWord )
			InterlockedOr64( pWaitWord, bit );

		const uint64_t before				 = InterlockedOr64( g_pJT_ThreadParkState, bit );
		bool		   stoppedByIdleCallback = false;
		if ( ( ( before ^ ReadAcquire64( pThreadSavedParkState ) ) & JT_EPOCH_MASK ) == 0 && !jobDone() &&
			 ( !pWaitWord || ( ReadAcquire64( pWaitWord ) & bit ) != 0 ) && !ReadAcquire( pbThreadParkingDisabled ) )
		{
			const auto idle = g_pJT_IdleCallbacks[slot];
			while ( ( ReadAcquire64( g_pJT_ThreadParkState ) & bit ) != 0 && !ReadAcquire( pbThreadParkingDisabled ) )
			{
				if ( idle )
				{
					idle();
					stoppedByIdleCallback = jobDone();
					if ( stoppedByIdleCallback || ( ReadAcquire64( g_pJT_ThreadParkState ) & bit ) == 0 )
						break;
				}
				JT_WaitForWork( slot, idle ? 1 : 100 );
			}
		}

		if ( ReadAcquire64( g_pJT_ThreadParkState ) & bit )
			InterlockedAnd64( g_pJT_ThreadParkState, ~bit );

		if ( stoppedByIdleCallback || jobDone() || helpWithJobTypes() )
			break;
	}

	if ( bPublishPredicate )
		JT_UnpublishPredicate( slot, previous );

	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void* HJT_HelpWithAnything(bool bShouldLoadPak)
{
	// !! Uncomment if needed !!
	// !! Consider initializing this in idetour iface if enabled !!
	//static void* const retaddr = Module_FindPattern(g_GameDll, "48 8B C4 ?? 41 54 41 55 48 81 EC 70 04 ?? ?? F2 0F 10 05 ?? ?? ?? 0B")
	//	.Offset(0x4A0).FindPatternSelf("48 8B ?? ?? ?? ?? 01", CMemory::Direction::DOWN).RCast<void*>();

	void* const results = JT_HelpWithAnything(bShouldLoadPak);

	//if (retaddr != _ReturnAddress()) // Check if this is called after 'PakFile_Init()'.
	//{
	//	return results;
	//}
	// Do stuff here after 'PakFile_Init()'.
	return results;
}

void VJobThread::Detour(const bool bAttach) const
{
	DetourSetup( &v_JT_HelpWithJobTypesOrSleep, &JT_HelpWithJobTypesOrSleep, bAttach );
	DetourSetup( &v_JT_WakeThreadsByMask, &JT_WakeThreadsByMask, bAttach );
}
