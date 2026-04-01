#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <unistd.h>

//Funkcija prolazi kroz mapiranu memoriju i iterpretira bajtove kao ELF strukture

void print_symbol_table(char *map, Elf64_Shdr *sym_shdr, Elf64_Shdr *str_shdr) {
    // Pokazivac na pocetak tabele simbola
    Elf64_Sym *symbols = (Elf64_Sym *)(map + sym_shdr->sh_offset);
    // Pokayivac na pocetak tabele imena
    char *strs = (char *)(map + str_shdr->sh_offset);
    
    int count = sym_shdr->sh_size / sym_shdr->sh_entsize;

    printf("\nSymbol table contains %d entries:\n", count);
    printf("%6s: %-16s %-5s %-8s %-8s %-5s %s\n", 
           "Num", "Value", "Size", "Type", "Bind", "Vis", "Name");

    for (int i = 0; i < count; i++) {
        // Uyimanje podataka
        unsigned char type = ELF64_ST_TYPE(symbols[i].st_info);
        unsigned char bind = ELF64_ST_BIND(symbols[i].st_info);
        unsigned char vis  = ELF64_ST_VISIBILITY(symbols[i].st_other);

        printf("%6d: %016lx %5ld %-8d %-8d %-5d %s\n",
               i,
               symbols[i].st_value,
               symbols[i].st_size,
               type,
               bind,
               vis,
               &strs[symbols[i].st_name]);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Upotreba: %s <binarni_fajl>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    //Velicina fajla za mmap
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        return 1;
    }

    //Mapiranje fajla
    char *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    //ELF yaglavlje na pocetku fajla
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)map;

    //Magicni bajtovi za provjeru da li je ELF
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1) {
        fprintf(stderr, "Fajl nije u ELF formatu\n");
        return 1;
    }

    //Pronalayenje tabele zaglavlja sekcija
    Elf64_Shdr *shdr = (Elf64_Shdr *)(map + ehdr->e_shoff);
    
    // Sekcija sa imenima svih sekcija(za stampanje tabele)
    char *shstrtab = (char *)(map + shdr[ehdr->e_shstrndx].sh_offset);

    //Ide kroz sve sekcije i trayi SHT_SYMTAB ili SHT_DYSYM
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB || shdr[i].sh_type == SHT_DYNSYM) {
            printf("\nSection [%2d] '%s':", i, &shstrtab[shdr[i].sh_name]);
            
      //Indeksi sekcija
            print_symbol_table(map, &shdr[i], &shdr[shdr[i].sh_link]);
        }
    }

    munmap(map, st.st_size);
    close(fd);
    return 0;
}
