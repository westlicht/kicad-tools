/*
 * RenumKiCadPCB.c v0.203
 *
 *  Created on: Aug 3, 2016
 *      Author: Brian
 */

/*
 * RenumKiCadPCBV200.c
 *
 *  Created on: Aug 15, 2016
 *      Author: Brian Piccioni DocumentedDesigns.com
 *      (c) Bian Piccioni
 *
 *      This is free software made available under the GNU General Public License(GPL) version 3 or greater.
 *      see https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *		It can be modified, copied, etc. provided:
 *		1) Original authorship of Brian Piccioni and DocumentDesigns.com is prominently noted
 *		2) Any derivative work or copy also be released under GPL V3 or later with the attribution mentioned in 1)
 *
 *		This software is release "as is" and with no warranty.
 *
 *		Please contact brian@documenteddesigns.com with any feature requests/bug notifications
 *		If you want me to fix a problem or add a feature I will expect you to provide sample files showing
 *		the problem or feature need.
 *
 * Changes:
 * 	V0.101 	Replaced 	"printf(ERRSTR)" with puts ( ERRSTR ) in ParseCommandLine because of compiler warning under Linux
 * 			Added		return( ERRINVALIDEXIT ); because of compiler warning under Linux
 * 			Moved		gettimeofday( ); from main() to ParseCommandLine and changed abort decision to ensure valid run time reported
 *
 *	V0.102	Rewrote much of the code to support schematic hiearchy sheets
 *			Added		-n (don't ask a question) to the command line
 *			Removed		-oOutputfile because it would not work with hierarchies
 *
 *	V0.103	Added 		UpdatePCBNetNames to deal with KiCad and pours

 *	V0.104	Rewrote 	UpdatePCBFileRefDes to do both the NetNames and the reference designations
 *						This cleans up and speeds up the process
 *						Fixed problem with ExtractPath when in the same directory as the files
 *
 *	V0.105				Modified ExtractPath to trim file extensions (i.e. filename.kicad_PCB is the same as filename)
 *						Moved files over to MingW which means no DLL is needed - the code is standalone.
 *						Added code to strip quotes inserted by Eclipse or Mingw on command line arguments
 *						Changed fread/fwrite attribute to rb, wb because Mingw libraries didn't like a character in the text files
 *
 *	V0.200				Added a menu driven interface. Changed command line arguments to align with menu.
 *						Flag warnings for parts found on PCB but not in schematic (except those flagged as ignore)
 *
 *	V0.201				Store /clear default values
 *
 *	V0.202				Changed "Jog" approach to "Grid" approach
 *
 *	V0.203				Cleaned up code. Do a global "round to grid". Write out log file, change file, etc.,
 *						removed Verbose, and Show Change Plan option. Fixed bug where ref des offset are relative to module
 *						axis not relative to module center
 *	V0.203				Cleaned up code. Do a global "round to grid". Write out coordinate file, update list, and
 *						ref des change plan files (_coord.txt, _update.txt, _change.txt).
 *						Removed Verbose, and Show Change Plan option. Fixed bug where ref des offset are relative to module
 *						axis not relative to module center
 *
 *	To do 				Figure out a GUI
 *						Add a function to strip prepend strings
 *
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<time.h>
#include 	<sys/time.h>
#include 	<math.h>


#ifdef		_WIN64
	#define		Windows
#endif

#ifdef		_WIN32
	#define		Windows
#endif


#ifdef		Windows					//Windows
#include	<conio.h>
#else	//Linux

#include <termios.h>
#include <unistd.h>
int	getch(void);
#endif


#define	MAXREFDES 64			//Max characters to copy
#define	MAXSTRING	100		//Maximum size of an error string

#define	SORTX	0			//Sort on X
#define	SORTY	1			//Sort on Y

#define	ASCENDING 0			//Sort low to high
#define	DESCENDING 1		//Sort high to low

/*
 * New sort definitions
 */
#define	SORTXFIRST	0b000			//Sort on X
#define	SORTYFIRST	0b100			//Sort on Y

#define	ASCENDINGFIRST 		0b00				//Sort low to high
#define	DESCENDINGFIRST  	0b10				//Sort high to low
#define	ASCENDINGSECOND 	0b00				//Sort low to high
#define	DESCENDINGSECOND	0b01				//Sort high to low

#define	MINGRID 0.001

#define	TRUE 				1
#define FALSE 				0

#ifdef		Windows					//Windows
#define	EASCII				0xe0
#define	CR					0x0d
#define	BS					0x08
#else	//Linux
#define	EASCII				0x1b
#define	CR					0x0a
#define	BS					0x7f

#endif

#define	ABORT				0x03
#define	KEYESCAPE			0X400
#define	DEL					( 0x053 | KEYESCAPE )
#define	LEFTARROW			( 0x04b | KEYESCAPE )
#define	RIGHTARROW			( 0x04d | KEYESCAPE )

#define	SKIPREFDES			-1	//Skip this refdes

#define FILENOTFOUND 		1	//File not found error code
#define MALLOCERROR 		2	//Can't allocate memory error code
#define READERROR  			3	//Can't read the file error code
#define	NOMODULESERROR 		4	//No (modules field in file
#define	ERRNOCOORD			5	//Module had no coordinate
#define	MODULEMISSMATCH		6	//Different number modules from declaration
#define	OUTFILEOPENERROR 	7	//Can't open the output file
#define	OUTFILEWRITEERROR	8	//Can't write to the outputfile
#define	NOCHANGERROR		9	//Can't find the refdes in the change array
#define	PCBMISSMATCHERROR	10	//Mismatch in modules found during update
#define	SCHWRITECREATEERROR	11	//Can't create schematic output file
#define	SCHMISSINGLERROR	12	//Missing L in schematic file
#define	SCHWRITEERROR		13	//Can't write to schematic output file
#define	ARGERROR 			14	//Not enough arguments
#define	PCBBACKUPERROR		15	//Can't back up the PCB file
#define	SCHBACKUPERROR		16	//Can't back up the SCH file
#define NOINPUTFILEERROR	17	//No input file specified
#define	ERRNABORT			18	//User aborted
#define	ERRINVALIDEXIT		19	//Somehow exit main without clean exit
#define	PARAMWRITECREATEERROR 20	//Can't write the parameter file

#define	SCHNOARREFERROR	20		//Missing the "Ref= in AR code
#define	SCHNOARREFERROR	20		//Missing the "Ref= in AR code

#define	TEMPFILENAME		"RenumKiCadPCB"

struct	KiCadModule {
	char	layer;						//What layer the module is on (usually 1 or 2)
	float	coord[2];					//X ad Y coordinate
	char 	RefDesType[MAXREFDES+1];		//What the RefDes Preamble is (i.e. U, R VR, etc.)
	char 	RefDesString[MAXREFDES+1];		//What the RefDes Preamble is (i.e. U, R VR, etc.)
	int 	RefDes;						//And what the RefDes is (i.e. 23)
	int		index;						//Used for sorting
};


struct	RefDes
{
	char 	RefDesType[MAXREFDES+1];			//What the RefDes Preamble is (i.e. U, R VR, etc.)
	char	OldRefDesString[MAXREFDES+1];		//What the old refdes preamble + number was
	char	*prepend;						//The prepend string
	int		OldRefDes;
	int 	NewRefDes;						//And what the RefDes is (i.e. 23)
	int		Found;							//Found the ref des in the schematic
};


//
// Function prototypes
//
char	*LoadFile( char *path, char *fname, char *extension );
void	ExtractPath( char *filename, char *path );
int		MakeBackupFile( char *path, char *filename, char *extension );
void	MakeFileName( char *filename, char *path, char *fname, char *extension, int namesize );
FILE	*OpenDebugFile( char *path, char *filename, char *ftype, char *extension, char *prompt );

int		LoadModuleArray( struct KiCadModule *ModuleArray, int modules, char *buffer ); 		//Load up the module array

void	FreeMemoryAndExit( int code );
void	RenumError( char *message, int code );

char	*FindAndSkip( char *dest, char *buffer, char *instr, char *limit );
char	*CopyText( char *dest, char *source );		//Copy the text part
void	SortOnXY( struct KiCadModule*sortarray, int XY, int sortdirection, float grid  );
void	SortKiCadModules( struct KiCadModule *modulearray, int modules, int sortcode );
int		MakeRefDesTypesArray( struct RefDes *outarray, struct KiCadModule *ModuleArray, int modules );
void	SetRefDesTypeStart( struct RefDes *typesarray, int startrefdes );					//Set to the starting top refdes
void	MakeRefDesChangeArray( struct KiCadModule *modulearray, struct RefDes *refdestypesarray,
					struct RefDes *refdeschangecrray, int modules, char *prepend, FILE *filehandle, char *text );

int		GetModuleCount( char *buffer );


void	UpdateSchematicFileRefDes( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray );
void	fprintfbuffer( char *buffer );
void	UpdatePCBFileRefDes( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray, char *buffer );

void	CopyKiCadModuleArrayElement( struct KiCadModule *dest, struct KiCadModule *source, float grid );
void	CopyKiCadModuleArray( struct KiCadModule *dest, struct KiCadModule *source, int modules, float grid );
int		CountTopModules( struct KiCadModule *ModuleArray,  int Modules );
int		SplitTopBottom( int Modules, int TopModules, struct KiCadModule *ModuleArray, struct KiCadModule *TopModuleArray, struct KiCadModule *BottomModuleArray  );
void	ParseCommandLine( int argc, char *argv[] );
void	ShowAndGetParameters( void );
void	RenumError( char *message, int code );
void	PrintHelpFile( char *argv );

void	PullFieldString( char *dest, char *buffer, char *instr );
int		CrawlSheets( char *path, char *SheetName, char *SheetNamePointer[], int *NumberOfSheets, int *SheetNameSize );
void	SafeStringCopy( char *dest, char *source, int bufsize );
void	SafeStringConcatinate( char *dest, char *source, int maxlen );
void	AddSheetName( char *SheetName, char *SheetNameList[] );
char	*ScanForSheets( char *scanner, char *sheetnamedest, int *sheetsize );
void	UpDateSchematicHierarchy( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray );
int		mygetch( void );

void	LoadParameterFile( void );
void	WriteParameterFile( void );
void	ResetParameters( void );

/*
 * Global variables (mostly filled by command line arguments )
 */

#define	MAXPATH		2048
#define	MAXPREPEND	MAXREFDES

char	G_FileName[ MAXPATH ];
char	G_TopPrependString[ MAXPREPEND ];
char	G_BottomPrependString[ MAXPREPEND ];
char 	G_ERRSTR[MAXSTRING];							//Common error string

char	*G_InputFileName = "";							//The input file name set by command line
char	*G_TopPrepend = "";								//T_";
char	*G_BottomPrepend = "";							//Optional strings to prepend to new refdeses

int		G_NoQuestion = 0;								//Don't ask if OK

int		G_TopSortCode = SORTYFIRST + ASCENDINGFIRST + ASCENDINGSECOND;
int		G_BottomSortCode = SORTYFIRST + ASCENDINGFIRST + DESCENDINGSECOND;

int		G_TopStartRefDes = 1;							//Start at 1 for the top
int		G_BottomStartRefDes = 0;	//1;				//Start at 1 for the bottom (this is optional)
int		G_SortOnModules = 1;							//if 0 sort on ref des location
float	G_Grid = 0.75;									//Anything near this (mm) is in the same line


