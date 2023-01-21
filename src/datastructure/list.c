#include <notstd/list.h>

/**********************/
/* singly linked list */
/**********************/

void* ls_ctor(void* v){
	listSingly_s* l = v;
	l->next = NULL;
	return l;
}

void* ls_push(void* vhead, void* v){
	listSingly_s** head = vhead;
	listSingly_s* l = v;
	l->next = *head;
	*head = l;
	return l;
}

void* ls_tail(void* vhead, void* v){
	listSingly_s** head = vhead;
	listSingly_s* l = v;
	for(; *head; head = &((*head)->next));
	*head = l;
	return l;
}

void* ls_before(void* vhead, void* vbefore, void* v){
	listSingly_s** head = vhead;
	listSingly_s* before = vbefore;
	listSingly_s* l = v;

	for(; *head; head = &((*head)->next)){
		if( *head == before ){
			l->next = *head;
			*head = l;
			return l;
		}
	}
	return NULL;
}

void* ls_after(void* vafter, void* v){
	listSingly_s* after = vafter;
	listSingly_s* l = v;
	l->next = after->next;
	after->next = l;
	return v;
}

void* ls_pop(void* vhead){
	listSingly_s** head = vhead;
	if( *head ){
		listSingly_s* ret = *head;
		(*head) = (*head)->next;
		return ret;
	}
	return NULL;
}

void* ls_extract(void* vhead, void* v){
	listSingly_s** head = vhead;
	listSingly_s*  l = v;
	for(; *head && *head != l; head = &((*head)->next));
	if( *head ) *head = (*head)->next;
	l->next = NULL;
	return l;
}

/**********************/
/* doubly linked list */
/**********************/

void* ld_ctor(void* v){
	listDoubly_s* l = v;
	l->next = l;
	l->prev = l;
	return l;
}

void* ld_after(void* vafter, void* v){
	listDoubly_s* a = vafter;
	listDoubly_s* l = v;

	l->prev->next = a->next;
	a->next->prev = l->prev;
	l->prev = a;
	a->next = l;
	return l;
}

void* ld_before(void* vbefore, void* v){
	listDoubly_s* b = vbefore;
	listDoubly_s* l = v;
	l->prev->next = b;
	b->prev->next = l;
	swap(l->prev, b->prev);
	return l;
}

void* ld_extract(void* v){
	listDoubly_s* l = v;
	l->next->prev = l->prev;
	l->prev->next = l->next;
	l->next = l->prev = l;
	return l;
}

void* ld_extract_preserve_next(void* pv){
	listDoubly_s** l = pv;
	listDoubly_s* next = (*l)->next;
	listDoubly_s* ret = ld_extract(*l);
	*l = next;
	return ret;
}

void* ld_extract_preserve_prev(void* pv){
	listDoubly_s** l = pv;
	listDoubly_s* prev = (*l)->prev;
	listDoubly_s* ret = ld_extract(*l);
	*l = prev;
	return ret;
}
