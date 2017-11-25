#ifndef B_PLUS_TREE_H
#define B_PLUS_TREE_H

#define B_PLUS_CHILDREN_NUM 3

struct bPlusNode_t {
    short int innode;
    void *children[B_PLUS_CHILDREN_NUM];
    void *key[B_PLUS_CHILDREN_NUM - 1];
};

struct bPlusTree_t {
    char *keyType;
    int keyLength;
    char *valType;
    int valLength;
    char *fileName;
    struct bPlusNode_t *root;
};

int BP_init(struct bPlusTree_t *ptr,
            char *fileName,
            char *keyType,
            int keyLength,
            char *valType,
            int valLength
);

int BP_delete(struct bPlusTree_t *ptr);


#endif