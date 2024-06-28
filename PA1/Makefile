TARGET=pa1
CC=gcc
CFLAGS=-Wall
OBJECTS=main.o util.o

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c util.h
	$(CC) $(CFLAGS) -c main.c -o main.o

util.o: util.c util.h
	$(CC) $(CFLAGS) -c util.c -o util.o

clean:
	rm -f $(OBJECTS) $(TARGET)
