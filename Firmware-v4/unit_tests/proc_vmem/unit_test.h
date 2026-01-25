#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#define GK_PROCESS_INTERFACE_START  0x3ffe0000000ULL

class Mutex
{

};

class MutexGuard
{
public:
	MutexGuard(Mutex& m) {}
};

class File
{ };

#endif

