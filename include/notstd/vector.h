#ifndef __NOTSTD_CORE_VECTOR_H__
#define __NOTSTD_CORE_VECTOR_H__

#include <notstd/core.h>

typedef struct vextend{
	size_t count;
	size_t max;
	unsigned sof;
}vextend_s;


#define vector_sizeof(PTR) ({\
	vextend_s* ve = mem_extend(PTR);	\
	ve->sof;\
})

#define vector_count(PTR) ({\
	vextend_s* ve = mem_extend(PTR);	\
	ve->count;\
})

#define vector_lenght(PTR) ({\
	vextend_s* ve = mem_extend(PTR);	\
	ve->max;\
})

#define VECTOR(TYPE, COUNT) (TYPE*)vector_new(sizeof(TYPE), COUNT)

#define foreach_vector(V, IT) for( size_t IT = 0; IT < vector_count(V); ++IT )

void* vector_new(size_t sof, size_t count);
void vector_clear(void* v);
void* vector_resize(void* v, size_t count);
void* vector_upsize(void* v, size_t count);
void* vector_shrink(void* v);
void* vector_remove(void* v, const size_t index, const size_t count);
void* vector_add(void* v, const size_t index, const size_t count);
void* vector_push(void* restrict v, void* restrict element);
void* vector_fetch(void* restrict v, __out size_t* id);
void* vector_head(void* restrict v, void* restrict element);
void* vector_pop(void* v, __out void* ret);
void* vector_fetch_pop(void* v, size_t* it);
void* vector_peek(void* v);
void* vector_ipeek(void* v, size_t id);
void* vector_top(void* v, __out void* ret);
void* vector_pick(void* v);
void vector_shuffle(void* v, size_t begin, size_t end);
void vector_qsort(void* v, qsort_f fn);

#endif 
