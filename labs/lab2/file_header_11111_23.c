#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

const char *get_elf_type(uint16_t type)
{
    switch (type)
    {
    case ET_NONE:
        return "NONE (No file type)";
    case ET_REL:
        return "REL (Relocatable file)";
    case ET_EXEC:
        return "EXEC (Executable file)";
    case ET_DYN:
        return "DYN (Shared object file)";
    case ET_CORE:
        return "CORE (Core file)";
    default:
        return "Unknown";
    }
}

void print_program_headers(FILE *file, uint64_t phoff, uint16_t phnum, int is_64)
{
    if (phnum == 0)
    {
        printf("\nNo program headers in this file.\n");
        return;
    }

    printf("\nProgram Headers:\n");
    printf("  %-15s %-10s %-18s %-10s %-10s %-5s\n",
           "Type", "Offset", "VirtAddr", "FileSiz", "MemSiz", "Flg");

    fseek(file, phoff, SEEK_SET);

    for (int i = 0; i < phnum; i++)
    {
        if (is_64)
        {
            Elf64_Phdr phdr;
            fread(&phdr, sizeof(Elf64_Phdr), 1, file);
            printf("  %-15x 0x%08lx 0x%016lx 0x%08lx 0x%08lx 0x%x\n",
                   phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_filesz, phdr.p_memsz, phdr.p_flags);
        }
        else
        {
            Elf32_Phdr phdr;
            fread(&phdr, sizeof(Elf32_Phdr), 1, file);
            printf("  %-15x 0x%08x 0x%08x         0x%08x 0x%08x 0x%x\n",
                   phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_filesz, phdr.p_memsz, phdr.p_flags);
        }
    }
}

void process_elf(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Greska pri otvaranju fajla");
        return;
    }

    unsigned char e_ident[EI_NIDENT];
    if (fread(e_ident, 1, EI_NIDENT, file) != EI_NIDENT)
    {
        fprintf(stderr, "Greska pri citanju zaglavlja.\n");
        fclose(file);
        return;
    }

    if (memcmp(e_ident, ELFMAG, SELFMAG) != 0)
    {
        fprintf(stderr, "Greska: Ovo nije elf fajl.\n");
        fclose(file);
        return;
    }

    int is_64 = (e_ident[EI_CLASS] == ELFCLASS64);
    fseek(file, 0, SEEK_SET);

    printf("ELF Header:\n");
    printf("  Magic:   ");
    for (int i = 0; i < EI_NIDENT; i++)
        printf("%02x ", e_ident[i]);
    printf("\n");

    uint64_t phoff;
    uint16_t phnum;

    if (is_64)
    {
        Elf64_Ehdr ehdr;
        fread(&ehdr, sizeof(Elf64_Ehdr), 1, file);
        printf("  Class:                             ELF64\n");
        printf("  Type:                              %s\n", get_elf_type(ehdr.e_type));
        printf("  Machine:                           0x%x\n", ehdr.e_machine);
        printf("  Entry point address:               0x%lx\n", ehdr.e_entry);
        printf("  Start of program headers:          %lu (bytes into file)\n", ehdr.e_phoff);
        printf("  Size of this header:               %d (bytes)\n", ehdr.e_ehsize);
        phoff = ehdr.e_phoff;
        phnum = ehdr.e_phnum;
    }
    else
    {
        Elf32_Ehdr ehdr;
        fread(&ehdr, sizeof(Elf32_Ehdr), 1, file);
        printf("  Class:                             ELF32\n");
        printf("  Type:                              %s\n", get_elf_type(ehdr.e_type));
        printf("  Machine:                           0x%x\n", ehdr.e_machine);
        printf("  Entry point address:               0x%x\n", ehdr.e_entry);
        printf("  Start of program headers:          %u (bytes into file)\n", ehdr.e_phoff);
        printf("  Size of this header:               %d (bytes)\n", ehdr.e_ehsize);
        phoff = ehdr.e_phoff;
        phnum = ehdr.e_phnum;
    }

    print_program_headers(file, phoff, phnum, is_64);

    fclose(file);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Upotreba: %s <elf_fajl>\n", argv[0]);
        return 1;
    }

    process_elf(argv[1]);
    return 0;
}