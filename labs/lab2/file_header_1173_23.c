#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

const char *get_file_type(uint16_t type)
{
    switch (type)
    {
    case ET_NONE: return "NONE (No file type)";
    case ET_REL:  return "REL (Relocatable file)";
    case ET_EXEC: return "EXEC (Executable file)";
    case ET_DYN:  return "DYN (Position-Independent Executable file)";
    case ET_CORE: return "CORE (Core file)";
    default:      return "UNKNOWN";
    }
}

const char *get_segment_type(uint32_t type)
{
    switch (type)
    {
    case PT_NULL:         return "NULL";
    case PT_LOAD:         return "LOAD";
    case PT_DYNAMIC:      return "DYNAMIC";
    case PT_INTERP:       return "INTERP";
    case PT_NOTE:         return "NOTE";
    case PT_SHLIB:        return "SHLIB";
    case PT_PHDR:         return "PHDR";
    case PT_TLS:          return "TLS";
    case PT_GNU_PROPERTY: return "GNU_PROPERTY";
    case PT_GNU_EH_FRAME: return "GNU_EH_FRAME";
    case PT_GNU_STACK:    return "GNU_STACK";
    case PT_GNU_RELRO:    return "GNU_RELRO";
    default:              return "UNKNOWN";
    }
}

void get_flags_string(uint32_t flags, char *buf)
{
    int pos = 0;

    if (flags & PF_R) buf[pos++] = 'R';
    if (flags & PF_W) buf[pos++] = 'W';
    if (flags & PF_X) buf[pos++] = 'E';

    if (pos == 0) buf[pos++] = '-';

    buf[pos] = '\0';
}

void print_program_headers_64(FILE *file, Elf64_Ehdr *ehdr)
{
    Elf64_Phdr phdr;

    printf("Elf file type is %s\n", get_file_type(ehdr->e_type));
    printf("Entry point 0x%lx\n", ehdr->e_entry);
    printf("There are %d program headers, starting at offset %lu\n",
           ehdr->e_phnum, ehdr->e_phoff);

    if (ehdr->e_phnum == 0)
    {
        printf("\nThere are no program headers.\n");
        return;
    }

    fseek(file, ehdr->e_phoff, SEEK_SET);

    printf("\nProgram Headers:\n");
    printf("  %s           %s       %s           %s           %s    %s     %s  %s\n",
       "Type", "Offset", "VirtAddr", "PhysAddr",
       "FileSiz", "MemSiz", "Flags", "Align");

    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (fread(&phdr, sizeof(Elf64_Phdr), 1, file) != 1)
        {
            fprintf(stderr, "Greska pri citanju.\n");
            return;
        }

        char flags[4];
        get_flags_string(phdr.p_flags, flags);

        printf("  %-14s 0x%010lx 0x%016lx 0x%016lx 0x%08lx 0x%08lx %-6s 0x%lx\n",
               get_segment_type(phdr.p_type),
               phdr.p_offset,
               phdr.p_vaddr,
               phdr.p_paddr,
               phdr.p_filesz,
               phdr.p_memsz,
               flags,
               phdr.p_align);

        // INTERP string
        if (phdr.p_type == PT_INTERP)
        {
            char interp[256];
            long current = ftell(file);

            fseek(file, phdr.p_offset, SEEK_SET);
            fread(interp, 1, phdr.p_filesz, file);

            printf("      [Requesting program interpreter: %s]\n", interp);

            fseek(file, current, SEEK_SET);
        }
    }
}

void process_elf(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Greska");
        return;
    }

    unsigned char ident[EI_NIDENT];

    fread(ident, 1, EI_NIDENT, file);

    if (memcmp(ident, ELFMAG, SELFMAG) != 0)
    {
        printf("Nije ELF fajl.\n");
        fclose(file);
        return;
    }

    fseek(file, 0, SEEK_SET);

    if (ident[EI_CLASS] == ELFCLASS64)
    {
        Elf64_Ehdr ehdr;
        fread(&ehdr, sizeof(ehdr), 1, file);
        print_program_headers_64(file, &ehdr);
    }
    else
    {
        printf("Podrzan je samo ELF64.\n");
    }

    fclose(file);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Upotreba: %s <elf>\n", argv[0]);
        return 1;
    }

    process_elf(argv[1]);
    return 0;
}