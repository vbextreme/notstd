/* fork from Volkan Yazıcı priority heap queue */
#include <notstd/phq.h>

#define PHQ_TAW_BIT    (1UL<<((sizeof(phqPriority_t) * 8UL)-1UL) )
#define PHQ_TAW_MAX    ( ~PHQ_TAW_BIT )
#define PHQ_TAW_GET(T) ( (T) & PHQ_TAW_BIT )
#define PHQ_TAW_RM(T)  ( (T) & PHQ_TAW_MAX )
#define PHQ_TAW_SET(T) ( (T) | PHQ_TAW_BIT )

typedef struct phqElement{
	phqPriority_t priority; /**< priority*/
	size_t index;           /**< index*/
	void* data;             /**< userdata*/
}phqElement_t;

typedef struct phq{
	unsigned size;           /**< size of elements*/
	unsigned count;          /**< count elements*/
	unsigned entaw;
	phqPriority_t taw;
	phqPriority_t nextaw;
	void* ctx;
	phqCompare_f cmp;        /**< compare function*/
	phqElement_t** elements; /**< array of elements*/
}phq_t;

#define left(i)   ((i) << 1)
#define right(i)  (((i) << 1) + 1)
#define parent(i) ((i) >> 1)

int phq_cmp_des(__unused phq_t* q, phqPriority_t a, phqPriority_t b){
	return (a < b);
}

int phq_cmp_asc(__unused phq_t* q, phqPriority_t a, phqPriority_t b){
	return (a > b);
}

int phq_cmp_taw_des(phq_t* phq, phqPriority_t a, phqPriority_t b){
	const phqPriority_t ta = PHQ_TAW_GET(a);
	const phqPriority_t tb = PHQ_TAW_GET(b);
	if( ta == tb ) return PHQ_TAW_RM(a) < PHQ_TAW_RM(b);
	return ta != phq->taw;
}

int phq_cmp_taw_asc(phq_t* phq, phqPriority_t a, phqPriority_t b){
	const phqPriority_t ta = PHQ_TAW_GET(a);
	const phqPriority_t tb = PHQ_TAW_GET(b);
	if( ta == tb ) return PHQ_TAW_RM(a) > PHQ_TAW_RM(b);
	return ta != phq->taw;
}

phq_t* phq_new(size_t size, phqCompare_f cmp, int tawenable){
	phq_t* q = NEW(phq_t);
	++size;
	q->entaw  = tawenable;
	q->taw    = 0;
	q->nextaw = PHQ_TAW_BIT;
    q->size   = size;
    q->count  = 0;
	q->cmp    = cmp;
	q->ctx    = NULL;
	q->elements = mem_gift(MANY(phqElement_t*, size), q);
	mem_zero(q->elements);
    return q;
}

size_t phq_size(phq_t *q){
    return q->size - 1;
}

size_t phq_count(phq_t *q){
    return q->count;
}

phqPriority_t phq_taw_current(phq_t* q){
	return q->taw;
}

phqPriority_t phq_taw_next(phq_t* q){
	return q->nextaw;
}

phqPriority_t phq_taw_priority(phqPriority_t p){	
	return PHQ_TAW_GET(p);
}

phqPriority_t phq_taw_peek(phq_t *q){
	if( q->count == 0) return q->nextaw;
    return PHQ_TAW_GET(q->elements[1]->priority);
}

int phq_taw_switch(phq_t* q){
	if( !q->entaw ){
		errno = ENOTSUP;
		return -1;
	}
	const phqPriority_t peek = phq_taw_peek(q);
	const phqPriority_t swit = q->nextaw ^ PHQ_TAW_BIT ;
	if( peek == swit ){
		errno = EPERM;
		return -1;
	}
	q->taw = q->nextaw;
	q->nextaw = swit;

	dbg_info("switch taw:%u next:%u", !!q->taw, !!q->nextaw);
	return 0;
}

void phq_ctx_set(phq_t* phq, void* ctx){
	phq->ctx = ctx;
}

void* phq_ctx(phq_t* phq){
	return phq->ctx;
}

__private void bubble_up(phq_t *q, size_t i){
    size_t parentnode;
    phqElement_t* movingnode = q->elements[i];
    phqPriority_t movingpri = movingnode->priority;

    for( parentnode = parent(i); ((i > 1) && q->cmp(q, q->elements[parentnode]->priority, movingpri)); i = parentnode, parentnode = parent(i) ){
        q->elements[i] = q->elements[parentnode];
        q->elements[i]->index = i;
    }

    q->elements[i] = movingnode;
	q->elements[i]->index = i;
}

