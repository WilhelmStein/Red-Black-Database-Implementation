#include "AM.h"
#include "bf.h"
#include "defn.h"
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

#define MAXKEYS(metaData)      ( (BF_BLOCK_SIZE - 9) / ((int)metaData[ATTRLENGTH1] + 4) )
#define KEYPOINTERSIZE(metaData)  ( (int)metaData[ATTRLENGTH1] + 4)

//RED BLOCKS
//      IDENTIFIER   (0)  //char
#define RECORDS      (1)  //int
#define NEXT         (5)  //int
#define REDKEY(x, metaData)    ( /*FIRSTKEY*/9 + ((x) * RECORDSIZE(metaData)) )
#define VALUE(x, metaData)     ( /*FIRSTVALUE*/9 + (int)metaData[ATTRLENGTH1] + ((x) * RECORDSIZE(metaData)) )

//BLACK BLOCKS
//      IDENTIFIER   (0) //char
#define NUMKEYS      (1) //int
//      NEXT         (5) //int
#define FIRST        (9) //int
#define BLACKKEY(x, metaData)  ( /*FIRSTKEY*/13 + (x) * ( 4 + (int)metaData[ATTRLENGTH1] ) )
#define POINTER(x, metaData)   ( /*FIRSTPOINTER*/9 + (x) * ( 4 + (int)metaData[ATTRLENGTH1] ) )

//META BLOCK
#define IDENTIFIER   (0)  //char
#define ATTRTYPE1	 (1)  //char	
#define ATTRLENGTH1  (2)  //int
#define ATTRTYPE2	 (3)  //char
#define ATTRLENGTH2  (4)  //int
#define ROOT		 (5)  //int
#define FILENAME	 (9)  //char*
 
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

static bool equal(void *value1, void *value2, char attrType1) 
{
	switch(attrType1) {
		case 'i':
			return *(int *)value1 == *(int *)value2;
		case 'f':
			return *(float *)value1 == *(float *)value2;
		case 'c':
			return !strcmp( (char *)value1, (char *)value2 );
		default: break;
	}
}

static bool compare(void *valueA, void *valueB, int op, char type)
{
	switch (op)
	{
		case EQUAL:
			return equal(valueA, valueB, type);
		case NOT_EQUAL:
			return !equal(valueA, valueB, type);
		case LESS_THAN:
			return less(valueA, valueB, type);
		case GREATER_THAN:
			return (!less(valueA, valueB, type) && !equal(valueA, valueB, type));
		case LESS_THAN_OR_EQUAL:
			return (less(valueA, valueB, type) || equal(valueA, valueB, type));
		case GREATER_THAN_OR_EQUAL:
			return !less(valueA, valueB, type);
	}
}

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
	unsigned i;
	for (i = 0; i < MAXOPENFILES; i++)
	{
		fileTable[i].fileDesc = UNDEFINED;
		fileTable[i].fileName = NULL;
	}

	for (i = 0; i < MAXSCANS; i++)
	{
		scanTable[i].fileDesc    = UNDEFINED;
		scanTable[i].blockIndex  = UNDEFINED;
		scanTable[i].recordIndex = UNDEFINED;
		scanTable[i].value       = NULL;
	}

	CALL_OR_EXIT(BF_Init(LRU));
}

