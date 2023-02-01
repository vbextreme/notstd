#ifndef __NOTSTD_RBTREE_H__
#define __NOTSTD_RBTREE_H__

#include <notstd/core.h>

typedef struct rbtNode rbtNode_t;
typedef struct rbtree rbtree_t;

typedef int (*rbtCompare_f)(const void* a, const void* b);

rbtNode_t* rbtree_insert(rbtree_t* rbt, rbtNode_t* n);
rbtNode_t* rbtree_remove(rbtree_t* rbt, rbtNode_t *z); 
rbtNode_t* rbtree_find(rbtree_t* rbt, const void* data);
rbtNode_t* rbtree_find_best(rbtree_t* rbt, const void* key);
rbtree_t* rbtree_new(cmp_f fn);
rbtree_t* rbtree_cmp(rbtree_t* t, cmp_f fn);

rbtNode_t* rbtree_node_new(void* data);
void* rbtree_node_data(rbtNode_t* n);
rbtNode_t* rbtree_node_data_set(rbtNode_t* n, void* data);
void rbtree_dump(rbtree_t* node);

#endif
