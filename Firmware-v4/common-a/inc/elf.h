#ifndef ELF_H
#define ELF_H

#include <cstdint>
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

typedef struct
{
    unsigned char e_ident[16]; /* ELF identification */
    Elf64_Half e_type; /* Object file type */
    Elf64_Half e_machine; /* Machine type */
    Elf64_Word e_version; /* Object file version */
    Elf64_Addr e_entry; /* Entry point address */
    Elf64_Off e_phoff; /* Program header offset */
    Elf64_Off e_shoff; /* Section header offset */
    Elf64_Word e_flags; /* Processor-specific flags */
    Elf64_Half e_ehsize; /* ELF header size */
    Elf64_Half e_phentsize; /* Size of program header entry */
    Elf64_Half e_phnum; /* Number of program header entries */
    Elf64_Half e_shentsize; /* Size of section header entry */
    Elf64_Half e_shnum; /* Number of section header entries */
    Elf64_Half e_shstrndx; /* Section name string table index */
} __attribute__((packed)) Elf64_Ehdr;

typedef struct
{
    Elf64_Word p_type; /* Type of segment */
    Elf64_Word p_flags; /* Segment attributes */
    Elf64_Off p_offset; /* Offset in file */
    Elf64_Addr p_vaddr; /* Virtual address in memory */
    Elf64_Addr p_paddr; /* Reserved */
    Elf64_Xword p_filesz; /* Size of segment in file */
    Elf64_Xword p_memsz; /* Size of segment in memory */
    Elf64_Xword p_align; /* Alignment of segment */
} __attribute__((packed)) Elf64_Phdr;

#define EM_AARCH64      0xb7

#define EI_CLASS        4
#define ELFCLASS64      2

#define EI_DATA         5
#define ELFDATA2LSB     1
#define ET_EXEC         2

#define PT_LOAD         1

#define PF_X            1
#define PF_W            2

typedef void (*epoint)();
int elf_load(const void *baseaddr, epoint *entry = nullptr, int el = 1);

#endif