int AM_CreateIndex(char *fileName, 
				   char attrType1, 
				   int attrLength1, 
				   char attrType2, 
				   int attrLength2) 
{

	if ( (attrType1 == FLOAT || attrType1 == INTEGER)  && attrLength1 != 4 ) return AM_errno = AME_ERROR;
	if ( (attrType2 == FLOAT || attrType2 == INTEGER)  && attrLength2 != 4 ) return AM_errno = AME_ERROR;

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

	int undefined = UNDEFINED;
	memcpy(&data[ROOT], &undefined, 4);

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
	BF_Block_Destroy(&block);
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
	bool flag = (data[IDENTIFIER] == INDEX);

	CALL_OR_EXIT(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

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

	return (i < MAXOPENFILES ? i : (AM_errno = AME_OPEN_FAILED_LIMIT));
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

static void printBlack(const int i, char * const blackData, char * const metaData)
{
	switch(metaData[(int)ATTRTYPE1])
	{
		case 'i':
			printf("  %d  |%d|", *(int *)&blackData[(int)BLACKKEY(i, metaData)], (int)blackData[(int)POINTER(i + 1, metaData)]);
			break;
		case 'f':
			printf("  %.2f  |%d|", *(float *)&blackData[(int)BLACKKEY(i, metaData)], (int)blackData[(int)POINTER(i + 1, metaData)]);
			break;
		case 'c':
			printf("  %s  |%d|", &blackData[(int)BLACKKEY(i, metaData)], (int)blackData[(int)POINTER(i + 1, metaData)]);
			break;
	}
}

static void printRed(const int i, char * const redData, char * const metaData)
{
	switch(metaData[(int)ATTRTYPE1])
	{
		case 'i':
			printf("%d:", *(int *)&redData[(int)REDKEY(i, metaData)]);
			break;
		case 'f':
			printf("%.2f:", *(float *)&redData[(int)REDKEY(i, metaData)]);
			break;
		case 'c':
			printf("%s: ", &redData[(int)REDKEY(i, metaData)]);
			break;
	}

	switch(metaData[(int)ATTRTYPE2])
	{
		case 'i':
			printf("%d| ", *(int *)&redData[(int)VALUE(i, metaData)]);
			break;
		case 'f':
			printf("%.2f| ", *(float *)&redData[(int)VALUE(i, metaData)]);
			break;
		case 'c':
			printf("%s| ", &redData[(int)VALUE(i, metaData)]);
			break;
	}
}

static void printRec(const int fd, const int blockIndex, char * const metaData)
{
	printf("\n");
	BF_Block *parentBlock;
	BF_Block_Init(&parentBlock);
	CALL_OR_EXIT( BF_GetBlock(fd, blockIndex, parentBlock) );
	char *parentData = BF_Block_GetData(parentBlock);

	if(parentData[IDENTIFIER] == BLACK)
	{
		printf("Block Id: %d\nType: Black\nNext: %d\nContents: |%d|", blockIndex, (int)parentData[NEXT], (int)parentData[(int)POINTER(0, metaData)]);
		for(int i = 0; i < parentData[NUMKEYS]; i++)
			printBlack(i, parentData, metaData);
		printf("\n");
		for(int i = 0; i <= (int)parentData[NUMKEYS]; i++) {
			int child = (int)parentData[(int)POINTER(i, metaData)];
			CALL_OR_EXIT( BF_UnpinBlock(parentBlock) );
			CALL_OR_EXIT( BF_UnpinBlock(parentBlock) );
			printRec(fd, child, metaData);
			CALL_OR_EXIT( BF_GetBlock(fd, blockIndex, parentBlock) );
		}
	}
	else
	{
		printf("Block Id: %d\nType: Red\nNext: %d\nContents: ", blockIndex, (int)parentData[NEXT]);
		for(int i = 0; i < parentData[RECORDS]; i++)
			printRed(i, parentData, metaData);
		printf("\n");
	}
	CALL_OR_EXIT( BF_UnpinBlock(parentBlock) );
	BF_Block_Destroy(&parentBlock);
}

static void debugPrint(const int fd)
{
	static unsigned iteration = 1;

	printf("\n<ITERATION> %u\n", iteration++);
	BF_Block *metaBlock;
	BF_Block_Init(&metaBlock);
	CALL_OR_EXIT( BF_GetBlock(fd, 0, metaBlock) );
	char *metaData = BF_Block_GetData(metaBlock);
	int root = (int)metaData[ROOT];
	printRec(fd, root, metaData);
	CALL_OR_EXIT( BF_UnpinBlock(metaBlock) );
	BF_Block_Destroy(&metaBlock);
}
  
static void *SplitBlack(int fileDesc, int target, void *key, char *metaData) 
{
	BF_Block *targetBlock;
	BF_Block_Init(&targetBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, target, targetBlock));
	char *data = BF_Block_GetData(targetBlock);

	//Make an array of all the keys in targetBlock + key so we can sort them
	char *tempArray = malloc( (int)metaData[ATTRLENGTH1] * ( (int)data[NUMKEYS] + 1) );
	memcpy( &tempArray[0], key, (size_t)metaData[ATTRLENGTH1] );
	for (int i = 1; i < ( (int)data[NUMKEYS] + 1); i++) {
		memcpy( &tempArray[i * (int)metaData[ATTRLENGTH1]], &data[BLACKKEY(i - 1, metaData)], (size_t)metaData[ATTRLENGTH1] );
	}
	//sort the array (its already sorted except first index)
	for (int i = 0; i < (int)metaData[ATTRLENGTH1] * (data[NUMKEYS] + 1); i += (int)metaData[ATTRLENGTH1]) {
		if ( !less(&tempArray[i], &tempArray[i + (int)metaData[ATTRLENGTH1]], (int)metaData[ATTRTYPE1]) ) {
			char *temp = malloc( (size_t)metaData[ATTRLENGTH1] );
			memcpy( temp, &tempArray[i], (size_t)metaData[ATTRLENGTH1] );
			memcpy( &tempArray[i], &tempArray[i + (int)metaData[ATTRLENGTH1]], (size_t)metaData[ATTRLENGTH1] );
			memcpy( &tempArray[i + (int)metaData[ATTRLENGTH1]], temp, (size_t)metaData[ATTRLENGTH1] );
			free(temp); 
		}
		else break;
	}
	//find middle index
	int middleIndex = ( (((int)data[NUMKEYS] + 1) % 2) ? (((int)data[NUMKEYS] + 1) / 2) : ((((int)data[NUMKEYS] + 1) / 2) - 1) ); 
	void *middleValue = malloc((size_t)metaData[ATTRLENGTH1]);
	memcpy( middleValue, &tempArray[middleIndex * (int)metaData[ATTRLENGTH1]], (size_t)metaData[ATTRLENGTH1] );
	free(tempArray);

	//create new block and split
	BF_Block *newBlock;
	BF_Block_Init(&newBlock);
	CALL_OR_EXIT(BF_AllocateBlock(fileDesc, newBlock));      
	char *newData = BF_Block_GetData(newBlock);

	newData[IDENTIFIER] = BLACK;
	//WATCH IT HERE
	int newKeys = ( ((int)(data[NUMKEYS]) % 2) ? (((int)data[NUMKEYS] / 2) + 1) : ((int)data[NUMKEYS] / 2));
	memcpy( &newData[NUMKEYS], &newKeys, 4);
	int oldKeys = (int)data[NUMKEYS] / 2;

	//if middle value is the new key we are trying to push
	if ( equal(middleValue, key, metaData[ATTRTYPE1]) ) {
		memcpy( &newData[BLACKKEY(0, metaData)], &data[BLACKKEY(oldKeys, metaData)], newKeys * KEYPOINTERSIZE(metaData) );

		int prevCounter;
		CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &prevCounter));
		prevCounter -= 2;
		memcpy( &newData[FIRST], &prevCounter, 4);
	}
	//if key is greater than middleValue push it in new block
	else if ( !less(key, middleValue, metaData[ATTRTYPE1]) ) {
		int i;
		//find the index of the middle value. it will be i
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( equal(middleValue, &data[BLACKKEY(i, metaData)], metaData[ATTRTYPE1]) )
				break;
		}
		//Copy to new block all data AFTER middle value (i + 1) 
		//WATCH THE -i
		memcpy( &newData[BLACKKEY(0, metaData)], &data[BLACKKEY(i + 1, metaData)], KEYPOINTERSIZE(metaData) * ((int)data[NUMKEYS] - i) );
		for (int j = 0; j < (int)newData[NUMKEYS]; j++) {
			//Push new key into correct place in NEW Block
			if ( less(key, &newData[BLACKKEY(j, metaData)], metaData[ATTRTYPE1]) ) {
				memcpy( &newData[BLACKKEY(j + 1, metaData)], &newData[BLACKKEY(j, metaData)], KEYPOINTERSIZE(metaData) * ((int)newData[NUMKEYS] - (j + 1)) );
				memcpy( &newData[BLACKKEY(j, metaData)], key, (size_t)metaData[ATTRLENGTH1] );
				
				int prevCounter;
				CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &prevCounter));
				prevCounter -= 2;
				//Pointer right of new key is new block
				memcpy( &newData[POINTER(j, metaData)], &prevCounter, 4);
				//First pointer of block is the right pointer of middleValue
				memcpy( &newData[FIRST], &data[POINTER(i, metaData)], 4);
				break;
			}
		}
	}
	//if key is less than middleValue push it new old block
	else {
		int i;
		//find the index of the middle value. it will be i
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( equal(middleValue, &data[BLACKKEY(i, metaData)], metaData[ATTRTYPE1]) )
				break;
		}
		//Copy to new block all data AFTER middle value (i + 1)
		//WATCH THE -i
		memcpy( &newData[BLACKKEY(0, metaData)], &data[BLACKKEY(i + 1, metaData)], KEYPOINTERSIZE(metaData) * ((int)data[NUMKEYS] - i) );
		//First pointer of block is the right pointer of middleValue
		memcpy( &newData[FIRST], &data[POINTER(i, metaData)], 4);
		for (int j = 0; j < (int)newData[NUMKEYS]; j++) {
			//Push new key into correct place in OLD block
			if ( less(key, &data[BLACKKEY(j, metaData)], metaData[ATTRTYPE1]) ) {
				memcpy( &data[BLACKKEY(j + 1, metaData)], &data[BLACKKEY(j, metaData)], KEYPOINTERSIZE(metaData) * ((int)data[NUMKEYS] - (j + 1)) );
				memcpy ( &newData[BLACKKEY(j, metaData)], key, (size_t)metaData[ATTRLENGTH1] );

				int prevCounter;
				CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &prevCounter));
				prevCounter -= 2;
				//Pointer right of new key is new block
				memcpy( &data[POINTER(j, metaData)], &prevCounter, 4);
				break;
			}
		}
	}

	memcpy( &data[NUMKEYS], &oldKeys, 4);

	memcpy( &newData[NEXT], &data[NEXT], 4);

	int newBlockCounter;
	CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlockCounter));
	newBlockCounter--;
	memcpy( &data[NEXT], &newBlockCounter, 4);

	BF_Block_SetDirty(targetBlock);
	CALL_OR_EXIT(BF_UnpinBlock(targetBlock));
	BF_Block_Destroy(&targetBlock);

	BF_Block_SetDirty(newBlock);
	CALL_OR_EXIT(BF_UnpinBlock(newBlock));
	BF_Block_Destroy(&newBlock);

	return middleValue;
}

