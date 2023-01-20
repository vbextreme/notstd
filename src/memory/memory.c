#include <notstd/core.h>
#include <notstd/futex.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#define HMEM_FLAG_PAGE       0x00000001
#define HMEM_FLAG_SHARED     0x00000002
#define HMEM_FLAG_UNLINK     0x00000004
#define HMEM_FLAG_VIRTUAL    0x00000008
#define HMEM_FLAG_CHECK      0xF1CA0000

#define HMEM_CHECK(HM) (((HM)->flags & 0xFFFF0000) == HMEM_FLAG_CHECK)
#define HMEM_PAGE(HM)  (void*)(ADDR(HM) - ((HM)->extend+(HM)->name))
#define HMEM_MEM(HM)   (void*)(ADDR(HM) + sizeof(hmem_s))
#define MEM_HMEM(A)    (hmem_s*)(ADDR(A) - sizeof(hmem_s))

typedef struct hmem hmem_s;

typedef struct memLink_s{
	struct memLink_s* next;
	hmem_s* hm;
}memLink_s;

//48b
typedef struct hmem{
	uint32_t name;
	uint32_t extend;
	mcleanup_f cleanup;
	memLink_s* childs;
	uint64_t refs;
	int32_t lock;
	uint32_t flags;
	uint64_t size;
}hmem_s;


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// lock multiple reader one writer fork https://gist.github.com/smokku/653c469d695d60be4fe8170630ba8205 

#define LOCK_OPEN    1
#define LOCK_WLOCKED 0

__private void lock_ctor(hmem_s* hm){
	hm->lock = LOCK_OPEN;
}

