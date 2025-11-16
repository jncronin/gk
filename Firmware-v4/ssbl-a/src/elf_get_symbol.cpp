#include "elf.h"
#include <cstring>

uintptr_t elf_get_symbol(const void *elf, const char *needle)
{
    // assume elf has already been validated

    auto eh = (const Elf64_Ehdr *)elf;
    for(auto shidx = 0U; shidx < eh->e_shnum; shidx++)
    {
        auto sh = (const Elf64_Shdr *)((uintptr_t)elf + eh->e_shoff + shidx * eh->e_shentsize);

        if(sh->sh_type == SHT_SYMTAB)
        {
            auto symtabidx = sh->sh_link;
            auto symtab_sh = (const Elf64_Shdr *)((uintptr_t)elf + eh->e_shoff + symtabidx * eh->e_shentsize);
            const char *symtab = (const char *)((uintptr_t)elf + symtab_sh->sh_offset);

            auto symcount = sh->sh_size / sh->sh_entsize;

            for(auto symidx = sh->sh_info; symidx < symcount; symidx++)
            {
                auto sym = (const Elf64_Sym *)((uintptr_t)elf + sh->sh_offset + symidx * sh->sh_entsize);

                auto symname = &symtab[sym->st_name];

                if(strcmp(needle, symname) == 0)
                {
                    return sym->st_value;
                }
            }
        }
    }

    return 0;
}
