#ifndef MEMORY_DEBUG
#undef DBG_ENABLE
#endif

#include <notstd/core.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

mProtect_s* mem_protect_ctor(mProtect_s* mp, void* addr){
	mp->raw = mem_raw(addr);
	mp->size = mem_size_raw(addr);
	mp->key = pkey_alloc(0, 0);
	if( mp->key > 0 ){
		dbg_info("use hw key");
		pkey_mprotect(mp->raw, mp->size, PROT_READ | PROT_WRITE, mp->key);
	}
	return mp;
}

mProtect_s* mem_protect_dtor(mProtect_s* mp){
	if( mp->key > 0 ){
		dbg_info("destroy hw key");
		pkey_free(mp->key);
		mp->key = -1;
	}
	return mp;
}

mProtect_s* mem_protect(mProtect_s* mp, unsigned mode){
	if( mp->key > 0 ){
		unsigned flags = 0;
		if( mode & MEM_PROTECT_WRITE ) flags = PKEY_DISABLE_WRITE;
		if( mode & MEM_PROTECT_READ ) flags = PKEY_DISABLE_ACCESS;
		pkey_set(mp->key, flags);
	}
	else{
		unsigned flags = PROT_READ | PROT_WRITE;
		if( mode & MEM_PROTECT_READ ) flags &= ~PROT_READ;
		if( mode & MEM_PROTECT_WRITE ) flags &= ~PROT_WRITE;
		mprotect(mp->raw, mp->size, flags);
	}
	return mp;
}

