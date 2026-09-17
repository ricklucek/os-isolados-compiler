CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -pedantic
TARGET := macaronica
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) examples/exemplo_minimo.mac

test: $(TARGET)
	@echo "Infraestrutura de testes criada. Os casos serão adicionados nos checkpoints seguintes."

clean:
	rm -f $(OBJ) $(TARGET)
