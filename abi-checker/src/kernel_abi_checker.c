/*
 * Build command
 *
 * gcc `pkg-config --cflags glib-2.0` kernel_abi_checker.c kernel_abi_checker_elf.c -lglib-2.0 -lelf
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "kernel_abi_checker.h"

/*
 * Functions write single record into ouptu symver file
 */
int writeKernelSymbolsFile( FILE *ptr, char *symbolNameCrc, char *symbolName, char *symbolModule, char *flag )
{
	return fprintf( ptr, "%s\t%s\t%s\t%s\n", symbolNameCrc, symbolName, symbolModule, flag );
}

/*
 * Function open *symver file
 */
FILE *getFile( char *fileName )
{
	FILE *fPtr = fopen( fileName, "r" );

	if( fPtr == (FILE *)NULL )
	{
		PRINT_ERROR( "Canniot open : %s\n", fileName );
		return (FILE *)NULL;
	}

	return fPtr;
}

FILE *getOutputFile( char *fileName )
{
	FILE *fPtr = fopen( fileName, "w" );

	if( fPtr == (FILE *)NULL )
	{
		PRINT_ERROR( "Canniot open : %s\n", fileName );
		return (FILE *)NULL;
	}

	return fPtr;
}

void releaseFile( FILE *fptr )
{
	if( fptr != (FILE *)NULL )
		fclose( fptr );
}

void freeHashElement( gpointer data )
{
	free( (void *)data );
}

/*
 * Function creates hash table
 */
GHashTable *createHashTable( void )
{
	return g_hash_table_new_full( g_str_hash, g_str_equal, NULL, freeHashElement );
}

/*
 * Function destroy hash table
 */
void destroyHashTable( GHashTable *tab )
{
	if( tab != (GHashTable *)NULL )
		g_hash_table_destroy( tab );
}

/*
 * Function read i mput file.
 * Function return single KernelSymbol structure.
 *
 * Return (KernelSymbol *)-1 - if errora
          (KernelSymbol *)NULL - eof
 *
 */

KernelSymbol *readFileLine( FILE *fptr )
{
	KernelSymbol *symbolPtr = (KernelSymbol *)NULL;
	char inputLine[_LINE_LENGTH_ + 1];
	char *crcPtr;
	char *symNamePtr;
	char *moduleNamePtr;
	char *flag;
	char *ptr;
	int len;
	int i;

	memset( inputLine, 0, sizeof( inputLine ) );
	ptr = fgets( inputLine, _LINE_LENGTH_, fptr );
	if( ptr == (char *) NULL )
		return (KernelSymbol *)NULL;

	i = 0;

	/* CRC */
	crcPtr = inputLine + i;
	for( ;inputLine[i] != '\0' && inputLine[i] != '\t' && i < _LINE_LENGTH_; i ++ );

	if( i >= _LINE_LENGTH_ )
	{
		PRINT_ERROR( "Incorect input record format : \"%s\"\n", inputLine );
		return (KernelSymbol *)-1;
	}
	inputLine[i] = '\0';
	i++;

	/* Symbol name */
	symNamePtr = inputLine + i;

	for( ;inputLine[i] != '\0' && inputLine[i] != '\t' && i < _LINE_LENGTH_; i ++ );
	if( i >= _LINE_LENGTH_ )
	{
		PRINT_ERROR( "Incorect input record format : \"%s\"\n", inputLine );
		return (KernelSymbol *)-1;
	}
	inputLine[i] = '\0';
	i ++;

	/* Module name */
	moduleNamePtr = inputLine + i;


	for( ;inputLine[i] != '\0' && inputLine[i] != '\t' && i < _LINE_LENGTH_; i ++ );
	if( i >= _LINE_LENGTH_ )
	{
		PRINT_ERROR( "Incorect input record format : \"%s\"\n", inputLine );
		return (KernelSymbol *)-1;
	}
	inputLine[i] = '\0';
	i ++;

	/* Flag */
	flag = inputLine + i;


	len = strlen( flag );
	flag[len ==  0 ? 0 : len -1] = '\0';

	/* Allocate new KernelSymbol */
	symbolPtr = (KernelSymbol *)malloc( sizeof( KernelSymbol ) );
	if( symbolPtr == (KernelSymbol *)NULL )
		return (KernelSymbol *)-1;

	for( i = 0; moduleNamePtr[i] != '\0' && moduleNamePtr[i] != '\n'; i ++ );
	moduleNamePtr[i] = '\0';

	/* Polulate output structure */
	strncpy( symbolPtr -> symbolName, symNamePtr, KERNEL_SYMBOL_NAME_LENGTH );
	strncpy( symbolPtr -> symbolNameCrc, crcPtr, KERNEL_SYMBOL_NAME_CRC );
	strncpy( symbolPtr -> symbolModule, moduleNamePtr, KERNEL_SYMBOL_MODULE );
	strncpy( symbolPtr -> flag, flag, KERNEL_SYMBOL_FLAG );
	symbolPtr -> status = KERNEL_SYMBOL_STATUS_NO_TESTED;

	/* Return KernModule structure */
	return symbolPtr;
}

