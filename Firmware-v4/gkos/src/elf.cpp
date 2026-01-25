#include "elf.h"
#include "process.h"
#include "syscalls_int.h"
#include "vmem.h"
#include "pmem.h"

int elf_load_fildes(int fd, PProcess p, Thread::threadstart_t *epoint)
{
    if(fd < 0)
        return -1;

    PFile pf;
    {
        CriticalGuard cg(p->open_files.sl);
        if((size_t)fd >= p->open_files.f.size())
        {
            return -1;
        }
        pf = p->open_files.f[fd];
    }

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
            bool writeable = (phdr.p_flags & PF_W) != 0;
            bool exec = (phdr.p_flags & PF_X) != 0;

            auto f_start = phdr.p_offset & ~(PAGE_SIZE - 1);
            auto f_dataend = phdr.p_offset + phdr.p_filesz;
            auto f_zeroend = (phdr.p_offset + phdr.p_memsz + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
            auto mem_start = phdr.p_vaddr & ~(PAGE_SIZE - 1);
            auto filesz = f_dataend - f_start;
            auto memsz = f_zeroend - f_start;

            bool is_tls = phdr.p_type == PT_TLS;
            if(is_tls)
            {
                if(p->vb_tls.valid)
                {
                    klog("elf: too many PT_TLS sections in file\n");
                    return -1;
                }

                MutexGuard mg(p->user_mem->m);
                p->vb_tls = p->user_mem->vblocks.AllocAny(
                    MemBlock::FileBackedReadOnlyMemory(0, phdr.p_memsz, pf, phdr.p_offset, phdr.p_filesz,
                        false, false), false);
                if(!p->vb_tls.valid)
                {
                    klog("elf: couldn't allocate vblock for PT_TLS of size %llu (%llu)\n",
                        memsz, vblock_size_for(memsz));
                    return -1;
                }
                p->vb_tls_data_size = memsz;
            }
            else if(writeable)
            {
                auto vbret = p->user_mem->vblocks.AllocFixed(
                    MemBlock::FileBackedReadWriteMemory(mem_start, memsz, pf, f_start,
                        filesz, true, exec));
                if(!vbret.valid)
                {
                    klog("elf: couldn't allocate block at %p - %p\n", (void *)mem_start,
                        (void *)(mem_start + memsz));
                    return -1;
                }
            }
            else
            {
                auto vbret = p->user_mem->vblocks.AllocFixed(
                    MemBlock::FileBackedReadOnlyMemory(mem_start, memsz, pf, f_start,
                        filesz, true, exec));
                if(!vbret.valid)
                {
                    klog("elf: couldn't allocate block at %p - %p\n", (void *)mem_start,
                        (void *)(mem_start + memsz));
                    return -1;
                }
            }
        }
    }

    if(epoint)
        *epoint = (Thread::threadstart_t)hdr.e_entry;
    
    return 0;
}
