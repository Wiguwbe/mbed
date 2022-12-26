
#include <stdio.h>

extern char test1[];
extern char test1_size;

extern char test2[];
extern char test2_size;

int main(int argc, char **argv)
{
	printf("--------\ntest1.txt:\n--------\n");
	fwrite(test1, 1, test1_size, stdout);
	printf("--------\ntest2.txt\n--------\n");
	fwrite(test2, 1, test2_size, stdout);
	printf("--------\n");
	return 0;
}
