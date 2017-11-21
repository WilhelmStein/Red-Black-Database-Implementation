#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int main() {
  int fd;
  BF_Block *block;
  BF_Block_Init(&block);
  char *data;

  CALL_OR_DIE(BF_Init(LRU));
  CALL_OR_DIE(BF_CreateFile("data1.db"))
  CALL_OR_DIE(BF_OpenFile("data1.db", &fd));
  for (int i = 0; i < 5; ++i) {
    CALL_OR_DIE(BF_AllocateBlock(fd, block));
    data = BF_Block_GetData(block);
    memset(data, 'C', BF_BLOCK_SIZE);
    BF_Block_SetDirty(block);
    if (BF_CloseFile(fd) == BF_AVAILABLE_PIN_BLOCKS_ERROR) {
      printf("Correctly returns error with code BF_AVAILABLE_PIN_BLOCKS_ERROR\n");
    }
    CALL_OR_DIE(BF_UnpinBlock(block));
  }
  CALL_OR_DIE(BF_CloseFile(fd));

  CALL_OR_DIE(BF_CreateFile("data2.db"))
  CALL_OR_DIE(BF_OpenFile("data2.db", &fd));
  for (int i = 0; i < 5; ++i) {
    CALL_OR_DIE(BF_AllocateBlock(fd, block));
    data = BF_Block_GetData(block);
    memset(data, 'X', BF_BLOCK_SIZE);
    BF_Block_SetDirty(block);
    if (BF_CloseFile(fd) == BF_AVAILABLE_PIN_BLOCKS_ERROR) {
      printf("Correctly returns error with code BF_AVAILABLE_PIN_BLOCKS_ERROR\n");
    }
    CALL_OR_DIE(BF_UnpinBlock(block));
  }
  CALL_OR_DIE(BF_CloseFile(fd));

  int fd1, fd2;
  CALL_OR_DIE(BF_OpenFile("data1.db", &fd1));
  CALL_OR_DIE(BF_OpenFile("data2.db", &fd2));

  CALL_OR_DIE(BF_GetBlock(fd1, 0, block));
  data = BF_Block_GetData(block);
  printf("data1.db stores the character %c\n", data[0]);
  CALL_OR_DIE(BF_UnpinBlock(block));

  CALL_OR_DIE(BF_GetBlock(fd2, 0, block));
  data = BF_Block_GetData(block);
  printf("data2.db stores the character %c\n", data[0]);
  CALL_OR_DIE(BF_UnpinBlock(block));
  CALL_OR_DIE(BF_CloseFile(fd1));
  CALL_OR_DIE(BF_CloseFile(fd2));
  
  BF_Block_Destroy(&block);
  CALL_OR_DIE(BF_Close());
}

