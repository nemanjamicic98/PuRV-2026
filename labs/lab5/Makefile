# Putanje do tvojih fajlova
DRIVER_DIR = ioctl-driver
APP_SRC = src/main/main.c
APP_BIN = conway_app

all:
	# Kompajliranje drajvera (poziva Makefile u ioctl-driver folderu)
	$(MAKE) -C $(DRIVER_DIR)
	# Kompajliranje aplikacije
	gcc -Wall -o $(APP_BIN) $(APP_SRC)

load:
	$(MAKE) -C $(DRIVER_DIR) load

unload:
	$(MAKE) -C $(DRIVER_DIR) unload

clean:
	$(MAKE) -C $(DRIVER_DIR) clean
	rm -f $(APP_BIN)