#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

#include "mbed.h"

/*
	interface:

	- init: open/prepare files and headers
	- add_file: add file to object (write data and add size)
	- finalize: write rest of sections and close/write file
*/

// also need linked-list for symtab
struct symtab_list {
	struct symtab_list *next;
	// for file
	Elf64_Sym st_file;
	// for size
	Elf64_Sym st_size;
};

// basically a linked-list of strings
struct strtab_list {
	struct strtab_list *next;
	char *st_file;
	char *st_size;
	unsigned st_file_len;
	unsigned st_size_len;
};


struct mbed_info {
	FILE *outf;
	char filename[512];	// should be big enough
	struct symtab_list *syms;
	struct symtab_list *syms_tail;	// faster appending
	struct strtab_list *strs;
	struct strtab_list *strs_tail;	// same
	// current .data size
	unsigned data_size;
	unsigned strtab_index;
};

// for internal use
typedef struct mbed_info mbed_t;


mbed_t *mbed_init(char *outfile)
{
	mbed_t *mi = (mbed_t*)malloc(sizeof(mbed_t));
	if(!mi)
		return NULL;

	mi->outf = fopen(outfile, "wb");
	if(!mi->outf) {
		free(mi);
		return NULL;
	}

	strcpy(mi->filename, outfile);	// let them overflow this

	mi->syms = mi->syms_tail = NULL;
	mi->strs = mi->strs_tail = NULL;
	mi->data_size = 0;
	mi->strtab_index = 1;	// null shall be first

	{ /* write initial data */
		Elf64_Ehdr elf_header = {
			.e_ident = {
				ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
				ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_SYSV
			},
			.e_type = ET_REL,
			.e_machine = EM_X86_64,
			.e_version = EV_CURRENT,
			.e_entry = 0,
			.e_phoff = 0,
			.e_shoff = 0x40,
			.e_flags = 0,
			.e_ehsize = sizeof(Elf64_Ehdr),
			.e_phentsize = sizeof(Elf64_Phdr),
			.e_phnum = 0,
			.e_shentsize = sizeof(Elf64_Shdr),
			.e_shnum = 5,
			.e_shstrndx = 4
		};

		if(fwrite(&elf_header, sizeof elf_header, 1, mi->outf) != 1) {
			// error
			fclose(mi->outf);
			free(mi);
			return NULL;
		}
	}

	{ /* skip section headers for now */
		Elf64_Shdr fake_shdr[5];
		memset(fake_shdr, 0, 5*sizeof(Elf64_Shdr));
		if(fwrite(fake_shdr, sizeof(Elf64_Shdr), 5, mi->outf) != 5) {
			fclose(mi->outf);
			free(mi);
			return NULL;
		}
	}

	// file index should be at .data section

	return mi;
}


