#include <cstdio>
#include <cstdint>
#include "logger.h"
#include "gic.h"
#include "vblock.h"
#include "vmem.h"

struct exception_regs
{
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18;
    uint64_t fp, lr;
};

static uint64_t TranslationFault_Handler(bool user, bool write, uint64_t address);

extern "C" uint64_t Exception_Handler(uint64_t esr, uint64_t far,
    uint64_t etype, exception_regs *regs, uint64_t lr)
{
    if(etype == 0x281)
    {
        gic_irq_handler();
        return 0;
    }
    else if(etype == 0x201 || etype == 0x401 || etype == 0x601)
    {
        // exception
        auto ec = (esr >> 26) & 0x3fULL;
        auto iss = esr & 0x1ffffffULL;
        if(ec == 0b100100 || ec == 0b100101)
        {
            // data abort

            auto dfsc = iss & 0x3fULL;

            if(dfsc >= 4 && dfsc <= 7)
            {
                // page fault

                bool user = (etype > 0x201) || (ec == 0b100100);
                bool write = (iss & (1ULL << 6)) != 0;

                return TranslationFault_Handler(user, write, far);
            }
        }
    }

    klog("EXCEPTION: type: %llx, esr: %llx, far: %llx, lr: %llx\n",
        etype, esr, far, lr);

    while(true);

    // we can change the address to return to by returning anything other than 0 here
    return 0;
}

static uint64_t UserThreadFault()
{
    klog("User thread fault\n");
    while(true);
}

static uint64_t SupervisorThreadFault()
{
    klog("Supervisor thread fault\n");
    while(true);
}

uint64_t TranslationFault_Handler(bool user, bool write, uint64_t far)
{
    klog("TranslationFault %s %s @ %llx\n", user ? "USER" : "SUPERVISOR",
        write ? "WRITE" : "READ", far);

    if(far >= 0x8000000000000000ULL)
    {
        if(user)
        {
            // user access to upper half
            return UserThreadFault();
        }

        // Check vblock for access
        auto [be, tag] = vblock_valid(far);
        if(!be.valid)
        {
            return SupervisorThreadFault();
        }

        if(tag & VBLOCK_TAG_GUARD_MASK)
        {
            // check against guard pages
            auto start = be.base;
            auto end = be.base + be.length;
            auto lower_guard = (tag >> VBLOCK_TAG_GUARD_LOWER_POS) & 0x3U;
            auto upper_guard = (tag >> VBLOCK_TAG_GUARD_UPPER_POS) & 0x3U;

            switch(lower_guard)
            {
                case GUARD_BITS_64k:
                    start += 64*1024ULL;
                    break;
                case GUARD_BITS_128k:
                    start += 128*1024ULL;
                    break;
                case GUARD_BITS_256k:
                    start += 256*1024ULL;
                    break;
                case GUARD_BITS_512k:
                    start += 512*1024ULL;
                    break;
            }
            switch(upper_guard)
            {
                case GUARD_BITS_64k:
                    end -= 64*1024ULL;
                    break;
                case GUARD_BITS_128k:
                    end -= 128*1024ULL;
                    break;
                case GUARD_BITS_256k:
                    end -= 256*1024ULL;
                    break;
                case GUARD_BITS_512k:
                    end -= 512*1024ULL;
                    break;
            }
            if(far < start || far >= end)
            {
                klog("pf: guard page hit\n");
                return SupervisorThreadFault();
            }
        }

        klog("pf: lazy map %llx, tag %lu\n", far, tag);

        if(vmem_map(far, 0, (tag & VBLOCK_TAG_USER) != 0,
            (tag & VBLOCK_TAG_WRITE) != 0,
            (tag & VBLOCK_TAG_EXEC) != 0) != 0)
        {
            return SupervisorThreadFault();
        }
        return 0;
    }

    while(true);
}
