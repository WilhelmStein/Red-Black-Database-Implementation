#include "b_plus_tree.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct bPlusTree_t bPlusTree;

int BP_init(bPlusTree *ptr,
            char *fileName,
            char *keyType,
            int keyLength,
            char *valType,
            int valLength) {
    
    strcpy(ptr->fileName, fileName);
    strcpy(ptr->keyLength, keyLength);
    strcpy(ptr->valLength, valLength);
    strcpy(ptr->keyType, keyType);
    strcpy(ptr->valType, valType);
}

int BP_delete(bPlusTree *ptr)
{

}