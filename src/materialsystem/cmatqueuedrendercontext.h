#ifndef MATQUEUEDRENDERCONTEXT_H
#define MATQUEUEDRENDERCONTEXT_H

struct CallQueue_s
{
	uint64_t m_nQueueCapacity;
	uint8_t *m_pQueueBuffer;
	uint32_t m_WriteOffset;
	uint32_t m_ReadPos;
	
    uint32_t lastEntryOffset;
	uint32_t readOffset;

	uint8_t probably_64byte_aligned[32];

	inline void *GetCurrentCallItem()
	{
		return &m_pQueueBuffer[m_ReadPos];
	}

	inline void *GetCurrentAllocatedItem()
	{
		return &m_pQueueBuffer[m_WriteOffset];
	}

	inline void AdvanceReadPos( uint32_t size )
	{
		m_ReadPos += size;
	}
};

inline int(**g_fnHasRenderCallQueue)(void);
inline CallQueue_s* (**g_fnAddRenderCallQueueItem)(void* method, const size_t structSize, const size_t alignmentMask);
inline int(**g_fnAdvanceRenderCallQueue)(const size_t structSize);

struct CallQueueCtx
{
	HANDLE			  hRenderThread;
	volatile uint32_t frameCounter;
	uint32_t		  unk0C;
	volatile uint64_t mainWaiting;
	volatile uint64_t renderIdle;
	void ( *blockingPumpCallbacks[3] )();
	uint32_t	   forceSyncFlag;
	volatile LONG	   quitFlag;
	CallQueue_s	   queues[3];
};

inline CallQueueCtx *g_CallQueues;

class VMatQueuedRenderContext : public IDetour
{
	virtual void GetAdr(void) const
	{
		LogVarAdr("g_fnHasRenderCallQueue", g_fnHasRenderCallQueue);
		LogVarAdr("g_fnAddRenderCallQueueItem", g_fnAddRenderCallQueueItem);
		LogVarAdr("g_fnAdvanceRenderCallQueue", g_fnAdvanceRenderCallQueue);
		LogVarAdr( "g_CallQueues", g_CallQueues );
	}
	virtual void GetFun(void) const { }
	virtual void GetVar(void) const
	{
		Module_FindPattern( g_GameDll, "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 56 48 83 EC ?? 65 48 8B 04 25 ?? ?? ?? ?? 4C 8D 35" )
            .FindPatternSelf( "4C 8D 35 26 8B" ).ResolveRelativeAddressSelf( 0x3, 0x7 ).GetPtr( g_CallQueues );

		const CMemory tempBase = Module_FindPattern(g_GameDll, "48 83 EC ? FF 15 ? ? ? ? 85 C0 74 ? BA ? ? ? ? 48 8D 0D ? ? ? ? 44 8D 42 ? FF 15 ? ? ? ? B9 ? ? ? ? 8B 50");
		tempBase.FindPattern("FF 15").ResolveRelativeAddressSelf(2, 6).GetPtr(g_fnHasRenderCallQueue);
		tempBase.Offset(0x10).FindPattern("FF 15").ResolveRelativeAddressSelf(2, 6).GetPtr(g_fnAddRenderCallQueueItem);
		tempBase.Offset(0x40).FindPattern("48 FF").ResolveRelativeAddressSelf(3, 7).GetPtr(g_fnAdvanceRenderCallQueue);
	}
	virtual void GetCon(void) const { }
	virtual void Detour(const bool bAttach) const { };
};

#endif // MATQUEUEDRENDERCONTEXT_H
