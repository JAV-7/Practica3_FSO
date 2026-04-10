# Compilador
CC = gcc

# Banderas de compilaciOn
CFLAGS = -Wall -O2

# LibrerIas
LDLIBS = -lm

# Programas a generar
TARGETS = mercator

all: $(TARGETS)

mercator: mercator.c
	$(CC) mercator.c -o mercator $(CFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGETS)
