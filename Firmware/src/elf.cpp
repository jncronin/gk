#include "elf.h"
#include "SEGGER_RTT.h"
#include "memblk.h"

#include <cstring>

#include "thread.h"
#include "scheduler.h"

extern Scheduler s;

void elf_load_memory(const void *e)
{
    // pointer
    auto p = reinterpret_cast<const char *>(e);

    // check header
    auto ehdr = reinterpret_cast<const Elf32_Ehdr *>(p);
    if(ehdr->e_ident[0] != 0x7f ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F')
    {
        SEGGER_RTT_printf(0, "invalid magic\n");
        return;
    }

	// Confirm its a 32 bit file
	if(ehdr->e_ident[EI_CLASS] != ELFCLASS32)
	{
        SEGGER_RTT_printf(0, "invalid elf class\n");
        return;
	}

	// Confirm its a little-endian file
	if(ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
	{
        SEGGER_RTT_printf(0, "not lsb\n");
        return;
	}

	// Confirm its an executable file
	if(ehdr->e_type != ET_EXEC)
	{
        SEGGER_RTT_printf(0, "not exec\n");
        return;
	}

	// Confirm its for the ARM architecture
	if(ehdr->e_machine != EM_ARM)
	{
        SEGGER_RTT_printf(0, "not arm\n");
        return;
	}

    // Iterate through program headers to determine the absolute size required to load
    uintptr_t max_size = 0;
    auto phdrs = p + ehdr->e_phoff;
    for(unsigned int i = 0; i < ehdr->e_phnum; i++)
    {
        auto phdr = reinterpret_cast<const Elf32_Phdr *>(phdrs + i * ehdr->e_phentsize);

        if(phdr->p_type == PT_LOAD ||
            phdr->p_type == PT_ARM_EXIDX)
        {
            auto cur_max = phdr->p_vaddr + phdr->p_memsz;
            if(cur_max > max_size)
                max_size = cur_max;
        }
    }

    SEGGER_RTT_printf(0, "need %d bytes\n", max_size);

    // get a relevant memory block AXISRAM > SDRAM
    auto memblk = memblk_allocate(max_size, MemRegionType::AXISRAM);
    if(!memblk.valid)
    {
        memblk = memblk_allocate(max_size, MemRegionType::SDRAM);
    }
    if(!memblk.valid)
    {
        SEGGER_RTT_printf(0, "failed to allocate memory\n");
        return;
    }
    SEGGER_RTT_printf(0, "loading to %x\n", memblk.address);

    // TODO: allocate this MPU region for us

    // Load segments
    auto base_ptr = memblk.address;
    for(unsigned int i = 0; i < ehdr->e_phnum; i++)
    {
        auto phdr = reinterpret_cast<const Elf32_Phdr *>(phdrs + i * ehdr->e_phentsize);

        if(phdr->p_type == PT_LOAD ||
            phdr->p_type == PT_ARM_EXIDX)
        {
            if(phdr->p_filesz)
            {
                memcpy((void *)(base_ptr + phdr->p_vaddr),
                    p + phdr->p_offset,
                    phdr->p_filesz);
            }
            if(phdr->p_filesz != phdr->p_memsz)
            {
                memset((void *)(base_ptr + phdr->p_vaddr + phdr->p_filesz),
                    0, phdr->p_memsz - phdr->p_filesz);
            }
        }
    }

    // Perform relocations
    auto shdrs = p + ehdr->e_shoff;
    for(unsigned int i = 0; i < ehdr->e_shnum; i++)
    {
        auto shdr = reinterpret_cast<const Elf32_Shdr *>(shdrs + i * ehdr->e_shentsize);
        if(shdr->sh_type != SHT_REL)
            continue;
        SEGGER_RTT_printf(0, "reloc section %d\n", i);

        auto symtab_idx = shdr->sh_link;
        auto relsect_idx = shdr->sh_info;
        auto entsize = shdr->sh_entsize;
        auto nentries = shdr->sh_size / entsize;

        auto symtab = reinterpret_cast<const Elf32_Shdr *>(shdrs + symtab_idx * ehdr->e_shentsize);
        [[maybe_unused]] auto relsect = reinterpret_cast<const Elf32_Shdr *>(shdrs + relsect_idx * ehdr->e_shentsize);

        auto relocs = p + shdr->sh_offset;

        for(unsigned int j = 0; j < nentries; j++)
        {
            auto rel = reinterpret_cast<const Elf32_Rel *>(relocs + j * entsize);

            auto r_sym_idx = rel->r_info >> 8;
            auto r_type = rel->r_info & 0xff;

            auto r_sym = reinterpret_cast<const Elf32_Sym *>(p + symtab->sh_offset + r_sym_idx *
                symtab->sh_entsize);

            void *dest = (void *)(base_ptr + rel->r_offset);
            uint32_t A = *(uint32_t *)dest;
            uint32_t P = (uint32_t)dest;
            [[maybe_unused]] uint32_t Pa = P & 0xfffffffc;
            uint32_t T = ((r_sym->st_info & 0xf) == STT_FUNC) ? 1 : 0;
            uint32_t S = base_ptr + r_sym->st_value;
            uint32_t mask = 0xffffffff;
            uint32_t value = 0;
            uint32_t nvalue = 0;

            /*if((uint32_t)dest >= 0x240020d4 && (uint32_t)dest <= 0x240020d8)
            {
                __asm volatile
                (
                    "bkpt  \n"
                    ::: "memory"
                );
            }*/

            switch(r_type)
            {
                case R_ARM_TARGET1:
                case R_ARM_ABS32:
                    mask = 0xffffffffUL;
                    A &= mask;
                    value = (S + A) | T;
                    nvalue = value;
                    break;

                case R_ARM_THM_JUMP24:
                case R_ARM_THM_CALL:
                    mask = 0x2fff07ffUL;
                    {
                        auto imm10 = A & 0x3ffUL;
                        auto Sp = (A >> 10) & 0x1;
                        auto imm11 = (A >> 16) & 0x7ffUL;
                        auto J1 = (A >> 29) & 0x1;
                        auto J2 = (A >> 27) & 0x1;

                        auto I1 = (J1 ^ Sp) ? 0UL : 1UL;
                        auto I2 = (J2 ^ Sp) ? 0UL : 1UL;

                        A = (imm11 << 1) | (imm10 << 12) | (I2 << 22) | (I1 << 23) | (Sp << 24);
                        if(Sp)
                            A |= 0xfe000000UL;
                    }

                    /* We are using -q for link, therefore the value stored in the opcode has already
                        been relocated according to:
                            value = ((S + A) | T) - P
                            
                            
                    */
                    
                    value = ((S + A) | T) - P;

                    {
                        auto Sp = (value >> 24) & 0x1;
                        auto I1 = (value >> 23) & 0x1;
                        auto I2 = (value >> 22) & 0x1;
                        auto imm10 = (value >> 12) & 0x3ffUL;
                        auto imm11 = (value >> 1) & 0x7ffUL;
                        auto nSp = Sp ? 0UL : 1UL;
                        auto J1 = I1 ? Sp : nSp;
                        auto J2 = I2 ? Sp : nSp;

                        nvalue = (J1 << 29) | (J2 << 27) | (imm11 << 16) |
                            (S << 10) | imm10;
                    }
                    break;

#if 0
                case R_ARM_THM_JUMP24:
                    mask = 0x2fff03ffUL;
                    {
                        auto imm10 = A & 0x3ffUL;
                        auto Sp = (A & 0x400UL) ? 1UL : 0UL;
                        auto imm11 = (A >> 16) & 0x7ffUL;
                        auto J1 = A & 0x20000000UL;
                        auto J2 = A & 0x08000000UL;

                        auto I1 = ((J1 && Sp) || (J1 == 0 && S == 0)) ? 1UL : 0UL;
                        auto I2 = ((J2 && Sp) || (J2 == 0 && S == 0)) ? 1UL : 0UL;

                        A = (imm11 << 1) | (imm10 << 12) | (I2 << 22) | (I1 << 23) | (Sp << 24);
                        if(Sp)
                            A |= 0xfe000000UL;
                    }
                    
                    value = ((S + A) | T) - P;

                    {
                        auto Sp = (value & 0x80000000UL) ? 1UL : 0UL;
                        auto I1 = (value & (1UL << 23)) ? 1UL : 0UL;
                        auto I2 = (value & (1UL << 22)) ? 1UL : 0UL;
                        auto imm10 = (value >> 12) & 0x3ffUL;
                        auto imm11 = (value >> 1) & 0x7ffUL;
                        auto J1 = ((I1 && Sp) || (I1 == 0 && Sp == 0)) ? 1UL : 0UL;
                        auto J2 = ((I2 && Sp) || (I2 == 0 && Sp == 0)) ? 1UL : 0UL;

                        value = (J1 << 29) | (J2 << 27) | (imm11 << 16) |
                            (S << 10) | imm10;
                    }
                    break;
#endif

                case R_ARM_PREL31:
                    mask = 0x7fffffffUL;
                    if(A & (1UL << 30))
                        A |= 0xf0000000UL;
                    value = ((S + A) | T) - P;
                    nvalue = value;

                    break;

                default:
                    SEGGER_RTT_printf(0, "unknown rel type %d\n", r_type);
                    return;
            }

            auto cval = *(uint32_t *)dest;
            cval &= ~mask;
            cval |= (nvalue & mask);
            *(uint32_t *)dest = cval;

            SEGGER_RTT_printf(0, "%d, %d (%d): dest: %x, S: %x, A: %x, P: %x, T: %x, value: %x\n",
                i, j, r_type, (uint32_t)dest, S, A, P, T, value);
        }
    }

    // get start address
    auto start = (void (*)(void *))(base_ptr + ehdr->e_entry);

    s.Schedule(Thread::Create("elffile", start, nullptr, true, 8,
        CPUAffinity::Either, 4096,
        MPUGenerate(base_ptr, max_size, 6, true, MemRegionAccess::RW, MemRegionAccess::NoAccess, WBWA_NS)));

    SEGGER_RTT_printf(0, "successfully loaded\n");
}