
CC=gcc
MB=../mbed

test-run: test-run.o files.o
	$(CC) -o test-run test-run.o files.o

files.o: test1.txt test2.txt
	$(MB) files.o test1.txt test1 test2.txt test2

test-run.o: test-run.c
	$(CC) -c test-run.c