/*
 * The Say Hello String
 */
char	G_HELLO[] =
"\nRenumKiCadPCB V0.203\nReleased under GPL 3.0 or later. See source for details.\
\n**** No warranty : use at your own risk ****\
\nWritten by Brian Piccioni. \nEmail brian@documenteddesigns.com with bug reports or feature requests";




struct	ParameterType {
		char	*Parametername;					//i.e. "File Name"
		char	Parametertype;					//Text or number for read
		void	*Pointertoparameter;			//Where it goes
	};


struct ParameterType G_ParameterFile[] =
	{
		{"File Name=", 'T', &G_FileName },
		{"Top Prepend=", 'T', &G_TopPrependString },
		{"Bottom Prepend=", 'T', &G_BottomPrependString },

		{"No Question=", 'N', &G_NoQuestion },
		{"Top Sort Direction=", 'N', &G_TopSortCode },
		{"Bottom Sort Direction=", 'N', &G_BottomSortCode },
		{"Top Start Reference Designation=", 'N', &G_TopStartRefDes },
		{"Bottom Start Reference Designation=", 'N', &G_BottomStartRefDes },
		{"Sort on Modules/Reference Designators=", 'N', &G_SortOnModules },
		{"Grid =", 'F', &G_Grid },
		{0,0,0}
	};

#define	PARAMETERFILENAME	"RenumParameters.txt"


struct timeval	G_StartTime;							//Program run start time
struct timeval	G_EndTime;								//Program run end time

/*
 * These are here to ensure all handles are closed/freed
 */
FILE 	*G_WriteFile;							//Write file handle
char	*G_Buffer;								//Used for malloc


/*
 * Let us begin
*/
int main( int argc, char *argv[] )
{
int		i, Modules, TopModules, BottomModules;		//The number of modules in the PCB, Top and Bottom

		setvbuf (stdout, NULL, _IONBF, 0);			//direct output to eclipse console
		if( argc > 1)
			ParseCommandLine( argc, argv );			//If there are command line arguments handle them

		if( G_Grid < MINGRID ) G_Grid = MINGRID;	//Make sure sort grid is > 0

		ShowAndGetParameters();						//Show what you got and allow them to change

		G_WriteFile = NULL;							//Write file handle
		WriteParameterFile( );						//Save these for later


/*
* Now process start the work
*/
char	path[ strlen( G_InputFileName ) + 1 ];
char	*buffer;

		ExtractPath( G_InputFileName, path );							//Get the part part of the source/destination

		printf("\n\nLoading PCB file %s.kicad_pcb", G_InputFileName );
		buffer = LoadFile( path, G_InputFileName, ".kicad_pcb" );		//Load this file and extension

		Modules = GetModuleCount( buffer );								//Extract the (modules field
struct KiCadModule ModuleArray[ Modules ];								//Allocate memory for the ModuleArray

		LoadModuleArray( ModuleArray, Modules, buffer ); 				//Load up the module array

FILE	*debughandle;
float	rounder, old_x, old_y;

		debughandle = OpenDebugFile( path, G_InputFileName, "_coord", ".txt", "Coordinate" );

		if( G_SortOnModules != 0 )						//Only if sorting by reference designator coordinate
			fprintf(debughandle, "**************************** Module Coordinates ***************************************");
		else
			fprintf(debughandle, "*********************** Reference Designator Coordinates *****************************");

		fprintf(debughandle, "\n#\tF/B\tRef\tX Coord\tY Coord\t\tGrid X\tGrid Y");

//
// Round the Module Array to the grid (only do this once)
//
		for( i = 0; i < Modules; i++ )
		{
			old_x = ModuleArray[i].coord[0];
			rounder = fmod( old_x, G_Grid );
			ModuleArray[i].coord[0] -= rounder;							//X coordinate down to grid
			if( rounder > G_Grid /2 ) ModuleArray[i].coord[0] += G_Grid  ;		//Round X coordinate up to grid

			old_y = ModuleArray[i].coord[1];
			rounder = fmod( old_y, G_Grid );
			ModuleArray[i].coord[1] -= rounder;							//Y coordinate down to grid
			if( rounder >= G_Grid /2 ) ModuleArray[i].coord[1] += G_Grid;		//Round Y coordinate up to grid

			fprintf(debughandle, "\n%d\t%c\t%s\t%.3f\t%.3f\t\t%.3f\t%.3f", i,
				ModuleArray[i].layer, ModuleArray[i].RefDesString,
					old_x, old_y, ModuleArray[i].coord[0], ModuleArray[i].coord[1] );
		}

		fclose( debughandle );

		TopModules = CountTopModules( ModuleArray, Modules );			//How many modules on the top?
		BottomModules = Modules - TopModules;

struct KiCadModule TopModuleArray[ TopModules ];						//Allocate memory for the top side ModuleArray
struct KiCadModule BottomModuleArray[ BottomModules ];					//Allocate memory for the bottom side ModuleArray

		SplitTopBottom( Modules, TopModules, ModuleArray, TopModuleArray, BottomModuleArray  );		//Create separate top and bottom array

struct RefDes RefDesChangeArray[ Modules ]; 							//Allocate for changes array

int		numrefdestypes = MakeRefDesTypesArray( RefDesChangeArray, ModuleArray, Modules );	//Go through and count the number of types of refdeses
struct 	RefDes RefDesTypeArray[ numrefdestypes ];		 						//Allocate for refdes type array

		MakeRefDesTypesArray( RefDesTypeArray, ModuleArray, Modules );			//Now fill up the array

		debughandle = OpenDebugFile( path, G_InputFileName, "_change", ".txt", "Change List" );

		fprintf( debughandle, "Reference Designator Change Plan for %s", G_InputFileName );
 		fprintf( debughandle, "\nThere are %d types of reference designations", numrefdestypes );

		for( i = 0; i < numrefdestypes; i++ )
		{
			if( i%8 == 0 ) fprintf( debughandle, "\n");
			fprintf( debughandle, "%s\t", RefDesTypeArray[i].RefDesType );
		}


		if( TopModules != 0 )			//Only do if there are modules on the top
		{
			SetRefDesTypeStart( RefDesTypeArray, G_TopStartRefDes );					//Set to the starting top refdes
			SortKiCadModules(  TopModuleArray, TopModules, G_TopSortCode);
			MakeRefDesChangeArray( TopModuleArray, RefDesTypeArray, RefDesChangeArray,
						TopModules, G_TopPrepend, debughandle, "\nFront Side Changes" );
		}

		if( BottomModules != 0 )		//Only do if there are modules on the bottom
		{
			SetRefDesTypeStart( RefDesTypeArray, G_BottomStartRefDes );					//Set to the starting bottom refdes
			SortKiCadModules(  BottomModuleArray, BottomModules, G_BottomSortCode ); //Now sort the bottom modules

			MakeRefDesChangeArray( BottomModuleArray, RefDesTypeArray, &RefDesChangeArray[TopModules],
						BottomModules, G_BottomPrepend, debughandle, "\n\nBack Side Changes" );
		}

		fclose( debughandle );
/*
 * Note that the PCB file is already in memory
 */
		UpdatePCBFileRefDes( path, G_InputFileName, Modules, RefDesChangeArray, buffer );		//Update the PCBs and the nets

		free( buffer );							//Free up that memory for the next step
		G_Buffer = NULL;						//And remember you did

		printf("\n\nTraversing schematic hierarchy starting with %s", G_InputFileName );
		UpDateSchematicHierarchy( path, G_InputFileName, Modules, RefDesChangeArray );				//Crawl through the design and update each schematic
		printf("\n\n");

		for( i = 0; i < Modules; i++ )
			if(( RefDesChangeArray[i].Found == 0 ) && ( RefDesChangeArray[i].OldRefDes != SKIPREFDES ))
				printf("\nWarning PCB component %s not found in schematic!", RefDesChangeArray[i].OldRefDesString  );

		printf("\n\nDone\n");
		FreeMemoryAndExit( EXIT_SUCCESS );
		return( ERRINVALIDEXIT );
}//Main()

/*
 * This is where we usually leave: ensures the memory is freed up
 */
void	FreeMemoryAndExit( int code )
{
	if( G_Buffer != NULL ) free( G_Buffer );									//Free all memory
	gettimeofday( &G_EndTime, NULL );				//Get the start time
	printf("\nRun time of %10.3f Seconds", (float) ((G_EndTime.tv_sec - G_StartTime.tv_sec ) + ((float)(G_EndTime.tv_usec - G_StartTime.tv_usec )/1000000.))) ;
	printf("\n\nHit any key to exit ");
	mygetch();
	printf("\n\n");
	exit( code );
}

/*
 * Common error exist routine
 */
void	RenumError( char *message, int code )
{
	printf("\n%s", message );					//Say the word
	if( G_WriteFile != NULL )
		fclose( G_WriteFile );						//Write file handle
	FreeMemoryAndExit( code );					//ByeBye
}
/*
 * A safe version of strcpy which no overwrite
 */

void	SafeStringCopy( char *dest, char *source, int bufsize )
{
int		srclen = strlen( source );
		--bufsize;							//Make room for the '\0'
		if( bufsize > 0 )
		{
			srclen = ( srclen > bufsize ? bufsize : srclen );
			while( srclen-- > 0 ) *dest++ = *source++;
		}
		*dest = '\0';
}

/*
 * A safe version of strcat which no overwrite
 */

void	SafeStringConcatinate( char *dest, char *source, int maxlen )
{
int		srclen = strlen( source );
int		destlen = strlen( dest );

		dest += destlen;					//Skip to the end of the destination

		srclen = (( srclen + destlen ) > maxlen ) ? ( maxlen - ( srclen + destlen ) - 1 ) : srclen;


		if( srclen >= 0 )								//If something to do
			while( srclen-- >= 0 ) *dest++ = *source++;	//And concatenate
}

//
// Make a backupfile by renaming the oldfile
//

int		MakeBackupFile( char *path, char *filename, char *extension )
{

int		extsize = strlen( extension ) + sizeof( "RenumBack") + 2;
int		filesize = strlen( path) + strlen( filename ) + extsize ;
char	newfilename[ filesize ];
char	oldfilename[ filesize ];
char	tmpextension[ extsize ];			//Temporary extension

		SafeStringCopy( tmpextension, "RenumBack", extsize ); 					//Backup text
		SafeStringConcatinate( tmpextension, extension, extsize ); 				//And the file extension
		MakeFileName( newfilename, path, filename, tmpextension, filesize );
		remove( newfilename );

		MakeFileName( oldfilename, path, filename, extension, filesize );
		return( rename( oldfilename, newfilename ));
}

/*
 * Extract the path from the file name
 * i.e. path ends up with C:/ etc if present
 *
 */
void	ExtractPath( char *filename, char *path )
{
int	i, j = strlen( filename );
int	lastslash = 0;

		*path = '\0';									//Assume no path
		for( i = 0; i < j; i++ )	//Find the last / or \ in the path
			if(( filename[i] == '\\') || ( filename[i] == '/')) lastslash = i;

		if( lastslash != 0 )			//If I have the path
		{
			for( i = 0; i <= lastslash; i++ ) path[i] = filename[i];	//Copy the path
			path[i] = '\0';				//Zero terminate
			SafeStringCopy( filename, filename + lastslash + 1, j ); //And copy the filename without the path
		}

		for( i = 0; i < j; i++ )	//Locate any "." indicating a file extension
			if( filename[i] == '.') filename[i] = '\0';			//Trim off the file extension

} //ExtractPath()

