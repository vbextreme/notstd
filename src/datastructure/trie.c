#include <notstd/trie.h>
#include <notstd/vector.h>

#define TRIE_ENDNODES 2

typedef struct trieN trieN_s;

typedef struct trieE{
	char*  str;
	unsigned len;
	struct trieN* next;
	void* data;
}trieE_s;

typedef struct trieN{
	trieE_s* ve;
}trieN_s;

struct trie{
	trieN_s* root;
	unsigned count;
};

__private trieN_s* tn_new(trie_t* tr){
	trieN_s* tn = mem_gift(NEW(trieN_s), tr);
	tn->ve = mem_gift(VECTOR(trieE_s, TRIE_ENDNODES), tn);
	return tn;
}

__private void tn_free(trie_t* t, trieN_s* n){
	mem_free(mem_give(n, t));
}

__private void tn_endpoint_add(trieN_s* n, const char* str, unsigned len, void* data, trieN_s* next){
	trieE_s* e = vector_push(&n->ve, NULL);
	e->next = next;
	e->data = data;
	e->len = len;
	e->str = mem_gift(MANY(char, e->len+1), n);
	memcpy(e->str, str, e->len);
	e->str[e->len] = 0;
}

trie_t* trie_new(void){
	trie_t* tr = NEW(trie_t);
	tr->root = tn_new(tr);
	return tr;
}

__private int stricmp(const char* a, const char* b){
	int i = 0;
	for(; a[i] && b[i] && a[i] == b[i]; ++i );
	return i;
}

__private trieE_s* e_find(trieN_s* n, const char* s, __out unsigned* im, __out unsigned* optit){
	foreach_vector(n->ve, it){
		if( (*im = stricmp(n->ve[it].str, s)) ){
			if( optit ) *optit = it;
			return &n->ve[it];
		}
	}
	return NULL;
}

__private void e_split(trie_t* tr, trieN_s* cn, trieE_s* e, unsigned im, const char* s, unsigned len, void* data){
	trieN_s* n = tn_new(tr);
	dbg_info("split %s", e->str);
	dbg_info("add to next node %*s", e->len-im, &e->str[im]);
	tn_endpoint_add(n, &e->str[im], e->len-im, e->data, e->next);
	dbg_info("add to next node %s", s);
	tn_endpoint_add(n, s, len, data, NULL);
	e->next = n;
	e->len = im;
	e->str = mem_gift(RESIZE(char, mem_give(e->str, cn), e->len+1), cn);
	e->str[e->len] = 0;
	e->data = NULL;
	dbg_info("splitted %s", e->str);
}

int trie_insert(trie_t* tr, const char* str, unsigned len, void* data){
	if( !str || !*str ) return -1;
	if( !len ) len = strlen(str);

	trieN_s* n = tr->root;
	while( len ){	
		unsigned im;
		trieE_s* e = e_find(n, str, &im, NULL);
		if( !e ){
			dbg_info("not find E, element not exists in this N, add and return");
			tn_endpoint_add(n, str, len, data, NULL);
			++tr->count;
			return 0;
		}

		dbg_info("stepping len(%u) im(%u) lim(%u)", len, im, len-im);
		iassert(im <= len);
		len -= im;
		str += im;

		if( !len ){
			if( im == e->len ){
				dbg_info("string is equal to previous endpoint, replace and return");
				e->data = data;
				return 0;
			}
			e_split(tr, n, e, im, str, len, data);
			return 0;
		}

		if( im < e->len ){
			e_split(tr, n, e, im, str, len, data);
			++tr->count;
			return 0;
		}

		if( !e->next ){
			dbg_info("string have more chars but no more node, create new node for remaning string and return");
			e->next = tn_new(tr);
			tn_endpoint_add(e->next, str, len, data, NULL);
			++tr->count;
			return 0;
		}

		iassert(e->next);
		n = e->next;
	}

	return -1;
}