static void *SplitRed(int fileDesc, int target, void *value1, void *value2, char *metaData)  
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

	int oldRecords = (int)data[RECORDS] / 2; 

	char *copyData;
	int newRecords, offset;

	if ( compare(value1, &data[REDKEY(oldRecords - 1, metaData)], LESS_THAN_OR_EQUAL, metaData[ATTRTYPE1]) ) {
		copyData = data;
		//needs to go in old block
		//split happens in oldRecords - 1
		newRecords = ( (int)data[RECORDS] % 2 ?  ((int)data[RECORDS] - oldRecords) : ((int)data[RECORDS] - oldRecords + 1) );
		offset = -1;
		memcpy( &newData[RECORDS], &newRecords, 4);
		int temp = oldRecords - 1;
		memcpy( &data[RECORDS], &temp, 4);
	}
	else {
		copyData = newData;
		//needs to go on new block
		//split happens in oldRecords
		newRecords = (int)data[RECORDS] - oldRecords - 1;
		offset = 0;
		int temp = oldRecords + 1;
		memcpy( &data[RECORDS], &temp, 4);
		memcpy( &newData[RECORDS], &newRecords, 4);
	}
	memcpy( &newData[REDKEY(0, metaData)], &data[REDKEY(oldRecords + offset, metaData)], RECORDSIZE(metaData) * newRecords);

	int i;
	for (i = 0; i < (int)copyData[RECORDS]; i++) {
		if (less( value1, &copyData[REDKEY(i, metaData)], (int)metaData[ATTRTYPE1]) ) {
			//Move all bigger keys-values to the right
			memcpy( &copyData[REDKEY(i + 1, metaData)], &copyData[REDKEY(i, metaData)], RECORDSIZE(metaData) *  ((int)copyData[RECORDS] - i));
			//Push new record
			memcpy( &copyData[REDKEY(i, metaData)], value1, (size_t)metaData[ATTRLENGTH1] );
			memcpy( &copyData[VALUE(i, metaData)], value2, (size_t)metaData[ATTRLENGTH2] );
			break;
		}
	}
	if (i == (int)copyData[RECORDS]) {
		memcpy( &copyData[REDKEY(i, metaData)], value1, (size_t)metaData[ATTRLENGTH1] );
		memcpy( &copyData[VALUE(i, metaData)], value2, (size_t)metaData[ATTRLENGTH2] );
	}

	int copyRecords = (int)copyData[RECORDS] + 1;
	memcpy( &copyData[RECORDS], &copyRecords, 4);

	memcpy( &newData[NEXT], &data[NEXT], 4);

	int newBlockCounter;
	CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlockCounter));
	newBlockCounter--;
	memcpy( &data[NEXT], &newBlockCounter, 4);

	void *retVal = malloc( (size_t)metaData[ATTRLENGTH1] );
	memcpy(retVal, &newData[REDKEY(0, metaData)], (size_t)metaData[ATTRLENGTH1] );

	BF_Block_SetDirty(targetBlock);
	CALL_OR_EXIT(BF_UnpinBlock(targetBlock));
	BF_Block_Destroy(&targetBlock);

	BF_Block_SetDirty(newBlock);
	CALL_OR_EXIT(BF_UnpinBlock(newBlock));
	BF_Block_Destroy(&newBlock);

	return retVal;
}

