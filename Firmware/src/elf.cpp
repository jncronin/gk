#include "elf.h"
#include "SEGGER_RTT.h"
#include "memblk.h"

#include <cstring>

#include "thread.h"
#include "scheduler.h"
#include "region_allocator.h"

#include "cache.h"
#include "ossharedmem.h"

#include "syscalls.h"
#include "syscalls_int.h"

static size_t get_arg_length(const std::string &pname, const std::vector<std::string> &params);
static void init_args(const std::string &pname, const std::vector<std::string> &params,
    void *buf);

class trampoline_provider
{
    private:
        std::map<uint32_t, uint32_t> registered_trampolines;
        uint32_t max_trampoline;
        uint32_t cur_trampoline;

    public:
        trampoline_provider(uint32_t _cur_trampoline, uint32_t _max_trampoline) :
            max_trampoline(_max_trampoline), cur_trampoline(_cur_trampoline) {}
        uint32_t alloc(uint32_t target)
        {
            auto iter = registered_trampolines.find(target);
            if(iter != registered_trampolines.end())
            {
                return iter->second;
            }
            cur_trampoline += 12U;
            if(cur_trampoline > max_trampoline)
                return 0U;
            registered_trampolines[target] = cur_trampoline - 12U;
            return cur_trampoline - 12U;
        }
        uint32_t get_cur() { return cur_trampoline; }
};

static int load_from(int fd, unsigned int offset,
    void *buf, unsigned int len)
{
    if(deferred_call(syscall_lseek, fd, offset, SEEK_SET) == (off_t)-1)
    {
        return -1;
    }
    return deferred_call(syscall_read, fd, (char *)buf, len);
}

template<typename T> static int load_from(int fd, unsigned int offset, T* buf)
{
    return load_from(fd, offset, (void *)buf, sizeof(T));
}

static MemRegion memblk_allocate_for_elf(size_t nbytes)
{
    auto mr = memblk_allocate(nbytes, MemRegionType::AXISRAM, "elf structure");
    //if(!mr.valid) mr = memblk_allocate(nbytes, MemRegionType::SRAM);
    if(!mr.valid) mr = memblk_allocate(nbytes, MemRegionType::SDRAM, "elf structure");
    return mr;
}

