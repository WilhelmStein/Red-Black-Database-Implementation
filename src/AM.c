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

#define RECORDSIZE   ( attrLength1 + attrLength2 )
#define MAXRECORDS   ( (BF_BLOCK_SIZE - 9) /  RECORDSIZE )

#define KEYPOINTERSIZE  ( attrLength1 + 4)
#define MAXKEYS     ( (BF_BLOCK_SIZE - 9) / KEYPOINTERSIZE )

//RED BLOCKS
//      IDENTIFIER   (0)  //char
#define RECORDS      (1)  //int
#define NEXT         (5)  //int
#define REDKEY(x)    ( /*FIRSTKEY*/9 + ((x) * RECORDSIZE) )
#define VALUE(x)     ( /*FIRSTVALUE*/9 + attrLength1 + ((x) * RECORDSIZE) )

//BLACK BLOCKS
//      IDENTIFIER   (0) //char
#define NUMKEYS      (1) //int
//      NEXT         (5) //int
#define FIRST        (9) //int
#define BLACKKEY(x)  ( /*FIRSTKEY*/13 + (x) * ( 4 + attrLength1 ) )
#define POINTER(x)   ( /*FIRSTPOINTER*/9 + (x) * ( 4 + attrLength1 ) )

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

static char attrType1 = UNDEFINED;
static int attrLength1 = UNDEFINED;
static char attrType2 = UNDEFINED;
static int attrLength2 = UNDEFINED;
static int root = UNDEFINED;

static void setMetaData(int fileDesc) {
	BF_Block * metaBlock;
	BF_Block_Init(&metaBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, 0, metaBlock));
	char *data = BF_Block_GetData(metaBlock);

	attrType1 = data[ATTRTYPE1];
	attrLength1 = (int)data[ATTRLENGTH1];
	attrType2 = data[ATTRTYPE2];
	attrLength2 = (int)data[ATTRLENGTH2];
	root = (int)data[ROOT];

	CALL_OR_EXIT(BF_UnpinBlock(metaBlock));
	BF_Block_Destroy(&metaBlock);
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
	// Check if the file is currently open i.e. Can be found inside the fileTable struct
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
		// The file can be destroyed so attempt to delete it
		int fileDesc;
		CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));

		if (!isAM(fileDesc))
			return (AM_errno = AME_NOT_AM_FILE);

		CALL_OR_EXIT(BF_CloseFile(fileDesc));

		return (AM_errno = (!remove(fileName) ? AME_OK : AME_DESTROY_FAILED_REMOVE));
	}

	// The file has been opened and thus cannot be deleted
	// Set AM_errno appropriately
	return (AM_errno = AME_DESTROY_FAILED_OPEN);
}

int AM_OpenIndex (char *fileName)
{
	// Open the specified file and check whether or not it is actually an AM file
	int fileDesc;
	CALL_OR_EXIT(BF_OpenFile(fileName, &fileDesc));

	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	AM_errno = AME_OK;
	
	// Scan the fileTable struct for an unused cell
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

	// If an unused cell was found return its index otherwise
	// return an error code after appropriately setting the AM_errno
	return (i < MAXOPENFILES ? i : (AM_errno = AME_OPEN_FAILED_LIMIT));
}

int AM_CloseIndex (int fileDesc)
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	// Check if there are active scans on the file
	// In case there are the file cannot be closed
	// Return appropriate error code
	int i;
	for (i = 0; i < MAXSCANS; i++)
		if (fileDesc == scanTable[i].fileDesc)
			return (AM_errno = AME_CLOSE_FAILED_SCANS);

	// In case there are not active scans on the file
	// Search the fileTable for an entry corresponding to the file
	for (i = 0; i < MAXOPENFILES; i++)
	{
		if (fileDesc == fileTable[i].fileDesc)
		{
			// Found -> Erase the entry's contents and BF_CloseFile the file
			CALL_OR_EXIT(BF_CloseFile(fileTable[i].fileDesc));

			free(fileTable[i].fileName);
			fileTable[i].fileName = NULL;
			fileTable[i].fileDesc = UNDEFINED;
			
			break;
		}
	}

	// Set appropriately AM_errno and return it
	return (AM_errno = (i > MAXOPENFILES ? AME_CLOSE_FAILED_UNOPENED : AME_OK));
}