void *newKey;

static int InsertRec(int fileDesc, void *value1, void *value2, char *metaData, int root) 
{

	printf("ROOT : %d\n", root);

	BF_Block *currentBlock;
	BF_Block_Init(&currentBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, root, currentBlock));
	char *data = BF_Block_GetData(currentBlock);

	if (data[IDENTIFIER] == RED) {
		//Check for space
		if ( (int)data[RECORDS] != MAXRECORDS(metaData) ) {
			int i;
			for (i = 0; i < (int)data[RECORDS]; i++) {
				if ( less(value1, &data[REDKEY(i, metaData)], (int)metaData[ATTRTYPE1]) ) {
					//Move all bigger keys-values to the right
					memcpy( &data[REDKEY(i + 1, metaData)], &data[REDKEY(i, metaData)], RECORDSIZE(metaData) * ((int)data[RECORDS] - i) );
					//Push new record
					memcpy( &data[REDKEY(i, metaData)], value1, (size_t)metaData[ATTRLENGTH1] );
					memcpy( &data[VALUE(i, metaData)], value2, (size_t)metaData[ATTRLENGTH2] );
					int records = (int)data[RECORDS] + 1;
					memcpy( &data[RECORDS], &records, 4 );
					int key = UNDEFINED;
					memcpy( newKey, &key, 4 );
					break;
				}
			}
			//if value is bigger than all other values in red block push it after all values
			if ( i == (int)data[RECORDS]) {
				memcpy( &data[REDKEY(i, metaData)], value1, (size_t)metaData[ATTRLENGTH1] );
				memcpy( &data[VALUE(i, metaData)], value2, (size_t)metaData[ATTRLENGTH2] );
				int records = (int)data[RECORDS] + 1;
				memcpy( &data[RECORDS], &records, 4) ;
				int key = UNDEFINED;
				memcpy( newKey, &key, 4 );
			}

			BF_Block_SetDirty(currentBlock);
			CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
			BF_Block_Destroy(&currentBlock);

			return 0;
		}
		else {
			printf("root before splitred: %d\n", root);
			void *key = SplitRed(fileDesc, root, value1, value2, metaData);
			memcpy( newKey, key, (int)metaData[ATTRLENGTH1] );
			free(key);
			//printf("splitred key: %d\n", *(int *)newKey);

			BF_Block_SetDirty(currentBlock);
			CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
			BF_Block_Destroy(&currentBlock);

			return 0;
		}
	}
	else {
		int i;
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( less(value1, &data[BLACKKEY(i, metaData)], (int)metaData[ATTRTYPE1]) ) {
				int nextPointer = (int)data[POINTER(i, metaData)];

				CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
				BF_Block_Destroy(&currentBlock);

				InsertRec( fileDesc, value1, value2, metaData, nextPointer );
				//recursion returns here
				if (*(int *)newKey != UNDEFINED) {
					//check for space
					if ( (int)data[NUMKEYS] != MAXKEYS(metaData) ) {
						int j;
						for (j = 0; j < (int)data[NUMKEYS]; j++ ) {
							if ( less(newKey, &data[BLACKKEY(j, metaData)], (int)metaData[ATTRTYPE1]) ) {
								//Move all bigger keys-pointers to the right
								memcpy( &data[BLACKKEY(j + 1, metaData)], &data[BLACKKEY(j, metaData)], KEYPOINTERSIZE(metaData) * ((int)data[NUMKEYS] - j) );
								//Push new key
								memcpy( &data[BLACKKEY(j, metaData)], newKey, (size_t)metaData[ATTRLENGTH1] );
								int newBlackBlock;
								CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlackBlock));
								newBlackBlock--;
								memcpy( &data[POINTER(j + 1, metaData)], &newBlackBlock, 4);
								int numkeys = (int)data[NUMKEYS] + 1;
								memcpy( &data[NUMKEYS], &numkeys, 4);
								int key = UNDEFINED;
								memcpy( newKey, &key, 4 );
								return 0;
							}
						}
						//if here its bigger than all keys so push it at the end
						memcpy( &data[BLACKKEY(j, metaData)], newKey, (size_t)metaData[ATTRLENGTH1] );
						int newBlackBlock;
						CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlackBlock));
						newBlackBlock--;
						memcpy( &data[POINTER(j + 1, metaData)], &newBlackBlock, 4);
						int numkeys = (int)data[NUMKEYS] + 1;
						memcpy( &data[NUMKEYS], &numkeys, 4 );
						int key = UNDEFINED;
						memcpy( newKey, &key, 4 );
						return 0;
					}
					else {
						void *key = SplitBlack(fileDesc, root, newKey, metaData);
						memcpy( newKey, key, (int)metaData[ATTRLENGTH1] );
						free(key);
					}
				}
				else return 0;
			}
		}

		if ( i == (int)data[NUMKEYS] ) {
			int nextPointer = (int)data[POINTER(i, metaData)];

			CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
			BF_Block_Destroy(&currentBlock);

			InsertRec( fileDesc, value1, value2, metaData, (int)data[POINTER((int)data[NUMKEYS], metaData)] );
			//base returns here
			if (*(int *)newKey != UNDEFINED) {
					//check for space
					if ( (int)data[NUMKEYS] != MAXKEYS(metaData) ) {
						int j;
						for (j = 0; j < (int)data[NUMKEYS]; j++ ) {
							if ( less(newKey, &data[BLACKKEY(j, metaData)], (int)metaData[ATTRTYPE1]) ) {
								//Move all bigger keys-pointers to the right
								memcpy( &data[BLACKKEY(j + 1, metaData)], &data[BLACKKEY(j, metaData)], KEYPOINTERSIZE(metaData) * ((int)data[NUMKEYS] - j) );
								//Push new key
								memcpy( &data[BLACKKEY(j, metaData)], newKey, (size_t)metaData[ATTRLENGTH1] );
								int newBlackBlock;
								CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlackBlock));
								newBlackBlock--;
								memcpy( &data[POINTER(j + 1, metaData)], &newBlackBlock, 4);
								int numkeys = (int)data[NUMKEYS] + 1;
								memcpy( &data[NUMKEYS], &numkeys, 4);
								int key = UNDEFINED;
								memcpy( newKey, &key, 4 );
								return 0;
							}
						}
						//if here its bigger than all keys so push it at the end
						memcpy( &data[BLACKKEY(j, metaData)], newKey, (size_t)metaData[ATTRLENGTH1] );
						int newBlackBlock;
						CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlackBlock));
						newBlackBlock--;
						memcpy( &data[POINTER(j + 1, metaData)], &newBlackBlock, 4);
						int numkeys = (int)data[NUMKEYS] + 1;
						memcpy( &data[NUMKEYS], &numkeys, 4 );
						int key = UNDEFINED;
						memcpy( newKey, &key, 4 );
						return 0;
					}
					else {
						void *key = SplitBlack(fileDesc, root, newKey, metaData);
						memcpy( newKey, key, (int)metaData[ATTRLENGTH1] );
						free(key);
					}
				}
			else return 0;
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
	int allblocks;
	CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &allblocks));
	int root = (int)metaData[ROOT];

  //First Insert//
	if(root == UNDEFINED)
	{
		//Create new red block
		BF_Block *newRedBlock;
		BF_Block_Init(&newRedBlock);
		CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newRedBlock) );

		int redBlockCounter;
		CALL_OR_EXIT( BF_GetBlockCounter(fileDesc, &redBlockCounter) );
		redBlockCounter--;

		char *data = BF_Block_GetData(newRedBlock);

		data[IDENTIFIER] = RED;

		memcpy( &data[REDKEY(0, metaData)], value1, (size_t)metaData[ATTRLENGTH1] );
		memcpy( &data[VALUE(0, metaData)], value2, (size_t)metaData[ATTRLENGTH2] );
		int records = 1;
		memcpy( &data[RECORDS], &records , 4);

		memcpy( &metaData[ROOT], &redBlockCounter, 4);

		int undefined = UNDEFINED;
		memcpy( &data[NEXT], &undefined, 4);

		BF_Block_SetDirty(newRedBlock);
		CALL_OR_EXIT(BF_UnpinBlock(newRedBlock));
		BF_Block_Destroy(&newRedBlock);

		BF_Block_SetDirty(metaBlock);
		CALL_OR_EXIT(BF_UnpinBlock(metaBlock));
		BF_Block_Destroy(&metaBlock);

		return (AM_errno = AME_OK);
	}
	else {
		//debugPrint(fileDesc);

		newKey = malloc((size_t)metaData[ATTRLENGTH1]);
		InsertRec(fileDesc, value1, value2, metaData, (int)metaData[ROOT]);
		if (*(int *)newKey != UNDEFINED) {
			BF_Block *newRoot;
			BF_Block_Init(&newRoot);
			CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newRoot) );

			int counter;
			CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &counter) );


			char *data = BF_Block_GetData(newRoot);

			data[IDENTIFIER] = BLACK;
			
			memcpy( &data[BLACKKEY(0, metaData)], newKey, (size_t)metaData[ATTRLENGTH1] );
			memcpy( &data[FIRST], &metaData[ROOT], 4);

			counter -= 2;
			memcpy( &data[POINTER(1, metaData)], &counter, 4);

			int one = 1;
			memcpy( &data[NUMKEYS], &one, 4 );

			int undefined = UNDEFINED;
			memcpy( &data[NEXT], &undefined, 4);

			counter++;
			memcpy(&metaData[ROOT], &counter, 4);

			BF_Block_SetDirty(newRoot);
			CALL_OR_EXIT(BF_UnpinBlock(newRoot));
			BF_Block_Destroy(&newRoot);

			BF_Block_SetDirty(metaBlock);
		}

		CALL_OR_EXIT(BF_UnpinBlock(metaBlock));
		BF_Block_Destroy(&metaBlock);

		debugPrint(fileDesc);
		return (AM_errno = AME_OK);
		
	}

	return (AM_errno = AME_ERROR);
}

