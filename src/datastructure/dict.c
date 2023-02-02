#include <notstd/dict.h>
#include <notstd/rbtree.h>

struct dict{
	rbtree_t* itree;
	rbtree_t* stree;
};

typedef struct dictE{
	generic_s key;
	generic_s value;
}dictE_s;

__private int icmp(const void* a, const void* b){
	const dictE_s* ea = a;
	const dictE_s* eb = b;
	return ea->key.l - eb->key.l;
}

__private int scmp(const void* a, const void* b){
	const dictE_s* ea = a;
	const dictE_s* eb = b;
	return strcmp(ea->key.lstr, eb->key.lstr);
}

dict_t* dict_new(void){
	dict_t* d = NEW(dict_t);
	d->itree = rbtree_new(icmp);
	d->stree = rbtree_new(scmp);
	return d;
}

generic_s* dicti(dict_t* d, long key){
	dictE_s e = { .key.type = G_LONG, .key.l = key };
	rbtNode_t* node = rbtree_find(d->itree, &e);
	if( !node ){
		dictE_s* n = NEW(dictE_s);
		n->key = GI(key);
		n->value = gi_unset();
		rbtNode_t* node = rbtree_node_new(n);
		mem_gift(n, node);
		rbtree_insert(d->itree, mem_gift(node, d->itree));
		return &n->value;
	}
	dictE_s* f = rbtree_node_data(node);
	return &f->value;
}

generic_s* dicts(dict_t* d, const char* key){
	dictE_s e = { .key.type = G_STRING, .key.lstr = key };
	rbtNode_t* node = rbtree_find(d->stree, &e);
	if( !node ){
		dictE_s* n = NEW(dictE_s);
		n->key = GI(key);
		n->value = gi_unset();
		rbtNode_t* node = rbtree_node_new(n);
		mem_gift(n, node);
		rbtree_insert(d->stree, mem_gift(node, d->stree));
		return &n->value;
	}
	dictE_s* f = rbtree_node_data(node);
	return &f->value;
}

int dictirm(dict_t* d, long key){
	dictE_s e = { .key.type = G_LONG, .key.l = key };
	rbtNode_t* node = rbtree_find(d->itree, &e);
	if( !node ){
		errno = ESRCH;
		return -1;
	}
	mem_free(mem_give(rbtree_remove(d->itree, node), d->itree));
	return 0;
}

int dictsrm(dict_t* d, const char* key){
	dictE_s e = { .key.type = G_LSTRING, .key.lstr = key };
	rbtNode_t* node = rbtree_find(d->stree, &e);
	if( !node ){
		errno = ESRCH;
		return -1;
	}
	mem_free(mem_give(rbtree_remove(d->stree, node), d->stree));
	return 0;
}




