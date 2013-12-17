#include "kernel_abi_checker.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libelf.h>
#include <stdio.h>
#include <fcntl.h>
#include <gelf.h>

GHashTable *readEfl( char *file )
{
	GHashTable *outputHashTable = (GHashTable *)NULL;
	Elf_Data *edata = (Elf_Data *)NULL;
	Elf_Scn *scn = (Elf_Scn *)NULL;
	char *base_ptr = (char *)NULL;
	char *sname = (char *)NULL;
	Elf *elf = (Elf *)NULL;
	struct stat elf_stats;
	int symbol_count;
	GElf_Shdr shdr;
	GElf_Sym sym;
	int size;
	int fd;
	int i;

	fd = open( file, O_RDONLY );
	if( fd < 0 )
	{
		PRINT_ERROR( "Cannot open efl file : %s\n", file );
		return (GHashTable *)NULL;
	}

	if( ( fstat( fd, &elf_stats ) ) )
	{
		PRINT_ERROR( "Cannot stat efl file : %s\n", file );
		close(fd );
		return (GHashTable *)NULL;
	}

	base_ptr = (char *) malloc( elf_stats.st_size );
	if( base_ptr == (char *)NULL )
	{
		PRINT_ERROR( "Memory allocation error.\n" );
		close( fd );
		return (GHashTable *)NULL;
	}

	if( ( read( fd, base_ptr, elf_stats.st_size ) ) < elf_stats.st_size )
	{
		PRINT_ERROR( "Cannot read input efl file : %s\n", file );
        free( base_ptr );
        close( fd );
		return (GHashTable *)NULL;
	}
	free( base_ptr );

	/* Create output hash table */
	outputHashTable = createHashTable();
	if( outputHashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Memory allocation for hash table error : %s\n", file );
        free( base_ptr );
        close( fd );
		return (GHashTable *)NULL;
	}

	if( elf_version( EV_CURRENT ) == EV_NONE )
		fprintf( stderr, "WARNING Elf Library is out of date!\n" );

	// Iterate through section headers again this time well stop when we find symbols
	elf = elf_begin( fd, ELF_C_READ, NULL );

	while( ( scn = elf_nextscn( elf, scn ) ) != (Elf_Scn *)NULL )
	{
		gelf_getshdr( scn, &shdr );

		// When we find a section header marked SHT_SYMTAB stop and get symbols
		if(shdr.sh_type == SHT_SYMTAB)
		{
			// edata points to our symbol table
			edata = elf_getdata(scn, edata);

			symbol_count = shdr.sh_size / shdr.sh_entsize;

			for( i = 0; i < symbol_count; i++ )
			{
				/* Get symbol */
				gelf_getsym( edata, i, &sym );

				/* Get symbol name length */
				size = strlen( elf_strptr( elf, shdr.sh_link, sym.st_name ) );
				if( size != 0 )
				{
					/* allocate memory for symbol name */
					sname = (char *)malloc( sizeof( char ) * ( size + 1 ) );
					if( sname == (char *)NULL )
					{
						PRINT_ERROR( "Memory allocation for hash table error : %s\n", file );
						destroyHashTable( outputHashTable );
				        close( fd );
						return (GHashTable *)NULL;
					}

					strcpy( sname, elf_strptr( elf, shdr.sh_link, sym.st_name ) );

					// Add symbol to hash
					if( addKernelSymboltoHashBase( (void *)sname, sname, outputHashTable ) < 0 )
					{
						// If duplicated key free memory
						free( sname );
					}

				}
			}
		}
	}

	/* Close file */
	close( fd );

	/* Return hash table */
	return outputHashTable;
}

/*
 * Private user data definition.
 */

typedef struct _efl_user_data_
{
	GHashTable *kernelHash;
	FILE *outFilePtr;
} _efl_user_data_;


void dumpModuleSymbolsIteration( gpointer key_, gpointer value_, gpointer user_data_ )
{
	char *key = (char *)key_;
	_efl_user_data_ *user_data = (_efl_user_data_ *)user_data_;
	KernelSymbol *ptr = (KernelSymbol *)NULL;

	ptr = (KernelSymbol *)g_hash_table_lookup( user_data -> kernelHash, key );
	if( ptr != (void *)NULL )
		writeKernelSymbolsFile( user_data -> outFilePtr, ptr -> symbolNameCrc, ptr -> symbolName, ptr -> symbolModule, ptr -> flag );
}

/*
 * Function dumps into output files kernel symbols that
 * are use in the module
 */
int dumpModuleSymbols( char *outoutFile, GHashTable *moduleSymbols, GHashTable *kernelSymbols )
{
	FILE *outputFilePtr = getOutputFile( outoutFile );
	_efl_user_data_ userData;

	if( outputFilePtr == (FILE *)NULL )
		return -1;

	userData.outFilePtr = outputFilePtr;
	userData.kernelHash = kernelSymbols;

	/* Loob module symbols */
	g_hash_table_foreach( moduleSymbols, dumpModuleSymbolsIteration, (gpointer)&userData );

	/* Close output file */
	releaseFile( outputFilePtr );

	return 0;
}
