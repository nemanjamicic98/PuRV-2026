/***************************************************************************//**
*  \file       main.c
*
*  \details    Userspace application to test Conway Game of Life IOCTL driver
*
*  \author     Natasa Miljevic
*
*  \Tested with Linux raspberrypi 6.12.47+rpt-rpi-v7
*
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
 
#define WR_VALUE _IOW('a','a',int32_t*)
#define RD_VALUE _IOR('a','b',int32_t*)
 
int main()
{
        int fd;
        int32_t states = 0;
        int32_t result = 0;
        unsigned char input[9];
        int i = 0;
 
        printf("\nOpening Driver\n");

        fd = open("/dev/etx_device", O_RDWR);
        if(fd < 0) 
        {
                printf("Cannot open device file...\n");
                return -1;
        }
 
        printf("Enter states for 9 cells (0-DEAD or 1-ALIVE): ");
        while(i < 9)
        {
            char c = getchar();

            if(c == '0' || c == '1')
            {
                input[i] = c;
                i++;
            }
        }


        printf("\nEntered matrix:\n");
        printf("%c %c %c\n", input[0], input[1], input[2]);
        printf("%c %c %c\n", input[3], input[4], input[5]);
        printf("%c %c %c\n", input[6], input[7], input[8]);

        
        states = ((input[0] - '0') << 8) |
                 ((input[1] - '0') << 7) |
                 ((input[2] - '0') << 6) |
                 ((input[3] - '0') << 5) |
                 ((input[4] - '0') << 4) |
                 ((input[5] - '0') << 3) |
                 ((input[6] - '0') << 2) |
                 ((input[7] - '0') << 1) |
                 ((input[8] - '0'));

        printf("\nWriting states to Driver\n");
        ioctl(fd, WR_VALUE, &states);
 
        printf("Reading new state from Driver\n");
        ioctl(fd, RD_VALUE, &result);
        printf("Result is %d\n", result);
 
        printf("Closing Driver\n");
        close(fd);

        return 0;
}