static void printBlack(const int i, char * const blackData, char * const metaData)
{
	switch(metaData[(int)ATTRTYPE1])
	{
		case 'i':
			printf("  %d  |%d|", *(int *)&blackData[(int)BLACKKEY(i)], (int)blackData[(int)POINTER(i + 1)]);
			break;
		case 'f':
			printf("  %.2f  |%d|", *(float *)&blackData[(int)BLACKKEY(i)], (int)blackData[(int)POINTER(i + 1)]);
			break;
		case 'c':
			printf("  %s  |%d|", &blackData[(int)BLACKKEY(i)], (int)blackData[(int)POINTER(i + 1)]);
			break;
	}
}

static void printRed(const int i, char * const redData, char * const metaData)
{
	switch(metaData[(int)ATTRTYPE1])
	{
		case 'i':
			printf("%d:", *(int *)&redData[(int)REDKEY(i)]);
			break;
		case 'f':
			printf("%.2f:", *(float *)&redData[(int)REDKEY(i)]);
			break;
		case 'c':
			printf("%s: ", &redData[(int)REDKEY(i)]);
			break;
	}

	switch(metaData[(int)ATTRTYPE2])
	{
		case 'i':
			printf("%d| ", *(int *)&redData[(int)VALUE(i)]);
			break;
		case 'f':
			printf("%.2f| ", *(float *)&redData[(int)VALUE(i)]);
			break;
		case 'c':
			printf("%s| ", &redData[(int)VALUE(i)]);
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
		printf("Block Id: %d\nType: Black\nNext: %d\nContents: |%d|", blockIndex, (int)parentData[NEXT], (int)parentData[(int)POINTER(0)]);
		for(int i = 0; i < parentData[NUMKEYS]; i++)
			printBlack(i, parentData, metaData);
		printf("\n");
		for(int i = 0; i <= (int)parentData[NUMKEYS]; i++) {
			int child = (int)parentData[(int)POINTER(i)];
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

void *newKey;

static void InsertInBlack(int fileDesc, char *data) 
{
	//This is the block that was created during previous split
	int newSplitBlock;
	CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newSplitBlock));
	newSplitBlock--;

	int i;
	for (i = 0; i < (int)data[NUMKEYS]; i++ ) {
		if ( less(newKey, &data[BLACKKEY(i)], attrType1) ) {
			//Move all bigger keys-pointers to the right
			memmove( &data[BLACKKEY(i + 1)], &data[BLACKKEY(i)], KEYPOINTERSIZE * ((int)data[NUMKEYS] - i) );
			break;
		}
	}

	//Push new key
	memcpy( &data[BLACKKEY(i)], newKey, attrLength1 );
	//New pointer is the newly split block
	memcpy( &data[POINTER(i + 1)], &newSplitBlock, 4);

	//Increase number of keys by 1
	int numkeys = (int)data[NUMKEYS] + 1;
	memcpy( &data[NUMKEYS], &numkeys, 4);

}

static void InsertInRed(int fileDesc, char *data, void *value1, void *value2) 
{
	int i;
	for (i = 0; i < (int)data[RECORDS]; i++) {
		if ( less(value1, &data[REDKEY(i)], attrType1) ) {
			//Move all bigger keys-values to the right
			memmove( &data[REDKEY(i + 1)], &data[REDKEY(i)], RECORDSIZE * ((int)data[RECORDS] - i) );
			break;
		}
	}

	//Push new record
	memcpy( &data[REDKEY(i)], value1, attrLength1 );
	memcpy( &data[VALUE(i)], value2, attrLength2 );

	//Increase number of records by 1
	int records = (int)data[RECORDS] + 1;
	memcpy( &data[RECORDS], &records, 4) ;

}

static void *SplitBlack(int fileDesc, int target, void *key) 
{
	BF_Block *targetBlock;
	BF_Block_Init(&targetBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, target, targetBlock));
	char *data = BF_Block_GetData(targetBlock);

	//Make an array of all the keys in targetBlock + key so we can sort them
	char *tempArray = malloc( attrLength1 * ( (int)data[NUMKEYS] + 1) );
	memcpy( &tempArray[0], key, attrLength1 );
	for (int i = 1; i < ( (int)data[NUMKEYS] + 1); i++) {
		memcpy( &tempArray[i * attrLength1], &data[BLACKKEY(i - 1)], attrLength1 );
	}
	//sort the array (its already sorted except first index)
	for (int i = 0; i < attrLength1 * (data[NUMKEYS] + 1); i += attrLength1) {
		if ( !less(&tempArray[i], &tempArray[i + attrLength1], attrType1) ) {
			char *temp = malloc( attrLength1 );
			memcpy( temp, &tempArray[i], attrLength1 );
			memmove( &tempArray[i], &tempArray[i + attrLength1], attrLength1 );
			memcpy( &tempArray[i + attrLength1], temp, attrLength1 );
			free(temp); 
		}
		else break;
	}
	//find middle index
	int middleIndex = ( (((int)data[NUMKEYS] + 1) % 2) ? (((int)data[NUMKEYS] + 1) / 2) : ((((int)data[NUMKEYS] + 1) / 2) - 1) ); 
	void *middleValue = malloc(attrLength1);
	memcpy( middleValue, &tempArray[middleIndex * attrLength1], attrLength1 );
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
	if ( equal(middleValue, key, attrType1) ) {
		memcpy( &newData[BLACKKEY(0)], &data[BLACKKEY(oldKeys)], newKeys * KEYPOINTERSIZE );

		int prevCounter;
		CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &prevCounter));
		prevCounter -= 2;
		memcpy( &newData[FIRST], &prevCounter, 4);
	}
	//if key is greater than middleValue push it in new block
	else if ( !less(key, middleValue, attrType1) ) {
		int i;
		//find the index of the middle value. it will be i
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( equal(middleValue, &data[BLACKKEY(i)], attrType1) )
				break;
		}
		//Copy to new block all data AFTER middle value (i + 1) 
		memcpy( &newData[BLACKKEY(0)], &data[BLACKKEY(i + 1)], KEYPOINTERSIZE * ((int)data[NUMKEYS] - i) );
		for (int j = 0; j < (int)newData[NUMKEYS]; j++) {
			//Push new key into correct place in NEW Block
			if ( less(key, &newData[BLACKKEY(j)], attrType1) ) {
				memmove( &newData[BLACKKEY(j + 1)], &newData[BLACKKEY(j)], KEYPOINTERSIZE * ((int)newData[NUMKEYS] - (j + 1)) );
				memcpy( &newData[BLACKKEY(j)], key, attrLength1 );
				
				int prevCounter;
				CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &prevCounter));
				prevCounter -= 2;
				//Pointer right of new key is new block
				memcpy( &newData[POINTER(j)], &prevCounter, 4);
				//First pointer of block is the right pointer of middleValue
				memcpy( &newData[FIRST], &data[POINTER(i)], 4);
				break;
			}
		}
	}
	//if key is less than middleValue push it new old block
	else {
		int i;
		//find the index of the middle value. it will be i
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( equal(middleValue, &data[BLACKKEY(i)], attrType1) )
				break;
		}
		//Copy to new block all data AFTER middle value (i + 1)
		//WATCH THE -i
		memcpy( &newData[BLACKKEY(0)], &data[BLACKKEY(i + 1)], KEYPOINTERSIZE * ((int)data[NUMKEYS] - i) );
		//First pointer of block is the right pointer of middleValue
		memcpy( &newData[FIRST], &data[POINTER(i)], 4);
		for (int j = 0; j < (int)newData[NUMKEYS]; j++) {
			//Push new key into correct place in OLD block

			if ( less(key, &data[BLACKKEY(j)], attrType1) ) {
				memmove( &data[BLACKKEY(j + 1)], &data[BLACKKEY(j)], KEYPOINTERSIZE * ((int)data[NUMKEYS] - (j + 1)) );
				memcpy ( &newData[BLACKKEY(j)], key, attrLength1 );

				int prevCounter;
				CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &prevCounter));
				prevCounter -= 2;
				//Pointer right of new key is new block
				memcpy( &data[POINTER(j)], &prevCounter, 4);
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

