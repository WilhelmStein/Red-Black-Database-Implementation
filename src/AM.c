#include "AM.h"
#include "bf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int AM_errno = AME_OK;

fileData fileTable[MAXOPENFILES];
scanData scanTable[MAXSCANS];

#define INDEX ('I')
#define BLACK ('B')
#define RED   ('R')

#define MAXRECORDS(metaData)   ( (BF_BLOCK_SIZE - 9) /  ((int)metaData[ATTRLENGTH1] + (int)metaData[ATTRLENGTH2]) )
#define RECORDSIZE(metaData)   ( (int)metaData[ATTRLENGTH1] + (int)metaData[ATTRLENGTH2] )

//RED BLOCKS
//      IDENTIFIER   (0)  //char
#define RECORDS      (1)  //int
#define PARENT       (5)  //int
#define REDKEY(x)    ( /*FIRSTKEY*/9 + (x * RECORDSIZE(metaData)) )
#define VALUE(x, metaData)     ( /*FIRSTVALUE*/9 + (int)metaData[ATTRLENGTH1] + (x * RECORDSIZE(metaData)) )

//BLACK BLOCKS
//      IDENTIFIER   (0) //char
#define NUMKEYS      (1) //int
//      PARENT       (5) //int
#define FIRST        (9) //int
#define BLACKKEY(x, metaData)  ( /*FIRSTKEY*/13 + x * ( 4 + (int)metaData[ATTRLENGTH1] ) )
#define POINTER(x, metaData)   ( /*FIRSTPOINTER*/9 + x * ( 4 + (int)metaData[ATTRLENGTH1] ) )

//META BLOCK
#define IDENTIFIER   (0)  //char
#define ATTRTYPE1	 (1)  //char	
#define ATTRLENGTH1  (2)  //int
#define ATTRTYPE2	 (3)  //char
#define ATTRLENGTH2  (4)  //int
#define ROOT		 (5)  //int
#define FILENAME	 (9)  //char*
 

#define CALL_OR_EXIT(call)		\
{                           	\
	BF_ErrorCode code = call; 	\
	if(code != BF_OK) {       	\
		BF_PrintError(code);    \
		exit( AME_ERROR );		\
	}                         	\
}

void AM_Init()
{
	for (unsigned i = 0; i < MAXOPENFILES; i++)
	{
		fileTable[i].fileDesc = UNDEFINED;
		fileTable[i].fileName = NULL;
	}

	for (unsigned i = 0; i < MAXSCANS; i++)
	{
		scanTable[i].fileDesc = UNDEFINED;
		scanTable[i].recordKey = NULL;
	}

	CALL_OR_EXIT(BF_Init(LRU));
}

int AM_CreateIndex(char *fileName, 
				   char attrType1, 
				   int attrLength1, 
				   char attrType2, 
				   int attrLength2) 
{
	CALL_OR_EXIT(BF_CreateFile(fileName));

	int fileDesc;
	BF_Block *block;
	BF_Block_Init(&block);

	CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));
	CALL_OR_EXIT(BF_AllocateBlock(fileDesc, block));
	char *data = BF_Block_GetData(block);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // +-------++-------------+-------------+---------------+-------------+---------------+--------+---------------+ //
	// | BYTES ||     1       |      1      |      1        |      1      |      1        |   4    | 1 - (512-9)   | //
	// | VARS  || identifier  |  attrType1  |  attrLength1  |  attrType2  |  attrLength2  |  root  |   fileName    | //
	// +-------++-------------+-------------+---------------+-------------+---------------+--------+---------------+ //
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	data[IDENTIFIER] = INDEX; 

	memcpy(&data[ATTRTYPE1], &attrType1, 1);

	memcpy(&data[ATTRLENGTH1], &attrLength1, 1);

	memcpy(&data[ATTRTYPE2], &attrType2, 1);

	memcpy(&data[ATTRLENGTH2], &attrLength2, 1);

	int zero = 0;
	memcpy(&data[ROOT], &zero, 4);

	memcpy(&data[FILENAME], fileName, strlen(fileName) + 1);

	printf("id = %c\n", data[IDENTIFIER]);
	printf("attrType1 = %c\n", data[ATTRTYPE1]);
	printf("attrLength1 = %d\n", data[ATTRLENGTH1]);
	printf("attrType2 = %c\n", data[ATTRTYPE2]);
	printf("attrLength2 = %d\n", data[ATTRLENGTH2]);
	printf("root at: %d\n", data[ROOT]);
	printf("fileName = %s\n\n", &data[FILENAME]);

  BF_Block_SetDirty(block);
	CALL_OR_EXIT(BF_UnpinBlock(block));
	CALL_OR_EXIT(BF_CloseFile(fileDesc));

	return AM_errno = AME_OK;
}

