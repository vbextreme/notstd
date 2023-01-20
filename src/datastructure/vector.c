#include <notstd/core/memory.h>
#include <notstd/vector.h>
#include <notstd/mth.h>

#define UPSIZE(VEXT, V, COUNT) do{\
	if( VEXT->count + COUNT > VEXT->max ){\
		VEXT->max = (VEXT->max+COUNT) * 2;\
		V = mem_realloc(V, VEXT->max * VEXT->sof);\
		VEXT = mem_extend(V);\
	}\
}while(0)


void* vector_new(size_t sof, size_t count){
	void* v = mem_alloc(sof * count, sizeof(vextend_s), 0, NULL, 0, NULL);
	vextend_s* ve = mem_extend(v);
	ve->count = 0;
	ve->max   = count;
	ve->sof   = sof;
	return v;
}

void vector_clear(void* v){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	ve->count = 0;
}

void* vector_resize(void* v, size_t count){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	ve->max = count;
	if( ve->count > ve->max ) ve->count = ve->max;
	return mem_realloc(v, ve->max * ve->sof);
}

void* vector_upsize(void* v, size_t count){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	UPSIZE(ve, v, count);
	return v;
}

void* vector_shrink(void* v){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	if( ve->max - ve->count > ve->max / 4 ){
		ve->max -= ve->max / 4;
		return mem_realloc(v, ve->max * ve->sof);
	}
	return v;
}

void* vector_remove(void* v, const size_t index, const size_t count){
	iassert(v);
	vextend_s* ve = mem_extend(v);

	if( index + count > ve->count){
		dbg_warning("index out of bound");	
		return v;
	}

	if( index + count < ve->count - 1 ){
		void* dst = (void*)(ADDR(v) + index * ve->sof);
		const void* src = (void*)(ADDR(v) + (index+count) * ve->sof);
		const size_t mem = (ve->count - (index+count-1)) * ve->sof;
		memmove(dst, src, mem);
	}
	ve->count -= count;
	return vector_shrink(v);
}

void* vector_add(void* v, const size_t index, const size_t count){
	iassert(v);
	vextend_s* ve = mem_extend(v);

	if( index > ve->count){
		dbg_warning("index out of bound");	
		return v;
	}

	ve = mem_extend(v);
	UPSIZE(ve, v, count);

	unsigned n = 0;
	if( index < ve->count ){
		n = index + count > ve->count ? count - ((index + count) - ve->count) : count;
	}

	if( n ){
		void* dst = (void*)(ADDR(v) + (index + n) * ve->sof);
		const void* src = (void*)(ADDR(v) + index * ve->sof);
		const size_t mem = (ve->count - (index+n)) * ve->sof;
		memmove(dst, src, mem);
	}
	
	ve->count += count;
	return v;
}

void* vector_push(void* restrict v, void* restrict element){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	UPSIZE(ve, v, 1);
	if( element ) memcpy(v + ve->count * ve->sof, element, ve->sof);
	++ve->count;
	return v;
}

void* vector_fetch(void* restrict v, __out size_t* id){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	UPSIZE(ve, v, 1);
	if( id ) *id = ve->count;
	++ve->count;
	return v;
}

void* vector_head(void* restrict v, void* restrict element){
	v = vector_add(v, 0, 1);
	memcpy(v, element, vector_sizeof(v));
	return v;
}

void* vector_pop(void* v, __out void* ret){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	--ve->count;
	if( ret ) memcpy(ret, v + ve->count * ve->sof, ve->sof);
	return vector_shrink(v);
}

void* vector_fetch_pop(void* v, size_t* it){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	--ve->count;
	if( it ) *it = ve->count ? ve->count - 1 : 0;
	return vector_shrink(v);
}

void* vector_peek(void* v){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	return v + (ve->count-1) * ve->sof;
}

void* vector_ipeek(void* v, size_t id){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	if( id >= ve->count ) id = ve->count - 1;
	return v + id * ve->sof;
}

void* vector_top(void* v, __out void* ret){	
	memcpy(ret, v, vector_sizeof(v));
	return vector_remove(v, 0, 1);
}

void* vector_pick(void* v){
	return v;
}

void vector_shuffle(void* v, size_t begin, size_t end){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	if( end == 0 ) end = ve->count-1;

	const size_t count = (end - begin) + 1;
	for( size_t i = begin; i <= end; ++i ){
		size_t j = begin + mth_random(count);
		if( j != i ){
			void* a = (void*)(ADDR(v) + i * ve->sof);
			void* b = (void*)(ADDR(v) + j * ve->sof);
			memswap(a , ve->sof, b, ve->sof);
		}
	}
}

void vector_qsort(void* v, qsort_f fn){
	iassert(v);
	vextend_s* ve = mem_extend(v);
	qsort(v, ve->count, ve->sof, fn);
}

