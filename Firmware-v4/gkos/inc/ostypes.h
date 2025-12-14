#ifndef OSTYPES_H
#define OSTYPES_H

#include <cstdint>
#include <memory>
#include "gkos_vmem.h"

class Process;
class Thread;

using id_t = unsigned int;

struct pidtid
{
    id_t pid, tid;
};

using PProcess = std::shared_ptr<Process>;
using PThread = std::shared_ptr<Thread>;

struct MemRegion
{
    uint64_t base;
    uint64_t length;
    bool valid;
};

struct PMemBlock : public MemRegion
{
    bool is_shared = false;
};

#define GUARD_BITS_64k                      0x1
#define GUARD_BITS_512k                     0x2
#define GUARD_BITS_1M                       0x3
#define GUARD_BITS_MAX                      0x3

struct VMemBlock : public MemRegion
{   
    bool user;
    bool write;
    bool exec;
    unsigned int lower_guard, upper_guard;
    unsigned int memory_type = MT_NORMAL;

    inline uint64_t guard_size (unsigned int guard_bits) const
    {
        switch(guard_bits)
        {
            case 0:
                return 0;
            case GUARD_BITS_64k:
                return 65536;
            case GUARD_BITS_512k:
                return 512*1024;
            case GUARD_BITS_1M:
                return 1024*1024;
            default:
                return 0;
        }
    }
    inline uint64_t end() const { return base + length; }
    inline uint64_t data_start() const { return base + guard_size(lower_guard); }
    inline uint64_t data_end() const { return end() - guard_size(upper_guard); }
    inline uint64_t data_length() const { return data_end() - data_start(); }
};

static constexpr VMemBlock InvalidVMemBlock()
{
    return { MemRegion { .base = 0, .length = 0, .valid = false } };
}

static constexpr PMemBlock InvalidPMemBlock()
{
    return { MemRegion { .base = 0, .length = 0, .valid = false } };
}

// TCB for saved state of a kernel stack
struct thread_saved_state
{
    uint64_t sp_el0;
    uint64_t sp_el1;
    uint64_t r19, r20, r21, r22, r23, r24, r25, r26, r27, r28;
    uint64_t ttbr0;
    uint64_t tpidr_el0;

    uint64_t res1, res2;    // for 32-byte alignment for q registers

    // FP registers - 24*128 bits (q0-q7 already saved)
    uint64_t fpu_regs[48];
};

static_assert(sizeof(thread_saved_state) == 512);

#endif
