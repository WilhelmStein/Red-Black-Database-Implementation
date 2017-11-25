#include "AM.h"
#include "bf.h"
#include <string.h>
#include <stdio.h>


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

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // +-------++-------------+-------------+---------------+-------------+---------------+--------+---------------+ //
  // | BYTES ||     1       |      1      |      4        |      1      |      4        |   12   | 1 - (512-23)  | //
  // | VARS  || identifier  |  attrType1  |  attrLength1  |  attrType2  |  attrLength2  |  root  |   fileName    | //
  // +-------++-------------+-------------+---------------+-------------+---------------+--------+---------------+ //
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  data[0] = 'i'; 

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
