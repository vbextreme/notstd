#include <notstd/dict.h>
#include <notstd/rbtree.h>

struct dict{
	rbtree_t* itree;
	rbtree_t* stree;
};

__private int icmp(const void* a, const void* b){
	const dictPair_s* ea = a;
	const dictPair_s* eb = b;
	return ea->key.l - eb->key.l;
}

__private int scmp(const void* a, const void* b){
	const dictPair_s* ea = a;
	const dictPair_s* eb = b;
	return strcmp(ea->key.lstr, eb->key.lstr);
}


dict_t* dict_new(void){
	dict_t* d = NEW(dict_t);
	d->itree = rbtree_new(icmp);
	d->stree = rbtree_new(scmp);
	mem_gift(d->itree, d);
	mem_gift(d->stree, d);
	return d;
}

generic_s* dicti(dict_t* d, long key){
	dictPair_s e = { .key.type = G_LONG, .key.l = key };
	rbtNode_t* node = rbtree_find(d->itree, &e);
	if( !node ){
		dictPair_s* n = NEW(dictPair_s);
		n->key = GI(key);
		n->value = gi_unset();
		rbtNode_t* node = rbtree_node_new(n);
		mem_gift(n, node);
		rbtree_insert(d->itree, mem_gift(node, d->itree));
		return &n->value;
	}
	dictPair_s* f = rbtree_node_data(node);
	return &f->value;
}

generic_s* dicts(dict_t* d, const char* key){
	dictPair_s e = { .key.type = G_STRING, .key.lstr = key };
	rbtNode_t* node = rbtree_find(d->stree, &e);
	if( !node ){
		dictPair_s* n = NEW(dictPair_s);
		n->key = GI(key);
		n->value = gi_unset();
		rbtNode_t* node = rbtree_node_new(n);
		mem_gift(n, node);
		rbtree_insert(d->stree, mem_gift(node, d->stree));
		return &n->value;
	}
	dictPair_s* f = rbtree_node_data(node);
	return &f->value;
}

int dictirm(dict_t* d, long key){
	dictPair_s e = { .key.type = G_LONG, .key.l = key };
	rbtNode_t* node = rbtree_find(d->itree, &e);
	if( !node ){
		errno = ESRCH;
		return -1;
	}
	mem_free(mem_give(rbtree_remove(d->itree, node), d->itree));
	return 0;
}

int dictsrm(dict_t* d, const char* key){
	dictPair_s e = { .key.type = G_LSTRING, .key.lstr = key };
	rbtNode_t* node = rbtree_find(d->stree, &e);
	if( !node ){
		errno = ESRCH;
		return -1;
	}
	mem_free(mem_give(rbtree_remove(d->stree, node), d->stree));
	return 0;
}

unsigned long dict_count(dict_t* dic){
	return rbtree_count(dic->itree) + rbtree_count(dic->stree);
}

struct dictit{
	unsigned long count;
	rbtree_i* it;
	rbtree_i* iti;
	rbtree_i* its;
};

dict_i* dict_iterator(dict_t* dic, unsigned offset, unsigned count){
	dict_i* it = NEW(dict_i);
	if( count == 0 ) count = dict_count(dic);
	it->iti = mem_gift(rbtree_inorder_iterator(dic->itree, 0, 0), it);
	it->its = mem_gift(rbtree_inorder_iterator(dic->stree, 0, 0), it);
	it->it = it->iti;
	it->count = dict_count(dic);
	while( offset --> 0 ) dict_iterate(it);
	it->count = count;
	return it;
}

void* dict_iterate(void* IT){
	dict_i* it = IT;
	if( !it->count ) return NULL;
	rbtNode_t* n = rbtree_inorder_iterate(it->it);
	if( !n ){
		it->it = it->its;
		n = rbtree_inorder_iterate(it->it);
	}
	return n ? rbtree_node_data(n) : NULL;
}


