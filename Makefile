CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -g
INCLUDE = -Iinclude

SRC = \
    src/main.c \
    src/tokenizer.c \
    src/parser.c \
    src/executor.c \
    src/builtins.c

OBJ = $(SRC:.c=.o)

TARGET = sh

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(TARGET)

re: fclean all
