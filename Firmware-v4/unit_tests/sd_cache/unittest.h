#ifndef UNITTEST_H
#define UNITTEST_H

#include <cstdint>
#include <memory>
#include <cstring>
#include <cstdio>

class VMemBlock
{
public:
	uintptr_t base;
	uintptr_t length;
	bool valid;

	uintptr_t data_start() const;
};

#define VBLOCK_64k	65536ULL
#define VBLOCK_512M (512 * 1024 * 1024ULL)

class MutexImpl;

class Mutex
{
private:
	std::unique_ptr<MutexImpl> impl;
	
public:
	void lock();
	void unlock();
	Mutex();
};

using PMutex = std::shared_ptr<Mutex>;

#define klog printf

class MutexList_t
{
public:
	PMutex Create();
};

extern MutexList_t MutexList;

VMemBlock vblock_alloc(size_t size, bool user, bool write, bool exec);

struct MemRegion
{
	uint64_t base;
	uint64_t length;
	bool valid;
};

class PhysMem_t
{
public:
	MemRegion acquire(uint64_t length);
};

extern PhysMem_t Pmem;

int vmem_map(uintptr_t vaddr, uintptr_t paddr, bool user, bool write, bool exec);

#define CACHE_LINE_SIZE     64ULL

enum CacheType_t { Data, Instruction, Both };
void InvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);
void CleanA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);
void CleanAndInvalidateA35Cache(uintptr_t base, uintptr_t length, CacheType_t ctype, bool for_dma = true);

#endif
