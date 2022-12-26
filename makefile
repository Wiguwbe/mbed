# do both library and cli-tool

CC=gcc
LIBS=-lelf

# cli tool
mbed: main.o mbed.o
	$(CC) -o $@ $^ $(LIBS)

libmbed.so: mbed-shared.o
	$(CC) -shared -o $@ $^ $(LIBS)

libmbed.a: mbed.o
	ar rcs $@ $^

mbed-shared.o: mbed.c
	$(CC) -c -fPIC $^ -o $@

%.o: %.c
	$(CC) -c $<