__private size_t maxchild(phq_t *q, size_t i){
    size_t childnode = left(i);

    if( childnode > q->count ) return 0;
    if( (childnode+1) < q->count+1 && q->cmp(q, q->elements[childnode]->priority, q->elements[childnode+1]->priority) ) childnode++;

    return childnode;
}

__private void percolate_down(phq_t *q, size_t i){
    size_t childnode;
    phqElement_t* movingnode = q->elements[i];
    phqPriority_t movingpri = movingnode->priority;

    while( (childnode = maxchild(q, i)) && q->cmp(q, movingpri, q->elements[childnode]->priority) ){
        q->elements[i] = q->elements[childnode];
        q->elements[i]->index = i;
        i = childnode;
    }

    q->elements[i] = movingnode;
    movingnode->index = i;
}

phqElement_t* phq_element_new(phqPriority_t priority, void* data){
	phqElement_t* el = NEW(phqElement_t);
	el->priority = priority;
	el->index = 0;
	el->data = data;
	return el;
}

phqPriority_t phq_element_priority_safe(phqElement_t* el){
	return PHQ_TAW_RM(el->priority);
}

phqPriority_t phq_element_priority(phqElement_t* el){
	return el->priority;
}

phqPriority_t phq_element_taw(phqElement_t* el){
	return PHQ_TAW_GET(el->priority);
}

void* phq_element_ctx(phqElement_t* el){
	return el->data;
}

void phq_element_ctx_set(phqElement_t* el, void* ctx){
	el->data = ctx;
}

phqElement_t* phq_insert(phq_t *q, phqElement_t* el){
    if( q->count >= q->size -1 ){
        size_t newsize = q->size * 2;
		q->elements = RESIZE(phqElement_t*, mem_give(q->elements, q), newsize);
		mem_gift(q->elements, q);
        q->size = newsize;
		memset(&q->elements[q->count+1], 0, sizeof(phqElement_t*) * (q->size-(q->count+1)));
		dbg_info("resize phq");
    }
	
	if( q->entaw ){
		el->priority = PHQ_TAW_RM(el->priority);
		el->priority |= q->nextaw;
	}

    size_t i = ++q->count;
    q->elements[i] = el;
	bubble_up(q, i);

	return el;
}

phqElement_t* phq_change_priority(phq_t *q, phqPriority_t newpri, phqElement_t* el){
	if( el->index == 0 ){
		el->priority = newpri;
		phq_insert(q, el);
		return el;
	}
	
	if( q->entaw ) newpri |= PHQ_TAW_GET(el->priority);

    size_t posn = el->index;
    phqPriority_t oldpri = el->priority;
	el->priority = newpri;
    
    if( q->cmp(q, oldpri, newpri) ){
		bubble_up(q, posn);
	}
    else{
        percolate_down(q, posn);
	}

	return el;
}

phqElement_t* phq_remove(phq_t *q, phqElement_t* el){
	if( el->index == 0 ){
		return el;
	}

	if( q->count == 1 ){
		q->elements[1] = 0;
		--q->count;
		el->index = 0;
		if( q->entaw ) el->priority = PHQ_TAW_RM(el->priority);
		return el;
	}

	q->taw = q->taw;

	size_t posn = el->index;
	q->elements[posn] = q->elements[q->count--];
    
	if( q->cmp(q, el->priority, q->elements[posn]->priority) ){
		bubble_up(q, posn);
	}
	else{
		percolate_down(q, posn);
	}

	q->elements[q->count+1] = 0;
	el->index = 0;
	if( q->entaw ) el->priority = PHQ_TAW_RM(el->priority);
	return el;
}

phqElement_t* phq_pop(phq_t *q){	
	if( q->count == 0 ) return NULL;
    phqElement_t* head = q->elements[1];
    q->elements[1] = q->elements[q->count--];
	percolate_down(q, 1);
	q->elements[q->count+1] = 0;
	head->index = 0;
	if( q->entaw ) head->priority = PHQ_TAW_RM(head->priority);
    return head;
}

phqElement_t* phq_peek(phq_t *q){
	if( q->count == 0) return NULL;
    return q->elements[1];
}

void phq_dump(phq_t* q){
	printf("phq dump, count: %u size: %u taw:%u\n", q->count, q->size, !!q->taw);
	for( unsigned i = 1; i <= q->count; ++i){
		printf("[%2zu|%2zu|%1u] ", q->elements[i]->index, PHQ_TAW_RM(q->elements[i]->priority), !!PHQ_TAW_GET(q->elements[i]->priority));
	}
	puts("");
}




