/*
 * Make the full file name including the path and extension
 */
void	MakeFileName( char *filename, char *path, char *fname, char *extension, int namesize )
{
	SafeStringCopy( filename, path, namesize );
	SafeStringConcatinate( filename, fname, namesize );
	SafeStringConcatinate( filename, extension, namesize );
}

//
// Open a file for debugging (turns myproject filename into myproject_ftype.extension and opens
// Returns the file handle
//
FILE	*OpenDebugFile( char *path, char *filename, char *ftype, char *extension, char *prompt )
{
int		namesize = strlen( path ) + strlen( G_InputFileName ) + 100;
char	outfilename[ namesize ];
char	infilename[ namesize ];			//make the filename and extension

		SafeStringCopy( infilename, G_InputFileName, namesize );
		SafeStringConcatinate( infilename, ftype, namesize );
		MakeFileName( outfilename, path, infilename, extension, namesize );
		printf("\nWriting %s file: %s", prompt, outfilename );

FILE	*handle = fopen ( outfilename,"w");

		if ( handle == NULL)						//Not found
			RenumError("Can't open debug file!", FILENOTFOUND );		//I am out of here

		return( handle );								//Open the file write only binary
}


//
// Open the file and load it into memory
// Return the pointer to the buffer. Exit if file not found
//
char	*LoadFile( char *path, char *fname, char *extension )
{
long 	filesize, readbytes;

int		namesize = strlen( path ) + strlen( fname ) + strlen( extension ) + 10;
char	filename[ namesize ];			//make the filename and extension
char	*buffer;

	MakeFileName( filename, path, fname, extension, namesize );

FILE	*readhandle = fopen ( filename,"rb");				//Open the file read only binary
	if (readhandle == NULL)						//Not found
		RenumError("File not found!", FILENOTFOUND );		//I am out of here

	fseek(readhandle, 0L, SEEK_END);				//Go to the end of the file
	filesize = sizeof(char) * (ftell( readhandle ));				//Where am I?
	fseek(readhandle, 0L, SEEK_SET);				//Go to the start of the file

	buffer = (char*) malloc ((sizeof(char) * filesize ) + 1); 	 	// allocate memory to contain the whole file plus a '\0'
	G_Buffer = buffer;										//Remember so the memory can be freed()

	if ( buffer == NULL)
		RenumError("Can't allocate memory for file!", MALLOCERROR );		//I am out of here

	readbytes = fread (buffer, sizeof( char ), filesize, readhandle);

int	fer = ferror( readhandle );
int	feo = feof( readhandle );

	fclose( readhandle );


	if( readbytes != filesize )
	{
		printf("\nRead %ld, filesize %ld, FERROR %d, FEOF %d", readbytes, filesize, fer, feo );
		RenumError("Read error!", READERROR );		//I am out of here
	}
	buffer[filesize] = '\0';					//ends with a null
	return( buffer );
}	//LoadFile()


/*
 * Get a float from a zero  terminated string
 * Return characters scanned until a space after the float
 */
char	*GetFloatFromString( float *dest, char *floatstring )
{
int 	i = 0;

		*dest = 0;

		while(( floatstring[i] <= ' ' ) && ( floatstring[i] != '\0')) i++;
		if( floatstring[ i ] != '\0')
		{
			*dest = atof( floatstring );
			while(( floatstring[i] != ' ') && ( floatstring[i] != ',' ) && ( floatstring[i] != '\0')) i++;
		}
		return( floatstring + i );
}

/*
 * Dertermine the number of modules in the PCB file
 */
int	GetModuleCount( char *buffer )
{
char	*found = strstr( buffer, "(modules ");				//Where the next module is
int		modules = 0;

	if( found != NULL ) modules = atoi( found + sizeof( "(modules" ));	//Get how many modules there arre
	if( modules == 0 )
		RenumError("No modules count in file!", NOMODULESERROR );		//I am out of here

	printf("\nThere are %d modules on the PCB\n", modules );
	return( modules );
} //GetModuleCount( char *buffer )


/*
 * Scan the PCB file in memory and fill in the Module Array
 */
int		LoadModuleArray( struct KiCadModule *ModuleArray, int modules, char *buffer ) 		//Load up the module array
{
char	*found, *nextmodule, *txtfound, *atfound;				//Where the next module is
char	workbuffer[2*MAXREFDES+1];		//Where the strings go for now
char	*anglepnt;

int 	i = 0, j;						//And what the RefDes is (i.e. 23)
float	ModXCoordinate, ModYCoordinate;				//What the X and Y Coordinate of the module is
float	modangle, sinmodangle, cosmodangle;

/*
 * I want to sort on the absolute position of the text reference. This avoids issues with, say resistors, where the module may be rotated
 * If you think about it you are going to look for the text reference.
 *
 * The tricky bit is that the text reference coordinates are relative to the module, so I get the module X, Y, then add the
 * text reference to XY to that
 */
	found = buffer;								//Got to start somewhere
	do
	{
		found = strstr( found, "(module ");		//Find the "(module " token
		if( found != NULL )
		{
			found += sizeof("(module")/sizeof(char);			//Skip this
			nextmodule = strstr( found, "(module ");			//Use this to find where the next module starts

			found = FindAndSkip( workbuffer, found, "(layer ", nextmodule );	//get the layer of the refdes
			ModuleArray[i].layer = workbuffer[0];

			found = FindAndSkip( workbuffer, found, "(at ", nextmodule );

			anglepnt = GetFloatFromString( &ModuleArray[i].coord[1], GetFloatFromString( &ModuleArray[i].coord[0], workbuffer ) );
			GetFloatFromString( &modangle, anglepnt );

			found = FindAndSkip( workbuffer, found, "(fp_text reference ", nextmodule );	//Find the reference designator

			txtfound = CopyText( ModuleArray[i].RefDesType, workbuffer );					//Copy the text part of the refdes
			ModuleArray[i].RefDes = atoi( txtfound );

			j = 0;
			while(( workbuffer[j] != ' ' ) && ( j < MAXREFDES ))
			{
				ModuleArray[i].RefDesString[j] = workbuffer[j];
				j++;
			}
			ModuleArray[i].RefDesString[j] = '\0';

			if(( ModuleArray[i].RefDesString[j-1] < '0' ) ||  ( ModuleArray[i].RefDesString[j-1] > '9' )) //If it doesn't end with a number (i.e. *
				ModuleArray[i].RefDes = SKIPREFDES;					//Skip it

			atfound = strstr( workbuffer, "(at ");
			if( atfound  == NULL )
			{	snprintf( G_ERRSTR, MAXSTRING, "Error: Missing coordinates for module %s%d", ModuleArray[i].RefDesType, ModuleArray[i].RefDes  );
				RenumError( G_ERRSTR, ERRNOCOORD );
			}

			GetFloatFromString( &ModYCoordinate, GetFloatFromString( &ModXCoordinate, atfound + sizeof("(at")));
//
//			anglepnt = GetFloatFromString( &ModYCoordinate, GetFloatFromString( &ModXCoordinate, atfound + sizeof("(at")));
//			GetFloatFromString( &refangle, anglepnt );
//
			if( G_SortOnModules == 0 )						//Only if sorting by reference designator coordinate
			{
				modangle = modangle * ( 3.14159265 / 180.0 );	//Convert angle to radians
				sinmodangle = sin( modangle  );				//Find the sine of the module angle
				cosmodangle = cos( modangle  );				//Find the sine of the module angle
				ModuleArray[i].coord[0] += (( ModXCoordinate * cosmodangle )  + ( ModYCoordinate * sinmodangle ));	//Save the coordinate (done for clarity
				ModuleArray[i].coord[1] += (( -ModXCoordinate * sinmodangle )  + ( ModYCoordinate * cosmodangle ));
			}
			ModuleArray[i].index = i;
			i++;
		}
	}while( found != NULL );

	if( i != modules )
		RenumError( "\nError: PCB modules does not match declaration", MODULEMISSMATCH );

	return( modules );
}


/*
 * Find the instr in the buffer only if less than limit (ignore limit if NULL)
 * copy the string after instr into the dest buffer up to MAXREFDES characters or until a ')' and 0 temrinate it
 * return pointer to buffer where instr was found after the copied string
 *
 * return NULL if not found
 */
char	*FindAndSkip( char *dest, char *buffer, char *instr, char *limit )
{
char	*found;
int		i;
		*dest = '\0';									//Assume not found

		found = strstr( buffer, instr );				//Use this to find where the next module starts

		if(( found > limit ) && ( limit != NULL ))		//In case not found in this module
			return( NULL );								//Stop here

		found += strlen( instr );						//Skip the input string

		for( i = 0; (( i < MAXREFDES) && (*found != ')')); i++ )	//End with ')'
			*dest++ = *found++;						//Copy the string

		*dest++ = '\0';									//Make sure zero terminate
		return( found );
} //FindAndSkip


/*
 * Copy only the text part of a string return pointer to non-alpha
 */
char	*CopyText( char *dest, char *source )		//Copy the text part
{
	while( isalpha( (int) *source ) != 0 )			//Until a non-character
		*dest++ = *source++;					//Copy
	*dest = '\0';								//Zero terminate the destination string
	return( source );
}//CopyTest()

//
// Sort the sort array on X or Y.
// Sort array structure is
// int	index;
// float coord[2]	//Element 0 is X by convention, element y is 1 by convention
//
// If XY = 0 I sort on X, if XY = 1 I sort on Y
//
// The sort array is terminated with index = -1;
//
// Sort ascending if 1 else descending
//
void	SortOnXY( struct KiCadModule*sortarray, int XY, int sortdirection, float grid  )
{
float	minmax, bigsmallnum;					//Assume board less than 10M wide
int		i, j = 0, tmpindex, arraysize = 0;

	while( sortarray[arraysize].index != -1 ) arraysize ++;		//Size the array
	if( arraysize == 0 ) return;				//Nothing to sort

struct KiCadModule	tmparray[ arraysize + 1 ];		//Make a temporary array

	CopyKiCadModuleArray( tmparray, sortarray, arraysize, grid );	//Copy the input array and adjust coordinates to grid because sort destroys the data

	if( sortdirection == ASCENDING ) 					//If a low to high sort`
		bigsmallnum = 10000.;					//Big coordinate
	else
		bigsmallnum = -10000.;					//Small coordinate

	do {
		minmax = bigsmallnum;						//Assume the worse.
		tmpindex = -1;								//If nothing found -1

		for( i = 0; i < arraysize; i++ ) {
			if( sortdirection == ASCENDING ) {
				if( tmparray[i].coord[XY]  < minmax ) {
					tmpindex = i;
					minmax = tmparray[i].coord[XY];		//Found the lowest
				}
			}
			//Done for clarity rather than a complex conditional
			else if( tmparray[i].coord[XY]  > minmax ) {
					tmpindex = i;
					minmax = tmparray[i].coord[XY];		//Found the highest or lowest
			}
		}

		if( tmpindex != -1 ) {											//If I found something
			tmparray[tmpindex].index = tmpindex;						//This says the location of the largest/smallest
			CopyKiCadModuleArrayElement( &sortarray[j++], &tmparray[tmpindex], grid );	//Copy over to the sort array
			tmparray[tmpindex].coord[XY] = bigsmallnum;					//Only once for this element
		}
	} while( j < arraysize );						//Until the end
}	// SortOnXY