__private void unlock(hmem_s* hm){
	int32_t current, wanted;
	do {
		current = hm->lock;
        if( current == LOCK_OPEN ) return;
		wanted = current == LOCK_WLOCKED ? LOCK_OPEN : current - 1;
	}while( __sync_val_compare_and_swap(&hm->lock, current, wanted) != current );
	futex(&hm->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
}

__private void lock_read(hmem_s* hm){
    int32_t current;
	while( (current = hm->lock) == LOCK_WLOCKED || __sync_val_compare_and_swap(&hm->lock, current, current + 1) != current ){
		while( futex(&hm->lock, FUTEX_WAIT, current, NULL, NULL, 0) != 0 ){
            cpu_relax();
            if (hm->lock >= LOCK_OPEN) break;
		}
	}
}

__private void lock_write(hmem_s* hm){
	unsigned current;
	while( (current = __sync_val_compare_and_swap(&hm->lock, LOCK_OPEN, LOCK_WLOCKED)) != LOCK_OPEN ){
		while( futex(&hm->lock, FUTEX_WAIT, current, NULL, NULL, 0) != 0 ){
			cpu_relax();
			if( hm->lock == LOCK_OPEN ) break;
		}
		if( hm->lock != LOCK_OPEN ){
			futex(&hm->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// memory manager


__private void* memory_page(size_t size, unsigned flags, int file){
	void* page = mmap(NULL, size,  PROT_READ | PROT_WRITE, flags, file, 0);
	if( page == MAP_FAILED ) die("mmap error:%m");
	return page;
}

__private int memory_open(int* attach, size_t size, const char* name, unsigned prv){
	int fm = shm_open(name, O_CREAT | O_EXCL | O_RDWR, prv);
	if( fm == -1 ){
		//memory exists, retry and attach
		fm = shm_open(name, O_RDWR, 0);
		if( fm == -1 ) die("unable to create/attach shared mem '%s':%m", name);
		struct stat ss;
		if( fstat(fm, &ss) == -1 ){
			close(fm);
			die("unable to attach shared mem '%s':%m", name);
		}
		if( (long)size != ss.st_size ) die("try attach memory '%s' with different size", name);
		*attach = 1;
	}
	else{
		if( ftruncate(fm, size) == -1 ){
			close(fm);
			shm_unlink(name);
			die("unable to create shared mem '%s':%m", name);
		}
		*attach = 0;
	}
	return fm;
}

__malloc void* mem_alloc(size_t size, unsigned extend, unsigned flags, const char* name, unsigned prv, void* virt){
	//calcolate size
	
	if( extend ) extend = ROUND_UP(extend, sizeof(uintptr_t));
	unsigned len = 0;
	if( name ){
		len = strlen(name) + 1;
		len = ROUND_UP(len, sizeof(uintptr_t));
	}
	size += sizeof(hmem_s) + len + extend;

	//allocate raw memory
	void* raw = NULL;
	unsigned hflags = HMEM_FLAG_CHECK;

	if( flags & MEM_FLAG_PAGE ){
		size = ROUND_UP(size, PAGE_SIZE);
		raw = page_alloc(size);
		hflags |= HMEM_FLAG_PAGE;
	}
	else if( flags & MEM_FLAG_SHARED ){
		size = ROUND_UP(size, PAGE_SIZE);
		hflags |= HMEM_FLAG_SHARED;
		if( name ){
			int attach = 0;
			int fd = memory_open(&attach, size, name, prv);
			if( !attach ) hflags |= HMEM_FLAG_UNLINK;
			raw = memory_page(size, MAP_SHARED, fd);
			close(fd);
		}
		else{
			raw = page_shared(size);
		}
	}
	else if( flags & MEM_FLAG_VIRTUAL ){
		hflags |= HMEM_FLAG_VIRTUAL;
		size = ROUND_UP(size, sizeof(uintptr_t));
		raw  = virt;
	}
	else{
		size = ROUND_UP(size, sizeof(uintptr_t));
		raw = malloc(size);
	}

	hmem_s* hm = (hmem_s*)(ADDR(raw) + extend + len);	
	hm->extend  = extend;
	hm->name    = len;
	hm->cleanup = NULL;
	hm->refs    = 1;
	hm->childs  = NULL;
	hm->flags   = hflags;
	hm->size    = size;
	lock_ctor(hm);
	if( name ) strcpy(raw, name);

	iassert( ADDR(HMEM_MEM(hm)) % sizeof(uintptr_t) == 0 );
	return HMEM_MEM(hm);
}

void* mem_realloc(void* mem, size_t size){
	hmem_s* hm = MEM_HMEM(mem);
	iassert( HMEM_CHECK(hm) );
	void* raw = HMEM_PAGE(hm);
	const unsigned len = hm->name;
	const unsigned ext = hm->extend;
	if( hm->flags & MEM_FLAG_VIRTUAL ) die("unable to resize virtual memory");

	size += sizeof(hmem_s) + len + ext;
		
	if( hm->flags & HMEM_FLAG_PAGE ){
		size = ROUND_UP(size, PAGE_SIZE);
		if( size == hm->size ) return mem;
		raw = page_realloc(raw, hm->size, size);
	}
	else if( hm->flags & HMEM_FLAG_SHARED ){
		size = ROUND_UP(size, PAGE_SIZE);
		if( hm->name ){
			die("not supported for now");
		}
		else{
			raw = page_realloc(raw, hm->size, size);
		}
	}
	else{
		size = ROUND_UP(size, sizeof(uintptr_t));
		raw = realloc(raw, size);
		if( !raw ) die("on realloc: %m");
	}

	hm = (hmem_s*)(ADDR(raw) + ext + len);	
	hm->size = size;

	iassert( ADDR(HMEM_MEM(hm)) % sizeof(uintptr_t) == 0 );
	return HMEM_MEM(hm);
}

void* mem_borrowed(void* child, void* parent){
	hmem_s* hchild = MEM_HMEM(child);
	hmem_s* hparent = MEM_HMEM(parent);
	iassert( HMEM_CHECK(hchild) );
	iassert( HMEM_CHECK(hparent));

	++hchild->refs;
	memLink_s* ml = malloc(sizeof(memLink_s));
	ml->hm = hchild;
	ml->next = hparent->childs;
	hparent->childs = ml;

	return child;
}

void* mem_gift(void* child, void* parent){
	hmem_s* hchild = MEM_HMEM(child);
	hmem_s* hparent = MEM_HMEM(parent);
	iassert( HMEM_CHECK(hchild) );
	iassert( HMEM_CHECK(hparent));

	memLink_s* ml = malloc(sizeof(memLink_s));
	ml->hm = hchild;
	ml->next = hparent->childs;
	hparent->childs = ml;

	return child;
}

void* mem_give(void* child, void* parent){
	hmem_s* hchild = MEM_HMEM(child);
	hmem_s* hparent = MEM_HMEM(parent);
	iassert( HMEM_CHECK(hchild) );
	iassert( HMEM_CHECK(hparent));

	memLink_s** ml = &hparent->childs;
	for(; (*ml); ml = &(*ml)->next ){
		if( (*ml)->hm == hchild ){
			void* tofree = *ml;
			*ml = (*ml)->next;
			free(tofree);
			return child;
		}
	}

	die("tried to give memory that you dont have");
	return NULL;
}

__private void hmem_free(hmem_s* hm){
	// dont free if memory are references from others memory
	iassert( hm->refs );
	if( --hm->refs ) return;

	// as long the children not removed
	memLink_s* next;
	while( hm->childs ){
		next = hm->childs->next;
		memLink_s* tofree = hm->childs;
		hmem_free(hm->childs->hm);
		hm->childs = next;
		free(tofree);
	}

	void* raw = HMEM_PAGE(hm);

	if( hm->cleanup ) hm->cleanup(HMEM_MEM(hm));
	if( hm->flags & HMEM_FLAG_PAGE ){
		hm->flags = 0;
		page_free(raw, hm->size);
	}
	else if( hm->flags & HMEM_FLAG_SHARED ){
		if( hm->flags & HMEM_FLAG_UNLINK ) shm_unlink(raw);
		hm->flags = 0;
		page_free(raw, hm->size);
	}
	else if( !(hm->flags & HMEM_FLAG_VIRTUAL) ){
		hm->flags = 0;
		free(raw);
	}
}

void mem_free(void* addr){
	if( !addr ) return;
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	hmem_free(hm);
}

void mem_free_raii(void* addr){
	mem_free(*(void**)addr);
}

size_t mem_size_raw(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	return hm->size;
}

int mem_lock_read(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	lock_read(hm);
	return 1;
}

int mem_lock_write(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	lock_write(hm);
	return 1;
}

int mem_unlock(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	unlock(hm);
	return 1;
}

int mem_check(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	return HMEM_CHECK(hm);
}

size_t mem_size(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	return hm->size - (sizeof(hmem_s)+hm->name+hm->extend);
}

void mem_zero(void* addr){
	size_t size = mem_size(addr);
	memset(addr, 0, size);
}

void* mem_raw(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	return HMEM_PAGE(hm);
}

const char* mem_name(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	if( !hm->name ) return NULL;
	return HMEM_PAGE(hm);
}

void* mem_extend(void* addr){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	void* raw = HMEM_PAGE(hm);
	return (void*)(ADDR(raw)+hm->name);
}

void mem_cleanup(void* addr, mcleanup_f fn){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	hm->cleanup = fn;
}

void mem_shared_unlink(void* addr, int mode){
	hmem_s* hm = MEM_HMEM(addr);
	iassert( HMEM_CHECK(hm) );
	if( mode ){
		hm->flags |= HMEM_FLAG_UNLINK;
	}
	else{
		hm->flags &= ~HMEM_FLAG_UNLINK;
	}
}