static void search(int fileDesc/*, int attrLength1, char type*/, char * metaData, int root, void * value, int * const b, int * const r)
{
	BF_Block * block;

	BF_Block_Init(&block);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, root, block));

	char * data = BF_Block_GetData(block);

	if (data[IDENTIFIER] == BLACK)
	{
		unsigned i;
		for (i = 0; i < (int) data[NUMKEYS]; i++)
		{
			void * key = (void *) &(data[BLACKKEY(i, metaData)]);
			if(compare(value, key, LESS_THAN, (char) metaData[ATTRTYPE1]))//if(compare(value, key, LESS_THAN, type)
				break;
		}

		// CALL_OR_DIE(BF_UnpinBlock(block));
		// BF_Block_Destroy(&block);

		int _root = POINTER(i, metaData); //9 + i * ( 4 + attrLength1 );
		search(fileDesc, /*attrLength1, type*/metaData, _root, value, b, r);
	}
	else if (data[IDENTIFIER] == RED)
	{
		unsigned i;
		for (i = 0; i < (int) data[RECORDS]; i++)
		{
			void * key = (void *) &(data[REDKEY(i, metaData)]);
			if(compare(value, key, GREATER_THAN_OR_EQUAL, (char) metaData[ATTRTYPE1]))//if(compare(value, key, GREATER_THAN_OR_EQUAL, type)
				break;
		}
		
		// CALL_OR_DIE(BF_UnpinBlock(block));
		// BF_Block_Destroy(&block);

		*b = root;
		*r = i;
	}

	CALL_OR_EXIT(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
}