/*
 *
 * Copy a KicadModuleArray
 *
 */

void	CopyKiCadModuleArray( struct KiCadModule *dest, struct KiCadModule *source, int modules, float grid )
{
int	i;
	for( i = 0; i < modules; i++ )
	{
		dest->layer = source->layer ;						//What layer the module is on (usually F or B )
		dest->coord[0] = source->coord[0];					//Copy X coordinate
		dest->coord[1] = source->coord[1];					//Copy Y  coordinate
		SafeStringCopy( dest->RefDesString, source->RefDesString, MAXREFDES );		//What the RefDes Preamble is (i.e. U, R VR, etc.)
		SafeStringCopy( dest->RefDesType, source->RefDesType, MAXREFDES );		//What the RefDes Preamble is (i.e. U, R VR, etc.)
		dest->RefDes = source->RefDes;						//What the RefDes is (i.e. 23)
		dest->index = source->index;						//And the index (used for sort)
		source++;
		dest++;
	}
}

/*
 *
 * Copy a KicadModuleArray Element
 *
 */

void	CopyKiCadModuleArrayElement( struct KiCadModule *dest, struct KiCadModule *source, float grid )
{
		dest->layer = source->layer ;						//What layer the module is on (usually F or B )
		dest->coord[0] = source->coord[0];					//Copy X coordinate
		dest->coord[1] = source->coord[1];					//Copy Y  coordinate
		SafeStringCopy( dest->RefDesString, source->RefDesString, MAXREFDES );		//What the RefDes Preamble is (i.e. U, R VR, etc.)
		SafeStringCopy( dest->RefDesType, source->RefDesType, MAXREFDES );		//What the RefDes Preamble is (i.e. U, R VR, etc.)
		dest->RefDes = source->RefDes;						//What the RefDes is (i.e. 23)
		dest->index = source->index;						//And the index (used for sort)
}

/*
 * This allow arbitrary starting refdes
 * It is mostly useful if you have a prepend (i.e B_R1)
 *
 */
void	SetRefDesTypeStart( struct RefDes *typesarray, int startrefdes )					//Set to the starting top refdes
{
int		i = 0;

	if( startrefdes == 0 ) return;							//Nothing to do

	while(typesarray[i].RefDesType[0] != '\0')			  	//Until the end
		typesarray[i++].NewRefDes = startrefdes;				//Set the starting point
}

/*
 * 	Figure out what types of reference designators there are
 *	Make an array with those and starting with refdes 1
 *	Returns the number of reference designator types
 */

int		MakeRefDesTypesArray( struct RefDes *outarray, struct KiCadModule *ModuleArray, int modules )
{
int	i, j, found, numrefdes = 0;

	outarray[0].RefDesType[0] = '\0';			//Zap the refdes string

	for( i = 0; i < modules; i++) {
		found = 0;								//Assume not found
		for( j = 0; j < numrefdes; j++ )
			if( strcmp( ModuleArray[i].RefDesType, outarray[j].RefDesType ) == 0 )
				found = 1;

		if( found == 0 )				//Not found in the table so insert it
		{
			SafeStringCopy( outarray[j].RefDesType, ModuleArray[i].RefDesType, MAXREFDES );				//Copy it in
			outarray[j].NewRefDes = 1;														//Start with a 1
			numrefdes++;																//One more type of refdes
			outarray[numrefdes].RefDesType[0] = '\0';									//Zap the string
		}
	}
	return( numrefdes );

} //MakeRefDesTypesArray()

/*
 * Now create an array which has the refdes type, the old refdes and the new refdes
 * There is an optional prepend string which allows refdeses to be renamed (eg) T_ or  B_ so you get T_R21 and B_U2
 */
void	MakeRefDesChangeArray( struct KiCadModule *modulearray, struct RefDes *refdestypesarray,
					struct RefDes *refdeschangecrray, int modules, char *prepend, FILE *debughandle, char *text )

{
int	i = 0, j, numrefdes = 0;

	fprintf( debughandle, "\n\n%s ", text );

	while( refdestypesarray[i++].RefDesType[0] != '\0' )
		numrefdes++;

	for( i = 0; i < modules; i++)
	{
		for( j = 0; j < numrefdes; j++ )
		{
			if( strcmp( modulearray[i].RefDesType, refdestypesarray[j].RefDesType ) == 0 )	//I found the type (U, R, VR, etc )
			{
				SafeStringCopy( refdeschangecrray[i].RefDesType, refdestypesarray[j].RefDesType, MAXREFDES ); //Copy the type (U, R, VR, etc)
				SafeStringCopy( refdeschangecrray[i].OldRefDesString, modulearray[i].RefDesString, MAXREFDES ); //Copy the type (U, R, VR, etc)
				refdeschangecrray[i].prepend = prepend;							//Know what to prepend to update
				refdeschangecrray[i].OldRefDes = modulearray[i].RefDes;			//Get the old refdes
				refdeschangecrray[i].NewRefDes = refdestypesarray[j].NewRefDes++;	//Show the new one and bump it
				refdeschangecrray[i].Found = 0;									//Assume not found in the schematic

				fprintf( debughandle, "\n#%d\t%s\t->\t", i, modulearray[i].RefDesString );
				if(refdeschangecrray[i].OldRefDes == SKIPREFDES )			//If I ignore this one
					fprintf( debughandle, "%s", refdeschangecrray[i].OldRefDesString );
				else
					fprintf( debughandle, "%s%d", modulearray[i].RefDesType, refdeschangecrray[i].NewRefDes );

				if(refdeschangecrray[i].OldRefDes == SKIPREFDES )
						fprintf(debughandle, "\t*** will be ignored ***");
			}
		}
	}
} //MakeRefDesChangeArray

/*
 * Count the number of top modules (also tells you bottom modules
 */

int	CountTopModules( struct KiCadModule *ModuleArray,  int Modules )
{
int	i, TopModules = 0;							//How many modules are on top and bottom side

	for( i = 0; i < Modules; i++ )				//Scan through the ModuleArray and
		if( ModuleArray[i].layer == 'F')		//Count the number on top
			TopModules++;

	return( TopModules );
}

/*
 * split the module array into top and bottom sides
 * return with the number of modules on top (because bottommodules = modules - topmodules )
 */

int	SplitTopBottom( int Modules, int TopModules, struct KiCadModule *ModuleArray, struct KiCadModule *TopModuleArray, struct KiCadModule *BottomModuleArray  )
{
int	i, j = 0, k = 0;
int	BottomModules = Modules - TopModules;			//How many modules are on bottom side

/*
 * Scan through to determine memory needs
 */
	for( i = 0; i < Modules; i++ )								//Scan through the ModuleArray and
		if( ModuleArray[i].layer == 'F')						//Count the number on top
			CopyKiCadModuleArrayElement( &TopModuleArray[ j++], &ModuleArray[i], (float) 0.0 );
		else if( ModuleArray[i].layer == 'B' )
			CopyKiCadModuleArrayElement( &BottomModuleArray[ k++], &ModuleArray[i], ( float) 0.0 );

	if(( BottomModules + TopModules ) != Modules )
		printf("\nWarning: %d modules neither top nor bottom ",  Modules - (BottomModules + TopModules ));

	return( TopModules );
}

/*
 * Skip whitespace and copy text until <= ' '
 */
char	*nexttext( char *buffer, char *text )		//Get (and ignore) the (old) ref des
{
int i = 0;
	while(( buffer[i] != '\0') && (buffer[i] <= ' '))		//Look for a space or less
		if(buffer[i] == ' ') i++;							//Skip until a space

int j = 0;
	while(( j < MAXREFDES ) && ( buffer[i] != '\0') && (buffer[i] > ' ') && (buffer[i] != '"'))		//Look for a space or more
		if(buffer[i] > ' ') text[j++] = buffer[i++];							//Skip until a space
	text[j] = '\0';
	return( buffer + i );
}

/*
 * This creates a KiCadModulearray terminated with index -1 from the ModuleArray
 * It sorts it by X ascending, then copies and sorts each X (row) by Y ascending
 */

void	SortKiCadModules( struct KiCadModule *modulearray, int modules, int sortcode )
{
int		i,	j, scanindex = 0, tmpindex;
float	min, srcval;					//Assume board less than 10M wide

int		sortxy = ((( sortcode & SORTYFIRST ) == 0 ) ? SORTX : SORTY );
int 	firstdirection = ((( sortcode & DESCENDINGFIRST ) == 0 ) ? ASCENDING : DESCENDING );
int 	secondirection = ((( sortcode & DESCENDINGSECOND ) == 0 ) ? ASCENDING : DESCENDING );

struct KiCadModule sortarray0[ modules + 1 ];		//Make a temporary array for x sort
struct KiCadModule sortarray1[ modules + 1 ];		//Make a temporary array for y sort

	CopyKiCadModuleArray( sortarray0, modulearray, modules, G_Grid );
	sortarray0[modules].index = -1;							//End the array
	SortOnXY( sortarray0, sortxy, firstdirection, G_Grid );	//Sort on the first order (x or y)
//
// Now make another sort array copy, this time with the next order
//
	for( i = 0; i < modules; i++ )
	{
		j = sortarray0[i].index;							//The index gives the sequence for the destination
		CopyKiCadModuleArrayElement( &sortarray1[i], &modulearray[j], G_Grid  );
		sortarray1[i].index = j;
	}

	sortarray1[i].index = -1;										//End the array

	while( scanindex <= modules )							//Do until the end
	{
		min = sortarray1[scanindex].coord[sortxy];			//Look for this X or Y
		for( i = scanindex;  i <=  modules; i++)
		{
			srcval = sortarray1[i].coord[sortxy];

			if((( firstdirection == ASCENDING ) && ( srcval > min ))  		//+/- the grid value
				|| (( firstdirection == DESCENDING ) && ( srcval < min ))) 	//If descending the first go
			{
				tmpindex = sortarray1[i].index;				//Save the place
				sortarray1[i].index = -1;					//Put in a fake end

				SortOnXY( &sortarray1[scanindex], ( sortxy ^ 1), secondirection, G_Grid );
				sortarray1[i].index = tmpindex;				//Reinstate the index
				scanindex = i;								//Look from here next time
				break;										//Get out of the for loop
			}
			else if( sortarray1[i].index == -1 )			//the last row
			{
				SortOnXY( &sortarray1[scanindex], ( sortxy ^ 1), secondirection, G_Grid );			//Sort on Y coordinates from here
				scanindex = modules + 1;					//Break the while
			}
		}
	}
	CopyKiCadModuleArray( modulearray, sortarray1, modules, G_Grid );			//Copy the sorted array to the module array

} //SortKiCadModules


/*
 *	This function deal with updating the schematic in a hierarchy
 *	First it figures out the storage requirements recursively
 *	Then it prepares a list of unique schematic file names
 *	Then it walks through that list and updates each of them with the new reference designators
 *
 */

