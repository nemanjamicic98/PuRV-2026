#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

/* 
   This function converts the numeric section type into a string.
   ELF section headers store the type as a number, so this function helps
   us print a name with meaning.
   If the type is not recognized, it returns "UNKNOWN".
*/
static const char *section_type_to_string(unsigned int type) 
{
    switch (type) 
    {
        case SHT_NULL:           return "NULL";
        case SHT_PROGBITS:       return "PROGBITS";
        case SHT_SYMTAB:         return "SYMTAB";
        case SHT_STRTAB:         return "STRTAB";
        case SHT_RELA:           return "RELA";
        case SHT_HASH:           return "HASH";
        case SHT_DYNAMIC:        return "DYNAMIC";
        case SHT_NOTE:           return "NOTE";
        case SHT_NOBITS:         return "NOBITS";
        case SHT_REL:            return "REL";
        case SHT_SHLIB:          return "SHLIB";
        case SHT_DYNSYM:         return "DYNSYM";
        case SHT_INIT_ARRAY:     return "INIT_ARRAY";
        case SHT_FINI_ARRAY:     return "FINI_ARRAY";
        case SHT_PREINIT_ARRAY:  return "PREINIT_ARRAY";
        case SHT_GROUP:          return "GROUP";
        case SHT_SYMTAB_SHNDX:   return "SYMTAB_SHNDX";
        case SHT_NUM:            return "SHT_NUM";
        case SHT_GNU_HASH:       return "GNU_HASH";
        case SHT_GNU_versym:     return "VERSYM";
        case SHT_GNU_verdef:     return "VERDEF";
        case SHT_GNU_verneed:    return "VERNEED";
        default:                 return "UNKNOWN";
    }
}

/* 
   This function converts section flags into a readable string:
   W - Writable,
   A - Occupies memory during execution,
   X - Executable,
   M - Might be merged,
   S - Contains nul-terminated strings,
   I - sh_info contains SHT index,
   L - Preserve order after combining,
   O - Non-standard OS specific handling required,
   G - Section is member of a group,
   T - Section hold thread-local data,
   C - Section with compressed data.
*/
static void flags_to_string(Elf64_Xword flags, char *buf, size_t size) 
{
    size_t pos = 0;

    if ((flags & SHF_WRITE)            && pos < size - 1) buf[pos++] = 'W';
    if ((flags & SHF_ALLOC)            && pos < size - 1) buf[pos++] = 'A';
    if ((flags & SHF_EXECINSTR)        && pos < size - 1) buf[pos++] = 'X';
    if ((flags & SHF_MERGE)            && pos < size - 1) buf[pos++] = 'M';
    if ((flags & SHF_STRINGS)          && pos < size - 1) buf[pos++] = 'S';
    if ((flags & SHF_INFO_LINK)        && pos < size - 1) buf[pos++] = 'I';
    if ((flags & SHF_LINK_ORDER)       && pos < size - 1) buf[pos++] = 'L';
    if ((flags & SHF_OS_NONCONFORMING) && pos < size - 1) buf[pos++] = 'O';
    if ((flags & SHF_GROUP)            && pos < size - 1) buf[pos++] = 'G';
    if ((flags & SHF_TLS)              && pos < size - 1) buf[pos++] = 'T';
    if ((flags & SHF_COMPRESSED)       && pos < size - 1) buf[pos++] = 'C';

    buf[pos] = '\0';
}

