#include "AM.h"
#include "bf.h"
#include <string.h>

int AM_errno = AME_OK;

#define CALL_OR_EXIT(call)    \
  {                           \
    BF_ErrorCode code = call; \
    if(code != BF_OK) {       \
      BF_PrintError(code);    \
      return AME_ERROR;       \
    }                         \
  }

void AM_Init() {
  BF_Init(LRU);
	return;
}


int AM_CreateIndex(char *fileName, 
	               char attrType1, 
	               int attrLength1, 
	               char attrType2, 
	               int attrLength2) 
{
  CALL_OR_EXIT(BF_CreateFile(fileName));

  int fd;
  BF_Block *block;
  BF_Block_Init(&block);

  CALL_OR_EXIT(BF_OpenFile(fileName, &fd));
  CALL_OR_EXIT(BF_AllocateBlock(fd, block));
  char *data = BF_Block_GetData(block);


  // BYTES         1          
  // VARS      identifier  -  attrType1  -  attrLength1  -  attrType2  -  attrLength2  -  fileName
  //
  //

  data[0] = 'i';

  memcpy(&data[1], fileName, strlen(fileName) + 1);

  memcpy(&data[1 + strlen(fileName) + 1], &attrType1, 1);

  char intToStr[12];
  sprintf(intToStr, "%d", attrLength1);
  
  memcpy(&data[1 + strlen(fileName) + 1 + 1 + sizeof(intToStr)], &attrType2, 1);

  sprintf(intToStr, "%d", attrLength2);
  memcpy(&data[1 + strlen(fileName) + 1 + 1 + sizeof(intToStr) + 1], intToStr, sizeof(intToStr));

  memcpy(&data[1 + strlen(fileName) + 1 + 1], intToStr, sizeof(intToStr));

  return AME_OK;
}


int AM_DestroyIndex(char *fileName) {
  return AME_OK;
}


int AM_OpenIndex (char *fileName) {
  return AME_OK;
}


int AM_CloseIndex (int fileDesc) {
  return AME_OK;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
  BF_Block *metaBlock;
  BF_Block_Init(&metaBlock);
  CALL_OR_EXIT( BF_GetBlock(fileDesc, 0, metaBlock) );
  char *metaContents = BF_Block_GetData(metaBlock);
  char rootStr[12];
  memcpy(rootStr, metaContents[11], 12);
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
    memcpy(metaContents[11], intToStr, 12);

    
  }
  return AME_OK;
}


int AM_OpenIndexScan(int fileDesc, int op, void *value) {
  return AME_OK;
}


void *AM_FindNextEntry(int scanDesc) {
	
}


int AM_CloseIndexScan(int scanDesc) {
  return AME_OK;
}


void AM_PrintError(char *errString) {
  
}

void AM_Close() {
  
}