void	UpDateSchematicHierarchy( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray )
{
int 	i, NumberOfSheets = 1;
int		SheetNameSize =	strlen( fname ) + sizeof( ".sch") + 2;					//Start with at least this file

char		RootFile[ SheetNameSize ];

	SafeStringCopy( RootFile, fname, SheetNameSize );
	SafeStringConcatinate( RootFile, ".sch", SheetNameSize );
	CrawlSheets( path, RootFile, NULL, &NumberOfSheets, &SheetNameSize );	//Get the number and total storage of the sheets

char	*SheetList[NumberOfSheets + 2];							//This array has pointers to the sheets
char	SheetNameBuffer[ SheetNameSize ];						//And this has the actual sheet names

	memset( SheetNameBuffer, 0, SheetNameSize / sizeof( char ));	//Make sure the names are all zeros
	for( i = 0; i <= NumberOfSheets + 2; i++ ) SheetList[i] = NULL;	//Fill up the pointers

	SheetList[0] = SheetNameBuffer;								//First Entry is NULL the start
	CrawlSheets( path, RootFile, SheetList, &NumberOfSheets, &SheetNameSize );	//Get the number and total storage of the sheets

	NumberOfSheets = 0;
	while( *SheetList[NumberOfSheets] != '\0' ) NumberOfSheets++;						//Count the number of unique sheets

	printf("\n\nThere are %d unique sheets:", NumberOfSheets  );

	for( i = 0; i < NumberOfSheets; i++)
			UpdateSchematicFileRefDes( path, SheetList[i], modules, RefDesChangeArray );

} //UpDateSchematicHierarchy

/*
 * Add if this is a new sheet name otherwise ignore
 * The SheetNameList first element points to the start go the SheetNamseBuffer, the rest of the pointers are NULL
 * If the SheetNameList pointer itself is nor, do not add the sheet name
 */

void	AddSheetName( char *SheetName, char *SheetNameList[] )
{
int	i = 0, found = 0;

	if( SheetNameList != NULL )					//This is the second pass so copy the data
	{
		while( SheetNameList[ i ] != NULL )			//Until the no strings to look at
			if( strcmp( SheetName, SheetNameList[ i++ ]) == 0 ) found = 1; 	//Look for the sheet name in the array of names

		if( found == 0 )					//This is a new string
		{
			strcpy( SheetNameList[i - 1], SheetName );			//Copy the name into the SheetNameBuffer
			SheetNameList[ i ] = SheetNameList[i - 1] + strlen( SheetName ) + 1;	//Update the array pointer (next pointer already NULL)
		}

	}
}// AddSheetName()

/*
 * This is a recursive function.
 * It loads a sheet into memory and scans it for $Sheet(s) and
 * 	1) counts the number of sheets
 * 	2) sizes the F1 field string (schematic.sch for the sheet)
 * 	3) if the SheetNamePointer != NULL it copies the schematic.sch to the memory pointed to by
 * 		SheetNamePointer, increments the pointer ( next pointer in array) and saves the address
 * 		after the '\0' in the next pointer location and puts a '\0' there
 *
 */
int	CrawlSheets( char *path, char *SheetName, char *SheetNameList[], int *NumberOfSheets, int *SheetNameSize )
{

int		i, sheets = 0, sheetnamesize = 0;			//Sheets and Bytes in all the sheets
char	*buffer, *scanner; 												//Load this file and extension

	AddSheetName( SheetName, SheetNameList );		//Add this sheetname to the array if a new name

	buffer = LoadFile( path, SheetName, "" );			//Load the file
	G_Buffer = buffer;									//In case of error
	scanner = buffer;

	do	//* First, scan through to see how much space you need.
	{
		scanner = ScanForSheets( scanner, NULL, &sheetnamesize );
		if( scanner != NULL ) ++sheets;

	}while( scanner != NULL );							//Until the EOF

	*NumberOfSheets += sheets;							//Accumulate the number of sheets
	*SheetNameSize += sheetnamesize;				//And the space needed for them

char	*sheetarray[ sheets ];								//Where the names are kept
char	sheetnames[ sheetnamesize + ( sheets * 2 )];		//Room for the sheet names

	scanner = buffer;							//Start over
	sheetnamesize = 0;							//Keep track of the length
	for( i = 0; i < sheets; i++ )				//This makes an array of names of files
	{
		sheetarray[i] = &sheetnames[sheetnamesize];
		scanner = ScanForSheets( scanner, sheetarray[i], &sheetnamesize );
	}
	free( buffer );				//I am now done with that file for name scanning
	G_Buffer = NULL;

	for( i = 0; i < sheets; i++ )	//Now crawl through each sheet
		CrawlSheets( path, sheetarray [i], SheetNameList, NumberOfSheets, SheetNameSize );

	return( 0 );
} //CrawlSheets()


/*
 * Scan for each sheet by name. If $Sheet is found, pull in the name
 * If sheetnamedest != Null copy it else just tally the size of the name
 * Return pointer past $EndSheet if found else NULL
 */

char	*ScanForSheets( char *scanner, char *sheetnamedest, int *sheetsize )
{
char	*endsearch;
char	tmpc;
int		namelen;

	scanner = strstr( scanner, "$Sheet" );				//Look for a sheet
	if( scanner != NULL )								//I found something
	{
		endsearch = strstr( scanner, "$EndSheet");		//Look no further
		if( endsearch == NULL )							//Bad file format
			return( NULL );

		tmpc = *endsearch;								//Save the character
		*endsearch = '\0';								//End the search here

char	ThisName[ endsearch - scanner ];				//File name can't be bigger

		PullFieldString( ThisName, scanner, "F1" );
		*endsearch = tmpc;								//Restore the byte
		scanner = endsearch + sizeof( "$EndSheet" );	//Skip to the end

		namelen = strlen( ThisName );

		if( sheetnamedest != NULL )
		{
			strcpy( sheetnamedest, ThisName );			//Get a copy of the name
			sheetnamedest[ namelen ] = '\0';			//Zero terminate it
		}
		*sheetsize += namelen + 1;						//Tally the bytes needed
	}
	return( scanner );									//Look from here
}

/*
 * Locate a field with text SearchFor and find the starting '"'
 * Append to dest up and including the quote
 * Copy the quoted test into fieldptr
 * Return pointer to the last '"'
 */

char	*CopyCompField( char *linestart, char *lineptr, char *dest, char *fieldptr )
{
char	tmpc;

		while(( *lineptr != '"' ) && ( *lineptr != '\0')) lineptr++;		//Skip to "
		++lineptr;					//Skip the quote (doesn't matter if it is '\0'
		tmpc = *lineptr;
		*lineptr = '\0';				//Terminate for the strcat
		strcat( dest, linestart );		//Copy up to here to destination
		*lineptr = tmpc;

		while(( *lineptr != '"' )  && ( *lineptr != '\0')) *fieldptr++ = *lineptr++;	 //copy the field
		*fieldptr = '\0';
		return( lineptr );			//Return where you ended up
}

/*
 * Find the schematic component in the RefDesCHangeArray and append it to the destination
 *
 */

void	FindSchematicComponent( char *dest, char *field, struct RefDes *RefDesChangeArray, int modules, int buflen )
{
int	i;
char	tmpstr[10];			//Room for integer string

	for( i = 0; i < modules; i++) 							//Search through the change array
	{
		if( strcmp( field, RefDesChangeArray[i].OldRefDesString	) == 0 )		//If the string is found
		{	//This is what I want to substitute
			if( RefDesChangeArray[i].OldRefDes == SKIPREFDES )		//Do not substitute with a new refdes
				SafeStringConcatinate( dest, RefDesChangeArray[i].OldRefDesString, buflen );	//Basically do nothing
			else
			{ //The component has been found
				SafeStringConcatinate( dest, RefDesChangeArray[i].prepend, buflen );
				SafeStringConcatinate( dest, RefDesChangeArray[i].RefDesType, buflen );
				sprintf( tmpstr, "%d", RefDesChangeArray[i].NewRefDes );
				SafeStringConcatinate( dest, tmpstr, buflen );
				RefDesChangeArray[i].Found++;							//Found it at least once.
			}
			break;			//Out of the search loop
		}
	}
	if( i >= modules )	//Not found in RefDesChangeArray
	{
			printf("\nSchematic refdes %s not found in change array!\n", field );		//In case of PWR, etc..
			SafeStringConcatinate( dest, field, buflen );
			SafeStringConcatinate( dest, "-NOTFOUND", buflen );
	}
} //FindSchematicComponent()

/*
 * Process the input component to the output component with the RefDesChangeArray
 */

void	ProcessComponent( char *dest, char *source, struct RefDes *RefDesChangeArray, int modules )
{
int		buflen = strlen( source );

char	aline[buflen];
char	field[buflen];
char	*lineptr = aline;
char	*fieldptr = field;
char	tmpc;

	*dest = '\0';							//Start with a blank sheet

	while( *source != 0 )					//Until the end
	{
		fieldptr = field;
		lineptr = aline;					//Point to start of line
		*lineptr = '\0';					//Zero terminate
		*fieldptr = '\0';

		while(( *source != '\n' ) && ( *source != '\r') && ( *source != '\0')) //Copy until newline or line feed
				*lineptr++ = *source++;

		while((( *source == '\n' ) || ( *source == '\r')) && ( *source != '\0'))	//Copy the newline and line feed
				*lineptr++ = *source++;
		*lineptr = '\0';							//Terminate
		field[0] = '\0';
		lineptr = aline;							//Point to start of line

		if( *lineptr == 'L')						//Is this the first line? (ie. L LED D201 )
		{
			lineptr += 2;
			while(( *lineptr != ' ' ) && ( *lineptr != '\0')) lineptr++;		//Skip to non space
			tmpc = *++lineptr;				//Skip the space and get the next character
			*lineptr = '\0';
			strcat( dest, aline );		//Copy up to here to destination
			*lineptr = tmpc;
			while(( *lineptr > ' ' )  && ( *lineptr != '\0')) *fieldptr++ = *lineptr++;	 //copy the field
			*fieldptr = '\0';		//Zero terminate

		}
		else if ( strstr ( lineptr, "F 0") == lineptr )		//"F 0" at start of line (i.e. F 0 "D501" H 5600 4025 50  0000 C CNN )
		{
			lineptr += ( sizeof( "F 0") - 1 );
			lineptr = CopyCompField( aline, lineptr, dest, field ); //Get the field starting at "
		}
		else if ( strstr ( lineptr, "AR") == lineptr )		//"AR" at start of line (i.e. AR Path="/579A9AFE/579AA2E5" Ref="D201"  Part="1" )
		{
			lineptr = strstr( lineptr, "Ref=" ) + sizeof( "Ref=") - 1;			//Find the Ref= tag
			lineptr = CopyCompField(  aline, lineptr, dest, field ); 				//Get the field starting at "
		}
		else
			strcat( dest, lineptr );	//No fields found just copy this line to the end

		if( field[0] != '\0' )						//I found something
		{
			if( field[0] == '#')						//Fake refdes (#pwr, etc
				strcat( dest, field );					//Copy the fake refdes
			else
				FindSchematicComponent( dest, field, RefDesChangeArray, modules, buflen );
			strcat( dest, lineptr );	//No fields found just copy this line to the end
		}
	}
} //ProcessComponent

/*
 *  * Scan through the PCB file and substitute RefDes Old for RefDes New
 * The input file name has already been determined by the hierarchy scan so no need to add .sch
 */

