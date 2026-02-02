#include "osmutex.h"

bool BinarySemaphore::Wait(kernel_time tout)
{
    return ss.Wait(SimpleSignal::Set, 0, tout);
}

bool BinarySemaphore::WaitOnce(kernel_time tout)
{
    return ss.WaitOnce(SimpleSignal::Set, 0, tout);
}

void BinarySemaphore::Signal()
{
    ss.Signal(SimpleSignal::Set, 1U);
}

void BinarySemaphore::Clear()
{
    ss.Signal(SimpleSignal::Set, 0U);
}

bool BinarySemaphore::Value()
{
    return ss.Value() != 0U;
}

bool CountingSemaphore::Wait(kernel_time tout)
{
    return ss.Wait(SimpleSignal::Sub, 1U, tout);
}

bool CountingSemaphore::WaitOnce(kernel_time tout)
{
    return ss.WaitOnce(SimpleSignal::Sub, 1U, tout);
}

void CountingSemaphore::Signal()
{
    ss.Signal(SimpleSignal::Add, 1U);
}

unsigned int CountingSemaphore::Value()
{
    return ss.Value();
}
