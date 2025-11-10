#include "elf.h"
#include "logger.h"
#include "vmem.h"

static void *get_dest_page(Elf64_Addr addr, bool writeable, bool exec, int el);

int elf_load(const void *base, epoint *entry, int el)
{
    if(el < 1 || el > 3)
        return -1;

    // check header for sanity
    auto hdr = reinterpret_cast<const Elf64_Ehdr *>(base);
    if(hdr->e_ident[0] != '\x7f' ||
        hdr->e_ident[1] != 'E' ||
        hdr->e_ident[2] != 'L' ||
        hdr->e_ident[3] != 'F')
    {
        klog("elf: ident fail %02x %02x %02x %02x\n",
            hdr->e_ident[0], hdr->e_ident[1], hdr->e_ident[2], hdr->e_ident[3]);
        return -1;
    }
    if(hdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        klog("elf: class fail %u\n", hdr->e_ident[EI_CLASS]);
    }
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        klog("elf: data type fail %u\n", hdr->e_ident[EI_DATA]);
    }
    if(hdr->e_type != ET_EXEC)
    {
        klog("elf: type fail %u\n", hdr->e_type);
    }
    if(hdr->e_machine != EM_AARCH64)
    {
        klog("elf: machine type fail %u\n", hdr->e_machine);
    }

    if(hdr->e_phentsize < sizeof(Elf64_Phdr))
    {
        klog("elf: phentsize too smzll: %u\n", hdr->e_phentsize);
    }

    // load the appropriate phdrs
    for(auto i = 0U; i < hdr->e_phnum; i++)
    {
        auto phdr = reinterpret_cast<const Elf64_Phdr *>((uintptr_t)base +
            hdr->e_phoff + hdr->e_phentsize * i);
        if(phdr->p_type == PT_LOAD)
        {
            auto src = reinterpret_cast<const void *>((uintptr_t)base + phdr->p_offset);

            bool writeable = (phdr->p_flags & PF_W) != 0;
            bool exec = (phdr->p_flags & PF_X) != 0;

            auto sz = 0U;
            while(sz < phdr->p_filesz)
            {
                auto dest = get_dest_page(phdr->p_vaddr + sz, writeable, exec, el);
                quick_copy_64(dest, src);
                sz += 65536;
            }
            while(sz < phdr->p_memsz)
            {
                auto dest = get_dest_page(phdr->p_vaddr + sz, writeable, exec, el);
                quick_clear_64(dest);
                sz += 65536;
            }
        }
    }

    if(entry)
        *entry = (epoint)hdr->e_entry;
    
    return 0;
}

void *get_dest_page(Elf64_Addr addr, bool writeable, bool exec, int el)
{
    if(el == 1)
    {
        return (void *)pmem_vaddr_to_paddr(addr, writeable, !exec);
    }
    else
    {
        klog("elf: unsupported EL: %d\n", el);
        while(true);
    }
}