void	UpdateSchematicFileRefDes( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray )
{
char	*found, *nextcomp;			//Where stuff is

int		outnamesize = strlen( path ) + strlen( fname ) + 2;
char	outfilename[ outnamesize ];			//make the filename and extension

int		complen;

char	*buffer = 	LoadFile( path, fname, "" );			//Load this file and extension
char	*head = buffer;										//Where to start or end writing.
char	*compend;

if( MakeBackupFile( path, fname, "" ) != 0 )
		RenumError( "Unable to create SCH Backup file", SCHBACKUPERROR );

	MakeFileName( outfilename, path, fname, "", outnamesize );

	printf("\n   Updating %s", outfilename );

	G_WriteFile = fopen ( outfilename,"wb+");			//Open the file

	if (G_WriteFile  == NULL)						//Not found
			RenumError( "Can't create schematic output file!", SCHWRITECREATEERROR );

		nextcomp = strstr( buffer, "$Comp");			//Use this to find where the next module starts
		while( nextcomp != NULL )	//Scan through the buffer (PCBFIle) and replace old refdes with new ones.
		{
			found = nextcomp; 								//Find the "$Comp" token
			if( found != NULL )								//I did find a modules token
			{
				if( fwrite( head, sizeof(char), (found - head)/sizeof(char) , G_WriteFile ) != (found - head)/sizeof(char))
					RenumError( "Can't write to file!", OUTFILEWRITEERROR );

				compend = strstr( found, "$EndComp") + sizeof( "$EndComp" ) - 1;			//Find the end of the component
				head = compend;

				complen = (compend - found);				//How big is it
		char	tmpcomponent[ complen + 3];					//place to make a copy of the component
		char	destcomponent[complen * 2 ];				//Where the output goes

				memcpy( tmpcomponent, found, complen );
				tmpcomponent[ complen ] = '\0';

				ProcessComponent( destcomponent, tmpcomponent, RefDesChangeArray, modules );
				fprintfbuffer( destcomponent );
				nextcomp = strstr( found + sizeof("$Comp"), "$Comp");			//Use this to find where the next module starts
			}
		}// while
/*
* All the modules have been found and replaced. Now write out the end of the file
*/
		fprintfbuffer( head );
		fclose (G_WriteFile);
} //UpdateSchematicFileRefDes()

/*
 * Enter a $Sheet structure which is zero terminated.
 * Find the field, then a '"'. Copy from the '"' to the next quote
 *
 *
 * return NULL if not found
 */
void	PullFieldString( char *dest, char *buffer, char *instr )
{
char	*found;

		*dest = '\0';									//Assume not found

		found = strstr( buffer, instr );
		if( found != NULL )			//I found the field
		{
			while(( *found != '"') && ( *found != '\0')) found++;	//find the '"'
			if( *found != '\0' ) found++;
			while(( *found != '"') && ( *found != '\0'))
				*dest++ = *found++;						//Copy the string
			*dest++ = '\0';									//Make sure zero terminate
		}
} //PullFieldString


/*
*	I use fprintf a lot
*/
void	fprintfbuffer( char *buffer )
{
		if( fprintf( G_WriteFile, "%s", buffer ) < 0 )		//Wite out what you got to here
			RenumError( "Can't write to file!", OUTFILEWRITEERROR );
}//void	fprintfbuffer( char *buffer )

/*
* This is a consolidated and simplified search and replace (rather than two passes for modules and nets )
*
*/
void	UpdatePCBFileRefDes( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray, char *buffer )
{
char	*head = buffer;				//Where to start or end writing.
char	*fp_text_ptr, *Net_ptr;
char	*changestr, *srcnptr, delimiter;

char 	OldRefDes[MAXREFDES+1];		//What the OldRefDes is (i.e. U, R VR, etc.)

int 	i, no_fp = 0, no_net = 0;	//Clear the no more flags
int		nummodules = 0;				//Number of refdes found
int		numnets = 0;				//Number of nets found

int		outnamesize = strlen( path ) + strlen( fname ) + sizeof( ".kicad_pcb" ) + 2;
char	outfilename[ outnamesize ];			//make the filename and extension

FILE	*debughandle;

		if( MakeBackupFile( path, fname, ".kicad_pcb" ) != 0 )
			RenumError( "Unable to create PCB Backup file", PCBBACKUPERROR );

		MakeFileName( outfilename, path, fname, ".kicad_pcb", outnamesize );

		G_WriteFile = fopen ( outfilename,"wb+");			//Open the file
		if (G_WriteFile  == NULL)						//Not found
				RenumError( "Can't create output file!", OUTFILEOPENERROR );

		debughandle = OpenDebugFile( path, fname, "_update", ".txt", "Reference designations and net list changes" );
		fprintf(debughandle, "***************************** Change List *****************************");
		fprintf(debughandle, "\n#\tWas\tIs\tType");

		while( *head != 0 )	//Until the end of the file
		{
			if( no_fp == 0 ) 										//If it isn't there once, it'll never be there
				fp_text_ptr = strstr( head, "fp_text reference");	//Find this field
			if( no_net == 0 )
				Net_ptr = strstr( head, "\"Net-(");					//Find that field

			if( fp_text_ptr == NULL ) no_fp = 1;					//No more fp_text references
			if( Net_ptr ==  NULL ) no_net = 1;						//No more Net-(

			if(( no_fp * no_net) == 1 ) break;						//Neither found so I am finished - just flush
			if(( no_fp + no_net ) == 0 )							//If neither are NULL
			{
				if( fp_text_ptr < Net_ptr )							//fp_text_ptr is first
					Net_ptr = NULL;									//Ignore the latter
				else
					fp_text_ptr = NULL;
			}

			if( fp_text_ptr == NULL )
			{ //I found the "Net-(" field
				srcnptr = Net_ptr + sizeof( "\"Net-(" ) - 1;				//Head is now after the string
				changestr = "Net ";
				delimiter = '-';//Delimiter for old text reference
			}
			else	//I found the "fp_text reference" field
			{
				srcnptr = fp_text_ptr + sizeof( "fp_text reference " ) - 1;	//Head is now after the string
				changestr = "Ref ";
				delimiter = '\0';								//Delimiter for old text reference
			}

			OldRefDes[0] = *srcnptr;						//Save the character in the old ref des array
			*srcnptr = '\0';								//Zero terminate
			fprintfbuffer( head );							//Write out what you got to here
//
// Save the old refdes just in case there is no change
//
			i = 1;												//Get the rest of the old reference designator
			while(( i < MAXREFDES) && ( srcnptr[i] > ' ')
				&& ( srcnptr[i] != delimiter ) && srcnptr[i] != '\0' )		//Until the dash usually
			{
				OldRefDes[i] = srcnptr[i];						//Copy the Old Ref Des and skip it
				i++;											//
			}
			OldRefDes[i] = '\0';								//End the string
			head = srcnptr + i;									//Remember where to pick this up

			for( i = 0; i < modules; i++)					//Search the change array
			{
				if( strcmp( OldRefDes, RefDesChangeArray[i].OldRefDesString	) == 0 )
				{	//This is what I want to substitute
					if( delimiter == '-' )numnets++;		//One more net
					else nummodules++ ;						//or module

					if( RefDesChangeArray[i].OldRefDes == SKIPREFDES )		//Do not substitute with a new refdes
						fprintfbuffer( OldRefDes );							//So write out the old one
					else	//Replace the refdes string
					{	//Found the new refdes so write that out
						if( fprintf( G_WriteFile, "%s%s%d", RefDesChangeArray[i].prepend,
								RefDesChangeArray[i].RefDesType, RefDesChangeArray[i].NewRefDes ) < 0 )				//Write it to a file
									RenumError( "Can't write to file!", OUTFILEWRITEERROR );
					}

					fprintf( debughandle, "\n%d\t%s\t%s%s%d\t%s", i, OldRefDes, RefDesChangeArray[i].prepend,
						RefDesChangeArray[i].RefDesType, RefDesChangeArray[i].NewRefDes, changestr );
					if( RefDesChangeArray[i].OldRefDes == SKIPREFDES ) fprintf( debughandle, "\t*** will be ignored ***");

					break;			//Out of the for search loop
				}
			}	//for
			if( i >= modules )					//If not found just write the old ref des
			{
				fprintfbuffer( OldRefDes );		//Write out what was the old refdes
				if( delimiter == '\0' )			//If a module
					printf("\n  Warning module reference designation %s not found in change array", OldRefDes );
			}
		}//While
		fprintfbuffer( head );								//Flush the rest of the file
		fclose (G_WriteFile);

		fclose ( debughandle );

		printf("\n\nUpdated %d modules and %d nets in the PCB file", nummodules, numnets );
}//void	UpdatePCBFileRefDes( char *path, char *fname, int modules, struct RefDes *RefDesChangeArray, char *buffer )


/*
* Parse the string to determine the sort direction
*
* Format is [X/Y][A/D][A/D] (case doesn't matter)
*
*/
struct	MenuChoice {
	char	*string;
	char	*abbreviation;
	int		code;
};

struct	MenuChoice G_DirectionsArray[] =
{
		{"Top to bottom, left to right", "TBLR", SORTYFIRST + ASCENDINGFIRST + ASCENDINGSECOND },
		{"Top to bottom, right to left", "TBRL", SORTYFIRST + ASCENDINGFIRST + DESCENDINGSECOND },
		{"Bottom to top, left to right", "BTLR", SORTYFIRST + DESCENDINGFIRST + ASCENDINGSECOND },
		{"Bottom to top, right to left", "BTRL", SORTYFIRST + DESCENDINGFIRST + DESCENDINGSECOND },
		{"Left to right, top to bottom", "LRTB", SORTXFIRST + ASCENDINGFIRST + ASCENDINGSECOND },
		{"Left to right, bottom to top", "LRBT", SORTXFIRST + ASCENDINGFIRST + DESCENDINGSECOND },
		{"Right to left, top to bottom", "RLTB", SORTXFIRST + DESCENDINGFIRST + ASCENDINGSECOND },
		{"Right to left, bottom to top", "RLBT", SORTXFIRST + DESCENDINGFIRST + DESCENDINGSECOND },
		{ NULL, NULL, -1 }
};

struct	MenuChoice G_ModulesRefDesArray[] =
{
		{"Reference Designation coordinates", "", 0 },
		{"Module coordinates", "", 1 },
		{ NULL, NULL, -1 }
};

void	SetSortDirection( char* argv, int *SortCode )
{

struct	MenuChoice *DirArray = &G_DirectionsArray[0];
int		i = 0;

	do
	{
		if( strcmp( DirArray[i].abbreviation, argv ) == 0 )
			*SortCode = DirArray[i].code;
	}while( DirArray[i++].abbreviation != NULL );
}

/*
 * Return a string which is in the same location as the code
 */
char	*GetMenuString( struct	MenuChoice *choices, int code )
{
int		i = 0;
		do
		{
			if( choices[i].code == code ) return( choices[i].string );
		} while( choices[i++].code != -1 );

		return( "Invalid menu code ");
} //GetSortString

/*
 * Delete to the right of the cursor
 */
int	DeleteRight( char *tmpstr, int location )
{
int	i, j;

	i = location;					//Save the location
	j = strlen( tmpstr );			//Length of original string

	if(( i < j ) && ( tmpstr[i] != '\0'))	//Nothing is a blank strung
	{
		for( ; i < j; i++ ) tmpstr[i] = tmpstr[i+1];

		for( i = 0; i < location; i++ ) putchar( 0x08);		//Back space all the way to the start
		printf("%s", tmpstr );								//And show the new string

		if( j > strlen( tmpstr ) )							//If it hangs over
			for( i = 0; i < ( j - strlen( tmpstr )); i++ ) putchar( ' ');				//Clear the line
		for( i = 1; i <= ( j - location ); i++ ) putchar(0x08);		//Back space to where you where
	}
	return( location );
}


