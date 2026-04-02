#include <cstdio>
#include <list>
#include <assert.h>
#include "block_allocator.h"
#include "coalescing_block_allocator.h"

using ba_t = BlockAllocator<int>;
using MemBlock = ba_t::BlockAddress;

class ckey_t
{
public:
	std::list<uintptr_t> base_addrs;

	void CoalesceFrom(ckey_t& other, bool other_is_prev)
	{
		if (other_is_prev)
		{
			base_addrs.insert(base_addrs.begin(),
				other.base_addrs.begin(), other.base_addrs.end());
		}
		else
		{
			base_addrs.insert(base_addrs.end(),
				other.base_addrs.begin(), other.base_addrs.end());
		}
	}

	ckey_t() = default;
	ckey_t(uintptr_t val) { base_addrs.push_back(val); }
};

using cba_t = CoalescingBlockAllocator<ckey_t>;
using cMemBlock = cba_t::BlockAddress;

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

cMemBlock MakecMemBlock(uintptr_t base, uintptr_t length)
{
	cMemBlock ret;
	ret.start = base;
	ret.length = length;
	printf("cmemblock: %p - %p created\n", (void*)ret.start, (void*)ret.end());
	return ret;
}

cMemBlock MakecMemBlock(uintptr_t length)
{
	cMemBlock ret;
	ret.start = 0;
	ret.length = length;
	printf("cmemblock: any %p created\n", (void*)length);
	return ret;
}

int dump(const MemBlock& mb)
{
	printf("dump: %p - %p\n", (void*)mb.start, (void*)mb.end());
	return 0;
}

int dump(const cMemBlock& mb, const ckey_t &v)
{
	printf("dump: %p - %p (", (void*)mb.start, (void*)mb.end());
	bool first = true;
	for (const auto& addr : v.base_addrs)
	{
		if (!first)
			printf(", ");
		first = false;
		printf("%llu", addr);
	}
	printf(")\n");
	return 0;
}


int main()
{
	ba_t a(65536, 0x100000000 - 65536);

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

	/* Check iterators of things which span a range */
	auto from = a.LeftBlock(0x400000);
	auto to = a.RightBlock(0x86ffff);
	printf("\niter from %p to %p\n", (void*)from->first.start,
		(void*)to->first.start);
	for (; from != to; from++)
	{
		dump(from->first);
	}


	cba_t ca({ 65536, 0x100000000 - 65536 });

	// replicate a typical address space
	assert(ca.AllocFixed(MakecMemBlock(0x400000, 0x40000), ckey_t(1)) != ca.end());
	assert(ca.AllocFixed(MakecMemBlock(0x440000, 0x10000), ckey_t(2)) != ca.end());
	assert(ca.AllocFixed(MakecMemBlock(0x460000, 0x10000), ckey_t(3)) != ca.end());
	assert(ca.AllocFixed(MakecMemBlock(0x450000, 0x10000), ckey_t(4)) != ca.end());

	printf("\n");
	for (const auto& cmb : ca)
		dump(cmb.first, cmb.second);

	assert(ca.AllocFixed(MakecMemBlock(0x410000, 0x8), ckey_t(5)) == ca.end());
	assert(ca.IsAllocated(0x0) == ca.end());
	assert(ca.IsAllocated(0x400000) != ca.end());

	printf("\n");
	for (const auto& cmb : ca)
		dump(cmb.first, cmb.second);

	assert(ca.AllocAny(0x200000, ckey_t(6)) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(7), false) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(8)) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(9), false) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(10)) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(11), false) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(12)) != ca.end());
	assert(ca.AllocAny(0x200000, ckey_t(13), false) != ca.end());

	assert(ca.AllocAny((2ULL * 1024 * 1024 * 1024), ckey_t(14), false) != ca.end());

	assert(ca.AllocAny(0x200000, ckey_t(15), true) != ca.end());

	printf("\n");
	for (const auto& cmb : ca)
		dump(cmb.first, cmb.second);

	return 0;
}