int AM_OpenIndexScan(int fileDesc, int op, void *value)
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	AM_errno = AME_OK;

	unsigned i;
	for (i = 0; i < MAXOPENFILES; i++)
		if (fileTable[i].fileDesc == fileDesc)
			break;

	if (i == MAXOPENFILES)
		return (AM_errno = AME_SCAN_FAILED_UNOPENED);

	for (i = 0; i < MAXSCANS; i++)
	{
		if (scanTable[i].fileDesc == UNDEFINED)
		{
			BF_Block * metaBlock;

			BF_Block_Init(&metaBlock);
			CALL_OR_EXIT(BF_GetBlock(fileDesc, 0, metaBlock));
			
			char * metaData = BF_Block_GetData(metaBlock);

			// const int attrLength1 = (int)  metaData[ATTRLENGTH1];
			// const int attrLength2 = (int)  metaData[ATTRLENGTH2];
			// const char type       = (char) metaData[IDENTIFIER];
			const int root        = (int)  metaData[ROOT];

			// CALL_OR_EXIT(BF_UnpinBlock(metaBlock));
			// BF_Block_Destroy(&metaBlock);

			int b, r;
			search(fileDesc, /*attrLength1, type*/metaData, root, value, &b, &r);

			scanTable[i].fileDesc = fileDesc;
			scanTable[i].op = op;
			if( ( op == NOT_EQUAL ) || (op == LESS_THAN) || (op == LESS_THAN_OR_EQUAL) )
			{
				scanTable[i].blockIndex = root;
				scanTable[i].recordIndex = 0;
			}
			else
			{
				scanTable[i].blockIndex = b;
				scanTable[i].recordIndex = r;
			}

			// Remember to free this shiet !!!
			scanTable[i].value = malloc((int) metaData[ATTRLENGTH1]);
			memcpy(scanTable[i].value, value, (int) metaData[ATTRLENGTH1]);
			scanTable[i].returnValue = malloc((int) metaData[ATTRLENGTH2]);

			CALL_OR_EXIT(BF_UnpinBlock(metaBlock));
			BF_Block_Destroy(&metaBlock);

			break;
		}
	}

	if (i == MAXSCANS)
		return (AM_errno = AME_SCAN_FAILED_LIMIT);

	return i;
}

