#include <cstdio>
#include <assert.h>
#include "block_allocator.h"

using ba_t = BlockAllocator<int>;
using MemBlock = ba_t::BlockAddress;

MemBlock MakeMemBlock(uintptr_t base, uintptr_t length)
{
	MemBlock ret;
	ret.start = base;
	ret.length = length;
	printf("memblock: %p - %p created\n", (void*)ret.start, (void*)ret.end());
	return ret;
}

MemBlock MakeMemBlock(uintptr_t length)
{
	MemBlock ret;
	ret.start = 0;
	ret.length = length;
	printf("memblock: any %p created\n", (void*)length);
	return ret;
}

int dump(const MemBlock& mb)
{
	printf("dump: %p - %p\n", (void*)mb.start, (void*)mb.end());
	return 0;
}

int main()
{
	ba_t a({ 65536, 0x100000000 - 65536 });

	// replicate a typical address space
	assert(a.AllocFixed(MakeMemBlock(0x400000, 0x40000)) != a.end());
	assert(a.AllocFixed(MakeMemBlock(0x440000, 0x10000)) != a.end());
	assert(a.AllocFixed(MakeMemBlock(0x460000, 0x10000)) != a.end());
	assert(a.AllocFixed(MakeMemBlock(0x450000, 0x10000)) != a.end());

	printf("\n");
	for (const auto& mb : a)
		dump(mb.first);

	assert(a.AllocFixed(MakeMemBlock(0x410000, 0x8)) == a.end());
	assert(a.IsAllocated(0x0) == a.end());
	assert(a.IsAllocated(0x400000) != a.end());
	assert(a.Dealloc(a.IsAllocated(0x440008)) != a.end());
	
	printf("\n");
	for (const auto& mb : a)
		dump(mb.first);

	assert(a.AllocAny(0x200000) != a.end());
	assert(a.AllocAny(0x200000, 0, false) != a.end());
	assert(a.AllocAny(0x200000) != a.end());
	assert(a.AllocAny(0x200000, 0, false) != a.end());
	assert(a.AllocAny(0x200000) != a.end());
	assert(a.AllocAny(0x200000, 0, false) != a.end());
	assert(a.AllocAny(0x200000) != a.end());
	assert(a.AllocAny(0x200000, 0, false) != a.end());

	assert(a.AllocAny((2ULL * 1024 * 1024 * 1024), false) != a.end());

	assert(a.AllocAny(0x200000, 0, true) != a.end());

	printf("\n");
	for (const auto& mb : a)
		dump(mb.first);

	return 0;
}
