#include <cstdio>
#include <assert.h>
#include "proc_vmem.h"

/* A special memblock type that reports destruction */
class VerboseMemBlock : public MemBlock
{
public:
	VerboseMemBlock(uintptr_t base, uintptr_t length)
	{
		b.base = base;
		b.length = length;
		b.valid = true;
		printf("memblock: %p - %p created\n", (void*)b.base, (void*)b.end());
	}

	VerboseMemBlock(uintptr_t length)
	{
		b.base = 0;
		b.length = length;
		b.valid = true;
		printf("memblock: any %p created\n", (void*)length);
	}

	~VerboseMemBlock()
	{
		printf("memblock: %p - %p destroyed\n", (void *)b.base, (void *)b.end());
	}

	int FillFirst(uintptr_t)
	{
		return 0;
	}

	int FillSubsequent(uintptr_t)
	{
		return 0;
	}

	int Sync(uintptr_t)
	{
		return 0;
	}
};

int dump(std::unique_ptr<MemBlock>& mb)
{
	printf("dump: %p - %p\n", (void*)mb->b.base, (void*)mb->b.end());
	return 0;
}

int main()
{
	MapVBlockAllocator a;

	// replicate a typical address space
	assert(a.AllocFixed(std::make_unique<VerboseMemBlock>(0x400000, 0x40000)).first);
	assert(a.AllocFixed(std::make_unique<VerboseMemBlock>(0x440000, 0x10000)).first);
	assert(a.AllocFixed(std::make_unique<VerboseMemBlock>(0x460000, 0x10000)).first);
	assert(a.AllocFixed(std::make_unique<VerboseMemBlock>(0x450000, 0x10000)).first);

	a.Traverse(dump);

	assert(a.AllocFixed(std::make_unique<VerboseMemBlock>(0x410000, 0x8)).first == false);
	assert(a.IsAllocated(0x0) == nullptr);
	assert(a.IsAllocated(0x400000)->b.valid);
	assert(a.Dealloc(a.IsAllocated(0x440008)) == 0);

	a.Traverse(dump);

	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000)).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000), false).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000)).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000), false).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000)).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000), false).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000)).first);
	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(0x200000), false).first);

	assert(a.AllocAny(std::make_unique<VerboseMemBlock>(2ULL * 1024 * 1024 * 1024), false).first);

	a.Traverse(dump);

	return 0;
}