/*
 * Function adds single kernel symbol into hashtable.
 * Functions assumes that hash table is correctlly allocated.
 *
 * Returns:
 * -1 - duplicated key
 *  0 - success
 */

int addKernelSymboltoHashBase( void *s, char *key, GHashTable *tab )
{
	void *testSym;

	/* Check duplicates */
	testSym = (void *)g_hash_table_lookup( tab, key );
	if( testSym != (void *)NULL )
		return -1;

	/* Add new symbol into hash table */
	g_hash_table_insert( tab, key, s );
	return 0;
}

int addKernelSymboltoHash( KernelSymbol *s, GHashTable *tab )
{
	return addKernelSymboltoHashBase( (void *)s, s -> symbolName, tab );
}

int readFile( FILE *fptr, GHashTable *tab )
{
        KernelSymbol *sym = (KernelSymbol *)NULL;
        while( 1 == 1 )
        {
                sym = readFileLine( fptr );
		if( sym == (KernelSymbol *)NULL )
		{
			/* Koniec pliku */
			break;
		}

		if( sym == (KernelSymbol *)-1 )
		{
			/* Error */
			return -1;
		}

		if( addKernelSymboltoHash( sym, tab ) < 0 )
		{
			/* Error - duplicated value */
			return -1;
		}
        }

	/* Success */
	return 0;
}

GHashTable *procesInputFile( char *name )
{
	GHashTable *hashTable = (GHashTable *)NULL;
	FILE *fptr = (FILE *)NULL;

	/* Open input file */
	fptr = getFile( name );
	if( fptr == (FILE *)NULL )
		return (GHashTable *)NULL;

	/* Create hash table for current symver */
	hashTable = createHashTable();
	if( hashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Error creating hash table \n" );
		releaseFile( fptr );
		return (GHashTable *)NULL;
	}

	/* Read and create current hash table */
	if( readFile( fptr, hashTable ) != 0 )
	{
		PRINT_ERROR( "Error reading %s\n", name );
		destroyHashTable( hashTable );
		releaseFile( fptr );
		return (GHashTable *)NULL;
	}

	/* Close input file */
	releaseFile( fptr );

	return hashTable;
}

typedef struct _foreachHashData_
{
	KernelSymbolStatistics *stat;
	GHashTable *curr;
	GHashTable *new;
} _foreachHashData_;

void calculateStatistics_1( gpointer key_, gpointer value_, gpointer user_data_ )
{
	KernelSymbol *value = (KernelSymbol *)value_;
	_foreachHashData_ *user_data = (_foreachHashData_ *)user_data_;
	KernelSymbol *newSym;

	/* Update current hash table symbols count */
	user_data -> stat -> symver_current_count ++;

	/* Search for symbol in new hash table */
	newSym = (KernelSymbol *)g_hash_table_lookup( user_data -> new, key_ );

	if( newSym == (KernelSymbol *)NULL )
	{
		/* Symbol was removed */

		/* Set status */
		value -> status = KERNEL_SYMBOL_STATUS_REMOVED;

		user_data -> stat -> removed_symbols_count ++;
		return;
	}

	newSym -> status = KERNEL_SYMBOL_STATUS_VIEWED;

	if( strcmp( value -> symbolNameCrc, newSym -> symbolNameCrc ) == 0 )
	{
		user_data -> stat -> no_changed_symbols_count ++;
		value -> status = KERNEL_SYMBOL_STATUS_NO_CHANGES;
	}
	else
	{
		user_data -> stat -> changed_symbols_count ++;
		value -> status = KERNEL_SYMBOL_STATUS_CHANGED;
	}
}

