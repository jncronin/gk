#include "unittest.h"
#include <unordered_map>

#ifdef _MSC_VER
#include <Windows.h>

#pragma comment(lib, "mincore.lib")

#define MAP_FAILED ((void *)(intptr_t)(-1))
#define PROT_READ	1
#define PROT_WRITE	2
#define MAP_ANON	1
#define MAP_PRIVATE	2

void* mmap(void* addr, size_t length, int prot, int flags, int fd, int offset)
{
	/* Specify alignment */
	MEM_ADDRESS_REQUIREMENTS addressReqs = { 0 };
	addressReqs.Alignment = VBLOCK_64k;
	MEM_EXTENDED_PARAMETER param = { 0 };
	param.Type = MemExtendedParameterAddressRequirements;
	param.Pointer = &addressReqs;

	auto ret = VirtualAlloc2(GetCurrentProcess(), NULL,
		length, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &param, 1);
	if (!ret)
		return MAP_FAILED;
	return ret;
}
#else
#include <sys/mman.h>
#endif

/* We build a pseudo memory system including vmem->pmem mapping and caches 
	Both vmem and pmem pointers refer to virtual memory in the current
		address space.

	The vmem read/writes only affect the virtual memory side.  On vmem_map
		we update the virtual side with the contents of the physical side.
	On cache operations we do the approriate copies.
*/

/* Utility vmemblock functions */
uintptr_t VMemBlock::data_start() const
{
	return base;
}

/* Virtual memory functions */
VMemBlock vblock_alloc(size_t size, bool user, bool write, bool exec)
{
	auto ret = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);

	if (ret == MAP_FAILED)
	{
		VMemBlock vb;
		vb.base = 0;
		vb.length = 0;
		vb.valid = false;
		return vb;
	}
	else
	{
		VMemBlock vb;
		vb.base = (uintptr_t)ret;
		vb.length = size;
		vb.valid = true;
		return vb;
	}
}

/* Physical memory functions */
PhysMem_t Pmem;

MemRegion PhysMem_t::acquire(uintptr_t size)
{
	auto ret = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);

	if (ret == MAP_FAILED)
	{
		MemRegion vb;
		vb.base = 0;
		vb.length = 0;
		vb.valid = false;
		return vb;
	}
	else
	{
		MemRegion vb;
		vb.base = (uintptr_t)ret;
		vb.length = size;
		vb.valid = true;
		return vb;
	}
}

/* Mapping functions */
static std::unordered_map<uintptr_t, uintptr_t> vmem_to_pmem;

int vmem_map(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec)
{
	/* Only map one page */
	vmem_to_pmem[vaddr] = paddr;
	memcpy((void*)vaddr, (const void*)paddr, VBLOCK_64k);
	return 0;
}

/* Cache management functions */
void InvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma)
{
	if (ctype == CacheType_t::Instruction)
		return;	// not implemented

	while (length)
	{
		auto cur_vpage = base & ~(VBLOCK_64k - 1);
		auto cur_page_offset = base - cur_vpage;

		auto cur_ppage = vmem_to_pmem[cur_vpage];

		auto amount_for_page = cur_vpage + VBLOCK_64k - base;
		if (amount_for_page > length)
			amount_for_page = length;

		memcpy((void*)(cur_vpage + cur_page_offset),
			(const void*)(cur_ppage + cur_page_offset),
			amount_for_page);
		
		base += amount_for_page;
		length -= amount_for_page;
	}
}

void CleanA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma)
{
	if (ctype == CacheType_t::Instruction)
		return;	// not implemented

	while (length)
	{
		auto cur_vpage = base & ~(VBLOCK_64k - 1);
		auto cur_page_offset = base - cur_vpage;

		auto cur_ppage = vmem_to_pmem[cur_vpage];

		auto amount_for_page = cur_vpage + VBLOCK_64k - base;
		if (amount_for_page > length)
			amount_for_page = length;

		memcpy((void*)(cur_ppage + cur_page_offset),
			(const void*)(cur_vpage + cur_page_offset),
			amount_for_page);

		base += amount_for_page;
		length -= amount_for_page;
	}
}
