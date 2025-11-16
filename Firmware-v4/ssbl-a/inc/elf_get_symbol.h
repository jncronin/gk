#ifndef ELF_GET_SYMBOL_H
#define ELF_GET_SYMBOL_H

#include <cstdint>

uintptr_t elf_get_symbol(const void *elf_file, const char *name);

#endif
