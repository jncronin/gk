#include "unittest.h"

#ifdef _MSC_VER
#include <Windows.h>

class MutexImpl
{
private:
	HANDLE m;

public:
	MutexImpl()
	{
		m = CreateMutex(NULL, FALSE, NULL);
	}

	void lock()
	{
		WaitForSingleObject(m, INFINITE);
	}

	void unlock()
	{
		ReleaseMutex(m);
	}
};

#else
#include <pthread.h>

class MutexImpl
{
private:
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

public:
	void lock() { pthread_mutex_lock(&m); }
	void unlock() { pthread_mutex_unlock(&m); }
};
#endif

/* Mutex functions for unit tests */
MutexList_t MutexList;

PMutex MutexList_t::Create()
{
	return std::make_shared<Mutex>();
}

Mutex::Mutex()
{
	impl = std::make_unique<MutexImpl>();
}

void Mutex::lock()
{
	impl->lock();
}

void Mutex::unlock()
{
	impl->unlock();
}
