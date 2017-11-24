#include "AM.h"
#include "bf.h"
// Basilis EDW
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

  memcpy(&data[0], fileName, strlen(fileName) + 1);

  memcpy(&data[strlen(fileName) + 1], &attrType1, 1);

  char intToStr[12];
  sprintf(intToStr, "%d", attrLength1);
  memcpy(&data[strlen(fileName) + 2], intToStr, sizeof(intToStr));

  memcpy(&data[strlen(fileName) + 2 + sizeof(intToStr)], &attrType2, 1);

  sprintf(intToStr, "%d", attrLength2);
  memcpy(&data[strlen(fileName) + 3 + sizeof(intToStr)], intToStr, sizeof(intToStr));
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
