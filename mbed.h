/*
	Interface to `mbed` "library"
*/

#ifndef _MBED_HEADER_H_
#define _MBED_HEADER_H_

/* opaque struct */
struct mbed_info;

/*
	initialize info/file

	`outfile`: filename to write to

	returns NULL on error
*/
struct mbed_info *
mbed_init(char *outfile);

/*
	embed file (and size) in object, with a specific filename

	`mbed_info`: info returned by `init()`
	`filename`: file to add
	`basename`: basename for file to add

	exported symbols will be:
	- `<basename>`: the data of the file
	- `<basename>_size`: the size of the data

	returns 0 on success, !0 on error
*/
int
mbed_add_file(
	struct mbed_info *mbed_info,
	char *filename,
	char *basename
);

/*
	finalize and write object file to disk

	`mbed_info`: info returned by `init()`

	returns 0 on sucess, !0 on error
*/
int
mbed_finalize(struct mbed_info *mbed_info);

/*
	finalize with a fail, memory will be freed, file will be deleted

	same interface as `mbed_finalize`,

	no error checking
*/
void
mbed_finalize_error(struct mbed_info *mbed_info);

#endif

