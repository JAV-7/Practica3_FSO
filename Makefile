# Makefile para mercator_msg.c
# Compilador
CC = gcc
 
# Flags de compilacion 
CFLAGS = -Wall

# Librerías (colas de mensajes POSIX y math)
LDLIBS = -lrt -lm

# Programa a compilar
TARGET = mercator_msg
SRC = mercator_msg.c

all: $(TARGET)

$(TARGET): $(SRC)
		$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

clean:
		rm -f $(TARGET)