static void *SplitRed(int fileDesc, int target, void *value1, void *value2)  
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

	if ( compare(value1, &data[REDKEY(oldRecords - 1)], LESS_THAN_OR_EQUAL, attrType1) ) {
		copyData = data;
		//needs to go in old block
		//split happens in oldRecords - 1
		newRecords = ( (int)data[RECORDS] % 2 ?  ((int)data[RECORDS] - oldRecords) : ((int)data[RECORDS] - oldRecords + 1) );
		offset = 0;
		memcpy( &newData[RECORDS], &newRecords, 4);
		int temp = oldRecords;
		memcpy( &data[RECORDS], &temp, 4);
	}
	else {
		copyData = newData;
		//needs to go on new block
		//split happens in oldRecords
		newRecords = (int)data[RECORDS] - oldRecords - 1;
		offset = 1;
		int temp = oldRecords + 1;
		memcpy( &data[RECORDS], &temp, 4);
		memcpy( &newData[RECORDS], &newRecords, 4);
	}
	memcpy( &newData[REDKEY(0)], &data[REDKEY(oldRecords + offset)], RECORDSIZE * newRecords);

	int i;
	for (i = 0; i < (int)copyData[RECORDS]; i++) {
		if ( less(value1, &copyData[REDKEY(i)], attrType1) ) {
			//Move all bigger keys-values to the right
			memmove( &copyData[REDKEY(i + 1)], &copyData[REDKEY(i)], RECORDSIZE *  ((int)copyData[RECORDS] - i));
			break;
		}
	}
	//Push new record
	memcpy( &copyData[REDKEY(i)], value1, attrLength1 );
	memcpy( &copyData[VALUE(i)], value2, attrLength2 );

	int copyRecords = (int)copyData[RECORDS] + 1;
	memcpy( &copyData[RECORDS], &copyRecords, 4);

	memcpy( &newData[NEXT], &data[NEXT], 4);

	int newBlockCounter;
	CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &newBlockCounter));
	newBlockCounter--;
	memcpy( &data[NEXT], &newBlockCounter, 4);

	void *retVal = malloc( attrLength1 );
	memcpy(retVal, &newData[REDKEY(0)], attrLength1 );

	BF_Block_SetDirty(targetBlock);
	CALL_OR_EXIT(BF_UnpinBlock(targetBlock));
	BF_Block_Destroy(&targetBlock);

	BF_Block_SetDirty(newBlock);
	CALL_OR_EXIT(BF_UnpinBlock(newBlock));
	BF_Block_Destroy(&newBlock);

	return retVal;
}