/*
 * Delete to the left of the cursor
 */
int	DeleteLeft( char *tmpstr, int location )
{
int	i, j;

	if( location > 0 )						//Not if at first character
	{
		i = location;						//Save the location
		j = strlen( tmpstr );				//Length of original string

		if( i == j ) tmpstr[--i] = '\0';
		else
			for( ; i <= j; i++ ) tmpstr[i-1] = tmpstr[i];		//Copy left

		for( i = 0; i < location; i++ ) putchar( 0x08);		//Back space all the way to the start
		printf("%s", tmpstr );								//And show the new string

		if( j > strlen( tmpstr ) )							//If it hangs over
				for( i = 0; i < ( j - strlen( tmpstr )); i++ ) putchar( ' ');				//Clear the line
		for( i = 0; i <= ( j - location ); i++ ) putchar(0x08);		//Back space to where you where
		--location;
	}
	return( location );
}

#ifdef		Windows					//Windows

int	getchEASCII( void )
{
	return( getch() );

}

#else

/*
* Deal with wonky Linux getch()
* reads from keypress, doesn't wait for a CR
*/

int getch(void)
{
struct termios oldattr, newattr;
int ch;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return( ch );
}


/*
* Deal with Linux escap sequences
*/

int	getchEASCII( void )
{
	if( getch() != '[') return( 0 );			//If not a proper escape sequence

int	i = getch();							//Get the second part
	if( i == 'D' ) return( LEFTARROW );
	if( i == 'C' ) return( RIGHTARROW );
	if(( i == '3' ) && (getch() == '~' )) return( DEL );
	return( 0 );
}

#endif

/*
 * My getch handles extended ascii
 */

int	mygetch( void )
{
int	i = getch();
	if( i == EASCII )
		i = ( getchEASCII() | KEYESCAPE );
	return( i );
}
/*
 * Get a string of maximum size (terminal does most of the editing )
 */

void	StringGet( char	*prompt, char *dest, char *original, int maxsize )
{
int		tmp;
char	tmpstr[maxsize+1];

int	i;
	for( i = 0; i < maxsize; i++ ) tmpstr[i] = '\0';		//Ensure an end
	SafeStringCopy( tmpstr, original, maxsize );

	i = strlen( tmpstr );
	printf("\n%s %s", prompt, tmpstr );
	do	//Get with some editing
	{
		tmp = mygetch();				//Get a character including extended ascii
		if(( tmp >= ' ' ) && ( tmp < 0x7f )) 		//If a printable
		{	putchar( tmp );
			tmpstr[i++] = (char) tmp;				//Save it
		}

		else //Maybe some editing to do
		{
			switch ( tmp )
			{
				case BS :			//Destructive backspace
					i = DeleteLeft( tmpstr, i );
				break;

				case LEFTARROW :	//Move left
					if( i > 0 )
					{
						printf("\x08");
						--i;
					}
				break;

				case RIGHTARROW :	//Move right
					if( tmpstr[i] != '\0' )
						printf("%c", tmpstr[i++] );
				break;

				case DEL :			//Delete
					DeleteRight( tmpstr, i );		//Deleted to the right of the cursor
					break;

			}
		}
	} while(( i < maxsize ) && ( tmp != ABORT ) && ( tmp != CR ));

	if( tmp != ABORT )
		SafeStringCopy( dest, tmpstr, maxsize );
} //StringGet()


/*
 * Select a choice from the menu
 */

void	MenuSelect( char *prompt, struct MenuChoice *choices, int *result )
{
int	i, j, tmpresult = *result;
int	tmpc, menuentry;

	printf("\nHit <space> to change, <cr> to select ");
	printf("\n%s ", prompt );

	do
	{
		menuentry = 0;
		do
		{
			if( choices[menuentry].code == -1 )
					{ menuentry = 0; break; }						//End of the menu so wrap
			if( choices[menuentry].code == *result ) break;			//found the spot
			menuentry++;
		}while( 1 );

		j = strlen ( choices[menuentry].string );	//How big is the related string?
		printf("%s", choices[menuentry].string );	//Show the string
		*result = choices[menuentry].code;					//Get this code

		tmpc = toupper( mygetch( ) );				//What to do?
		for( i = 0; i < j; i++ ) putchar('\b');		//First back up to the beginning of the string
		for( i = 0; i < j; i++ ) putchar( ' ');		//Clear the line
		for( i = 0; i < j; i++ ) putchar( '\b' );		//And back up to the beginning again

		if( tmpc == ' ')							//If a space go to next menu item
			*result = choices[menuentry+1].code;		//Get this code next time
		else if(( tmpc == CR ) || ( tmpc == ABORT )) 	//Abort or enter
			break;
	} while( 1 );



	if( tmpc == ABORT ) *result = tmpresult;			//Restore if it was abort.
}



/*
 * Parse the command line arguments there is at least 1: the input file name
 */


/*
 * Set the input file name and create a backup copy
 */
void	SetInfile( char* argv )
{
	G_InputFileName = argv;						//set the input file name
}

/*
 * Set the top level prepend string
 */
void	SetTopPrepend( char *argv )
{
	G_TopPrepend = argv;			//set the input file name
}

/*
 * Set the bottom level prepend string
 */
void	SetBottomPrepend( char *argv )
{
	G_BottomPrepend = argv;			//set the input file name
}

/*
 * Set the grid parameter G_Grid
 *
 */
void	Setgrid( char *argv )
{
	G_Grid = atof( argv );
	if( G_Grid < MINGRID ) G_Grid = MINGRID;
}

/*
 * Set the top starting reference designator
 */
void	SetTopStartRefDes( char *argv )
{
	G_TopStartRefDes = abs( atoi( argv ));
}

/*
 * Set the bottom starting reference designator
 */
void	SetBottomStartRefDes( char *argv )
{
	G_BottomStartRefDes = abs( atoi( argv ));
}

/*
 * Set Top Sort Modes
 */
void	SetTopSort( char *argv )
{
	SetSortDirection( argv, &G_TopSortCode );

}

/*
 * Set Bottom Sort Modes
 */

void	SetBottomSort( char *argv )
{
	SetSortDirection( argv, &G_BottomSortCode );
}

/*
 * Sort on moudule location rather than refdes
 */
void	SetSortOnModules( char *argv )
{
	G_SortOnModules = 1;
}

/*
 * Don't ask whether to run
 */
void	SetNoQuestion( char *argv )
{
	G_NoQuestion = 1;
}

/*
 * Command error
 */

void	CommandError( char *argv )
{
	printf("\nInvalid command %s", argv );
}


typedef void ( *CommandFunctionPtr )( char *argv );

struct	CommandParse {
	char	*Comstring;						//Variable length command string
	CommandFunctionPtr CommandFunction;		//Execut this function if there is a match

};

struct 	CommandParse G_CommandParseArray[] = 	//Work through this array to parse commands
{
		{"-i", SetInfile },						//Set the input file name
		{"-g", Setgrid },
		{"-fp", SetTopPrepend },				//Set the top prepend text
		{"-bp", SetBottomPrepend },				//Set the bottom prepend text
		{"-fs", SetTopSort },					//Set the top sort modes
		{"-bs", SetBottomSort },				//Set the bottom sort modes
		{"-fr", SetTopStartRefDes },			//Set the top starting reference designator
		{"-br", SetBottomStartRefDes },			//Set the bottom starting reference designator
		{"-m", SetSortOnModules },				//Set the sort mode to sort on modules rate than refdes
		{"-y", SetNoQuestion },					//Don't ask permission to run
		{"-?", PrintHelpFile },					//Print out the command list
		{ NULL, CommandError},					//End of list
};

/*
 * Parse the command line arguments
 * Note the char * globals (i.e. G_BottomPrepend ) are pointers to strings, not strings
 * So don't copy them because you'll overwrite memory!
 */
void	ParseCommandLine( int argc, char *argv[] )
{
int		cmdarrayindex;
int		i;

char	*arg, *tmp, *comstring;

	for( i = 1; i < argc; i++)			//Go through the commands and parse them
	{
		cmdarrayindex = 0;								//Start at the beginning
		while(( comstring = G_CommandParseArray[cmdarrayindex].Comstring ) != NULL )
		{
			arg = argv[i];
			if( *arg == '\'' ) arg++;		//For some reason under msys2, Eclipse wraps debug arguments with ' so ignore

			while(( *comstring != '\0') && (*comstring == *arg++ )) ++comstring;		//Search so see if comstring at start of argv
			if( *comstring == '\0')
			{
				tmp = arg;
				while( *tmp != '\0') if( *tmp == '\'') *tmp = '\0'; else tmp++;				//Strip off trailing ' if any
				( G_CommandParseArray[cmdarrayindex].CommandFunction ) (arg);
				break;	//Out of the look for loop
			}
			++cmdarrayindex;							//Check the next in the list
		}
	}
}//ParseCommandLine


//
// Check if the input file exists. Return 0 if it does not
//

int		CheckInputFileName( void )
{
int	i, j = strlen( G_InputFileName );
int	lastslash = 0;

	for( i = 0; i < j; i++ )	//Find the last / or \ in the path
		if(( G_InputFileName[i] == '\\') || ( G_InputFileName[i] == '/')) lastslash = i;

	for( i = lastslash; i < j; i++ )	//Locate any "." indicating a file extension
		if( G_InputFileName[i] == '.') G_InputFileName[i] = '\0';			//Trim off the file extension

	SafeStringConcatinate( G_InputFileName, ".kicad_pcb", MAXPATH ); 			//And the file extension


FILE	*readhandle = fopen ( G_InputFileName,"rb");			//Open the file read only binary
	if (readhandle == NULL)											//Not found
	{
		printf("\n\nFile not found: %s\n\n", G_InputFileName );
		return( 0 );				//Nada
	}
	else
	fclose( readhandle );
	return( 1 );
}

/*
 * Get the input file name
 */

int	GetInputFileName( void )
{
	StringGet( "\n\nFile name: ", G_FileName, G_InputFileName, MAXPATH );
	G_InputFileName = G_FileName;
	return( CheckInputFileName());
}

/*
 * Show the menu choices
 */

//
// This writes the parameter file into the local directory
//
void	WriteParameterFile( void )
{
int		i = 0;
char	buffer[ MAXPATH * 2 ]; 	 	// allocate more memory than you'd need
char	tbuf[MAXPATH];
char	parametertype;
void	*parameterpointer;

	memset( buffer, 0, sizeof(char ) * MAXPATH * 2);				//Zero the destination buffer

	while(G_ParameterFile[i].Parametername != 0 )
	{
		SafeStringConcatinate( buffer, (char *) G_ParameterFile[i].Parametername, ((sizeof(char) * MAXPATH * 2)) );

		parametertype = G_ParameterFile[i].Parametertype;
		parameterpointer = G_ParameterFile[i].Pointertoparameter;

		if( parametertype == 'T' )		//If a text field
				sprintf( tbuf, "%s\n", ( char *)parameterpointer );

		else if( parametertype == 'F' )	//If a floating point
				sprintf( tbuf, "%f\n", *( float *)parameterpointer );

		else if( parametertype == 'N' )	//If a decimal number
				sprintf( tbuf, "%d\n", *( int *)parameterpointer );


		SafeStringConcatinate( buffer, tbuf, ((sizeof(char) * MAXPATH * 2)) );
		i++;		//Until the end
	}

	FILE	*WriteFile = fopen ( PARAMETERFILENAME,"wb+");			//Open the file
		if (WriteFile  == NULL) RenumError( "Can't create parameter file!", PARAMWRITECREATEERROR );

		i = strlen( buffer );

		if( fwrite( buffer, sizeof(char), i, WriteFile ) != i )
			RenumError( "Can't write parameter file!", PARAMWRITECREATEERROR );
		fclose( WriteFile );

	}	//WriteParameterFile()