// Utility Function :
// The user needs to take care of
// opening and closing the specified file
static bool isAM (const int fileDesc)
{
	BF_Block * block;
	BF_Block_Init(&block);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, 0, block));

	const char * const data = BF_Block_GetData(block);
	bool flag = (IDENTIFIER == INDEX);

	CALL_OR_EXIT(BF_UnpinBlock(block));

	return flag;
}

int AM_DestroyIndex(char *fileName)
{
	bool isOpen = false;
	for (unsigned i = 0; i < MAXOPENFILES; i++)
	{
		if (fileTable[i].fileDesc != UNDEFINED && !strcmp(fileName, fileTable[i].fileName))
		{
			isOpen = true;

			break;
		}
	}

	if (!isOpen)
	{
		int fileDesc;
		CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));

		if (!isAM(fileDesc))
			return (AM_errno = AME_NOT_AM_FILE);

		CALL_OR_EXIT(BF_CloseFile(fileDesc));

		return (AM_errno = (!remove(fileName) ? AME_OK : AME_DESTROY_FAILED_REMOVE));
	}

	return (AM_errno = AME_DESTROY_FAILED_OPEN);
}

int AM_OpenIndex (char *fileName)
{
	int fileDesc;
	CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));

	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	AM_errno = AME_OK;
	
	unsigned i;
	for (i = 0; i < MAXOPENFILES; i++)
	{
		if (fileTable[i].fileDesc == UNDEFINED)
		{
			fileTable[i].fileDesc = fileDesc;
			fileTable[i].fileName = (char *) malloc((strlen(fileName) + 1) * sizeof(char));
			strcpy(fileTable[i].fileName, fileName);

			break;
		}
	}

	return (i < MAXOPENFILES ? i : (AM_errno = AME_OPEN_FAILED));
}

int AM_CloseIndex (int fileDesc)
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	int i;
	for (i = 0; i < MAXSCANS; i++)
		if (fileDesc == scanTable[i].fileDesc)
			return (AM_errno = AME_CLOSE_FAILED_SCANS);

	for (i = 0; i < MAXOPENFILES; i++)
	{
		if (fileDesc == fileTable[i].fileDesc)
		{
			CALL_OR_EXIT(BF_CloseFile(fileTable[i].fileDesc));

			free(fileTable[i].fileName);
			fileTable[i].fileName = NULL;
			fileTable[i].fileDesc = UNDEFINED;
			
			break;
		}
	}

	return (AM_errno = (i > MAXOPENFILES ? AME_CLOSE_FAILED_UNOPENED : AME_OK));
}

static void printBlack(const int i, const char *blackData, const char *metaData)
{
	switch(metaData[(int)ATTRTYPE1])
	{
		case 'i':
			printf("  %d  |%d|", (int)blackData[(int)BLACKKEY(i, metaData)], (int)blackData[(int)POINTER(i + 1, metaData)]);
			break;
		case 'f':
			printf("  %f  |%d|", (float)blackData[(int)BLACKKEY(i, metaData)], (int)blackData[(int)POINTER(i + 1, metaData)]);
			break;
		case 'c':
			printf("  %s  |%d|", blackData[(int)BLACKKEY(i, metaData)], (int)blackData[(int)POINTER(i + 1, metaData)]);
			break;
	}
}

static void printRed(const int i, char *redData, char *metaData)
{
	switch(metaData[(int)ATTRTYPE1])
	{
		case 'i':
			printf("%d :", (int)redData[(int)REDKEY(i, metaData)]);
			break;
		case 'f':
			printf("%f :", (float)redData[(int)REDKEY(i, metaData)]);
			break;
		case 'c':
			printf("%s : ", redData[(int)REDKEY(i, metaData)]);
			break;
	}

	switch(metaData[(int)ATTRTYPE2])
	{
		case 'i':
			printf("%d", (int)redData[(int)VALUE(i, metaData)]);
			break;
		case 'f':
			printf("%f", (float)redData[(int)VALUE(i, metaData)]);
			break;
		case 'c':
			printf("%s", redData[(int)VALUE(i, metaData)]);
			break;
	}
}