static int InsertRec(int fileDesc, void *value1, void *value2, int subRoot) 
{

	BF_Block *currentBlock;
	BF_Block_Init(&currentBlock);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, subRoot, currentBlock));
	char *data = BF_Block_GetData(currentBlock);

	//Base case
	if (data[IDENTIFIER] == RED) {
		//Check for space
		if ( (int)data[RECORDS] != MAXRECORDS ) {
			InsertInRed(fileDesc, data, value1, value2);
			//Split did not happen on this level so key goes undefined
			int key = UNDEFINED;
			memcpy( newKey, &key, 4);
		}
		else {
			void *key = SplitRed(fileDesc, subRoot, value1, value2);
			memcpy( newKey, key, attrLength1 );
			free(key);
		}
		BF_Block_SetDirty(currentBlock);
		CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
		BF_Block_Destroy(&currentBlock);

		return AM_errno = AME_OK;
	}
	//Recursion
	else {
		int i;
		for (i = 0; i < (int)data[NUMKEYS]; i++) {
			if ( less(value1, &data[BLACKKEY(i)], attrType1) ) {
				break;
			}
		}

		int nextPointer = (int)data[POINTER(i)];
		CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
		BF_Block_Destroy(&currentBlock);

		InsertRec( fileDesc, value1, value2, nextPointer );
		//recursion returns here

		//if split happenned
		if (*(int *)newKey != UNDEFINED) {

			BF_Block *currentBlock;
			BF_Block_Init(&currentBlock);
			CALL_OR_EXIT(BF_GetBlock(fileDesc, subRoot, currentBlock));
			char *data = BF_Block_GetData(currentBlock);

			//check for space
			if ( (int)data[NUMKEYS] != MAXKEYS ) {
				//if there is space
				InsertInBlack(fileDesc, data);
				//Split did not happen on this level so key goes undefined
				int key = UNDEFINED;
				memcpy( newKey, &key, 4);
			}
			else {
				//if there is not
				void *key = SplitBlack(fileDesc, subRoot, newKey);
				memcpy( newKey, key, attrLength1 );
				free(key);
			}
			BF_Block_SetDirty(currentBlock);
			CALL_OR_EXIT(BF_UnpinBlock(currentBlock));
			BF_Block_Destroy(&currentBlock);
		}
		//if split did not happen
		return AM_errno = AME_OK;
	}
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) 
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);
	
	setMetaData(fileDesc);

  	//First Insert//
	if(root == UNDEFINED)
	{
		//Create new red block which will be root
		BF_Block *newRedBlock;
		BF_Block_Init(&newRedBlock);
		CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newRedBlock) );

		int redBlockCounter;
		CALL_OR_EXIT( BF_GetBlockCounter(fileDesc, &redBlockCounter) );
		redBlockCounter--;

		char *data = BF_Block_GetData(newRedBlock);

		data[IDENTIFIER] = RED;

		memcpy( &data[REDKEY(0)], value1, attrLength1 );
		memcpy( &data[VALUE(0)], value2, attrLength2 );
		int records = 1;
		memcpy( &data[RECORDS], &records , 4);

		BF_Block * metaBlock;
		BF_Block_Init(&metaBlock);
		CALL_OR_EXIT(BF_GetBlock(fileDesc, 0, metaBlock));
		char *metaData = BF_Block_GetData(metaBlock);
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

		newKey = malloc(attrLength1);
		InsertRec(fileDesc, value1, value2, root);

		//IF ROOT WAS SPLIT CREATE NEW ROOT
		if (*(int *)newKey != UNDEFINED) {
			BF_Block *newRoot;
			BF_Block_Init(&newRoot);
			CALL_OR_EXIT( BF_AllocateBlock(fileDesc, newRoot) );

			int counter;
			CALL_OR_EXIT(BF_GetBlockCounter(fileDesc, &counter) );


			char *data = BF_Block_GetData(newRoot);

			data[IDENTIFIER] = BLACK;
			
			memcpy( &data[BLACKKEY(0)], newKey, attrLength1 );
			memcpy( &data[FIRST], &root, 4);

			counter -= 2;
			memcpy( &data[POINTER(1)], &counter, 4);

			int one = 1;
			memcpy( &data[NUMKEYS], &one, 4 );

			int undefined = UNDEFINED;
			memcpy( &data[NEXT], &undefined, 4);

			counter++;
			BF_Block * metaBlock;
			BF_Block_Init(&metaBlock);
			CALL_OR_EXIT(BF_GetBlock(fileDesc, 0, metaBlock));
			char *metaData = BF_Block_GetData(metaBlock);
			memcpy(&metaData[ROOT], &counter, 4);

			BF_Block_SetDirty(newRoot);
			CALL_OR_EXIT(BF_UnpinBlock(newRoot));
			BF_Block_Destroy(&newRoot);

			BF_Block_SetDirty(metaBlock);
			CALL_OR_EXIT(BF_UnpinBlock(metaBlock));
			BF_Block_Destroy(&metaBlock);
		}

		debugPrint(fileDesc);
		return (AM_errno = AME_OK);
	}

	return (AM_errno = AME_ERROR);
}

