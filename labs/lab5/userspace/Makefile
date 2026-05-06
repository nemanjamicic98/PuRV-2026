# Naziv izvršnog fajla
TARGET = conway

# Kompajler i zastavice
CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -pthread

# Izvorni fajlovi (automatski pronalazi sve .c fajlove)
SRCS = conway.c init.c
# Objekat fajlovi (.o)
OBJS = $(SRCS:.c=.o)

# Glavno pravilo: kompajliraj sve
all: $(TARGET)

# Povezivanje objekat fajlova u izvršni program
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Kompajliranje svakog .c fajla u .o fajl
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Brisanje privremenih fajlova i izvršnog programa
clean:
	rm -f $(OBJS) $(TARGET)

# Pokretanje programa sa defaultnim argumentom (1 sekunda)
run: $(TARGET)
	./$(TARGET) 1