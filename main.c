#include <stdio.h>

#include "mbed.h"

int main(int argc, char **argv)
{
	if((argc&1) && argc < 2) {
		fprintf(stderr, "usage: %s <out-file> <file> <basename> [<file> <basename>...]\n", *argv);
		return 1;
	}

	struct mbed_info *mi = mbed_init(argv[1]);
	if(!mi) {
		fprintf(stderr, "failed to initialize mbed\n");
		return 1;
	}

	for(int i=2; i<argc; i+=2) {
		if(mbed_add_file(mi, argv[i], argv[i+1])) {
			fprintf(stderr, "failed to add file\n");
			mbed_finalize_error(mi);
			return 1;
		}
	}

	if(mbed_finalize(mi)) {
		fprintf(stderr, "failed to finalize mbed\n");
		mbed_finalize_error(mi);
		return 1;
	}

	return 0;
}
