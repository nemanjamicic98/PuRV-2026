# Naziv izvršnog fajla
TARGET = conway

# Direktorijumi
SRC_DIR = src
INC_DIR = inc
OBJ_DIR = obj

# Kompajler i zastavice
CC = gcc
CFLAGS = -Wall -Wextra -pthread -I$(INC_DIR)
LDFLAGS = -pthread

# Izvorni fajlovi (pronalazi ih u src folderu)
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Objektni fajlovi (mapira src/*.c u obj/*.o)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Glavno pravilo
all: $(TARGET)

# Povezivanje objektnih fajlova u izvršni program
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Kompajliranje svakog .c fajla iz src u .o fajl u obj
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Kreiranje obj direktorijuma ako ne postoji
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Brisanje objektnih fajlova, obj foldera i izvršnog programa
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# Pokretanje programa
run: $(TARGET)
	./$(TARGET) 1