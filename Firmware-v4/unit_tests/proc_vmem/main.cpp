#include <cstdio>
#include <assert.h>
#include "proc_vmem.h"

MemBlock MakeMemBlock(uintptr_t base, uintptr_t length)
{
	MemBlock ret;
	ret.b.base = base;
	ret.b.length = length;
	ret.b.valid = true;
	printf("memblock: %p - %p created\n", (void*)ret.b.base, (void*)ret.b.end());
	return ret;
}

MemBlock MakeMemBlock(uintptr_t length)
{
	MemBlock ret;
	ret.b.base = 0;
	ret.b.length = length;
	ret.b.valid = true;
	printf("memblock: any %p created\n", (void*)length);
	return ret;
}

int dump(MemBlock& mb)
{
	printf("dump: %p - %p\n", (void*)mb.b.base, (void*)mb.b.end());
	return 0;
}

int main()
{
	MapVBlockAllocator a;

	// replicate a typical address space
	assert(a.AllocFixed(MakeMemBlock(0x400000, 0x40000)).valid);
	assert(a.AllocFixed(MakeMemBlock(0x440000, 0x10000)).valid);
	assert(a.AllocFixed(MakeMemBlock(0x460000, 0x10000)).valid);
	assert(a.AllocFixed(MakeMemBlock(0x450000, 0x10000)).valid);

	a.Traverse(dump);

	assert(a.AllocFixed(MakeMemBlock(0x410000, 0x8)).valid == false);
	assert(a.IsAllocated(0x0).b.valid == false);
	assert(a.IsAllocated(0x400000).b.valid);
	assert(a.Dealloc(a.IsAllocated(0x440008)) == 0);
	

	a.Traverse(dump);

	assert(a.AllocAny(MakeMemBlock(0x200000)).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000), false).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000)).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000), false).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000)).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000), false).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000)).valid);
	assert(a.AllocAny(MakeMemBlock(0x200000), false).valid);

	assert(a.AllocAny(MakeMemBlock(2ULL * 1024 * 1024 * 1024), false).valid);

	a.Traverse(dump);

	return 0;
}