void *AM_FindNextEntry(int scanDesc)
{
	//open metaData

	BF_Block *metaBlock;
	BF_Block_Init(&metaBlock);
	CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, 0, metaBlock) );
	char *metaData = BF_Block_GetData(metaBlock);
	//open last indexed block
	BF_Block *currentBlock;
	BF_Block_Init(&currentBlock);
	CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, scanTable[scanDesc].blockIndex, currentBlock) );
	char * currentData = BF_Block_GetData(currentBlock);

	bool found = false;
	int j = scanTable[scanDesc].blockIndex;
	while(true) {
		for(int i = scanTable[scanDesc].recordIndex; i < (int)currentData[RECORDS]; i++) {
			if(compare( (void *)currentData[(int)REDKEY(i ,metaData)], scanTable[scanDesc].value, scanTable[scanDesc].op, metaData[ATTRTYPE1]))
			{
				memcpy(&(scanTable[scanDesc].returnValue), &(currentData[(int)VALUE(i ,metaData)]), (int)metaData[ATTRLENGTH2]);
				( (i + 1) == (int)currentData[RECORDS] ) ? (scanTable[scanDesc].recordIndex = 0) : (scanTable[scanDesc].recordIndex = i + 1);
				found = true;
				break;
			}
		}
		if( found )
		{
			scanTable[scanDesc].blockIndex = j;
			break;
		}
		if ( ( j = (int)currentData[NEXT] ) != -1 )
		{
			CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
			CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, j, currentBlock) );
			currentData = BF_Block_GetData(currentBlock);
			break;
		}
	}
	if(!found)
	{
		AM_errno = AME_ERROR;
		scanTable[scanDesc].returnValue = NULL;
	}

	/*switch(scanTable[scanDesc].op)
	{
		case EQUAL:
			if( ( scanTable[scanDesc].recordIndex < currentData[RECORDS] ) && ( compare( (void *)currentData[(int)REDKEY( scanTable[scanDesc].recordIndex ,metaData)], scanTable[scanDesc].value, scanTable[scanDesc].op, metaData[ATTRTYPE1] ) ))
			{
				memcpy(&(scanTable[scanDesc].returnValue), &(currentData[(int)VALUE(scanTable[scanDesc].recordIndex ,metaData)]), (int)metaData[ATTRLENGTH2]);
				scanTable[scanDesc].recordIndex++;
				break;
			}
			AM_errno = AME_ERROR;
			scanTable[scanDesc].returnValue = NULL;
			break;
		case NOT_EQUAL:
			bool found = false;
			int j = scanTable[scanDesc].blockIndex;
			while(true) {
				for(int i = scanTable[scanDesc].recordIndex; i < (int)currentData[RECORDS]; i++) {
					if(compare( (void *)currentData[(int)REDKEY(i ,metaData)], scanTable[scanDesc].value, scanTable[scanDesc].op,metaData[ATTRTYPE1]))
					{
						memcpy(&(scanTable[scanDesc].returnValue), &(currentData[(int)VALUE(i ,metaData)]), (int)metaData[ATTRLENGTH2]);
						scanTable[scanDesc].recordIndex = i + 1;
						j = -1;
						found = true;
						break;
					}
				}
				if( j == -1 )//perhaps mistake
					break;
				if ( ( j = (int)currentData[NEXT] ) == -1 )
				{
					CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
					CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, j, currentBlock) );
					currentData = BF_Block_GetData(currentBlock);
				}
				else
					scanTable[scanDesc].recordIndex = 0;
			}
			if(!found)
			{
				AM_errno = AME_ERROR;
				scanTable[scanDesc].returnValue = NULL;
			}
			break;
		case LESS_THAN:
			bool found = false;
			int j = metaData[ROOT];
			while(true) {
				for(int i = scanTable[scanDesc].recordIndex; i < (int)currentData[RECORDS]; i++) {
					if(compare( (void *)currentData[(int)REDKEY(i ,metaData)], scanTable[scanDesc].value, scanTable[scanDesc].op, metaData[ATTRTYPE1]))
					{
						memcpy(&(scanTable[scanDesc].returnValue), &(currentData[(int)VALUE(i ,metaData)]), (int)metaData[ATTRLENGTH2]);
						scanTable[scanDesc].recordIndex = i + 1;
						j = -1;
						found = true;
						break;
					}
				}
				if( j == -1 )
					break;
				if ( ( j = (int)currentData[NEXT] ) == -1 )
				{
					CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
					CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, j, currentBlock) );
					currentData = BF_Block_GetData(currentBlock);
				}
				else
					scanTable[scanDesc].recordIndex = 0;
			}
			if(!found)
			{
				AM_errno = AME_ERROR;
				scanTable[scanDesc].returnValue = NULL;
			}
			break;
		case GREATER_THAN:
			bool found = false;
			int j = metaData[ROOT];
			while(true) {
				for(int i = scanTable[scanDesc].recordIndex; i < (int)currentData[RECORDS]; i++) {
					if(compare( (void *)currentData[(int)REDKEY(i ,metaData)], scanTable[scanDesc].value, scanTable[scanDesc].op, metaData[ATTRTYPE1]))
					{
						memcpy(&(scanTable[scanDesc].returnValue), &(currentData[(int)VALUE(i ,metaData)]), (int)metaData[ATTRLENGTH2]);
						scanTable[scanDesc].recordIndex = i + 1;
						j = -1;
						found = true;
						break;
					}
				}
				if( j == -1 )
					break;
				if ( ( j = (int)currentData[NEXT] ) == -1 )
				{
					CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
					CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, j, currentBlock) );
					currentData = BF_Block_GetData(currentBlock);
				}
				else
					scanTable[scanDesc].recordIndex = 0;
			}
			if(!found)
			{
				AM_errno = AME_ERROR;
				scanTable[scanDesc].returnValue = NULL;
			}
			break;
		case LESS_THAN_OR_EQUAL:
			bool found = false;
			int j = metaData[ROOT];
			while(true) {
				for(int i = scanTable[scanDesc].recordIndex; i < (int)currentData[RECORDS]; i++) {
					if(compare( (void *)currentData[(int)REDKEY(i ,metaData)], scanTable[scanDesc].value, scanTable[scanDesc].op, metaData[ATTRTYPE1]))
					{
						memcpy(&(scanTable[scanDesc].returnValue), &(currentData[(int)VALUE(i ,metaData)]), (int)metaData[ATTRLENGTH2]);
						scanTable[scanDesc].recordIndex = i + 1;
						j = -1;
						found = true;
						break;
					}
				}
				if( j == -1 )
					break;
				if ( ( j = (int)currentData[NEXT] ) == -1 )
				{
					CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
					CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, j, currentBlock) );
					currentData = BF_Block_GetData(currentBlock);
				}
				else
					scanTable[scanDesc].recordIndex = 0;
			}
			if(!found)
			{
				AM_errno = AME_ERROR;
				scanTable[scanDesc].returnValue = NULL;
			}
			break;
		case GREATER_THAN_OR_EQUAL:
	}*/
	//close last indexed block
	CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
	BF_Block_Destroy(&currentBlock);
	//close meta block
	CALL_OR_EXIT( BF_UnpinBlock(metaBlock) );
	BF_Block_Destroy(&metaBlock);
	return scanTable[scanDesc].returnValue;
}