void* trie_find(trie_t* tr, const char* str, unsigned len){
	if( !str || !*str ) return NULL;
	if( !len ) len = strlen(str);

	trieN_s* n = tr->root;
	trieE_s* e = NULL;

	do{	
		unsigned im;
		if( !(e = e_find(n, str, &im, NULL)) ) return NULL;
		iassert(im <= len);
		len -= im;
		str += im;
		n = e->next;
	}while( n && len );

	if( len ) return NULL;
	iassert( e );
	//iassert( e->data );
	return e->data;
}

__private void e_merge(trie_t* tr, trieN_s* n, trieE_s* e){
	trieE_s* m = &e->next->ve[0];
	dbg_info("merge %s++%s", e->str, m->str);

	unsigned len = e->len;
	e->len = e->len + m->len;
	e->str = mem_gift(RESIZE(char, mem_give(e->str, n), e->len+1), n);
	memcpy(&e->str[len], m->str, m->len);
	e->str[e->len] = 0;
	e->data = m->data;
	tn_free(tr, e->next);
	e->next = m->next;
}


__private int rm_rec(trie_t* tr, trieN_s* n, const char* str, unsigned len){
	dbg_info("rec %s", str);
	trieE_s* e = NULL;
	unsigned im;
	unsigned it;
	if( !(e = e_find(n, str, &im, &it)) ) return -1;
	iassert(im <= len);
	len -= im;
	str += im;
	if( len ){
		if( !e->next ) return -1;
		if( rm_rec(tr, e->next, str, len) ) return -1;
	}
	
	if( e->next && !vector_count(&e->next->ve) ){
		dbg_info("next element is clear, destroy and remove current element");
		tn_free(tr, e->next);
		mem_free(mem_give(e->str, n));
		vector_remove(&n->ve, it, 1);
	}
	else if( !e->next ){
		dbg_info("not have next can free(%u) %s", it, e->str);
		mem_free(mem_give(e->str, n));
		vector_remove(&n->ve, it, 1);
	}
	else if( !len ){
		if( e->data == NULL ) return -1;
		dbg_info("is endnode but shared node");
		e->data = NULL;
	}
	else if( e->next && vector_count(&e->next->ve) == 1 && !e->data ){
		dbg_info("next have only one element, can merge");
		e_merge(tr, n, e);
	}

	
	return 0;
}

//ensure data exists before remove
int trie_remove(trie_t* tr,  const char* str, unsigned len){
	dbg_info("remove %s", str);
	if( rm_rec(tr, tr->root, str, len ? len : strlen(str)) ) return -1;
	--tr->count;
	return 0;
}

__private void pt(unsigned tab){
	while( tab-->0 ) putchar(' ');
}

__private void tn_dump(trieN_s* n, unsigned tab){
	foreach_vector(n->ve, i){
		pt(tab);
		fputs(n->ve[i].str, stdout);
		if( n->ve[i].data ) putchar('*');
		putchar('\n');
		if( n->ve[i].next ) tn_dump(n->ve[i].next, tab + n->ve[i].len);
	}
}

void trie_dump(trie_t* tr){
	tn_dump(tr->root, 0);
}

struct trieit{
	unsigned count;
	trieN_s** stk;
	trieN_s*  cur;
	unsigned  icur;
};

trie_i* trie_iterator(trie_t* tr, unsigned off, unsigned count){
	trie_i* it = NEW(trie_i);
	if( !count ) count = tr->count;
	it->stk = mem_gift(VECTOR(trieN_s*, tr->count+1), it);
	it->cur = tr->root;
	it->icur = 0;
	it->count = tr->count+1;
	while( off-->0 ) trie_iterate(it);
	it->count = count;
	return it;
}

void* trie_iterate(void* IT){
	trie_i* it = IT;
	void* ret = NULL;
	if( !it->count ) return NULL;

	while( !ret ){
		if( it->icur >= vector_count(&it->icur) ){
			if( !vector_pop(&it->stk, &it->cur) ) return NULL;
			it->icur = 0;
		}
	
		if( it->cur->ve[it->icur].next ) vector_push(&it->stk, &it->cur->ve[it->icur].next);
		if( it->cur->ve[it->icur].data ) ret = it->cur->ve[it->icur].data;
		++it->icur;
	}

	--it->count;
	return ret;
}

