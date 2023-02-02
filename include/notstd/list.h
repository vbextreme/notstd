#ifndef __NOTSTD_CORE_LIST_H__
#define __NOTSTD_CORE_LIST_H__

#include <notstd/core.h>

//TODO map

/**************************/
/*** simple linked list ***/
/**************************/

//argument vhead is alway passed to address in all functions

//your list need be compatible list is null termnated
typedef struct listSingly{
	struct listSingly* next;
}listSingly_s;

//next element
#define ls_next(L) ((L)->next)

//iterate to list
//list_s* head = NULL;
//ls_push(....);
//foreach_ls(head, it){
//	printf("%p\n", it);
//}
#define foreach_ls(L, IT) for(typeof(L) IT = (L); IT; (IT)=ls_next(IT))

//initialize element of list, return v
void* ls_ctor(void* v);

//push element in head return element
//list_s* head = NULL;
//ls_push(&head, ls_ctor(NEW(list_s)));
void* ls_push(void* vhead, void* v);

//push element at ends of list, return v
void* ls_tail(void* vhead, void* v);

//push element before and return v or null if not before exists
void* ls_before(void* vhead, void* vbefore, void* v);

//push element after return v
void* ls_after(void* vafter, void* v);

//pop element, return element or null if no more elements
void* ls_pop(void* vhead);

//extract element from list, return v always
void* ls_extract(void* vhead, void* v);

/***************************************/
/*** double linked list concatenated ***/
/***************************************/

//never pass address of element

//your list need be compatible list, list is circular
typedef struct listDoubly{
	struct listDoubly* next;
	struct listDoubly* prev;
}listDoubly_s;

//next element
#define ld_next(L) ((L)->next)

//next element
#define ld_prev(L) ((L)->prev)

//initialize element of list, return v
void* ld_ctor(void* v);

//add list v after vafter, return v
void* ld_after(void* vafter, void* v);

//add list v before vbefore, return v
void* ld_before(void* vbefore, void* v);

//extract element from list, return v
void* ld_extract(void* v);

//pv is pointer to element, extract element and set pv to next, always extract, not return null
//list_s* head;
//list_s* top = ld_extract_preserve_next(&head);
void* ld_extract_preserve_next(void* pv);

//same ld_extract_preserve_next but preserve prev
void* ld_extract_preserve_prev(void* pv);

//foreach_ld works only if you have sentinel elemnt on the list, L is a sentinel element
#define foreach_ld(L, IT) for( typeof(L) IT = ld_next(L); IT != L; IT = ld_next(IT) )

#endif