void calculateStatistics_2( gpointer key_, gpointer value_, gpointer user_data_ )
{
	KernelSymbol *value = (KernelSymbol *)value_;
	_foreachHashData_ *user_data = (_foreachHashData_ *)user_data_;

	/* Update current hash table symbols count */
	user_data -> stat -> symver_new_count ++;

	if( value -> status == KERNEL_SYMBOL_STATUS_NO_TESTED )
	{
		user_data -> stat -> new_symbols_count ++;
		value -> status = KERNEL_SYMBOL_STATUS_NEW;
	}
}

void collectStatistics( GHashTable *curr, GHashTable *new, KernelSymbolStatistics *statistics )
{
	_foreachHashData_ foreachdata;

	/* Init foreach data */
	memset( (void *)statistics, 0, sizeof( KernelSymbolStatistics ) );
	foreachdata.stat = statistics;
	foreachdata.curr = curr;
	foreachdata.new = new;

	/* Scan current hash table */
	g_hash_table_foreach( curr, calculateStatistics_1, (gpointer)&foreachdata );

	/* Scan new hash table */
	g_hash_table_foreach( new, calculateStatistics_2, (gpointer)&foreachdata );
}

void reportSymbolsDetails( gpointer key_, gpointer value_, gpointer user_data_ )
{
	KernelSymbol *value = (KernelSymbol *)value_;

	switch( *((int *)user_data_) )
	{
	case 0 :	/* New */
		if( value -> status == KERNEL_SYMBOL_STATUS_NEW )
			PRINT_INFO_RAW( "\t| NEW     | %40s | %10s | %s\n", value -> symbolName, value -> symbolNameCrc, value -> symbolModule );
		break;

	case 1 :	/* Changed */
		if( value -> status == KERNEL_SYMBOL_STATUS_CHANGED )
			PRINT_INFO_RAW( "\t| CHANGED | %40s | %10s | %s\n", value -> symbolName, value -> symbolNameCrc, value -> symbolModule );
		break;

	case 2 :	/* Removed */
		if( value -> status == KERNEL_SYMBOL_STATUS_REMOVED )
			PRINT_INFO_RAW( "\t| REMOVED | %40s | %10s | %s\n", value -> symbolName, value -> symbolNameCrc, value -> symbolModule );
		break;
	}
}
//[CHANGED]         cfg80211_send_rx_assoc         0x80e54114                  vmlinux

void listChangedSymbols( GHashTable *curr, GHashTable *new, KernelSymbolStatistics *statistics )
{
	int mode = 0;
	int printFooter = 0;

	if( statistics -> new_symbols_count == 0 && statistics -> changed_symbols_count == 0 && statistics -> removed_symbols_count == 0 )
		return;

	PRINT_INFO_RAW( "\n" );

	PRINT_INFO_RAW( "\tNew symbols in kernel\n" );
	PRINT_INFO_RAW( "\t+---------+------------------------------------------+------------+--------------------------------\n" );
	PRINT_INFO_RAW( "\t| Change  |                 Linux kernel symbol name |        CRC | Module name \n" );
	PRINT_INFO_RAW( "\t+---------+------------------------------------------+------------+--------------------------------\n" );

	if( statistics -> new_symbols_count != 0 && new != (GHashTable *)NULL )
	{
		mode = 0;
		g_hash_table_foreach( new, reportSymbolsDetails, (gpointer)&mode );
		printFooter = 1;
	}

	if( statistics -> changed_symbols_count != 0 && curr != (GHashTable *)NULL )
	{
		if( statistics -> new_symbols_count != 0 )
			PRINT_INFO_RAW( "\t+---------+------------------------------------------+------------+--------------------------------\n" );

		mode = 1;
		g_hash_table_foreach( curr, reportSymbolsDetails, (gpointer)&mode );
		printFooter = 1;
	}

	if( statistics -> removed_symbols_count != 0 && curr != (GHashTable *)NULL )
	{
		if( statistics -> new_symbols_count != 0 || statistics -> changed_symbols_count != 0 )
			PRINT_INFO_RAW( "\t+---------+------------------------------------------+------------+--------------------------------\n" );

		mode = 2;
		g_hash_table_foreach( curr, reportSymbolsDetails, (gpointer)&mode );
		printFooter = 1;
	}

	if( printFooter == 1 )
		PRINT_INFO_RAW( "\t+---------+------------------------------------------+------------+--------------------------------\n" );
}

