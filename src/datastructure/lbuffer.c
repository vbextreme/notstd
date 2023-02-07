#include <notstd/lbuffer.h>
#include <notstd/list.h>

typedef struct buffer{
	struct buffer* next;
	struct buffer* prev;
	uint8_t* mem;
	unsigned len;
	unsigned ir;
}buffer_s;

struct lbuffer{
	buffer_s* fill;
	buffer_s* ready;
	buffer_s* empty;
	size_t   total;
	unsigned size;
	unsigned emptyCount;
	unsigned readyCount;
};

__private buffer_s* b_alloc(lbuffer_t* lb){
	if( lb->empty ){
		--lb->emptyCount;
		buffer_s* b;
		if( lb->empty->next == lb->empty ){
			b = lb->empty;
			lb->empty = NULL;
		}
		else{
			b = ld_extract_preserve_next(&lb->empty);
		}
		b->ir  = 0;
		b->len = 0;
		return b;
	}
	buffer_s* n = mem_gift(NEW(buffer_s), lb);
	ld_ctor(n);
	n->ir  = 0;
	n->len = 0;
	n->mem = mem_gift(MANY(uint8_t, lb->size), n);
	return n;
}

__private void b_free(lbuffer_t* lb, buffer_s* b){
	mem_free(mem_give(b, lb));
}

__private buffer_s* lb_fill_finished(lbuffer_t* lb){
	lb->total += lb->fill->len;

	if( lb->ready ){
		ld_before(lb->ready, lb->fill);
	}
	else{
		lb->ready = lb->fill;
	}

	lb->fill = b_alloc(lb);
	++lb->readyCount;
	//dbg_info("ready %u", lb->readyCount);
	return lb->fill;
}

__private void lb_shrink(lbuffer_t* lb){
	dbg_info("empty buffers(%u) (%lu)<(%u/4)%u", lb->emptyCount, lb->total, lb->emptyCount * lb->size, (lb->emptyCount * lb->size)/4 );
	if( lb->total < (lb->emptyCount * lb->size) / 4 ){
		unsigned count = lb->emptyCount / 2;
		dbg_info("to many buffers(%u) remove %u", lb->emptyCount,  count);
		while( count --> 0 && lb->empty ){
			if( lb->empty->next == lb->empty ){
				b_free(lb, lb->empty);
				lb->empty = NULL;
			}
			else{
				buffer_s* b = ld_extract_preserve_next(&lb->empty);
				b_free(lb, b);
			}
			--lb->emptyCount;
		}
		dbg_info("now empty pages: %u", lb->emptyCount);
	}
}

__private buffer_s* lb_ready_finished(lbuffer_t* lb){
	buffer_s* e;
	if( lb->ready->next == lb->ready ){
		e = lb->ready;
		lb->ready = NULL;
	}
	else{
		e = ld_extract_preserve_next(&lb->ready);
	}

	if( lb->empty ){
		ld_before(lb->empty, e);
	}
	else{
		lb->empty = e;
	}
	
	--lb->readyCount;
	++lb->emptyCount;

	lb_shrink(lb);

	return lb->ready;
}

lbuffer_t* lbuffer_new(unsigned size){
	lbuffer_t* lb = NEW(lbuffer_t);
	lb->emptyCount = 0;
	lb->readyCount = 0;
	lb->size = size;
	lb->ready = NULL;
	lb->empty = NULL;
	lb->total = 0;
	lb->fill  = b_alloc(lb);

	return lb;
}

unsigned lbuffer_current_fill_available(lbuffer_t* lb){
	if( !lb->fill->mem ) return 0;
	return lb->size - lb->fill->len;
}

void* lbuffer_fill(lbuffer_t* lb, __out unsigned* size){
	*size = lb->size - lb->fill->len;
	return &lb->fill->mem[lb->fill->len];
}

unsigned lbuffer_fill_commit(lbuffer_t* lb, unsigned size){
	if( size > lb->size - lb->fill->len ){
		dbg_warning("try to commit more memory you have");
		size = lb->size - lb->fill->len;
	}
	lb->fill->len += size;
	if( lb->fill->len >= lb->size ) lb_fill_finished(lb);
	return size;
}

void lbuffer_fill_finished(lbuffer_t* lb){
	if( lb->fill->len ) lb_fill_finished(lb);
}

void* lbuffer_read(lbuffer_t* lb, __out unsigned* size){
	if( !lb->ready ){
		*size = 0;
		return NULL;
	}
	*size = lb->ready->len - lb->ready->ir;
	return &lb->ready->mem[lb->ready->ir];
}

unsigned lbuffer_read_commit(lbuffer_t* lb, unsigned size){
	if( !lb->ready ){
		dbg_warning("try to commit on empty buffer");
		return 0;
	}
	if( size > lb->ready->len - lb->fill->ir ){
		dbg_warning("try to commit more memory you have");
		size = lb->ready->len - lb->ready->ir;
	}
	lb->ready->ir += size;
	lb->total -= size;
	if( lb->ready->ir >= lb->ready->len ) lb_ready_finished(lb);
	return size;
}

void lbuffer_ready_finished(lbuffer_t* lb){
	if( lb->ready ) lb_ready_finished(lb);
}

size_t lbuffer_available(lbuffer_t* lb){
	return lb->total;
}




























