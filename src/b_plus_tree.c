#include "b_plus_tree.h"

typedef struct bPlusTree_t bPlusTree;

int BP_init(bPlusTree *ptr,
            char *fileName,
            char *keyType,
            int keyLength,
            char *valType,
            int valLength) {
    
    if( !strcmp(keyType, "i") ) //integer
    {
        
    }
    else if ( !strcmp(keyType, "f") )//float
    {

    }
    else if ( !strcmp(keyType, "c") )//char *
    {

    }
    
}

int BP_delete(bPlusTree *ptr)
{

}