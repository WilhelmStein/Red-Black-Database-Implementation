#ifndef B_PLUS_TREE_H
#define B_PLUS_TREE_H

#define B_PLUS_CHILDREN_NUM 3

struct bPlusNode {
    struct bPlusNode *children[B_PLUS_CHILDREN_NUM];

};

typedef struct bPlusNode * bPlusTree;

int BP_init(bPlusTree *ptr);
int BP_delete(bPlusTree *ptr);
#endif