DRIVER_DIR = ../sysfs
APP_SRC = src/game_of_life.c
APP_BIN = conway_app

all:
	# Компајлирање драјвера
	$(MAKE) -C $(DRIVER_DIR)
	# Компајлирање апликације
	gcc -Wall -o $(APP_BIN) $(APP_SRC)

load:
	$(MAKE) -C $(DRIVER_DIR) load

unload:
	$(MAKE) -C $(DRIVER_DIR) unload

clean:
	$(MAKE) -C $(DRIVER_DIR) clean
	rm -f $(APP_BIN)