int AM_CloseIndexScan(int scanDesc)
{
	if (scanDesc < 0 || scanDesc >= MAXSCANS)
		return (AM_errno = AME_CLOSE_SCAN_NON_EXISTENT);
	
	if (scanTable[scanDesc].fileDesc == UNDEFINED)
		return (AM_errno = AME_CLOSE_SCAN_NON_EXISTENT);

	//TODO: maybe not needed
	if (!isAM(scanTable[scanDesc].fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);
		
	scanTable[scanDesc].fileDesc    = UNDEFINED;
	scanTable[scanDesc].blockIndex  = UNDEFINED;
	scanTable[scanDesc].recordIndex = UNDEFINED;

	free(scanTable[scanDesc].value);
	scanTable[scanDesc].value       = NULL;

	return (AM_errno = AME_OK);
}

static char * errorMessage[] =
{
	[AME_OK                         * (-1)]	"<Message>: Successful operation",
	[AME_EOF                        * (-1)]	"<Message>: End of file reached",
	[AME_ERROR                      * (-1)] "<Error>: Unexpected error occured",
	[AME_NOT_AM_FILE                * (-1)] "<Invalid Operation>: Attempting to open a file of unknown format",
	[AME_DESTROY_FAILED_REMOVE      * (-1)]	"<Invalid Operation>: Attempting to remove unexistent file",
	[AME_DESTROY_FAILED_OPEN        * (-1)] "<Invalid Operation>: Attempting to destroy an open file",
	[AME_OPEN_FAILED_LIMIT          * (-1)] "<Error>: Open File limit reached",
	[AME_CLOSE_FAILED_SCANS         * (-1)]	"<Invalid Operation>: Attempting to close a file that is currently being scanned",
	[AME_CLOSE_FAILED_UNOPENED      * (-1)] "<Invalid Operation>: Attempting to close unopened file",
	[AME_INSERT_FAILED              * (-1)] "<Error>: Failed to insert new entry",
	[AME_SCAN_FAILED_LIMIT          * (-1)] "<Error>: Scan limit reached",
	[AME_SCAN_FAILED_UNOPENED       * (-1)] "<Invalid Operation>: Attempting to scan unopened file",
	[AME_CLOSE_SCAN_NON_EXISTENT    * (-1)] "<Invalid Operation>: Attempting to terminate non-existent scan",
	[AME_CREATE_FAILED              * (-1)] "<Error>: Creation requirements not met"
};

void AM_PrintError(char *errString)
{
	fprintf(stderr, "%s%s\n", errString, errorMessage[AM_errno * (-1)]);
}

void AM_Close()
{
	unsigned i;
	for (i = 0; i < MAXOPENFILES; i++)
		if (fileTable[i].fileName != NULL)
			free(fileTable[i].fileName);

	for (i = 0; i < MAXSCANS; i++)
		if (scanTable[i].value != NULL)
				free(scanTable[i].value);

	CALL_OR_EXIT(BF_Close());
}
