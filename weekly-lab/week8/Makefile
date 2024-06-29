TARGET=w8
CC=gcc
# CFLAGS=-Wall
OBJECTS=w8.o

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lreadline

w7.o: w7.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
