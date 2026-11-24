#include <tier0/jobthread.h>

void VJobThread::Detour(const bool bAttach) const
{
	DetourSetup( &v_JT_HelpWithJobTypesOrSleep, &JT_HelpWithJobTypesOrSleep, bAttach );
	DetourSetup( &v_JT_WakeThreadsByMask, &JT_WakeThreadsByMask, bAttach );
}
