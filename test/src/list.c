#include <notstd/list.h>

typedef struct list{
	struct list* next;
	int val;
}list_s;

typedef struct dlist{
	struct dlist* next;
	struct dlist* prev;
	int val;
}dlist_s;

__private list_s* list_ctor(list_s* l, int val){
	ls_ctor(l);
	l->val = val;
	return l;
}

#define N 8
void uc_ls(){
	dbg_info("create list");
	list_s* head = NULL;
	for( unsigned i = 0; i < N; ++i ){
		ls_push(&head, list_ctor(NEW(list_s), i));
	}
	foreach_ls(head, it){
		printf("%d", it->val);
	}
	dbg_info("");

	dbg_info("add tail");
	ls_tail(&head, list_ctor(NEW(list_s), -1));
	foreach_ls(head, it){
		printf("%d", it->val);
	}
	dbg_info("");

	dbg_info("add before head->next");
	ls_before(&head, head->next, list_ctor(NEW(list_s), 9999));
	foreach_ls(head, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("add after head->next");
	ls_after(head->next, list_ctor(NEW(list_s), 7777));
	foreach_ls(head, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("extract and free head->next->next");
	mem_free(ls_extract(&head, head->next->next));
	foreach_ls(head, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("pop and free all");
	list_s* p;
	while( (p=ls_pop(&head)) ){
		mem_free(p);
	}
	foreach_ls(head, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");
}

__private dlist_s* dlist_ctor(dlist_s* l, int val){
	ld_ctor(l);
	l->val = val;
	return l;
}

/*
__private void dump_ld(dlist_s* s){
	dlist_s* head = s;
	do{
		printf("[%p<-(%d)%p->%p]\n", s->prev, s->val, s, s->next);
		s = s->next;
	}while( s != head );
	puts("");
}
*/

void uc_ld(){
	dbg_info("create doubly circular list with first element are sentinel");
	dlist_s* sentinel = dlist_ctor(NEW(dlist_s), -1);
	for( unsigned i = 0; i < N; ++i ){
		ld_before(sentinel, dlist_ctor(NEW(dlist_s), i));
	}
	foreach_ld(sentinel, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("add after sentinel->next->next");
	ld_after(sentinel->next->next, dlist_ctor(NEW(dlist_s), 9999));
	foreach_ld(sentinel, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("create secondary list and add before sentinel->next");
	dlist_s* secondary = dlist_ctor(NEW(dlist_s), 1000);
	for( unsigned i = 1001; i < 1004; ++i ){
		ld_before(secondary, dlist_ctor(NEW(dlist_s), i));
	}
	ld_before(sentinel->next, secondary);
	foreach_ld(sentinel, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("create secondary list and add after sentinel->next");
	secondary = dlist_ctor(NEW(dlist_s), 2000);
	for( unsigned i = 2001; i < 2004; ++i ){
		ld_before(secondary, dlist_ctor(NEW(dlist_s), i));
	}
	ld_after(sentinel->next, secondary);
	foreach_ld(sentinel, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("free and extract sentinel->next->next");
	mem_free(ld_extract(sentinel->next->next));
	foreach_ld(sentinel, it){
		dbg_info("%d", it->val);
	}
	dbg_info("");

	dbg_info("free all");
	dlist_s* it = sentinel->next;
	while( it != sentinel ){
		mem_free(ld_extract_preserve_next(&it));
	}
	mem_free(sentinel);
	dbg_info("");
}

