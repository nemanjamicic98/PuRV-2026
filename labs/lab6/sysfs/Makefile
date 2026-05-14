obj-m += conway_ioctl_driver.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	# Koristimo $(PWD) varijablu koja se automatski postavlja u shell-u
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

load:
	sudo insmod conway_ioctl_driver.ko
	# Davanje dozvole običnom korisniku za pristup uređaju [cite: 8, 12]
	sudo chmod 666 /dev/etx_device

unload:
	sudo rmmod conway_ioctl_driver