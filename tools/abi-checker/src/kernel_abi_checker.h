#ifndef _kernel_abi_checker_h_
#define _kernel_abi_checker_h_

#include <stdio.h>
#include <glib.h>

/*
 * Definitions
 */
#define KERNEL_SYMBOL_NAME_LENGTH	64
#define KERNEL_SYMBOL_NAME_CRC		32
#define KERNEL_SYMBOL_MODULE		256
#define KERNEL_SYMBOL_FLAG			256

#define KERNEL_SYMBOL_STATUS_NO_TESTED	0
#define KERNEL_SYMBOL_STATUS_NO_CHANGES	1
#define KERNEL_SYMBOL_STATUS_NEW		2
#define KERNEL_SYMBOL_STATUS_REMOVED	3
#define KERNEL_SYMBOL_STATUS_CHANGED	4
#define KERNEL_SYMBOL_STATUS_VIEWED		5

#define _LINE_LENGTH_ 4096

/*
 * Data types
 */
typedef struct KernelSymbol
{
	char symbolName[KERNEL_SYMBOL_NAME_LENGTH + 1];
	char symbolNameCrc[KERNEL_SYMBOL_NAME_CRC + 1];
	char symbolModule[KERNEL_SYMBOL_MODULE + 1];
	char flag[KERNEL_SYMBOL_FLAG + 1];
	int status;

} KernelSymbol;

typedef struct KernelSymbolStatistics
{
	int symver_current_count;
	int symver_new_count;

	int new_symbols_count;
	int removed_symbols_count;
	int changed_symbols_count;
	int no_changed_symbols_count;

} KernelSymbolStatistics;

extern int writeKernelSymbolsFile( FILE *, char *, char *, char *, char * );
extern int addKernelSymboltoHashBase( void *, char *, GHashTable * );
extern int dumpModuleSymbols( char *, GHashTable *, GHashTable * );
extern void destroyHashTable( GHashTable * );
extern GHashTable *createHashTable( void );
extern FILE *getOutputFile( char * );
extern void releaseFile( FILE * );
GHashTable *readEfl( char * );

#define PRINT_ERROR(...)  do { fprintf( stderr, "ERROR : " ); fprintf( stderr, __VA_ARGS__ ); } while( 0 )
#define PRINT_INFO_RAW(...)   do { fprintf( stdout, __VA_ARGS__ ); } while( 0 )
#define PRINT_INFO(...)   do { fprintf( stderr, "INFO  : " ); fprintf( stdout, __VA_ARGS__ ); } while( 0 )

#endif
