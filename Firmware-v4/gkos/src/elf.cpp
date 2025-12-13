#include "elf.h"
#include "process.h"
#include "syscalls_int.h"
#include "vmem.h"
#include "pmem.h"

int elf_load_fildes(int fd, PProcess p, Thread::threadstart_t *epoint)
{
    if(fd < 0)
        return -1;

    // we use syscall_read to write to kernel heap structures here
    ThreadPrivilegeEscalationGuard tpeg;
        
    // check header for sanity
    Elf64_Ehdr hdr;
    deferred_call(syscall_lseek, fd, 0, SEEK_SET);
    auto [ bret, berrno ] = deferred_call(syscall_read, fd, (char *)&hdr, sizeof(hdr));
    if(bret != sizeof(hdr))
    {
        klog("elf_load_fildes: failed to read header: %d, %d\n", bret, berrno);
        return -1;
    }

    if(hdr.e_ident[0] != '\x7f' ||
        hdr.e_ident[1] != 'E' ||
        hdr.e_ident[2] != 'L' ||
        hdr.e_ident[3] != 'F')
    {
        klog("elf: ident fail %02x %02x %02x %02x\n",
            hdr.e_ident[0], hdr.e_ident[1], hdr.e_ident[2], hdr.e_ident[3]);
        return -1;
    }
    if(hdr.e_ident[EI_CLASS] != ELFCLASS64)
    {
        klog("elf: class fail %u\n", hdr.e_ident[EI_CLASS]);
        return -1;
    }
    if(hdr.e_ident[EI_DATA] != ELFDATA2LSB)
    {
        klog("elf: data type fail %u\n", hdr.e_ident[EI_DATA]);
        return -1;
    }
    if(hdr.e_type != ET_EXEC)
    {
        klog("elf: type fail %u\n", hdr.e_type);
        return -1;
    }
    if(hdr.e_machine != EM_AARCH64)
    {
        klog("elf: machine type fail %u\n", hdr.e_machine);
        return -1;
    }

    if(hdr.e_phentsize < sizeof(Elf64_Phdr))
    {
        klog("elf: phentsize too small: %u\n", hdr.e_phentsize);
        return -1;
    }

    // load the appropriate phdrs
    for(auto i = 0U; i < hdr.e_phnum; i++)
    {
        Elf64_Phdr phdr;
        deferred_call(syscall_lseek, fd, hdr.e_phoff + hdr.e_phentsize * i, SEEK_SET);
        std::tie(bret, berrno) = deferred_call(syscall_read, fd, (char *)&phdr, sizeof(Elf64_Phdr));
        if(bret != sizeof(Elf64_Phdr))
        {
            klog("elf_load_filedes: failed to load phdr: %d, %d\n", bret, berrno);
            return -1;
        }
        if(phdr.p_type == PT_LOAD || phdr.p_type == PT_TLS)
        {
            auto foffset = phdr.p_offset;
            deferred_call(syscall_lseek, fd, foffset, SEEK_SET);

            bool writeable = (phdr.p_flags & PF_W) != 0;
            bool exec = (phdr.p_flags & PF_X) != 0;

            auto filesz = (unsigned long long)phdr.p_filesz;
            auto memsz = (unsigned long long)phdr.p_memsz;
            auto vaddr = phdr.p_vaddr;

            bool is_tls = phdr.p_type == PT_TLS;
            if(is_tls)
            {
                if(p->vb_tls.valid)
                {
                    klog("elf: too many PT_TLS sections in file\n");
                    return -1;
                }

                CriticalGuard cg(p->user_mem->sl);
                p->vb_tls = vblock_alloc(vblock_size_for(memsz), false, false, false, 0, 0, p->user_mem->blocks);
                if(!p->vb_tls.valid)
                {
                    klog("elf: couldn't allocate vblock for PT_TLS of size %llu (%llu)\n",
                        memsz, vblock_size_for(memsz));
                    return -1;
                }
                p->vb_tls_data_size = memsz;
                vaddr = p->vb_tls.data_start();
            }

            while(memsz)
            {
                // allocate memory for the current page
                auto cur_page = vaddr & ~(VBLOCK_64k - 1ULL);
                uintptr_t dest_page = 0;

                {
                    CriticalGuard cg(p->user_mem->sl, p->owned_pages.sl);
                    VMemBlock cur_vblock;
                    if(is_tls)
                    {
                        // fake vblock for 1 page to satisfy per-page mapping done later
                        cur_vblock.base = cur_page;
                        cur_vblock.length = VBLOCK_64k;
                        cur_vblock.valid = true;
                        cur_vblock.user = false;
                        cur_vblock.write = false;
                        cur_vblock.exec = false;
                        cur_vblock.lower_guard = 0;
                        cur_vblock.upper_guard = 0;
                    }
                    else
                    {
                        // normal PT_LOAD segment
                        cur_vblock = vblock_alloc_fixed(VBLOCK_64k, cur_page, true, writeable, exec, 0, 0,
                            p->user_mem->blocks);
                    }

                    if(!cur_vblock.valid)
                    {
                        klog("elf_load_fildes: failed to allocate %llx\n", cur_page);
                        return -1;
                    }

                    auto cur_ppage = Pmem.acquire(VBLOCK_64k);
                    if(!cur_ppage.valid)
                    {
                        klog("elf_load_fildes: failed to allocate pmem for %llx\n", cur_page);
                        return -1;
                    }

#if DEBUG_ELF
                    klog("elf_load_fildes: mapping v %llx to p %llx within ttbr0: %llx\n",
                        cur_vblock.base, cur_ppage.base, p->user_mem->ttbr0);
#endif
                    vmem_map(cur_vblock, cur_ppage, p->user_mem->ttbr0);

                    dest_page = PMEM_TO_VMEM(cur_ppage.base);

                    p->owned_pages.add(cur_ppage);
                }

                auto dest_offset_within_page = vaddr - cur_page;
                auto max_copy = VBLOCK_64k - dest_offset_within_page;

                if(filesz)
                {
                    auto to_load = std::min(max_copy, filesz);

                    std::tie(bret, berrno) = deferred_call(syscall_read, fd, (char *)(dest_page + dest_offset_within_page), (int)to_load);

                    if(bret != (int)to_load)
                    {
                        klog("elf_load_fildes: failed to load program %d, %d\n", bret, berrno);
                        return -1;
                    }

                    filesz -= to_load;
                    memsz -= to_load;
                    max_copy -= to_load;
                    dest_offset_within_page += to_load;
                    vaddr += to_load;
                }
                if(memsz && max_copy)
                {
                    auto to_blank = std::min(max_copy, memsz);
                    memset((void *)(dest_page + dest_offset_within_page), 0, to_blank);
                    memsz -= to_blank;
                    vaddr += to_blank;
                }
            }
        }
    }

    if(epoint)
        *epoint = (Thread::threadstart_t)hdr.e_entry;
    
    return 0;
}
