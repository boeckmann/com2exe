/*
COM2EXE - converts DOS .COM files to .EXE files
Copyright (C) 2024 Bernd Boeckmann

Distributed under the ISC license.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#pragma pack(push, 1)

typedef struct {
	char sig[2];
	unsigned short bytes_last_page;
	unsigned short pages_count;
	unsigned short reloc_count;
	unsigned short header_para_size;
	unsigned short min_paras;
	unsigned short max_paras;
	unsigned short start_ss;
	unsigned short start_sp;
	unsigned short chksum;
	unsigned short start_ip;
	unsigned short start_cs;
	char reserved[488];

} MZHEADER;

#pragma pack(pop)

int main( int argc, char *argv[] )
{
	static char buf[4096];
	static MZHEADER header;

	char *in_fn = NULL, *out_fn = NULL;
	FILE *in_f = NULL, *out_f = NULL;
	unsigned long in_size = 0;
	int plain_copy = 0;

	int result = -1;

	if ( argc != 2 ) {
		puts( "Usage: COM2EXE <COM file>\n" );
		result = 1;
		goto bye;
	}

	{	/* build input and output file names */
		char *fn = argv[1], *p;

		in_fn = malloc( strlen( fn ) + 5 );	/* +5 for possible suffix extension */
		out_fn = malloc( strlen( fn ) + 5 );

		if ( in_fn == NULL || out_fn == NULL ) {
			puts( "allocation error" );
			result = 2;
			goto bye;
		}

		/* build input file name */
		strcpy( in_fn, fn );
		if ( !strchr( in_fn, '.') ) {
			strcat( in_fn, ".com" );
		}

		/* build output file name */
		strcpy( out_fn, in_fn );
		p = out_fn + strlen( out_fn ) - 1;
		while ( *p != '.' ) *p-- = '\0';
		strcat( out_fn, "exe" );

		if ( strcasecmp( in_fn, out_fn ) == 0 ) {
			puts( "will not overwrite file!" );
			result = 8;
			goto bye;
		}
	}

	{ /* open input file and test if already MZ */
		in_f = fopen( in_fn, "rb" );
		if ( in_f == NULL ) {
			puts( "error opening input file" );
			result = 3;
			goto bye;
		}
		/* determine .COM size */
		fseek( in_f, 0, SEEK_END );
		in_size = (unsigned long)ftell( in_f );
		fseek( in_f, 0, SEEK_SET );

		if ( in_size >= 2 ) {
			/* check if not already an .EXE */
			if ( fread( buf, 2, 1, in_f ) != 1 ) {
				puts( "can not read input file" );
				result = 7;
				goto bye;
			}
			fseek( in_f, 0, SEEK_SET );
			if ( buf[0] == 'M' || buf[1] == 'Z' ) {
				puts( "already a MZ-executable, making plain copy" );
				plain_copy = 1;
			}
		}
	}
	{	/* open output file */
		out_f = fopen( out_fn, "wb" );
		if ( out_f == NULL ) {
			puts( "error opening output file" );
			result = 4;
			goto bye;
		}
	}

	if ( !plain_copy ) { /* write header */
		header.sig[0] = 'M';
		header.sig[1] = 'Z';
		header.pages_count = ( (in_size + sizeof(MZHEADER) + 511) >> 9 );
		header.bytes_last_page = (in_size + sizeof(MZHEADER)) & 0x1ff;
		header.header_para_size = (sizeof(MZHEADER) + 15) >> 4;
	
		if ( in_size < 0xff00 ) {
			/* make sure at least 64K are reserved for the program, as we
			   unconditionally set SP to 0xfffe. The 0xff00 = 0x10000 - 0x100
			   comes from the PSP that contributes to the size */
			header.min_paras = (0xff00 - in_size + 15) >> 4;
		}
		header.max_paras = 0xffff;
		header.start_ss = 0xfff0;
		header.start_sp = 0xfffe;
		header.start_cs = 0xfff0;
		header.start_ip = 0x0100;
	
		if ( fwrite( &header, sizeof(header), 1, out_f ) != 1 ) {
			puts( "error writing MZ header" );
			result = 5;
			goto bye;
		}
	}

	{ /* copy .COM to .EXE */
		size_t len;
		while ( ( len = fread( buf, 1, sizeof(buf), in_f) ) != 0 ) {
			if ( !fwrite( buf, len, 1, out_f ) ) {
				puts( "error writing .EXE file" );
				result = 6;
				goto bye;
			}
		}
		if ( !feof( in_f ) ) {
			puts( "error reading .COM file" );
			result = 7;
			goto bye;
		}
	}

	result = 0;

bye:
	if ( out_f != NULL ) fclose( out_f );
	if ( in_f != NULL ) fclose( in_f );

	if ( result == 6 || result == 7 ) {
		/* remove output file if read or write error occured */
		remove( out_fn );
	}

	if ( out_fn != NULL ) free( out_fn );
	if ( in_fn != NULL ) free( in_fn );

	return result;
}
