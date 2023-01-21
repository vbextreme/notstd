#ifndef __NOTSTD_CORE_VECTOR_H__
#define __NOTSTD_CORE_VECTOR_H__

#include <notstd/core.h>

// generic implementation of vector, all function need called with pointer to vector
// all functions works only with pointer at the first element of vector.
//
// int *a = VECTOR(int, 10);
// vector_clear(&a); // ok
// vector_clear(a); // error
// vector_clear(&a[1]); //error

typedef struct vextend{
	size_t count;
	size_t max;
	unsigned sof;
}vextend_s;

//return sizeof element
#define vector_sizeof(PTR) ({\
	vextend_s* ve = mem_extend(*(void**)(PTR));	\
	ve->sof;\
})

//return numbers of element in array
#define vector_count(PTR) ({\
	vextend_s* ve = mem_extend(*(void**)(PTR));	\
	ve->count;\
})

//return max numbers of element you can reserved
#define vector_lenght(PTR) ({\
	vextend_s* ve = mem_extend(*(void**)(PTR));	\
	ve->max;\
})

//create new vector type safe
#define VECTOR(TYPE, COUNT) (TYPE*)vector_new(sizeof(TYPE), COUNT)

//iterate over vector
#define foreach_vector(V, IT) for( size_t IT = 0; IT < vector_count(V); ++IT )

//create new vector, sof is size of element, count is how many memory reserved for vector, can increade or decrease, return always a valid pointer
void* vector_new(size_t sof, size_t count);

//set to 0 count of elements
void vector_clear(void* v);

//resize numbers of max elements you can have return *v
void* vector_resize(void* v, size_t count);

//check if you max size can countains count elements(if vector_count + count > vector_lenght), otherwise increment the max size, return *v
void* vector_upsize(void* v, size_t count);

//check if you max size is to big, release eccessive max size, return *v
void* vector_shrink(void* v);

//remove count element from index, auto shrink
//int* a = VECTOR(int, 10);
//vector_push(&a, 1);vector_push(&a, 2);vector_push(&a, 3);
////[1,2,3]
//vector_remove(&a, 1, 1);
////[1,3]
// return *v or NULL if index out of bounds
void* vector_remove(void* v, const size_t index, const size_t count);

//insert count element from index, auto resize and copy value if exists
//char* a = VECTOR(char, 3);
//vector_insert(&a, 0, "hello", strlen("hello");
////['h','e','l','l','o']
//vector_insert(&a, 4, NULL, 1);
//a[4] = ' ';
/////['h','e','l','l',' ','o']
//return &v[index] or NULL if index > count, insert to index==count is same to call push
void* vector_insert(void* v, const size_t index, void* value, const size_t count);

//push one element to the end of vector, auto upsize, copy element if not NULL return &v[ve->count]
void* vector_push(void* restrict v, void* restrict element);

//same push, increase count and upsize, but return last index where to write
//size_t last = vector_fetch(&a);
//a[last] = 0;
size_t vector_fetch(void* restrict v);

//extract last element, if ret value is copied, if not elements return NULL othervwise *v
void* vector_pop(void* v, __out void* ret);

//shuffle vector from begin to end included, if end == 0 end setted to vector_count
void vector_shuffle(void* v, size_t begin, size_t end);

//sorting vector
void vector_qsort(void* v, qsort_f fn);

#endif 
