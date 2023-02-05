#ifndef __NOTSTD_CORE_TRIE_H__
#define __NOTSTD_CORE_TRIE_H__

#include <notstd/core.h>

typedef struct trie trie_t;
typedef struct trieit trie_i;

trie_t* trie_new(void);
int trie_insert(trie_t* tr, const char* str, unsigned len, void* data);
void* trie_find(trie_t* tr, const char* str, unsigned len);
int trie_remove(trie_t* tr,  const char* str, unsigned len);
void trie_dump(trie_t* tr);

trie_i* trie_iterator(trie_t* tr, unsigned off, unsigned count);
void* trie_iterate(void* IT);

#endif
