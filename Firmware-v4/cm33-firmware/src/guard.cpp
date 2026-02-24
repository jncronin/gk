#include "guard.h"

CriticalGuard::CriticalGuard()
{
    cpsr = DisableInterrupts();
}

CriticalGuard::~CriticalGuard()
{
    RestoreInterrupts(cpsr);
}
