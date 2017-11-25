#include "AM.h"
#include "bf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int AM_errno = AME_OK;

fileData fileTable[MAXOPENFILES];
scanData scanTable[MAXSCANS];

#define INDEX ('i')
#define BLACK ('b')
#define RED   ('r')

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
	// | BYTES ||     1       |      1      |      4        |      1      |      4        |   12   | 1 - (512-23)  | //
	// | VARS  || identifier  |  attrType1  |  attrLength1  |  attrType2  |  attrLength2  |  root  |   fileName    | //
	// +-------++-------------+-------------+---------------+-------------+---------------+--------+---------------+ //
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	data[0] = INDEX; 

	memcpy(&data[1], &attrType1, 1);

	char intToStr[4];
	sprintf(intToStr, "%d", attrLength1);
	memcpy(&data[1 + 1], intToStr, sizeof(intToStr));

	memcpy(&data[1 + 1 + sizeof(intToStr)], &attrType2, 1);

	sprintf(intToStr, "%d", attrLength2);
	memcpy(&data[1 + 1 + sizeof(intToStr) + 1], intToStr, sizeof(intToStr));

	char rootStr[12];
	sprintf(rootStr, "%d", 0);
	memcpy(&data[1 + 1 + sizeof(intToStr) + 1 + sizeof(intToStr)], rootStr, sizeof(rootStr));

	memcpy(&data[1 + 1 + sizeof(intToStr) + 1 + sizeof(intToStr) + sizeof(rootStr)], fileName, strlen(fileName) + 1);

	printf("id = %c\n", data[0]);
	printf("attrType1 = %c\n", data[1]);
	printf("attrLength1 = %s\n", &data[2]);
	printf("attrType2 = %c\n", data[6]);
	printf("attrLength2 = %s\n", &data[7]);
	printf("root at: %s\n", &data[11]);
	printf("fileName = %s\n\n", &data[23]);

	return AME_OK;
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
			return (AM_errno = AME_ERROR);

		CALL_OR_EXIT(BF_CloseFile(fileDesc));

		return (AM_errno = (!remove(fileName) ? AME_OK : AME_ERROR));
	}

	return (AM_errno = AME_ERROR);
}

int AM_OpenIndex (char *fileName)
{
	int fileDesc;
	CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));

	if (!isAM(fileDesc))
		return (AM_errno = AME_ERROR);

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

	return (i < MAXOPENFILES ? i : (AM_errno = AME_ERROR));
}

int AM_CloseIndex (int fileDesc)
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_ERROR);

	int i;
	for (i = 0; i < MAXSCANS; i++)
		if (fileDesc == scanTable[i].fileDesc)
			return (AM_errno = AME_ERROR);

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

	return (AM_errno = (i > MAXOPENFILES ? AME_ERROR : AME_OK));
}

int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
	if (!isAM(fileDesc))
		return (AM_errno = AME_ERROR);

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
	if (!isAM(fileDesc))
		return (AM_errno = AME_ERROR);

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
		if (fileTable[i].fileName != NULL)
			free(fileTable[i].fileName);

	for (unsigned i = 0; i < MAXSCANS; i++)
		if (scanTable[i].recordKey != NULL)
			free(scanTable[i].recordKey);

	CALL_OR_EXIT(BF_Close());
}