void reportsStatistics( GHashTable *curr, GHashTable *new, KernelSymbolStatistics *statistics )
{
	listChangedSymbols( curr, new, statistics );

	PRINT_INFO_RAW( "\n" );
	PRINT_INFO_RAW( "\tKernel symbols version statistics \n" );
	PRINT_INFO_RAW( "\t-------------------------------------------------\n" );
	PRINT_INFO_RAW( "\tSymbols in actual kernel/module . %d\n", statistics -> symver_current_count );
	if( new != (GHashTable *)NULL )
		PRINT_INFO_RAW( "\tSymbols in new kernel/module .... %d\n", statistics -> symver_new_count );
	PRINT_INFO_RAW( "\tNew symbols ..................... %d\n", statistics -> new_symbols_count );
	PRINT_INFO_RAW( "\tRemoved symbols ................. %d\n", statistics -> removed_symbols_count );
	PRINT_INFO_RAW( "\tChanged symbols ................. %d\n", statistics -> changed_symbols_count );
	PRINT_INFO_RAW( "\tUnchanged symbols ............... %d\n", statistics -> no_changed_symbols_count );
	PRINT_INFO_RAW( "\n" );
}

void printUsage( char *progName, int type )
{
	switch( type )
	{
	case 0 :
		PRINT_INFO_RAW( "\tUsage: %s test-kernel in_symver_file_1 in_symver_file_2\n", progName );
		break;

	case 1 :
		PRINT_INFO_RAW( "\tUsage: %s build-list in_module_symbols_file in_kernel_symver_file outt_module_symver_file\n", progName );
		break;

	case 2 :
		PRINT_INFO_RAW( "\tUsage: %s dump-module kernel_symver_file ko_module_file output_module_symver_file\n", progName );
		break;

	case 4 :
		PRINT_INFO_RAW( "\tUsage: %s test-module kernel_symver_file ko_module_file \n", progName );
		break;

	default :
		PRINT_INFO_RAW( "\tUsage: %s test|build-list file1 file2 [output_file] \n", progName );
		break;
	}
}

/*
 * Function parses input parameters and return.
 * Return:
 * 		-1 - error
 * 		 0 - test mode  - the program will compare two symver files
 * 		 1 - build mode - for given module sybmbols file and kernel symver creates files
 * 		                  with list kernel symbols used by the module.
 */
int parsInputParameters( int argc, char **argv, char **f1, char **f2, char **f3 )
{
	char *toolName = argv[0];

	if( argc < 2 )
	{
		printUsage( toolName, -1 );
		return -1;
	}

	if( strcmp( argv[1], "test-kernel" ) == 0 )
	{
		/* Test mode */
		if( argc < 4 )
		{
			printUsage( toolName, 0 );
			return -1;
		}
		*f1 = argv[2];
		*f2 = argv[3];
		*f3 = (char *)NULL;

		return 0;

	}
	else
	if( strcmp( argv[1], "build-list" ) == 0 )
	{
		/* Build mode */
		if( argc < 5 )
		{
			printUsage( toolName, 1 );
			return -1;
		}
		*f1 = argv[2];
		*f2 = argv[3];
		*f3 = argv[4];

		return 1;
	}
	else
	if( strcmp( argv[1], "dump-module" ) == 0 )
	{
		/* Build mode */
		if( argc < 5 )
		{
			printUsage( toolName, 2 );
			return -1;
		}
		*f1 = argv[2];
		*f2 = argv[3];
		*f3 = argv[4];

		return 2;
	}
	else
	if( strcmp( argv[1], "usage" ) == 0 )
	{
		return 3;
	}
	else
	if( strcmp( argv[1], "test-module" ) == 0 )
	{
		/* Test mode */
		if( argc < 4 )
		{
			printUsage( toolName, 4 );
			return -1;
		}
		*f1 = argv[2];
		*f2 = argv[3];
		*f3 = (char *)NULL;

		return 4;
	}


	printUsage( toolName, -1 );
	return -1;
}

