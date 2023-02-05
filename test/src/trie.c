#include <notstd/trie.h>

//TODO test iterator

void uc_trie(void){
	char* words[] = {
		"hello",
		"world",
		"her",
		"wonderland",
		"wonderwoman",
		"love",
		"loves",
		"loto",
		"laica",
		"loving",
		"world!",
		NULL
	};
	
	trie_t* t = trie_new();

	dbg_info("insert");	
	unsigned i = 0;
	do{
		dbg_info("insert %s", words[i]);
		trie_insert(t, words[i], 0, (void*)(uintptr_t)i+1);
	}while(words[++i]);
	trie_dump(t);

	dbg_info("find");
	i=0;
	do{
		uintptr_t v = (uintptr_t)trie_find(t, words[i], 0);
		if( v != i+1 ) die("try to find %s with id %u but get id %lu", words[i], i+1, v);
	}while(words[++i]);

	dbg_info("remove dis");
	i=0;
	do{
		if( i & 1 ){
			dbg_info("remove %s", words[i]);
			if( trie_remove(t, words[i], 0) ) die("remove return error");
			}
	}while(words[++i]);
	trie_dump(t);

	dbg_info("check remove ok");
	i=0;
	do{
		if( i & 1 ){
			uintptr_t v = (uintptr_t)trie_find(t, words[i], 0);
			if( v ) die("try to find removed element %s with id %u but get id %lu", words[i], i+1, v);
		}
		else{
			uintptr_t v = (uintptr_t)trie_find(t, words[i], 0);
			if( v != i+1 ) die("try to find %s with id %u but get id %lu", words[i], i+1, v);
		}
	}while(words[++i]);

	dbg_info("check remove return -1 if not exists and remove all nodes");
	i=0;
	do{
		int ret = trie_remove(t, words[i], 0);
		if( (i & 1) && ret != -1 ) die("trie[%u] not returned -1", i);
	}while(words[++i]);
	trie_dump(t);
}
