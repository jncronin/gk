#include "elf.h"
#include "SEGGER_RTT.h"
#include "memblk.h"

#include <cstring>

#include "thread.h"
#include "scheduler.h"
#include "region_allocator.h"

#include "cache.h"
#include "ossharedmem.h"

static uint64_t prog_software_init_hook();
static size_t get_arg_length(const std::string &pname, const std::vector<std::string> &params);
static void init_args(const std::string &pname, const std::vector<std::string> &params,
    void *buf);

int elf_load_memory(const void *e, const std::string &pname,
    const std::vector<std::string> &params,
    uint32_t heap_size, CPUAffinity affinity,
    Thread **startup_thread_ret, Process **proc_ret,
    MemRegion stack, bool is_priv)
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
        return -1;
    }

	// Confirm its a 32 bit file
	if(ehdr->e_ident[EI_CLASS] != ELFCLASS32)
	{
        SEGGER_RTT_printf(0, "invalid elf class\n");
        return -1;
	}

	// Confirm its a little-endian file
	if(ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
	{
        SEGGER_RTT_printf(0, "not lsb\n");
        return -1;
	}

	// Confirm its an executable file
	if(ehdr->e_type != ET_EXEC)
	{
        SEGGER_RTT_printf(0, "not exec\n");
        return -1;
	}

	// Confirm its for the ARM architecture
	if(ehdr->e_machine != EM_ARM)
	{
        SEGGER_RTT_printf(0, "not arm\n");
        return -1;
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
        return -1;
    }
    SEGGER_RTT_printf(0, "loading to %x\n", memblk.address);

    // Create a stack for thread0
    if(!stack.valid)
        stack = memblk_allocate_for_stack(4096, affinity);
    auto stack_end = stack.address + stack.length;

    // Create region for arguments
    auto arg_length = get_arg_length(pname, params);
    auto arg_reg = memblk_allocate_for_stack(arg_length, affinity);
    if(!arg_reg.valid)
    {
        __asm__ volatile("bkpt \n" ::: "memory");
        return -1;
    }

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
        if(!(relsect->sh_flags & SHF_ALLOC))
        {
            continue;
        }

        auto relocs = p + shdr->sh_offset;

        for(unsigned int j = 0; j < nentries; j++)
        {
            auto rel = reinterpret_cast<const Elf32_Rel *>(relocs + j * entsize);

            auto r_sym_idx = rel->r_info >> 8;
            auto r_type = rel->r_info & 0xff;

            auto r_sym = reinterpret_cast<const Elf32_Sym *>(p + symtab->sh_offset + r_sym_idx *
                symtab->sh_entsize);

            /*if((uint32_t)dest >= 0x240020d4 && (uint32_t)dest <= 0x240020d8)
            {
                __asm volatile
                (
                    "bkpt  \n"
                    ::: "memory"
                );
            }*/

            /* We generate executables with the -q option, therefore relocations are already applied
                The only changes we need to make are to absolute relocations where we add base_ptr */

            switch(r_type)
            {
                case R_ARM_TARGET1:
                case R_ARM_TARGET2:
                case R_ARM_ABS32:
                    {
                        if((base_ptr + rel->r_offset) & 0x3)
                        {
                            SEGGER_RTT_printf(0, "unaligned reloc at %x\n", base_ptr + rel->r_offset);
                            __asm__ volatile ("bkpt \n" ::: "memory");
                        }

                        void *dest = (void *)(base_ptr + rel->r_offset);
                        uint32_t A = *(uint32_t *)dest;
                        uint32_t P = (uint32_t)dest;
                        [[maybe_unused]] uint32_t Pa = P & 0xfffffffc;
                        [[maybe_unused]] uint32_t T = ((r_sym->st_info & 0xf) == STT_FUNC) ? 1 : 0;
                        [[maybe_unused]] uint32_t S = base_ptr + r_sym->st_value;
                        uint32_t mask = 0xffffffff;
                        uint32_t value = 0;

                        bool is_stack = false;
                        bool is_software_init_hook = false;

                        if(r_sym->st_shndx == SHN_UNDEF)
                        {
                            // need to special-case "_stack" and "software_init_hook" labels
                            auto strtab_idx = symtab->sh_link;
                            auto strtab = reinterpret_cast<const Elf32_Shdr *>(shdrs + strtab_idx * ehdr->e_shentsize);
                            if(r_sym->st_name)
                            {
                                if(strcmp("__stack", &p[strtab->sh_offset + r_sym->st_name]) == 0)
                                {
                                    is_stack = true;
                                }
                                if(strcmp("software_init_hook", &p[strtab->sh_offset + r_sym->st_name]) == 0)
                                {
                                    is_software_init_hook = true;
                                }
                            }

                            if(!is_stack && !is_software_init_hook)
                            {
                                break;      // leave as zero
                            }
                        }
                    
                        mask = 0xffffffffUL;
                        A &= mask;
                        value = A + base_ptr;

                        if(is_stack)
                        {
                            value = stack_end;
                        }
                        else if(is_software_init_hook)
                        {
                            value = (uint32_t)(uintptr_t)prog_software_init_hook;
                            value |= 0x1;
                        }

                        {
                            auto cval = *(uint32_t *)dest;
                            cval &= ~mask;
                            cval |= (value & mask);
                            *(uint32_t *)dest = cval;
                        }
                    }

                    break;

                case R_ARM_THM_JUMP24:
                case R_ARM_THM_CALL:
                case R_ARM_PREL31:
                    /* relative reloc, do nothing */
                    break;

                case R_ARM_NONE:
                    /* do nothing */
                    break;

                default:
                    SEGGER_RTT_printf(0, "unknown rel type %d\n", r_type);
                    return -1;
            }
        }
    }

    // Invalidate I-Cache for the appropriate region(s)
    for(unsigned int i = 0; i < ehdr->e_phnum; i++)
    {
        auto phdr = reinterpret_cast<const Elf32_Phdr *>(phdrs + i * ehdr->e_phentsize);

        if(phdr->p_flags & PF_X)
        {
            CleanOrInvalidateM7Cache(base_ptr + phdr->p_vaddr, phdr->p_memsz, CacheType_t::Data);
            InvalidateM7Cache(base_ptr + phdr->p_vaddr, phdr->p_memsz, CacheType_t::Instruction);
        }
        else if(phdr->p_flags & (PF_R | PF_W))
        {
            CleanOrInvalidateM7Cache(base_ptr + phdr->p_vaddr, phdr->p_memsz, CacheType_t::Data);
        }
    }

    // get start address
    auto start = (void *(*)(void *))(base_ptr + ehdr->e_entry);

    // Create process and the first thread
    SRAM4RegionAllocator<Process> alloc;
    auto ploc = alloc.allocate(1);
    if(!ploc)
    {
        __BKPT();
        while(true);
        return -1;
    }

    auto proc = new(ploc) Process();
    proc->name = pname;
    proc->brk = 0;
    proc->code_data = memblk;
    proc->default_affinity = affinity;
    uint32_t act_heap_size = heap_size;
    while(true)
    {
        proc->heap = memblk_allocate(act_heap_size, MemRegionType::AXISRAM);
        if(!proc->heap.valid)
        {
            proc->heap = memblk_allocate(act_heap_size, MemRegionType::SDRAM);
        }
        if(!proc->heap.valid)
        {
            act_heap_size /= 2;

            if(act_heap_size < 8192)
            {
                __BKPT();
                while(true);
            }
        }
        else
        {
            break;
        }
    }
    if(act_heap_size != heap_size)
    {
        SEGGER_RTT_printf(0, "elf: couldn't allocate heap size of %u, only %u available\n",
            heap_size, act_heap_size);
    }
    memset(&proc->open_files[0], 0, sizeof(File *) * GK_MAX_OPEN_FILES);
    proc->open_files[STDIN_FILENO] = new SeggerRTTFile(0, true, false);
    proc->open_files[STDOUT_FILENO] = new SeggerRTTFile(0, false, true);
    proc->open_files[STDERR_FILENO] = new SeggerRTTFile(0, false, true);

    init_args(pname, params, (void *)arg_reg.address);
    proc->mr_params = arg_reg;
    proc->argc = *(int *)arg_reg.address;
    proc->argv = (char **)(arg_reg.address + 4);

    auto startup_thread = Thread::Create(pname + "_0", start, nullptr, is_priv, GK_PRIORITY_NORMAL, *proc,
        affinity, stack,
        MPUGenerate(base_ptr, max_size, 6, true, MemRegionAccess::RW, MemRegionAccess::RW, WBWA_NS),
        MPUGenerate(proc->heap.address, proc->heap.length, 7, false, MemRegionAccess::RW, MemRegionAccess::RW, WBWA_NS));

    if(startup_thread_ret)
        *startup_thread_ret = startup_thread;
    else
        Schedule(startup_thread);
    if(proc_ret)
        *proc_ret = proc;

    SEGGER_RTT_printf(0, "successfully loaded, entry: %x\n", (uint32_t)(uintptr_t)start);
    SEGGER_RTT_printf(0, "%s: Exec.Command(\"ReadIntoTraceCache 0x%08x 0x%08x\");\n",
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
        {
            SharedMemoryGuard smg((const void *)lr, 4, false, true);
            *(uint32_t *)lr = 0xbf00bf00U;
#if !GK_DUAL_CORE && !GK_DUAL_CORE_AMP
            CleanM7Cache((uint32_t)lr, 4, CacheType_t::Data);
#endif
        }
        InvalidateM7Cache(lr, 4, CacheType_t::Instruction);

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

uint64_t prog_software_init_hook()
{
    uint32_t lr;
    __asm__ volatile ("mov %0, lr \n" : "=r" (lr));
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
        {
            SharedMemoryGuard smg((const void *)lr, 4, false, true);
            *(uint32_t *)lr = 0xbf00bf00U;
#if !GK_DUAL_CORE && !GK_DUAL_CORE_AMP
            CleanM7Cache((uint32_t)lr, 4, CacheType_t::Data);
#endif
        }
        InvalidateM7Cache(lr, 4, CacheType_t::Instruction);

        // now return argc:arvg as r1:r0
        auto &p = GetCurrentThreadForCore()->p;
        uint32_t argc = (uint32_t)p.argc;
        uint32_t argv = (uint32_t)p.argv;
        return (uint64_t)argc | (((uint64_t)argv) << 32);
    }

    return 0ULL;
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