int testKernel( char *f1, char *f2 )
{
	GHashTable *curr_hashTable = (GHashTable *)NULL;
	GHashTable *new_hashTable = (GHashTable *)NULL;
	KernelSymbolStatistics statistics;

	/*
	 * ----------------------------------------------
	 * Read current symver file
	 * ----------------------------------------------
	 */
	curr_hashTable = procesInputFile( f1 );
	if( curr_hashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Error read input \"%s\" file.\n", f1 );
		return 1;
	}

	/*
	 * ----------------------------------------------
	 * Read new symver file
	 * ----------------------------------------------
	*/
	new_hashTable = procesInputFile( f2 );
	if( new_hashTable == (GHashTable *)NULL )
	{
		destroyHashTable( curr_hashTable );
		PRINT_ERROR( "Error read input \"%s\" file.\n", f2 );
        return 1;
	}

	/*
	 * ----------------------------------------------
	 * Compute statistics
	 * ----------------------------------------------
	 */
	collectStatistics( curr_hashTable, new_hashTable, &statistics );

	/*
	 * ---------------------------------------------
	 * Report changes
	 * ---------------------------------------------
	 */
	reportsStatistics( curr_hashTable, new_hashTable, &statistics );

	/*
	 * --------------------------------------------
	 * Free hash tables
	 * --------------------------------------------
	 */
	destroyHashTable( curr_hashTable );
	destroyHashTable( new_hashTable );

	if( statistics.changed_symbols_count != 0 || statistics.removed_symbols_count != 0 || statistics.new_symbols_count != 0 )
		return 2;

	return 0;
}

void collectStatisticsModule( GHashTable *curr, GHashTable *module, KernelSymbolStatistics *statistics )
{
	_foreachHashData_ foreachdata;

	/* Init foreach data */
	memset( (void *)statistics, 0, sizeof( KernelSymbolStatistics ) );
	foreachdata.stat = statistics;
	foreachdata.curr = module;
	foreachdata.new = curr;

	/* Scan current hash table */
	g_hash_table_foreach( module, calculateStatistics_1, (gpointer)&foreachdata );
}


int testModule( char *f1, char *f2 )
{
	GHashTable *curr_hashTable = (GHashTable *)NULL;
	GHashTable *module_hashTable = (GHashTable *)NULL;
	KernelSymbolStatistics statistics;

	/*
	 * ----------------------------------------------
	 * Read current symver file
	 * ----------------------------------------------
	 */
	curr_hashTable = procesInputFile( f1 );
	if( curr_hashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Error read input \"%s\" file.\n", f1 );
		return 1;
	}

	/*
	 * ----------------------------------------------
	 * Read module symbols
	 * ----------------------------------------------
	*/
	module_hashTable = procesInputFile( f2 );
	if( module_hashTable == (GHashTable *)NULL )
	{
		destroyHashTable( curr_hashTable );
		PRINT_ERROR( "Error read input \"%s\" file.\n", f2 );
		return 1;
	}

	/*
	 * ----------------------------------------------
	 * Compute statistics
	 * ----------------------------------------------
	 */
	collectStatisticsModule( curr_hashTable, module_hashTable, &statistics );

	/*
	 * ---------------------------------------------
	 * Report changes
	 * ---------------------------------------------
	 */
	reportsStatistics( module_hashTable, (GHashTable *)NULL, &statistics );

	/*
	 * --------------------------------------------
	 * Free hash tables
	 * --------------------------------------------
	 */
	destroyHashTable( curr_hashTable );
	destroyHashTable( module_hashTable );

	if( statistics.changed_symbols_count != 0 || statistics.removed_symbols_count != 0 )
		return 2;

	return 0;
}

