#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define SIZE 3
#define MAGIC_NO 'k'
#define WR_VALUE _IOW(MAGIC_NO, 'a', int32_t*)
#define RD_VALUE _IOR(MAGIC_NO, 'b', int32_t*)

int board[SIZE][SIZE];

int main() {
    int fd;
    int r = 1, c = 1; 
    int32_t states = 0;
    int32_t result = 0;
    int i_in = 0;

    fd = open("/dev/life_service", O_RDWR);
    if (fd < 0) {
        perror("Greska pri otvaranju drajvera");
        return -1;
    }

    printf("Unesi 9 cifara: ");
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            char ch = getchar();
            while (ch != '0' && ch != '1') ch = getchar();
            board[i][j] = ch - '0';
        }
    }

    printf("\nIzgled matrice:\n");
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            printf("%d ", board[i][i == i ? j : j]); 
        }
        printf("\n");
    }

    int bit_pos = 8;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int nr = r + i;
            int nc = c + j;
            
            int val = board[nr][nc];
            states |= (val << bit_pos);
            bit_pos--;
        }
    }

    ioctl(fd, WR_VALUE, &states);
    ioctl(fd, RD_VALUE, &result);

    printf("\nCentralna celija je: %d\n", r, c, result);

    close(fd);
    return 0;
}
