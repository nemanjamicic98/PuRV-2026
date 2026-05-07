/*
 * main.c — Fajl za testiranje Conway IOCTL drivera
 *
 * Kompajliranje: gcc -Wall -o conway_app main.c
 * Pokretanje:    ./conway_app
 *
 * Unosi se 9 cifara (0 ili 1) koje predstavljaju 3x3 mrezu:
 *
 *   1 2 3
 *   4 5 6   <-- 5 je centralna celija
 *   7 8 9
 *
 * Program pakuje te bite u int32_t, salje driveru, i cita rezultat.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <errno.h>

#define WR_VALUE _IOW('a', 'a', int32_t*)
#define RD_VALUE _IOR('a', 'b', int32_t*)

int main(void)
{
    int fd;
    int32_t states = 0;
    int32_t result = 0;
    unsigned char input[9];
    int i = 0;

    printf("Otvaranje drivera...\n");
    fd = open("/dev/etx_device", O_RDWR);
    if (fd < 0) {
        perror("Ne mogu otvoriti /dev/etx_device");
        printf("Provjeri: sudo insmod conway_ioctl_driver.ko\n");
        return -1;
    }

    /* TEST: citanje bez pisanja (ocekujemo gresku) --- */
    printf("\nTest: READ bez prethodnog WRITE...\n");
    int ret = ioctl(fd, RD_VALUE, &result);
    if (ret < 0)
        printf("  OK - greska detektovana (errno=%d), driver ispravno reaguje\n", errno);
    else
        printf("  GRESKA - trebalo je odbiti zahtjev!\n");

    /* Normalan tok: unos, write, read --- */
    printf("\nUnesi stanja 9 celija (samo 0 i 1, bez razmaka):\n");
    printf("  Format: gornji_red srednji_red donji_red\n");
    printf("  Primjer: 010101010\n");
    printf("  (centralna celija je 5. po redu)\n");
    printf("Unos: ");

    while (i < 9) {
        char c = getchar();
        if (c == '0' || c == '1') {
            input[i] = c;
            i++;
        }
    }

    printf("\nUnesena mreza:\n");
    printf("  %c %c %c\n", input[0], input[1], input[2]);
    printf("  %c %c %c\n", input[3], input[4], input[5]);
    printf("  %c %c %c\n", input[6], input[7], input[8]);
    printf("  (centralna celija = %c)\n", input[4]);

    /*
     * Pakovanje u int32_t:
     * input[0] -> bit 8 (najznacajniji od 9)
     * input[4] -> bit 4 (centralna)
     * input[8] -> bit 0 (najmanje znacajan)
     */
    states = ((input[0] - '0') << 8) |
             ((input[1] - '0') << 7) |
             ((input[2] - '0') << 6) |
             ((input[3] - '0') << 5) |
             ((input[4] - '0') << 4) |
             ((input[5] - '0') << 3) |
             ((input[6] - '0') << 2) |
             ((input[7] - '0') << 1) |
             ((input[8] - '0') << 0);

    printf("\nSaljem driveru (value=0x%x)...\n", states);
    ioctl(fd, WR_VALUE, &states);

    printf("Citam novo stanje centralne celije...\n");
    ioctl(fd, RD_VALUE, &result);

    printf("\nRezultat: centralna celija ce biti %s\n",
           result == 1 ? "ZIVA (1)" : "MRTVA (0)");

    close(fd);
    printf("Driver zatvoren.\n");
    
    return 0;
}