// Recursively search the B+ tree for the specified value
static void search(int fileDesc, int subRoot, void * value, int * const b, int * const r)
{
	BF_Block * block;

	BF_Block_Init(&block);
	CALL_OR_EXIT(BF_GetBlock(fileDesc, subRoot, block));

	char * data = BF_Block_GetData(block);

	if (data[IDENTIFIER] == BLACK)
	{
		// Compare each BLACKKEY with the given value (LESS_THAN)
		unsigned i;
		for (i = 0; i < (int) data[NUMKEYS]; i++)
		{
			void * key = (void *) &(data[BLACKKEY(i)]);
			if(compare(value, key, LESS_THAN, attrType1))
				break;
		}

		int _subRoot = (int)data[POINTER(i)];

		CALL_OR_EXIT(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);

		// (pointer_(i) | key_(i) | pointer_(i + 1))
		// Recursively search subtree at pointer_(i) where key_(i) >= value
		// If there s no such case search subtree at pointer_("NUMKEYS")
		search(fileDesc, _subRoot, value, b, r);
	}
	else if (data[IDENTIFIER] == RED)
	{
		// Compare each REDKEY with the given value (GREATER_THAN_OR_EQUAL)
		// so that |REDKEY - value| = min
		unsigned i;
		for (i = 0; i < (int) data[RECORDS]; i++)
		{
			void * key = (void *) &(data[REDKEY(i)]);
			if(compare(value, key, GREATER_THAN_OR_EQUAL, attrType1))
				break;
		}

		CALL_OR_EXIT(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);

		// Return the red block's index and the record's index
		// in order to make good use of them in "AM_FindNextEntry"
		*b = subRoot;
		*r = i;
	}
}