//
// This loads the parameter file from the local directory and sets the values
//
void	LoadParameterFile( void )
{
int		i, j;
char 	parametertype;
void	*parameterpointer;

FILE	*ReadFile = fopen ( PARAMETERFILENAME,"rb");			//Open the file read only binary
		if (ReadFile  == NULL) return;

		fseek(ReadFile, 0L, SEEK_END);							//Go to the end of the file
long	filesize = sizeof(char) * (ftell( ReadFile ));			//Where am I?
		fseek(ReadFile, 0L, SEEK_SET);							//Go to the start of the file

char	buffer[ ((sizeof(char) * filesize ) + 2) ]; 	 		// allocate memory to contain the whole file plus a '\0'
char	*bufpnt = buffer;
char	dest[ ((sizeof(char) * filesize ) + 2) ]; 	 		// allocate memory to contain the whole file plus a '\0'

		if( fread (buffer, sizeof( char ), filesize, ReadFile ) != filesize ) 	// And read the file into memory
			RenumError("Paramter file read error", MALLOCERROR );		//I am out of here
//
// Now i have the parameter file in buffer
//
		buffer[ (sizeof(char) * filesize ) ] = '\0';
		do {													//Copy a string to a work buffer
			i = 0;												//From the beginning
			while(( *bufpnt != '\0' ) && (*bufpnt != '\n'))		//Until end or newline
				dest[i++] = *bufpnt++;

			if( i > 0 )			//File not finished
			{
				dest[i] = '\0';									//Make an end of line
				while(( *bufpnt <= ' ') && (*bufpnt != '\0')) bufpnt++; //Skip to next line

				do	if( dest[i] == '=') break; while( --i > 0 ); //Find the equals sign (delimits name, value

				if( i == 0 )
					printf("\n\nInvalid Parameter File! %s ", dest );
				else // I have a likely valid parameter line
				{
					dest[i++] = '\0';						//Terminate parameter name, point to next character

					j = 0;

					while(G_ParameterFile[j].Parametername != 0 )
					{
						if( strstr( G_ParameterFile[j].Parametername, dest ) == G_ParameterFile[j].Parametername )	//Did I find the parameter?
						{
							parametertype = G_ParameterFile[j].Parametertype;
							parameterpointer = G_ParameterFile[j].Pointertoparameter;

							if( parametertype == 'T' )				//If a text field
									sprintf( ( char *)parameterpointer, "%s", dest + i );

							else if( parametertype == 'N' )			//If a decimal number
									*( int *)parameterpointer = atoi( dest + i );

							else if( parametertype == 'F' )			//If a float number
							{
								float tmp = atof( dest + i );
								if( tmp < 0 ) tmp = -tmp;
								*( float *)parameterpointer = tmp;
							}

							break;
						} else( j++ );
					}

					if(G_ParameterFile[j].Parametername == 0 )
						printf("\n   Not found! [%s] [%s]", dest, dest+i);
				}
			}
		} while( i != 0 );	//Until the end of the file
}


void	ResetParameters( void )
{
		G_InputFileName = "";							//The input file name set by command line
		G_TopPrepend = "";								//T_";
		G_BottomPrepend = "";							//Optional strings to prepend to new refdeses
		G_NoQuestion = 0;								//Don't ask if OK
		G_TopSortCode = SORTYFIRST + ASCENDINGFIRST + ASCENDINGSECOND;
		G_BottomSortCode = SORTYFIRST + ASCENDINGFIRST + DESCENDINGSECOND;

		G_TopStartRefDes = 1;							//Start at 1 for the top
		G_BottomStartRefDes = 0;	//1;				//Start at 1 for the bottom (this is optional)
		G_SortOnModules = 1;							//if 0 sort on ref des location
		G_Grid = 1.0;									//Anything near this (mm) is in the same line
} //ResetParameters




void	ShowMenu( void )
{
	printf("\n\n*****************************************************************");
	printf("\n[1] KiCad design file root PCB name : %s", G_InputFileName );
	printf("\n[2] Front sort: %s ", GetMenuString( G_DirectionsArray, G_TopSortCode ) );
	printf("\n[3] Front reference designators start at %d", G_TopStartRefDes);

	printf("\n[4] Back sort: %s", GetMenuString( G_DirectionsArray, G_BottomSortCode ) );
	printf("\n[5] Back reference designators start " );

	if( G_BottomStartRefDes == 0 )
		printf("where the front ends ");
	else
		printf("at %d ", G_BottomStartRefDes );

	printf("\n[6] Sorting on: %s ", GetMenuString( G_ModulesRefDesArray, G_SortOnModules ));
	printf("\n[7] There is a grid setting of: %.3f", G_Grid );

	printf("\n[8] Front references designators prepend string: %s", G_TopPrepend );
	printf("\n[9] Back references designators prepend string: %s", G_BottomPrepend );

	if(( strlen( G_TopPrepend ) != 0 ) || ( strlen( G_BottomPrepend ) != 0 ))
			printf("\n\t\t ******* Warning only use prepends once per side: they add up! *******\n");

	printf("\n[L] Load parameters.");
	printf("\n[Z] Reset to defaults.");
	printf("\n[H] Show the command line syntax ");
} //ShowMenu()



/*
 * Show what you are going to do and unless -Y parameter, allow changes/updates
char	G_TopPrependString[ MAXPREPEND ];
char	G_BottomPrependString[ MAXPREPEND ];
 *
 */
void	ShowAndGetParameters( )
{
int		i = -1;
	printf("\n\n");
	printf("%s\n", G_HELLO );

	if( strlen( G_InputFileName ) != 0 )
		SafeStringCopy( G_FileName, G_InputFileName, MAXPATH );

	else
	{
		LoadParameterFile(  );				//Try and load the parameter file
		G_InputFileName = G_FileName;
	}

	i = CheckInputFileName();					//If a name, check if exists

	if( i == 0 )
	{
		ShowMenu( );								//Say what is going on
		i = GetInputFileName();									//If no name or invalid get name
	}

	if(( G_NoQuestion == 0 ) || ( i == 0 ))				//Unless directed to skip this
	{
		ShowMenu( );						//Say what is going on
		do
		{
			printf("\n\nEnter number or letter in brackets or 'R' to Run (ctl-C aborts): ");
			i = toupper( mygetch());			//Get the reply
			if( i == ABORT )
			{
				gettimeofday( &G_StartTime, NULL );				//Get the start time
				FreeMemoryAndExit( 0 );							//Control C
			}
			printf("%c\n\n", i );

			switch ( i )
			{
				case '1' :
					GetInputFileName( );
				break;

				case '2' :
					MenuSelect( "Front sort direction: ", G_DirectionsArray, &G_TopSortCode );
				break;

				case '3' :
					sprintf( G_ERRSTR, "%d", G_TopStartRefDes );
					StringGet( "Front reference designators start: ", G_ERRSTR, G_ERRSTR, 6 );
					G_TopStartRefDes = abs( atoi( G_ERRSTR ) );
					if( G_TopStartRefDes == 0 ) G_TopStartRefDes = 1;
				break;

				case '4' :
					MenuSelect( "Back sort direction: ", G_DirectionsArray, &G_BottomSortCode );
				break;

				case '5' :
					sprintf( G_ERRSTR, "%d", G_BottomStartRefDes );
					StringGet( "Back reference designators start (0 means where Front ends): ", G_ERRSTR, G_ERRSTR, 6 );
					G_BottomStartRefDes = abs( atoi( G_ERRSTR ) );
					break;

				case '6' :
					MenuSelect("Sort on: ", G_ModulesRefDesArray, &G_SortOnModules );
				break;

				case '7' :
					sprintf( G_ERRSTR, "%.3f", G_Grid );
					StringGet( "Grid Setting: ",  G_ERRSTR, G_ERRSTR, 10 );
					G_Grid = atof( G_ERRSTR );
					if( G_Grid < 0 ) G_Grid = - G_Grid;

				break;

				case '8' :
					StringGet( "Front reference designator prepend string", G_TopPrependString, G_TopPrepend, MAXPREPEND );
					G_TopPrepend = G_TopPrependString;
				break;

				case '9' :
					StringGet( "Back reference designator prepend string", G_BottomPrependString, G_BottomPrepend,  MAXPREPEND );
					G_BottomPrepend = G_BottomPrependString;
				break;

				case 'H' :
					PrintHelpFile( G_ERRSTR );				//Use a dummy argument
					break;

				case 'L' :
					LoadParameterFile(  );
					G_InputFileName = G_FileName;
					break;

				case 'Z' :
					ResetParameters( );
					break;

				case 'R' : break;

				default : printf("\nInvalid selection" ); break;
			}
			if( i != 'R') ShowMenu( );						//Say what is going on
		} while( i != 'R');
	}
	gettimeofday( &G_StartTime, NULL );				//Get the start time
}

/*
 * The help file
 */


char	G_HELPFILE[] = "\nRenumberKiCadPCB command line options								\n\
-iInfile	The Input file names (infile.kicad_pcb,  infile.sch) (required)					\n\
		Infile is renamed infileRenumBack.kicad_pcb, infileRenumBack.sch					\n\n\
-fs		Front sort direction																\n\
		-fs[1ST][2ND] where [1ST or [2ND] are TB (top to bottom) or LR (left to right		\n\
		DEFAULT is fsTBLR (top to bottom, left to right)									\n\
-bs		Bottom sort direction same arguments as -fs											\n\n\
		DEFAULT is bsTBRL (top to bottom, right to left)									\n\n\
-j		Set the sort grid spacing (i.e 0.1 and 0.15 are the same if grin is 0.5	)			\n\n\
-fp		Top refdes prepend string (i.e. tpT_ means R1 will be T_R1 if it is on top side		\n\
		DEFAULT is empty																	\n\n\
-bp		Bottom refdes prepend string (i.e. bpB_ means R2 will be B_R2 if it is on bottom side\n\
		DEFAULT is empty																	\n\n\
-fr		Top refdes start value (i.e. fp100 means parts will start at 100 on the front side	\n\
		DEFAULT is 1																		\n\n\
-br		Bottom refdes start value (i.e. br100 means R2 will be R102 if it is on bottom side	\n\
		DEFAULT is to continue from the last front refdes									\n\n\
-m		Sort on module location. Default is sort on Ref Des Location						\n\
-y		No Y/N question asked																\n\
-z		Zero settings (reset to defaults) 													\n\
-?		Print out this file\n\n";


/*
 * Print out the command list
 */

void	PrintHelpFile( char *argv )
{
	printf("%s", G_HELPFILE );
}


/*
 *  the end
 */
