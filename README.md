# mbed

Embed multiple files into an ELF object file.

The idea is to simplify `objcopy` for embedding files, both on its command
line and the names of the symbols

### Usage

For the command line tool, usage is as

`mbed <outfile> <file1> <file1-basename> [<file2> <file2-basename> ...]`

where
- `outfile` is the output object file;
- `fileN` is the file to embed;
- `fileN-basename` is the basename for the exported variables.

---

The variables exported are:
- `const char <fileN-basename>[]` holding the pointer to the data;
- `const unsigned int <fileN-basename>_size` holding the size of the data.

The `test` folder should have an example, with a makefile. `readelf -a` should
allow viewing of the exposed symbols.

### Library

The current makefile also allows libraries to be build, both dynamic and
static.

The interface to the library is exposed in `mbed.h`. It should be
self-explanatory.

### Dependencies

The tool/library depends on `libelf`. Please see your system's notes on how to
install it.
