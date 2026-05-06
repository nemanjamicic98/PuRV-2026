#ifndef CONWAY_IOCTL_H
#define CONWAY_IOCTL_H

#include <linux/ioctl.h>

#define WR_VALUE _IOW('C', 1, int[9])
#define RD_VALUE _IOR('C', 2, int)

#endif