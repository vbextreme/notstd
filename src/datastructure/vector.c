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
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	ve->count = 0;
}

void* vector_resize(void* v, size_t count){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	ve->max = count;
	if( ve->count > ve->max ) ve->count = ve->max;
	*pv = mem_realloc(v, ve->max * ve->sof);
	return *pv;
}

void* vector_upsize(void* v, size_t count){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	UPSIZE(ve, *pv, count);
	return *pv;
}

void* vector_reserve(void* v, size_t count){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	UPSIZE(ve, pv, count);
	ve->count += count;
	return *pv;
}

void* vector_shrink(void* v){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	if( ve->max - ve->count > ve->max / 4 ){
		ve->max -= ve->max / 4;
		*pv = mem_realloc(*pv, ve->max * ve->sof);
	}
	return *pv;
}

void* vector_remove(void* v, const size_t index, const size_t count){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);

	if( ! count ) return *pv;

	if( index + count > ve->count){
		dbg_warning("index out of bound");	
		return NULL;
	}

	if( index == ve->count - 1 ){
		--ve->count;
		return vector_shrink(v);
	}

	void* dst = (void*)(ADDR(*pv) + index * ve->sof);
	const void* src = (void*)(ADDR(*pv) + (index+count) * ve->sof);
	const size_t mem = (ve->count - (index+count)) * ve->sof;
	iassert( ADDR(src) + mem <= ADDR(*pv) + ve->count * ve->sof );
	memmove(dst, src, mem);
	ve->count -= count;

	return vector_shrink(v);
}

void* vector_insert(void* v, const size_t index, void* value, const size_t count){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(v);

	if( index > ve->count){
		dbg_warning("index out of bound");	
		return NULL;
	}

	ve = mem_extend(v);
	UPSIZE(ve, *pv, count);

	void* dst = (void*)(ADDR(*pv) + (index + count) * ve->sof);

	if( index < ve->count ){
		const void* src = (void*)(ADDR(*pv) + index * ve->sof);
		const size_t mem = count * ve->sof;
		memmove(dst, src, mem);
	}
	
	if( value ){
		memcpy(dst, value, count * ve->sof);
	}
	
	ve->count += count;
	return dst;
}

void* vector_push(void* restrict v, void* restrict element){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	UPSIZE(ve, *pv, 1);
	void* dst = (void*)(ADDR(*pv) + ve->count * ve->sof);
	if( element ) memcpy(dst, element, ve->sof);
	++ve->count;
	return dst;
}

size_t vector_fetch(void* restrict v){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	UPSIZE(ve, *pv, 1);
	return ve->count++;
}

void* vector_pop(void* v, __out void* ret){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	if( ve->count == 0 ) return NULL;
	--ve->count;
	if( ret ){
		void* src = (void*)(ADDR(*pv) + ve->count * ve->sof);
		memcpy(ret, src, ve->sof);
	}
	return vector_shrink(v);
}

void vector_shuffle(void* v, size_t begin, size_t end){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);

	if( end == 0 ) end = ve->count-1;
	if( end == 0 ) return;

	const size_t count = (end - begin) + 1;
	for( size_t i = begin; i <= end; ++i ){
		size_t j = begin + mth_random(count);
		if( j != i ){
			void* a = (void*)(ADDR(*pv) + i * ve->sof);
			void* b = (void*)(ADDR(*pv) + j * ve->sof);
			memswap(a , ve->sof, b, ve->sof);
		}
	}
}

void vector_qsort(void* v, cmp_f fn){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	qsort(*pv, ve->count, ve->sof, fn);
}

void* vector_bsearch(void* v, void* search, cmp_f fn){
	void** pv = v;
	iassert(pv && *pv);
	vextend_s* ve = mem_extend(*pv);
	return bsearch(search, *pv, ve->count, ve->sof, fn);
}
