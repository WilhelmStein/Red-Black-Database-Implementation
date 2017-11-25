#include "AM.h"
#include "bf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int AM_errno = AME_OK;

fData fTable[MAXOPENFILES];

#define INDEX ('I')
#define BLACK ('B')
#define RED   ('R')

#define IDENTIFIER  (data[0])  //char
#define ATTRTYPE1	(data[1])  //char	
#define ATTRLENGTH1 (data[2])  //int
#define ATTRTYPE2	(data[3])  //char
#define ATTRLENGTH2 (data[4])  //int
#define ROOT		(data[5])  //int
#define FILENAME	(&data[9]) //char*
 

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
		fTable[i].fileDesc = UNDEFINED;
		fTable[i].fileName = NULL;
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

	IDENTIFIER = INDEX; 

	memcpy(&ATTRTYPE1, &attrType1, 1);

	memcpy(&ATTRLENGTH1, &attrLength1, 1);

	memcpy(&ATTRTYPE2, &attrType2, 1);

	memcpy(&ATTRLENGTH2, &attrLength2, 1);

	int zero = 0;
	memcpy(&ROOT, &zero, 4);

	memcpy(FILENAME, fileName, strlen(fileName) + 1);

	printf("id = %c\n", IDENTIFIER);
	printf("attrType1 = %c\n", ATTRTYPE1);
	printf("attrLength1 = %d\n", ATTRLENGTH1);
	printf("attrType2 = %c\n", ATTRTYPE2);
	printf("attrLength2 = %d\n", ATTRLENGTH2);
	printf("root at: %d\n", ROOT);
	printf("fileName = %s\n\n", FILENAME);

	CALL_OR_EXIT(BF_UnpinBlock(block));
	CALL_OR_EXIT(BF_CloseFile(fileDesc));

	return AME_OK;
}

int AM_DestroyIndex(char *fileName)
{
	bool isOpen = false;
	for (unsigned i = 0; i < MAXOPENFILES; i++)
	{
		if (fTable[i].fileDesc != UNDEFINED && !strcmp(fileName, fTable[i].fileName))
		{
			isOpen = true;
			break;
		}
	}

	if (!isOpen)
	{
		return (AM_errno = (!remove(fileName) ? AME_OK : AME_ERROR));
	}

	return (AM_errno = AME_ERROR);
}

int AM_OpenIndex (char *fileName)
{
	int fileDesc;
	CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));
	
	unsigned i;
	for (i = 0; i < MAXOPENFILES; i++)
	{
		if (fTable[i].fileDesc == UNDEFINED)
		{
			fTable[i].fileDesc = fileDesc;
			fTable[i].fileName = (char *) malloc((strlen(fileName) + 1) * sizeof(char));
			strcpy(fTable[i].fileName, fileName);
			break;
		}
	}

	return (i < MAXOPENFILES ? i : (AM_errno = AME_ERROR));
}

int AM_CloseIndex (int fileDesc)
{
	int i;
	for (i = 0; i < MAXOPENFILES; i++)
	{
		if (fileDesc == fTable[i].fileDesc)
		{
			CALL_OR_EXIT(BF_CloseFile(fTable[i].fileDesc));

			free(fTable[i].fileName);
			fTable[i].fileName = NULL;
			fTable[i].fileDesc = UNDEFINED;
			
			break;
		}
	}

	// Check for scans !!!

	return (AM_errno = (i > MAXOPENFILES ? AME_ERROR : AME_OK));
}

int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
	BF_Block *metaBlock;
	BF_Block_Init(&metaBlock);
	CALL_OR_EXIT( BF_GetBlock(fileDesc, 0, metaBlock) );

	char *metaContents = BF_Block_GetData(metaBlock);
	char rootStr[12];
	memcpy(rootStr, &metaContents[11], 12);

	int rootInt = atoi(rootStr);
	if(rootInt == 0)
	{

		BF_Block *newBlock;
		BF_Block_Init(&newBlock);
		CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newBlock) );
		char *data = BF_Block_GetData(newBlock);

		IDENTIFIER = RED;

		int blockCount;
		CALL_OR_EXIT( BF_GetBlockCounter(fileDesc, &blockCount) );
		char intToStr[12];
		sprintf(intToStr, "%d", blockCount);
		memcpy(&metaContents[11], intToStr, 12);
	}
	return AME_OK;
}

int AM_OpenIndexScan(int fileDesc, int op, void *value)
{
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
	[AME_OK		* (-1)]	"Success...\n",
	[AME_EOF	* (-1)]	"Reached end of file...\n",
	[AME_ERROR	* (-1)] "General error message...\n"
};

void AM_PrintError(char *errString)
{
	printf("%s%s", errString, errorMessage[AM_errno * (-1)]);
}

void AM_Close()
{
	for (unsigned i = 0; i < MAXOPENFILES; i++)
	{
		if (fTable[i].fileName != NULL)
		{
			free(fTable[i].fileName);
		}
	}

	CALL_OR_EXIT(BF_Close());
}
