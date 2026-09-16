.RECIPEPREFIX = >

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
TARGET = servidor
SRC = servidor.c

all:
>$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: all
>./$(TARGET)

clean:
>rm -f $(TARGET)
