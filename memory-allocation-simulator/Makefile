CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=c99 -O2
TARGET   = memsim
SRC      = main.c allocator.c

all: $(TARGET)

$(TARGET): $(SRC) allocator.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all run clean