int main(int argc, char *argv[]) 
{
    FILE *file;                  // File pointer used to open and read the ELF file
    Elf64_Ehdr ehdr;             // Stores the main ELF header information
    Elf64_Shdr *sh_table = NULL; // Array of section headers
    Elf64_Shdr shstr_hdr;        // Section header of .shstrtab
    char *shstrtab = NULL;       // String table containing section names
    int i;

    if (argc != 2) 
    {
        fprintf(stderr, "Usage: %s <elf-file>\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "rb");
    if (file == NULL) 
    {
        perror("fopen");
        return 1;
    }

    if (fread(&ehdr, 1, sizeof(ehdr), file) != sizeof(ehdr)) 
    {
        fprintf(stderr, "Error: Failed to read ELF header\n");
        fclose(file);
        return 1;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) 
    {
        fprintf(stderr, "Error: File is not an ELF file\n");
        fclose(file);
        return 1;
    }

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) 
    {
        fprintf(stderr, "Error: Only ELF64 files are supported\n");
        fclose(file);
        return 1;
    }

    if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0)
    {
        fprintf(stderr, "Error: No section header table found\n");
        fclose(file);
        return 1;
    }

    if (ehdr.e_shentsize != sizeof(Elf64_Shdr)) 
    {
        fprintf(stderr, "Error: Unexpected section header entry size\n");
        fclose(file);
        return 1;
    }

    sh_table = malloc(ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (sh_table == NULL) 
    {
        fprintf(stderr, "Error: Memory allocation failed for section headers\n");
        fclose(file);
        return 1;
    }

    if (fseek(file, (long)ehdr.e_shoff, SEEK_SET) != 0) 
    {
        fprintf(stderr, "Error: Failed to seek to section header table\n");
        free(sh_table);
        fclose(file);
        return 1;
    }

    if (fread(sh_table, sizeof(Elf64_Shdr), ehdr.e_shnum, file) != ehdr.e_shnum) 
    {
        fprintf(stderr, "Error: Failed to read section headers\n");
        free(sh_table);
        fclose(file);
        return 1;
    }

    if (ehdr.e_shstrndx >= ehdr.e_shnum)
    {
        fprintf(stderr, "Error: Invalid section name string table index\n");
        free(sh_table);
        fclose(file);
        return 1;
    }

    shstr_hdr = sh_table[ehdr.e_shstrndx];

    shstrtab = malloc(shstr_hdr.sh_size);
    if (shstrtab == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed for .shstrtab\n");
        free(sh_table);
        fclose(file);
        return 1;
    }

    if (fseek(file, (long)shstr_hdr.sh_offset, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: Failed to seek to .shstrtab\n");
        free(shstrtab);
        free(sh_table);
        fclose(file);
        return 1;
    }

    if (fread(shstrtab, 1, shstr_hdr.sh_size, file) != shstr_hdr.sh_size)
    {
        fprintf(stderr, "Error: Failed to read .shstrtab\n");
        free(shstrtab);
        free(sh_table);
        fclose(file);
        return 1;
    }

    printf("There are %u section headers, starting at offset 0x%lx:\n\n",
           ehdr.e_shnum, (unsigned long)ehdr.e_shoff);

    printf("Section Headers:\n");
    printf("  [Nr] %-17s %-16s %-16s %-8s\n",
           "Name", "Type", "Address", "Offset");
    printf("       %-16s %-16s %-6s %-5s %-5s %-5s\n",
           "Size", "EntSize", "Flags", "Link", "Info", "Align");

    for (i = 0; i < ehdr.e_shnum; i++) 
    {
        const char *name = "";
        char flags[32];

        if (sh_table[i].sh_name < shstr_hdr.sh_size) 
        {
            name = shstrtab + sh_table[i].sh_name;
        }

        flags_to_string(sh_table[i].sh_flags, flags, sizeof(flags));

        printf("  [%2d] %-17.17s %-16s %016lx %08lx\n",
               i,
               name,
               section_type_to_string(sh_table[i].sh_type),
               (unsigned long)sh_table[i].sh_addr,
               (unsigned long)sh_table[i].sh_offset);

        printf("       %016lx %016lx %-6s %5u %5u %5lu\n",
               (unsigned long)sh_table[i].sh_size,
               (unsigned long)sh_table[i].sh_entsize,
               flags,
               sh_table[i].sh_link,
               sh_table[i].sh_info,
               (unsigned long)sh_table[i].sh_addralign);
    }

    free(shstrtab);
    free(sh_table);
    fclose(file);
    return 0;
}
