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
  int fd1, fd2, fd3, fd4;
  BF_Block *block;

  BF_Block_Init(&block);

  CALL_OR_DIE(BF_Init(LRU));

  printf("Creating Files ...\n");
  CALL_OR_DIE(BF_CreateFile("data1.db"));
  CALL_OR_DIE(BF_CreateFile("data2.db"));
  CALL_OR_DIE(BF_CreateFile("data3.db"));

  printf("Open Files ...\n");
  CALL_OR_DIE(BF_OpenFile("data1.db", &fd1));
  CALL_OR_DIE(BF_OpenFile("data2.db", &fd2));
  CALL_OR_DIE(BF_OpenFile("data3.db", &fd3));
  
  printf("Insert Data Into Files ...\n");
  char* data;
  for (int i = 0; i < 1000; ++i) {
    CALL_OR_DIE(BF_AllocateBlock(fd1, block));
    data = BF_Block_GetData(block);
    memset(data, i % 127, BF_BUFFER_SIZE);
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));

    CALL_OR_DIE(BF_AllocateBlock(fd2, block));
    data = BF_Block_GetData(block);
    memset(data, i % 127, BF_BUFFER_SIZE);
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));

    CALL_OR_DIE(BF_AllocateBlock(fd3, block));
    data = BF_Block_GetData(block);
    memset(data, i % 127, BF_BUFFER_SIZE);
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
  }

  printf("Read Data From Files ...\n");
  CALL_OR_DIE(BF_OpenFile("data1.db", &fd4));
  for (int i = 0; i < 1000; ++i) {

    printf("Read Data From data1.db ...\n");
    CALL_OR_DIE(BF_GetBlock(fd1, i, block));
    data = BF_Block_GetData(block);
    printf("fileDesk = %d block = %d and data = %d\n", fd1, i, data[0]);
    CALL_OR_DIE(BF_UnpinBlock(block));

    printf("Read Data From data2.db ...\n");
    CALL_OR_DIE(BF_GetBlock(fd2, i, block));
    data = BF_Block_GetData(block);
    printf("fileDesk = %d block = %d and data = %d\n", fd2, i, data[0]);
    CALL_OR_DIE(BF_UnpinBlock(block));

    printf("Read Data From data3.db ...\n");
    CALL_OR_DIE(BF_GetBlock(fd3, i, block));
    data = BF_Block_GetData(block);
    printf("fileDesk = %d block = %d and data = %d\n", fd3, i, data[0]);
    CALL_OR_DIE(BF_UnpinBlock(block));

    printf("Read Data From data1.db ...\n");
    CALL_OR_DIE(BF_GetBlock(fd4, i, block));
    data = BF_Block_GetData(block);
    printf("fileDesk = %d block = %d and data = %d\n", fd4, i, data[0]);
    CALL_OR_DIE(BF_UnpinBlock(block));
  }

  printf("Closing Files ...\n");
  BF_CloseFile(fd1);
  BF_CloseFile(fd2);
  BF_CloseFile(fd3);
  BF_CloseFile(fd4);
  printf("Close BF ...\n");
  BF_Close();

  printf("Open BF ...\n");
  CALL_OR_DIE(BF_Init(LRU));
  CALL_OR_DIE(BF_OpenFile("data2.db", &fd2));
  int blocks_num;
  CALL_OR_DIE(BF_GetBlockCounter(fd2, &blocks_num));

  printf("Read data from data2.db ...\n");
  for (int i = 0; i < blocks_num; ++i) {
    CALL_OR_DIE(BF_GetBlock(fd2, i, block));
    data = BF_Block_GetData(block);
    printf("block = %d and data = %d\n", i, data[i % BF_BUFFER_SIZE]);
    CALL_OR_DIE(BF_UnpinBlock(block));
  }

  printf("Close BF ...\n");
  BF_Block_Destroy(&block);
  BF_CloseFile(fd2);
  BF_Close();
}