int buildListMode( char *f1, char *f2, char *f3 )
{
	GHashTable *curr_hashTable = (GHashTable *)NULL;
	char inputLine[_LINE_LENGTH_ + 1];
	FILE *symListFptr = (FILE *)NULL;
	FILE *outFptr = (FILE *)NULL;
	KernelSymbol *kSym;
	int size;

	/*
	 * ----------------------------------------------
	 * Read current symver file
	 * ----------------------------------------------
	 */
	curr_hashTable = procesInputFile( f1 );
	if( curr_hashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Error read input \"%s\" file.\n", f1 );
		return 1;
	}

	/*
	 * Open files
	 */
	outFptr = getOutputFile( f3 );
	symListFptr = getFile( f2 );

	if( symListFptr == (FILE *)NULL || outFptr == (FILE *)NULL )
	{
		destroyHashTable( curr_hashTable );
		releaseFile( symListFptr );
		releaseFile( outFptr );
		PRINT_ERROR( "Error open files : \"%s\", \"%s\".\n", f2, f3 );
		return 1;
	}

	while( fgets( inputLine, _LINE_LENGTH_, symListFptr ) )
	{
		size = strlen( inputLine );
		inputLine[size == 0 ? 0 :size - 1] = '\0';

		/* Search for symbol in new hash table */
		kSym = (KernelSymbol *)g_hash_table_lookup( curr_hashTable, inputLine );

		if( kSym != (KernelSymbol *)NULL )
			writeKernelSymbolsFile( outFptr, kSym -> symbolNameCrc, kSym -> symbolName, kSym -> symbolModule, kSym -> flag );
	}

	destroyHashTable( curr_hashTable );
	releaseFile( symListFptr );
	releaseFile( outFptr );

	return 0;
}

int callDumpModuleSymbols( char *f1, char *f2, char *f3 )
{
	GHashTable *curr_hashTable = (GHashTable *)NULL;
	GHashTable *module_hashTable = (GHashTable *)NULL;

	/*
	 * ----------------------------------------------
	 * Read current symver file
	 * ----------------------------------------------
	 */
	curr_hashTable = procesInputFile( f1 );
	if( curr_hashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Error read input \"%s\" file.\n", f1 );
		return 1;
	}

	/*
	 * ----------------------------------------------
	 * Read kernel module *.ko file
	 * ----------------------------------------------
	 */
	module_hashTable = readEfl( f2 );
	if( module_hashTable == (GHashTable *)NULL )
	{
		PRINT_ERROR( "Error read input \"%s\" file.\n", f2 );
		destroyHashTable( curr_hashTable );
		return 1;
	}

	/*
	 * Dump module symbols into output file
	 */
	if( dumpModuleSymbols( f3, module_hashTable, curr_hashTable ) < 0 )
	{
		PRINT_ERROR( "Error creating dump file \"%s\"\n", f3 );
		destroyHashTable( curr_hashTable );
		return 1;
	}

	/*
	 * Free hash tables
	 */
	destroyHashTable( module_hashTable );
	destroyHashTable( curr_hashTable );

	return 0;
}

int printUsageInfo( char *f1 )
{
	PRINT_INFO_RAW( "\n" );
	PRINT_INFO_RAW( "\tProgram usage\n" );
	PRINT_INFO_RAW( "\t-----------------------------------------------\n" );
	PRINT_INFO_RAW( "\n" );
	printUsage( f1, 0 );
	printUsage( f1, 1 );
	printUsage( f1, 2 );

	return 0;
}

int main(int argc, char** argv)
{
	char *f1 = (char *)NULL;
	char *f2 = (char *)NULL;
	char *f3 = (char *)NULL;
	int rc = -1;

	int mode = parsInputParameters( argc, argv, &f1, &f2, &f3 );

	switch( mode )
	{
	case 0 :
		rc = testKernel( f1, f2 );
		break;

	case 1 :
		rc = buildListMode( f1, f2, f3 );
		break;

	case 2 :
		rc = callDumpModuleSymbols( f1, f2, f3 );
		break;

	case 3 :
		rc = printUsageInfo( argv[0] );
		break;

	case 4 :
		rc = testModule( f1, f2 );
		break;
	}

	PRINT_INFO_RAW( "\n" );

	if( rc == 2 )
		PRINT_INFO_RAW( "\tChanges !!!\n" );
	else
	if( rc == 1 )
		PRINT_INFO_RAW( "\tError !!!\n" );
	else
		PRINT_INFO_RAW( "\tDone\n" );

	PRINT_INFO_RAW( "\n" );

	return ( rc != 0 ) ? 1 : 0;
}