int elf_load_fildes(int fd,
	Process &p,
	uint32_t *epoint,
    const std::string &pname,
	uint32_t stack_end,
	const std::vector<std::string> &params)
{
    // load and check header
    Elf32_Ehdr ehdr;
    if(deferred_call(syscall_read, fd, (char *)&ehdr, sizeof(ehdr)) != sizeof(ehdr))
    {
        return -1;
    }

    if(ehdr.e_ident[0] != 0x7f ||
        ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' ||
        ehdr.e_ident[3] != 'F')
    {
        klog("invalid magic\n");
        return -1;
    }

	// Confirm its a 32 bit file
	if(ehdr.e_ident[EI_CLASS] != ELFCLASS32)
	{
        klog("invalid elf class\n");
        return -1;
	}

	// Confirm its a little-endian file
	if(ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
	{
        klog("not lsb\n");
        return -1;
	}

	// Confirm its an executable file
	if(ehdr.e_type != ET_EXEC)
	{
        klog("not exec\n");
        return -1;
	}

	// Confirm its for the ARM architecture
	if(ehdr.e_machine != EM_ARM)
	{
        klog("not arm\n");
        return -1;
	}


    // Iterate through program headers to determine the absolute size required to load
    uintptr_t max_size = 0;
    for(unsigned int i = 0; i < ehdr.e_phnum; i++)
    {
        Elf32_Phdr phdr;

        if(deferred_call(syscall_lseek, fd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) == (off_t)-1)
        {
            return -1;
        }
        if(deferred_call(syscall_read, fd, (char *)&phdr, sizeof(phdr)) != sizeof(phdr))
        {
            return -1;
        }

        if(phdr.p_type == PT_LOAD ||
            phdr.p_type == PT_ARM_EXIDX)
        {
            auto cur_max = phdr.p_vaddr + phdr.p_memsz;
            if(cur_max > max_size)
                max_size = cur_max;
        }
        else if(phdr.p_type == PT_TLS)
        {
            p.has_tls = true;
            p.tls_base = phdr.p_vaddr;
            p.tls_memsz = phdr.p_memsz;
            p.tls_filsz = phdr.p_filesz;
        }
    }

    // Create region for userspace thread finalizer code, aligned on 32-byte boundary
    if(p.is_priv)
    {
        p.thread_finalizer = 0;
    }
    else
    {
        if(max_size & 0x1fU)
        {
            max_size = (max_size + 0x1fU) & ~0x1fU;
        }

        p.thread_finalizer = max_size;
        max_size += 32;
    }

    // Create region for arguments
    auto arg_length = get_arg_length(pname, params);
    auto arg_offset = max_size;
    max_size += arg_length;

    klog("need %d bytes\n", max_size);

    // get a relevant memory block AXISRAM > SDRAM
    auto memblk = memblk_allocate(max_size, MemRegionType::AXISRAM, pname + " code/data");
    if(!memblk.valid)
    {
        memblk = memblk_allocate(max_size, MemRegionType::SDRAM, pname + " code/data");
    }
    if(!memblk.valid)
    {
        klog("failed to allocate memory\n");
        return -1;
    }
    klog("loading to %x\n", memblk.address);
    [[maybe_unused]] auto arg_base = memblk.address + arg_offset;
    p.code_data = memblk;

    // Load segments
    auto base_ptr = memblk.address;
    if(p.has_tls)
    {
        p.tls_base += base_ptr;
    }

    for(unsigned int i = 0; i < ehdr.e_phnum; i++)
    {
        Elf32_Phdr phdr;

        if(deferred_call(syscall_lseek, fd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) == (off_t)-1)
        {
            return -1;
        }
        if(deferred_call(syscall_read, fd, (char *)&phdr, sizeof(phdr)) != sizeof(phdr))
        {
            return -1;
        }

        if(phdr.p_type == PT_LOAD ||
            phdr.p_type == PT_ARM_EXIDX)
        {
            if(phdr.p_filesz)
            {
                if(deferred_call(syscall_lseek, fd, phdr.p_offset, SEEK_SET) == (off_t)-1)
                {
                    return -1;
                }
                if(deferred_call(syscall_read, fd, (char *)(base_ptr + phdr.p_vaddr), phdr.p_filesz) != (int)phdr.p_filesz)
                {
                    return -1;
                }
            }
            if(phdr.p_filesz != phdr.p_memsz)
            {
                SharedMemoryGuard smg((const void *)(base_ptr + phdr.p_vaddr + phdr.p_filesz),
                    phdr.p_memsz - phdr.p_filesz, false, true);
                memset((void *)(base_ptr + phdr.p_vaddr + phdr.p_filesz),
                    0, phdr.p_memsz - phdr.p_filesz);
            }
        }
    }

    // see if we have a hot section to handle separately (i.e. load to ITCM)
    // they are currently stored within the main .text section bounded by __start_hot and __end_hot
    unsigned int shot = 0;
    unsigned int ehot = 0;
    if(p.use_hot_region)
    {
        for(unsigned int i = 0; i < ehdr.e_shnum; i++)
        {
            Elf32_Shdr shdr;
            if(load_from(fd, ehdr.e_shoff + i * ehdr.e_shentsize, &shdr) != sizeof(shdr))
            {
                klog("elf: couldn't load section header\n");
                return -1;
            }
            if(shdr.sh_type != SHT_SYMTAB)
                continue;

            auto strtab_idx = shdr.sh_link;
            auto first_glob = shdr.sh_info;
            auto entsize = shdr.sh_entsize;
            auto nentries = shdr.sh_size / entsize;

            Elf32_Shdr strtab;
            if(load_from(fd, ehdr.e_shoff + strtab_idx * ehdr.e_shentsize, &strtab) != sizeof(strtab))
            {
                klog("elf: couldn't load .strtab section header\n");
                return -1;
            }

            // load symtab and strtab in full
            auto mr_shdr = memblk_allocate_for_elf(shdr.sh_size);
            if(!mr_shdr.valid)
            {
                klog("elf: couldn't allocate %d bytes for section\n", shdr.sh_size);
                return -1;
            }
            auto mr_strtab = memblk_allocate_for_elf(strtab.sh_size);
            if(!mr_strtab.valid)
            {
                klog("elf: couldn't allocate %d bytes for strtab\n", shdr.sh_size);
                memblk_deallocate(mr_shdr);
                return -1;
            }

            if(load_from(fd, shdr.sh_offset, (void *)mr_shdr.address, shdr.sh_size) != (int)shdr.sh_size)
            {
                klog("elf: couldn't load section %d\n", i);
                memblk_deallocate(mr_shdr);
                memblk_deallocate(mr_strtab);
                return -1;
            }
            if(load_from(fd, strtab.sh_offset, (void *)mr_strtab.address, strtab.sh_size) != (int)strtab.sh_size)
            {
                klog("elf: couldn't load strtab %d\n", strtab_idx);
                memblk_deallocate(mr_shdr);
                memblk_deallocate(mr_strtab);
                return -1;
            }

            for(unsigned int j = first_glob; j < nentries; j++)
            {
                auto sym = reinterpret_cast<Elf32_Sym *>(mr_shdr.address + j * entsize);
                if(!strcmp("__start_hot", (char *)(mr_strtab.address + sym->st_name)))
                {
                    shot = sym->st_value;
                }
                if(!strcmp("__end_hot", (char *)(mr_strtab.address + sym->st_name)))
                {
                    ehot = sym->st_value;
                }
                if(shot && ehot)
                    break;
            }

            memblk_deallocate(mr_shdr);
            memblk_deallocate(mr_strtab);

            if(shot && ehot)
                break;
        }
    }

    MemRegion mr_itcm = InvalidMemregion();
    if(shot && shot != ehot)
    {
        klog("elf: found hot section: %x to %x\n", shot, ehot);

        auto hot_len = ehot - shot;

        if(GetCoreID() == 0)
        {
            mr_itcm = memblk_allocate(hot_len, MemRegionType::ITCM, pname + " .text.hot");
        }
        if(!mr_itcm.valid)
        {
            mr_itcm = memblk_allocate(hot_len, MemRegionType::AXISRAM, pname + " .text.hot");
        }
        if(!mr_itcm.valid)
        {
            mr_itcm = memblk_allocate(hot_len, MemRegionType::SRAM, pname + " .text.hot");
        }

        if(mr_itcm.valid)
        {
            klog("elf: loading hot section to %08x\n", mr_itcm.address);
            memcpy((void *)mr_itcm.address, (void *)(base_ptr + shot), ehot - shot);
        }
        else
        {
            klog("elf: unable to allocate ITCM memory for hot section\n");
        }
    }

    // provide entry point
    if(epoint)
    {
        *epoint = base_ptr + ehdr.e_entry;
    }

    // hope we have enough space to add inter-section trampoline code
    unsigned int hot_section_end = ehot - shot;
    unsigned int normal_section_end = max_size;
    hot_section_end = (hot_section_end + 0x3U) & ~0x3U;
    normal_section_end = (normal_section_end + 0x3U) & ~0x3U;
    trampoline_provider tp_hot(mr_itcm.address + hot_section_end, mr_itcm.address + mr_itcm.length);
    trampoline_provider tp_norm(base_ptr + normal_section_end, base_ptr + memblk.length);

    // perform relocations
    for(unsigned int i = 0; i < ehdr.e_shnum; i++)
    {
        Elf32_Shdr shdr;
        if(load_from(fd, ehdr.e_shoff + i * ehdr.e_shentsize, &shdr) != sizeof(shdr))
        {
            return -1;
        }
        if(shdr.sh_type != SHT_REL)
            continue;
        klog("elf: reloc section %u\n", i);

        //auto symtab_idx = shdr.sh_link;
        auto relsect_idx = shdr.sh_info;
        auto entsize = shdr.sh_entsize;
        auto nentries = shdr.sh_size / entsize;

        Elf32_Shdr relsect;
        if(load_from(fd, ehdr.e_shoff + relsect_idx * ehdr.e_shentsize, &relsect) != sizeof(relsect))
        {
            klog("elf: couldn't load relsect section header %u\n", relsect_idx);
            return -1;
        }

        if(!(relsect.sh_flags & SHF_ALLOC))
        {
            continue;
        }

        // load shdr and symtab in full
        auto mr_shdr = memblk_allocate_for_elf(shdr.sh_size);
        if(!mr_shdr.valid)
        {
            klog("elf: couldn't allocate %u bytes for reloc section %u\n", shdr.sh_size, i);
            return -1;
        }
        if(load_from(fd, shdr.sh_offset, (void *)mr_shdr.address, shdr.sh_size) != (int)shdr.sh_size)
        {
            memblk_deallocate(mr_shdr);
            return -1;
        }

        for(unsigned int j = 0; j < nentries; j++)
        {
            auto rel = reinterpret_cast<Elf32_Rel *>(mr_shdr.address + j * entsize);

            //auto r_sym_idx = rel->r_info >> 8;
            auto r_type = rel->r_info & 0xff;

            if(r_type == R_ARM_NONE)
                continue;

            uint32_t src = rel->r_offset;
            auto orig_reloc_val = *(volatile uint32_t *)(src + base_ptr);
            uint32_t target;

            switch(r_type)
            {
                case R_ARM_TARGET1:
                case R_ARM_ABS32:
                    target = orig_reloc_val;
                    break;

                case R_ARM_TARGET2:
                case R_ARM_REL32:
                case R_ARM_PREL31:
                    // ((S + A) | T) - P
                    {
                        uint32_t se_reloc_val;
                        if(r_type == R_ARM_REL32 || r_type == R_ARM_TARGET2)
                        {
                            se_reloc_val = orig_reloc_val;
                        }
                        else if((orig_reloc_val >> 30) & 0x1)
                        {
                            se_reloc_val = orig_reloc_val | 0x80000000U;
                        }
                        else
                        {
                            se_reloc_val = orig_reloc_val;
                        }

                        target = se_reloc_val + src;
                    }
                    break;

                case R_ARM_THM_CALL:
                case R_ARM_THM_JUMP24:
                    /* Interpret BL/B opcode */
                    {
                        auto J1 = (orig_reloc_val >> (13+16)) & 0x1U;
                        auto J2 = (orig_reloc_val >> (11+16)) & 0x1U;
                        auto imm11 = (orig_reloc_val >> (16)) & 0x7ffU;
                        auto S = (orig_reloc_val >> 10) & 0x1U;
                        auto imm10 = (orig_reloc_val) & 0x3ffU;

                        auto I1 = (J1 ^ S) ? 0U : 1U;
                        auto I2 = (J2 ^ S) ? 0U : 1U;

                        auto imm32 = (imm11 << 1) |
                            (imm10 << 12) |
                            (I2 << 22) |
                            (I1 << 23) |
                            (S << 24);
                        if(S) imm32 |= 0xff000000U;

                        target = rel->r_offset + 4 + imm32;
                    }
                    break;

                case R_ARM_TLS_LE32:
                    target = orig_reloc_val;
                    break;

                default:
                    // for now...
                    klog("elf: cannot deal with target from %d yet\n", r_type);
                    target = orig_reloc_val;
                    BKPT();
                    break;
            }

            bool relsrc_is_hot = mr_itcm.valid && (src >= shot) && (src < ehot);
            bool reltarget_is_hot = mr_itcm.valid && (r_type != R_ARM_TLS_LE32) &&
                (target >= shot) && (target < ehot);

            if(relsrc_is_hot)
            {
                src += mr_itcm.address - shot;
            }
            else
            {
                src += base_ptr;
            }
            if(reltarget_is_hot)
            {
                target += mr_itcm.address - shot;
            }
            else if(r_type != R_ARM_TLS_LE32)
            {
                target += base_ptr;
            }

            if(relsrc_is_hot || reltarget_is_hot)
            {
                klog("elf: hot relocation required type %d targeting %x at %x\n", r_type,
                    target, src);
            }


            /* We generate executables with the -q option, therefore relocations are already applied
                The only changes we need to make are to absolute relocations where we add base_ptr */

            switch(r_type)
            {
                case R_ARM_TARGET1:
                case R_ARM_ABS32:
                case R_ARM_TLS_LE32:
                    *(volatile uint32_t *)src = target;
                    break;

                case R_ARM_TLS_LE12:
                    // unsupported
                    BKPT();
                    break;

                case R_ARM_PREL31:
                    {
                        auto old_val = *(volatile uint32_t *)src;
                        old_val &= 0x80000000U;
                        old_val |= ((target - src) & 0x7fffffffU);
                        *(volatile uint32_t *)src = old_val;
                    }
                    break;

                case R_ARM_TARGET2:
                case R_ARM_REL32:
                    *(volatile uint32_t *)src = target - src;
                    break;

                case R_ARM_THM_JUMP24:
                case R_ARM_THM_CALL:
                    /* relative reloc, do nothing unless normal->hot or hot->norma; */

                    if((relsrc_is_hot && !reltarget_is_hot) || (reltarget_is_hot && !relsrc_is_hot))
                    {
                        // may need to inject extra code here to handle ARM BL limits
                        // can use R12 for this so ldr r12, [pc + ...]; bx r12; .short pad; .word (target | 0x1)
                        // 0xf8df, 0xc004
                        // 0x4760
                        // 0x0000
                        // target | 0x1

                        src += 4;

                        auto adjust = (target > src) ? (target - src) : (src - target);
                        bool is_positive = target > src;
                        bool can_fit = is_positive ? (adjust <= 16777214) : (adjust <= 16777216);

                        if(can_fit)
                        {
                            klog("elf: hot reloc: can fit in current reloc\n");
                            // TODO
                            BKPT();
                        }
                        else
                        {
                            // get some space - hopefully
                            unsigned int trampoline_addr;
                            if(relsrc_is_hot)
                            {
                                trampoline_addr = tp_hot.alloc(target);
                                if(trampoline_addr == 0)
                                {
                                    // fail
                                    klog("elf: not enough space to generate hot section trampoline code\n");
                                    // TODO cleanup
                                    return -1;
                                }
                            }
                            else
                            {
                                // reltarget is hot
                                trampoline_addr = tp_norm.alloc(target);
                                if(trampoline_addr == 0)
                                {
                                    // fail
                                    klog("elf: not enough space to generate normal section trampoline code\n");
                                    // TODO cleanup
                                    return -1;
                                }
                            }

                            klog("elf: creating trampoline at %08x for %08x to %08x\n",
                                trampoline_addr, src, target);

                            // trampoline
                            auto trampoline = (volatile uint32_t *)trampoline_addr;
                            trampoline[0] = 0xc004f8df;
                            trampoline[1] = 0x00004760;
                            trampoline[2] = target | 0x1U;

                            // get new adjust - in theory always positive
                            auto new_adjust = trampoline_addr - src;

                            // check fits
                            if(new_adjust > 16777214U)
                            {
                                klog("elf: new trampoline too far from BL instruction!\n");
                                BKPT();
                                // TODO cleanup
                                return -1;
                            }

                            // rewrite current reloc
                            switch(r_type)
                            {
                                case R_ARM_THM_CALL:
                                case R_ARM_THM_JUMP24:
                                    {
                                        [[maybe_unused]] unsigned int S = 0; // always positive
                                        unsigned int imm11 = (new_adjust >> 1) & 0x7ff;
                                        unsigned int imm10 = (new_adjust >> 12) & 0x3ff;
                                        unsigned int I2 = (new_adjust >> 22) & 0x1;
                                        unsigned int I1 = (new_adjust >> 23) & 0x1;
                                        unsigned int J1 = I1 ? 0U : 1U;
                                        unsigned int J2 = I2 ? 0U : 1U;

                                        // second word in ARM is highest in memory
                                        auto old_inst = *(volatile uint32_t *)(src - 4);
                                        old_inst &= 0xd000f800U;
                                        uint32_t new_bl = old_inst |
                                            (J1 << (13+16)) |
                                            (J2 << (11+16)) |
                                            (imm11 << 16) |
                                            (S << 10) |
                                            (imm10);
                                        *(volatile uint32_t *)(src - 4) = new_bl;
                                    }

                                    break;

                                default:
                                    // unsupported for now
                                    klog("elf: unsupported trampoline reloc type\n");
                                    // TODO cleanup
                                    BKPT();
                                    return -1;
                            }
                        }
                    }
                    break;

                default:
                    klog("elf: unknown rel type %d\n", r_type);
                    memblk_deallocate(mr_shdr);
                    return -1;
            }
        }

        memblk_deallocate(mr_shdr);
    }

    if(mr_itcm.valid)
    {
        hot_section_end = tp_hot.get_cur() - mr_itcm.address;
        klog("elf: total hot size %d bytes (%d code and %d trampoline)\n",
            hot_section_end, ehot - shot, hot_section_end - ehot + shot);
    }

    // Invalidate I-Cache for the appropriate region(s)
    for(unsigned int i = 0; i < ehdr.e_phnum; i++)
    {
        Elf32_Phdr phdr;

        if(deferred_call(syscall_lseek, fd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) == (off_t)-1)
        {
            return -1;
        }
        if(deferred_call(syscall_read, fd, (char *)&phdr, sizeof(phdr)) != sizeof(phdr))
        {
            return -1;
        }

        auto cache_line_start = phdr.p_vaddr & ~0x1fU;
        auto cache_line_end = (phdr.p_vaddr + phdr.p_memsz + 0x1fU) & ~0x1fU;
        auto cache_size = cache_line_end - cache_line_start;

        if(phdr.p_flags & PF_X)
        {
            CleanOrInvalidateM7Cache(cache_line_start, cache_size, CacheType_t::Data);
            InvalidateM7Cache(cache_line_start, cache_size, CacheType_t::Instruction);
        }
        else if(phdr.p_flags & (PF_R | PF_W))
        {
            CleanOrInvalidateM7Cache(cache_line_start, cache_size, CacheType_t::Data);
        }
    }

    // Create the userspace cleanup code, if necessary
    if(!p.is_priv)
    {
        p.thread_finalizer += base_ptr;
        auto tf = (uint16_t *)p.thread_finalizer;

        // mov r1, r0; mov r0, #0; svc #0; bl .
        static_assert(((unsigned int)__syscall_thread_cleanup) < 256U);
        tf[0] = 0x1c01;
        tf[1] = 0x2000 | (unsigned int)__syscall_thread_cleanup;
        tf[2] = 0xdf00;
        tf[3] = 0xe7fe;

        CleanOrInvalidateM7Cache(p.thread_finalizer, 32, CacheType_t::Data);
        InvalidateM7Cache(p.thread_finalizer, 32, CacheType_t::Instruction);
    }
    if(mr_itcm.valid && mr_itcm.address >= 0x20000000)
    {
        // not needed for ITCM
        CleanOrInvalidateM7Cache(mr_itcm.address, mr_itcm.length, CacheType_t::Data);
        InvalidateM7Cache(mr_itcm.address, mr_itcm.length, CacheType_t::Instruction);
    }
    if(mr_itcm.valid)
    {
        p.mr_hot = mr_itcm;
    }

    // setup args
    init_args(pname, params, (void *)arg_base);
    p.argc = *(int *)arg_base;
    p.argv = (char **)(arg_base + 4);

    klog("successfully loaded, entry: %x\n", (uint32_t)(uintptr_t)base_ptr + ehdr.e_entry);
    klog("%s: Exec.Command(\"ReadIntoTraceCache 0x%08x 0x%08x\");\n",
        pname.c_str(), (unsigned long)base_ptr, (unsigned long)max_size);

    return 0;
}

void handle_newlibinithook(uint32_t lr, uint32_t *retaddr)
{
    lr &= ~01U;

    uint32_t clr_value;
    {
        SharedMemoryGuard smg((const void *)lr, 4, true, false);
        clr_value = *(uint32_t *)lr;
    }
    if(clr_value == 0x21002000U)
    {
        // we are being called by a newlib _mainCRTStartup which explicitly sets argc/arvg to zero
        //  fix this
        auto lr_cache_line_start = lr & ~0x1fU;
        auto lr_cache_line_end = (lr + 4 + 0x1fU) & ~0x1fU;
        auto lr_cache_size = lr_cache_line_end - lr_cache_line_start;
        {
            SharedMemoryGuard smg((const void *)lr, 4, false, true);
            *(uint32_t *)lr = 0xbf00bf00U;
#if !GK_DUAL_CORE && !GK_DUAL_CORE_AMP
            // SharedMemoryGuard will do nothing in unicore

            CleanM7Cache(lr_cache_line_start, lr_cache_size, CacheType_t::Data);
#endif
        }
        InvalidateM7Cache(lr_cache_line_start, lr_cache_size, CacheType_t::Instruction);

        // now return argc:arvg as r1:r0
        auto &p = GetCurrentThreadForCore()->p;
        uint32_t argc = (uint32_t)p.argc;
        uint32_t argv = (uint32_t)p.argv;
        retaddr[0] = argc;
        retaddr[1] = argv;
    }
    else
    {
        retaddr[0] = 0;
        retaddr[1] = 0;
    }
}

/* Determine how much space we need to store the arguments to this program.
    Argument layout is:
        0                       - argc
        4                       - argv[][argc]
        4 + argc * 4            - argv[0]
        alignup(4)              - argv[1] 
            etc
*/

size_t get_arg_length(const std::string &pname, const std::vector<std::string> &params)
{
    size_t cur_size = 0;

    // argc
    cur_size += 4;

    // argv * 4
    cur_size += (params.size() + 1U) * 4;

    // pname
    {
        auto clen = pname.size() + 1U;   // null terminator
        clen = (clen + 3) & ~3U;
        cur_size += clen;
    }

    // each argv
    for(const auto &p : params)
    {
        auto clen = p.size() + 1U;   // null terminator
        clen = (clen + 3) & ~3U;
        cur_size += clen;
    }

    return cur_size;
}

void init_args(const std::string &pname, const std::vector<std::string> &params, void *buf)
{
    uint32_t addr = (uint32_t)(uintptr_t)buf;
    auto len = get_arg_length(pname, params);

    {
        SharedMemoryGuard smg(buf, len, false, true);

        // argc
        *(uint32_t *)addr = params.size() + 1U;
        addr += 4;

        // argv array - just reserve space, fill in later
        auto argv_array_addr = addr;
        addr += (params.size() + 1U) * 4;
        std::vector<uint32_t> argv_addresses;

        // pname
        {
            argv_addresses.push_back(addr); // store current address for later

            strcpy((char *)addr, pname.c_str());
            addr += pname.size() + 1U;
            addr = (addr + 3U) & ~3U;
        }


        // each argv
        for(const auto &p : params)
        {
            argv_addresses.push_back(addr); // store current address for later

            strcpy((char *)addr, p.c_str());
            addr += p.size() + 1U;
            addr = (addr + 3U) & ~3U;
        }

        // argv array
        addr = argv_array_addr;
        for(auto paddr : argv_addresses)
        {
            *(uint32_t *)addr = paddr;
            addr += 4;
        }
    }
}