int mbed_add_file(mbed_t *mi, char *filename, char *basename)
{
	unsigned total_size = 0;

	// prepare symtab stuff
	struct symtab_list *sym_node = (struct symtab_list *)malloc(sizeof(struct symtab_list));
	if(!sym_node)
		return 1;
	sym_node->next = NULL;

	{ /* prepare first symbol (file) */
		Elf64_Sym *sy = &sym_node->st_file;
		sy->st_name = mi->strtab_index;
		sy->st_value = mi->data_size;
		sy->st_size = 0;
		sy->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
		sy->st_other = STV_DEFAULT;
		sy->st_shndx = 1;
	}

	{ /* copy file */
		// already buffered anyway
		unsigned char buffer[128];
		int part_len;
		FILE *inf = fopen(filename, "rb");
		if(!inf)
			return 1;

		while((part_len = fread(buffer, 1, 128, inf)) == 128) {
			if(fwrite(buffer, 1, part_len, mi->outf) != part_len) {
				fclose(inf);
				return 2;
			}
			total_size += part_len;
		}
		if(part_len>0) {
			if(fwrite(buffer, 1, part_len, mi->outf) != part_len) {
				fclose(inf);
				return 3;
			}
			total_size += part_len;
		}

		if(!feof(inf)) {
			// failed to read
			fclose(inf);
			return 4;
		}
		fclose(inf);
	}

	mi->data_size += total_size;

	{ /* another symbol on symtab (size) */
		Elf64_Sym *sy = &sym_node->st_size;
		sy->st_name = 0;	// set later
		sy->st_value = mi->data_size;	// update later
		sy->st_size = sizeof(total_size);
		sy->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
		sy->st_other = STV_DEFAULT;
		sy->st_shndx = 1;
	}

	{ /* write file size value */
		// align at "4"
		char *align = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
		int align_amount = sizeof(total_size) - ( mi->data_size & (sizeof(total_size)-1));
		if(align_amount == sizeof(total_size)) align_amount = 0;
		if(align_amount) {
			if(fwrite(align, 1, align_amount, mi->outf) != align_amount) {
				return 5;
			}
			// update value with align
			sym_node->st_size.st_value += align_amount;
		}


		// write data
		if(fwrite(&total_size, sizeof(total_size), 1, mi->outf) != 1) {
			return 6;
		}

		mi->data_size += align_amount + sizeof(total_size);
	}

	/* add symbols to list */
	if(!mi->syms) {
		mi->syms = mi->syms_tail = sym_node;
	} else {
		// tail append
		mi->syms_tail->next = sym_node;
		mi->syms_tail = sym_node;
	}

	{ /* add strings to list */
		struct strtab_list *str_node = (struct strtab_list*)malloc(sizeof(struct strtab_list));
		if(!str_node)
			return 1;
		str_node->next = NULL;
		int basename_len = strlen(basename);
		str_node->st_file = (char*)malloc(basename_len+1);
		if(!str_node->st_file) {
			free(str_node);
			return 1;
		}
		strcpy(str_node->st_file, basename); // safe enough

		str_node->st_size = (char*)malloc(basename_len+6);
		if(!str_node->st_size) {
			free(str_node->st_file);
			free(str_node);
			return 1;
		}
		sprintf(str_node->st_size, "%s_size", basename);	// safe enough

		// store sizes for easeness, include null byte
		str_node->st_file_len = basename_len + 1;
		str_node->st_size_len = basename_len + 6;

		// add to list
		if(!mi->strs) {
			mi->strs = mi->strs_tail = str_node;
		} else {
			mi->strs_tail->next = str_node;
			mi->strs_tail = str_node;
		}

		// update symtab values
		mi->strtab_index += basename_len+1;	// file symbol name
		sym_node->st_size.st_name = mi->strtab_index;
		mi->strtab_index += basename_len+6;
	}

	// all good?
	return 0;
}

/*
	final ELF format:

	0x00
	ELF Header

	+ 0x40
	Sect Headers

	+ 5*sizeof(Shdr)
	.data

	+ data_len
	.symtab

	+ num_sym * sizeof(Sym)
	.strtab

	+ len(.strtab)
	.shstrtab
*/

