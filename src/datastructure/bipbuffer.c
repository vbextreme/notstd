#include <notstd/bipbuffer.h>

struct bipbuffer{
	void*    mem;
	__atomic unsigned r;
	__atomic unsigned w;
	unsigned max;
	unsigned sof;
};

bipbuffer_t* bipbuffer_new(unsigned sof, unsigned max){
	max = ROUND_UP_POW_TWO32(max);
	bipbuffer_t* bb = NEW(bipbuffer_t);
	bb->mem  = mem_gift(mem_alloc(sizeof(char)*(sof*max), 0, 0, NULL, 0, NULL), bb);
	bb->max  = max;
	bb->sof  = sof;
	bb->r    = 0;
	bb->w    = 0;
	return bb;
}

int bipbuffer_empty(bipbuffer_t* bb){
	return bb->r == bb->w;
}

int bipbuffer_full(bipbuffer_t* bb){
	return FAST_MOD_POW_TWO(bb->w + 1, bb->max) == bb->r;
}

void bipbuffer_clear(bipbuffer_t* bb){
	bb->r = bb->w = 0;
}

__private unsigned bb_w_available(bipbuffer_t* bb){
	return bb->w >= bb->r ? bb->max - bb->w: bb->r - (bb->w+1);
}

__private unsigned bb_r_available(bipbuffer_t* bb){
	return bb->r > bb->w ? bb->max - bb->r: bb->w - bb->r;
}

__private void* bb_itoaddr(unsigned i, void* addr, unsigned sof){
	return (void*)(ADDR(addr)+i*sof);
}

void* bipbuffer_fill(bipbuffer_t* bb, __out unsigned* count){
	*count = bb_w_available(bb);
	return bb_itoaddr(bb->w, bb->mem, bb->sof);
}

unsigned bipbuffer_fill_commit(bipbuffer_t* bb, unsigned count){
	unsigned available = bb_w_available(bb);
	if( available < count ) count = available;
	bb->w = FAST_MOD_POW_TWO(bb->w + count, bb->max);
	//dbg_info("W:%u", bb->w);
	return count;
}

void* bipbuffer_read(bipbuffer_t* bb, __out unsigned* count){
	*count = bb_r_available(bb);
	return bb_itoaddr(bb->r, bb->mem, bb->sof);
}

unsigned bipbuffer_read_commit(bipbuffer_t* bb, unsigned count){
	unsigned available = bb_r_available(bb);
	if( available < count ) count = available;
	bb->r = FAST_MOD_POW_TWO(bb->r + count, bb->max);
	//dbg_info("R:%u", bb->r);
	return count;
}