int AM_OpenIndexScan(int fileDesc, int op, void *value)
{
	if (!isAM(fileDesc))
		return (AM_errno = AME_NOT_AM_FILE);

	AM_errno = AME_OK;

	// Search the fileTable for an entry corresponding to the specified fileDesc
	unsigned i;
	for (i = 0; i < MAXOPENFILES; i++)
		if (fileTable[i].fileDesc == fileDesc)
			break;

	// In case the file has not been opened yet
	// and thus cannot be found inside the fileTable
	// appropriately set the AM_errno and return
	if (i == MAXOPENFILES)
		return (AM_errno = AME_SCAN_FAILED_UNOPENED);

	for (i = 0; i < MAXSCANS; i++)
	{
		if (scanTable[i].fileDesc == UNDEFINED)
		{
			// We found an empty cell

			setMetaData(fileDesc);

			int subRoot = root;

			scanTable[i].fileDesc = fileDesc;
			scanTable[i].op = op;
			if( ( op == NOT_EQUAL ) || (op == LESS_THAN) || (op == LESS_THAN_OR_EQUAL) )
			{
				BF_Block * current;
				BF_Block_Init(&current);

				CALL_OR_EXIT(BF_GetBlock(fileDesc, subRoot, current));

				char * data = BF_Block_GetData(current);

				while (data[IDENTIFIER] != RED)
				{
					subRoot = (int) data[POINTER(0)];

					CALL_OR_EXIT(BF_UnpinBlock(current));

					CALL_OR_EXIT(BF_GetBlock(fileDesc, subRoot, current));

					data = BF_Block_GetData(current);
				}
				
				CALL_OR_EXIT(BF_UnpinBlock(current));
				BF_Block_Destroy(&current);

				// In this case(s) we need to scan the red level of
				// the B+ tree left to right and thus we start the scan
				// from the left-most record

				scanTable[i].blockIndex = subRoot;
				scanTable[i].recordIndex = 0;
			}
			else
			{
				// Follow documentation at Function "search"
				int b, r;
				search(fileDesc, subRoot, value, &b, &r);

				scanTable[i].blockIndex = b;
				scanTable[i].recordIndex = r;
			}

			// Remember to free this shiet !!!
			scanTable[i].value = malloc(attrLength1);
			memcpy(scanTable[i].value, value, attrLength1);

			break;
		}
	}

	if (i == MAXSCANS)
		return (AM_errno = AME_SCAN_FAILED_LIMIT);

	return i;
}

void *AM_FindNextEntry(int scanDesc)
{
	AM_errno = AME_OK;
	setMetaData(scanTable[scanDesc].fileDesc);

	//open last indexed block
	BF_Block *currentBlock;
	BF_Block_Init(&currentBlock);
	CALL_OR_EXIT( BF_GetBlock(scanTable[scanDesc].fileDesc, scanTable[scanDesc].blockIndex, currentBlock) );
	char * currentData = BF_Block_GetData(currentBlock);
  
	void *returnValue = malloc(attrLength2);
	int i = scanTable[scanDesc].recordIndex;
	if(scanTable[scanDesc].op == EQUAL)
	{
		bool found = false;
		for(int i = scanTable[scanDesc].recordIndex; i < currentData[RECORDS]; i++) {
			if(compare( (void *)&currentData[(int)REDKEY(i)], scanTable[scanDesc].value, scanTable[scanDesc].op, attrType1))
			{
				memcpy(returnValue , &(currentData[(int)VALUE(i)]), attrLength2);
				scanTable[scanDesc].recordIndex = i + 1;
				found = true;
			}
		}
		if(!found)
		{
			AM_errno = AME_EOF;
			returnValue = NULL;
		}
	}
	else
	{

		if(compare( (void *)&currentData[(int)REDKEY(i)], scanTable[scanDesc].value, scanTable[scanDesc].op, attrType1))
		{
			memcpy(returnValue , &(currentData[(int)VALUE(i)]), attrLength2);
			if( (i + 1) == (int)currentData[RECORDS] )
			{
				scanTable[scanDesc].recordIndex = 0;
				scanTable[scanDesc].blockIndex = (int)currentData[NEXT];
			}
			else
				scanTable[scanDesc].recordIndex = i + 1;
		}
		else
		{
			AM_errno = AME_EOF;
			returnValue = NULL;
		}

	}
	//close last indexed block
	CALL_OR_EXIT( BF_UnpinBlock(currentBlock) );
	BF_Block_Destroy(&currentBlock);


	return returnValue;
}

int AM_CloseIndexScan(int scanDesc)
{
	// In case of an invalid scanDesc
	// appropriately set AM_errno and return 
	if (scanDesc < 0 || scanDesc >= MAXSCANS)
		return (AM_errno = AME_CLOSE_SCAN_NON_EXISTENT);
	
	if (scanTable[scanDesc].fileDesc == UNDEFINED)
		return (AM_errno = AME_CLOSE_SCAN_NON_EXISTENT);
		
	// Erase the scan entry
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