static void printRec(const int fd, const int blockIndex, const char *metaData)
{
	BF_Block *parentBlock;
	BF_Block_Init(&parentBlock);
	CALL_OR_EXIT( BF_GetBlock(fd, blockIndex, parentBlock) );
	char *parentData = BF_Block_GetData(parentBlock);

	if(parentData[IDENTIFIER] == BLACK)
	{
		printf("Block Id: %d\nType: Black\nContents: |%d|", blockIndex, (int)parentData[(int)POINTER(0, metaData)]);
		for(int i = 0; i < parentData[NUMKEYS]; i++)
			printBlack(i, parentData, metaData);
		printf("\n");
		for(int i = 0; i < (int)parentData[NUMKEYS]; i++) {
			int child = (int)parentData[(int)POINTER(i, metaData)];
			CALL_OR_EXIT( BF_UnpinBlock(parentBlock) );
			printRec(fd, child, metaData);
			CALL_OR_EXIT( BF_GetBlock(fd, blockIndex, parentBlock) );
		}
	}
	else
	{
		printf("Block Id: %d\nType: Red\nContents: ", blockIndex);
		for(int i = 0; i < parentData[RECORDS]; i++)
			printRed(i, parentData, metaData);
		printf("\n");
	}
	CALL_OR_EXIT( BF_UnpinBlock(parentBlock) );
}

static void debugPrint(const int fd)
{
	BF_Block *metaBlock;
	BF_Block_Init(&metaBlock);
	CALL_OR_EXIT( BF_GetBlock(fd, 0, metaBlock) );
	char *metaData = BF_Block_GetData(metaBlock);
	int root = (int)metaData[ROOT];
	printRec(fd, root, metaData);
	CALL_OR_EXIT( BF_UnpinBlock(metaData) );
}

static bool less(void *value1, void *value2, char attrType1) 
{
	switch(attrType1) {
		case 'i':
			return *(int *)value1 < *(int *)value2;
		case 'f':
			return *(float *)value1 < *(float *)value2;
		case 'c':
			return strcmp( (char *)value1, (char *)value2 ) < 0;
		default: break;
	}
}

static int SplitBlack(int fileDesc, int target, void *key, char *metaData) {

}

static int SplitRed(int fileDesc, int target, void *value1, void *value2, char *metaData)  
{
	BF_Block *targetBlock;
	BF_Block_Init(&targetBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, target, targetBlock));
	char *data = BF_Block_GetData(targetBlock);

	BF_Block *newBlock;
	BF_Block_Init(&newBlock);
	CALL_OR_EXIT(BF_AllocateBlock(fileDesc, newBlock));
	char *newData = BF_Block_GetData(newBlock);

	newData[IDENTIFIER] = RED;
	int newRecords = ( ((int)(data[RECORDS]) % 2) ? (int)data[RECORDS] / 2 : ((int)data[RECORDS] / 2) + 1); 
	memcpy( &newData[RECORDS], &newRecords, 4);
	int oldRecords = (int)data[RECORDS] / 2; 

	memcpy( &newData[REDKEY(0)], &data[REDKEY(oldRecords)], RECORDSIZE(metaData) * newRecords );
	for (int i = 0; i < (int)newData[RECORDS]; i++) {
		if (less(value1, newData[REDKEY(i)], metaData) ) {
			//Move all bigger keys-values to the right
			memcpy( &data[REDKEY(i + 1)], &data[REDKEY(i)], RECORDSIZE(metaData) * ((int)newData[RECORDS] - i) );
			//Push new record
			memcpy( &data[REDKEY(i)], value1, (size_t)metaData[ATTRLENGTH1]);
			memcpy( &data[VALUE(i, metaData)], value2, (size_t)metaData[ATTRLENGTH2]);
			break;
		}
	}
	memcpy( &data[RECORDS], &oldRecords, 4);

	BF_Block_SetDirty(targetBlock);
	CALL_OR_EXIT(BF_UnpinBlock(targetBlock));
	BF_Block_SetDirty(newBlock);
	CALL_OR_EXIT(BF_UnpinBlock(newBlock));

	int retVal;
	CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &retVal));

	return retVal;

}

