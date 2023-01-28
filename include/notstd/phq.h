#ifndef __NOTSTD_CORE_PHQ_H__
#define __NOTSTD_CORE_PHQ_H__

#include <notstd/core.h>

/* priority heap queue, aka binary heap
 * extra functionality is taw, reusable same queue for next priority data
*/

// phq type, adt
typedef struct phq phq_t;

// element of phq, adt
typedef struct phqElement phqElement_t;

// priority type
typedef unsigned long phqPriority_t;

// compare function for order queue
typedef int (*phqCompare_f)(phq_t* phq, size_t a, size_t b);

// order descending
int phq_cmp_des(__unused phq_t* q, phqPriority_t a, phqPriority_t b);
// order ascending
int phq_cmp_asc(__unused phq_t* q, phqPriority_t a, phqPriority_t b);
// descending with taw
int phq_cmp_taw_des(phq_t* phq, phqPriority_t a, phqPriority_t b);
// ascending with taw
int phq_cmp_taw_asc(phq_t* phq, phqPriority_t a, phqPriority_t b);

// create new queue with size are number of empty slot in queue, the queue automatic upsize the queue if you add more elements.
// cmp is function used for compare elements in queue
// set TRUE tawenable for using taw
phq_t* phq_new(size_t size, phqCompare_f cmp, int tawenable);

// return numbers of max elements can enter in queue before resize
size_t phq_size(phq_t *q);

// return numbers of elements in queue
size_t phq_count(phq_t *q);

// current taw, realy you need use this only in custom compare function
phqPriority_t phq_taw_current(phq_t* q);

// taw for inserted elements, not realy need
phqPriority_t phq_taw_next(phq_t* q);

// get taw from priority, not realy need, taw is removed when pop element
phqPriority_t phq_taw_priority(phqPriority_t p);

// get taw value of element on top of the queue, if not elements return nexttaw
phqPriority_t phq_taw_peek(phq_t *q);

// switch current taw to next
// return -1 && errno = ENOTSUP if taw is not enable
// return -1 && errno = EPERM if can't switch taw in this state, you need consume all element before switch
// return 0 successfull
int phq_taw_switch(phq_t* q);

// set a generic ctx
void phq_ctx_set(phq_t* phq, void* ctx);
// get generic ctx
void* phq_ctx(phq_t* phq);

// create new element, with priority and custom data
phqElement_t* phq_element_new(phqPriority_t priority, void* data);

// if taw is enable use this functions for ensure not taw are in priority, phq_peek not remove taw from element
phqPriority_t phq_element_priority_safe(phqElement_t* el);

// return raw priority
phqPriority_t phq_element_priority(phqElement_t* el);

// return taw in element, realy this works only if peek element
phqPriority_t phq_element_taw(phqElement_t* el);

// get element data
void* phq_element_ctx(phqElement_t* el);
// set element data
void phq_element_ctx_set(phqElement_t* el, void* ctx);

// insert new element
phqElement_t* phq_insert(phq_t *q, phqElement_t* el);

// change priority to element, if element is not in queue is automatic call insert.
// on taw queue, if element is not in queue is insert with nexttaw, if element is in queue taw is not change
phqElement_t* phq_change_priority(phq_t *q, phqPriority_t newpri, phqElement_t* el);

// remove any element from queue
phqElement_t* phq_remove(phq_t *q, phqElement_t* el);

// pop element
phqElement_t* phq_pop(phq_t *q);

// peek element
phqElement_t* phq_peek(phq_t *q);

// dump
void phq_dump(phq_t* q);

#endif