int mbed_finalize(mbed_t *mi)
{
	// 1. write symtab
	// 2. write strtab
	// 3. write shstrtab
	// 4. go back and write section headers
	int data_size = mi->data_size;
	int symtab_size = 0;
	int strtab_size = mi->strtab_index;
	int shstrtab_size = 0;

	{ /* write .symtab */
		// align symtab
		char *align = "\0\0\0\0\0\0\0";
		int align_amount = 8 - (mi->data_size & 0x7);
		if(align_amount == 8) align_amount = 0;
		if(align_amount) {
			if(fwrite(align, 1, align_amount, mi->outf) != align_amount)
				return 1;
			data_size += align_amount;
		}

		// write symtab
		// null symbol first
		Elf64_Sym null_sym;
		memset(&null_sym, 0, sizeof null_sym);
		if(fwrite(&null_sym, sizeof null_sym, 1, mi->outf) != 1) {
			return 2;
		}
		symtab_size += sizeof null_sym;
		// and the rest
		struct symtab_list *syp;
		for(syp = mi->syms; syp; syp = syp->next) {
			if(fwrite(&syp->st_file, sizeof(Elf64_Sym), 1, mi->outf) != 1)
				return 2;
			if(fwrite(&syp->st_size, sizeof(Elf64_Sym), 1, mi->outf) != 1)
				return 2;
			symtab_size += sizeof(Elf64_Sym) << 1;
		}
	}

	{ /* write .strtab */
		// null header
		if(fputc(0, mi->outf))
			return 3;
		struct strtab_list *stp;
		for(stp = mi->strs; stp; stp = stp->next) {
			if(fwrite(stp->st_file, 1, stp->st_file_len, mi->outf) != stp->st_file_len)
				return 3;
			if(fwrite(stp->st_size, 1, stp->st_size_len, mi->outf) != stp->st_size_len)
				return 3;
		}
	}

	{ /* write .shstrtab */
		char *shstrtab = "\0.symtab\0.strtab\0.shstrtab\0.data\0";
		shstrtab_size = 33;
		if(fwrite(shstrtab, 1, shstrtab_size, mi->outf) != shstrtab_size)
			return 4;
	}

	{ /* go back and write section headers */
		// go back
		fseek(mi->outf, sizeof(Elf64_Ehdr), SEEK_SET);

		Elf64_Shdr sect_header[5];
		memset(sect_header, 0, 5*sizeof(Elf64_Shdr));
		// only set the non-zero
		Elf64_Shdr *sh = sect_header;
		// null header
		sh->sh_type = SHT_NULL;	// probably zero tho
		// .data header
		sh++;
		sh->sh_name = 27;
		sh->sh_type = SHT_PROGBITS;
		sh->sh_flags = SHF_WRITE|SHF_ALLOC;
		sh->sh_offset = 0x40 + 5*sizeof(Elf64_Shdr);
		sh->sh_size = mi->data_size;
		sh->sh_addralign = 1;
		// .symtab header
		sh++;
		sh->sh_name = 1;
		sh->sh_type = SHT_SYMTAB;
		sh->sh_offset = (sh-1)->sh_offset + data_size;	// not `mi->data_size`, this one includes the align
		sh->sh_size = symtab_size;
		sh->sh_link = 3;
		sh->sh_info = 1;
		sh->sh_addralign = 8;
		sh->sh_entsize = sizeof(Elf64_Sym);
		// .strtab header
		sh++;
		sh->sh_name = 9;
		sh->sh_type = SHT_STRTAB;
		sh->sh_offset = (sh-1)->sh_offset + symtab_size;
		sh->sh_size = strtab_size;
		sh->sh_addralign = 1;
		// .shstrtab header
		sh++;
		sh->sh_name = 17;
		sh->sh_type = SHT_STRTAB;
		sh->sh_offset = (sh-1)->sh_offset + strtab_size;
		sh->sh_size = shstrtab_size;
		sh->sh_addralign = 1;

		// write it all down
		if(fwrite(sect_header, sizeof(Elf64_Shdr), 5, mi->outf) != 5)
			return 5;
	}


	if(fclose(mi->outf))
		return 6;

	{ /* free syms */
		struct symtab_list *syp;
		while(mi->syms) {
			syp = mi->syms->next;
			free(mi->syms);
			mi->syms = syp;
		}
	}
	{ /* free strs */
		struct strtab_list *stp;
		while(mi->strs) {
			stp = mi->strs->next;
			free(mi->strs->st_file);
			free(mi->strs->st_size);
			free(mi->strs);
			mi->strs = stp;
		}
	}

	return 0;
}


void mbed_finalize_error(mbed_t *mi)
{
	// close and remove file
	fclose(mi->outf);
	unlink(mi->filename);

	{ /* free syms */
		struct symtab_list *syp;
		while(mi->syms) {
			syp = mi->syms->next;
			free(mi->syms);
			mi->syms = syp;
		}
	}
	{ /* free strs */
		struct strtab_list *stp;
		while(mi->strs) {
			stp = mi->strs->next;
			free(mi->strs->st_file);
			free(mi->strs->st_size);
			free(mi->strs);
			mi->strs = stp;
		}
	}
}