static int InsertRec(int fileDesc, void *value1, void *value2, char *metaData, int root) 
{
	BF_Block *currentBlock;
	BF_Block_Init(&currentBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, root, currentBlock));
	char *data = BF_Block_GetData(currentBlock);

	if (data[IDENTIFIER] == RED) {
		//Check for space
		if ( (int)data[RECORDS] != MAXRECORDS(metaData) ) {
			for (int i = 0; i < (int)data[RECORDS]; i++) {
				if ( less(value1, data[REDKEY(i)], metaData) ) {
					//Move all bigger keys-values to the right
					memcpy( &data[REDKEY(i + 1)], &data[REDKEY(i)], RECORDSIZE(metaData) * ((int)data[RECORDS] - i) );
					//Push new record
					memcpy( &data[REDKEY(i)], value1, (size_t)metaData[ATTRLENGTH1]);
					memcpy( &data[VALUE(i, metaData)], value2, (size_t)metaData[ATTRLENGTH2]);
					int records = (int)data[RECORDS] + 1;
					memcpy( &data[RECORDS], records, 4);
					break;
				}
			}
		}
		else {
			;//split
		}
	}
	else {
		int i;
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( less(value1, data[BLACKKEY(i, metaData)], metaData) ) {
				int nextPointer = (int)data[POINTER(i, metaData)];
				CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
				InsertRec( fileDesc, value1, value2, metaData, nextPointer );

				break;
			}
		}

		if ( i == (int)data[NUMKEYS] ) {
			int nextPointer = (int)data[POINTER(i, metaData)];
			CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
			InsertRec( fileDesc, value1, value2, metaData, (int)data[POINTER((int)data[NUMKEYS], metaData)] );
		}
	}
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) 
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);
	
	BF_Block *metaBlock;
	BF_Block_Init(&metaBlock);
	CALL_OR_EXIT( BF_GetBlock(fileDesc, 0, metaBlock) );

	char *metaData = BF_Block_GetData(metaBlock);

	int root = (int)metaData[ROOT];

  //First Insert//
	if(root == 0)
	{
		//Create new red block
		BF_Block *newRedBlock;
		BF_Block_Init(&newRedBlock);
		CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newRedBlock) );
		int redBlockCounter;
		CALL_OR_EXIT( BF_GetBlockCounter(fileDesc, &redBlockCounter) );
		CALL_OR_EXIT( BF_GetBlock(fileDesc, redBlockCounter, newRedBlock) );
		char *data = BF_Block_GetData(newRedBlock);

		data[IDENTIFIER] = RED;

		memcpy( &data[REDKEY(0)], value1, (size_t)metaData[ATTRLENGTH1] );
		memcpy( &data[VALUE(0, metaData)], value2, (size_t)metaData[ATTRLENGTH2] );
		int records = 1;
		memcpy( &data[RECORDS], &records , 4);


		//Create new black block
		BF_Block *newBlackBlock;
		BF_Block_Init(&newBlackBlock);
		CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newBlackBlock) );
		int blackBlockCounter;
		CALL_OR_EXIT( BF_GetBlockCounter(fileDesc, &blackBlockCounter) );
		CALL_OR_EXIT( BF_GetBlock(fileDesc, blackBlockCounter, newBlackBlock) );
		data = BF_Block_GetData(newBlackBlock);

		data[IDENTIFIER] = BLACK;

		memcpy( &metaData[ROOT], &blackBlockCounter, 4 );

		memcpy( &data[BLACKKEY(0, metaData)], value1, (size_t)metaData[ATTRLENGTH1] );
		memcpy( &data[POINTER(0, metaData)], &blackBlockCounter, 4 );

		return (AM_errno = AME_OK);
	}

	return AME_OK;
}

int AM_OpenIndexScan(int fileDesc, int op, void *value)
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	return AME_OK;
}

void *AM_FindNextEntry(int scanDesc)
{

}

int AM_CloseIndexScan(int scanDesc)
{
  return AME_OK;
}

static char * errorMessage[] =
{
	[AME_OK							* (-1)]	"<Message>: Successful operation",
	[AME_EOF						* (-1)]	"<Message>: End of file reached",
	[AME_ERROR						* (-1)] "<Error>: Unexpected error occured",
	[AME_NOT_AM_FILE				* (-1)] "<Invalid Operation>: Attempting to open a file of unknown format",
	[AME_DESTROY_FAILED_REMOVE		* (-1)]	"<Invalid Operation>: Attempting to remove unexistent file",
	[AME_DESTROY_FAILED_OPEN        * (-1)] "<Invalid Operation>: Attempting to destroy an open file",
	[AME_OPEN_FAILED				* (-1)] "<Error>: Open File limit reached",
	[AME_CLOSE_FAILED_SCANS			* (-1)]	"<Invalid Operation>: Attempting to close a file that is currently being scanned",
	[AME_CLOSE_FAILED_UNOPENED      * (-1)] "<Invalid Operation>: Attempting to close unopened file",
	[AME_INSERT_FAILED				* (-1)] "<Error>: Failed to insert new entry"
};

void AM_PrintError(char *errString)
{
	fprintf(stderr, "%s%s\n", errString, errorMessage[AM_errno * (-1)]);
}

void AM_Close()
{
	for (unsigned i = 0; i < MAXOPENFILES; i++)
		if (fileTable[i].fileName != NULL)
			free(fileTable[i].fileName);

	for (unsigned i = 0; i < MAXSCANS; i++)
		if (scanTable[i].recordKey != NULL)
			free(scanTable[i].recordKey);

	CALL_OR_EXIT(BF_Close());
}
