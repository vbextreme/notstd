#undef DBG_ENABLE
#define PAGE_IMPLEMENT
#include <notstd/core.h>

#include <sys/mman.h>
#include <linux/mman.h>

size_t PAGE_SIZE;

void page_begin(void){
	PAGE_SIZE = OS_PAGE_SIZE;
}

__malloc void* page_alloc(size_t size){
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if( addr == MAP_FAILED ) die("page alloc mmap error:%m");
	dbg_info("page alloc: %p", addr);
	return addr;
}

__malloc void* page_shared(size_t size){
	size = ROUND_UP(size, PAGE_SIZE);
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if( addr == MAP_FAILED ) die("page shared mmap error:%m");
	dbg_info("page shared: %p", addr);
	return addr;
}

__malloc void* page_attach(size_t size, const char* name){
	int fm = shm_open(name, O_RDWR, 0);
	if( fm == -1 ) die("unable to create/attach shared mem '%s':%m", name);
	struct stat ss;
	if( fstat(fm, &ss) == -1 ){
		close(fm);
		die("unable to attach shared mem '%s':%m", name);
	}
	if( (long)size != ss.st_size ) die("try attach memory '%s' with different size", name);
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fm, 0);
	if( addr == MAP_FAILED ) die("mmap error:%m");
	close(fm);
	dbg_info("page attach(%s): %p", name, addr);
	return addr;
}

__malloc void* page_named(size_t size, const char* name, unsigned prv){
	int fm = shm_open(name, O_CREAT | O_EXCL | O_RDWR, prv);
	if( fm == -1 ) return page_attach(size, name);
	if( ftruncate(fm, size) == -1 ){
		close(fm);
		shm_unlink(name);
		die("unable to create shared mem '%s':%m", name);
	}
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fm, 0);
	if( addr == MAP_FAILED ) die("mmap error:%m");
	close(fm);
	dbg_info("page named(%s): %p", name, addr);
	return addr;
}

__malloc void* page_realloc(void* page, size_t size, size_t newsize){
	newsize = ROUND_UP(newsize, PAGE_SIZE);
	if( size == newsize ) return page;
	void* addr = mremap(page, size, newsize, MREMAP_MAYMOVE);
	if( addr == MAP_FAILED ) die("mremap error:%m");
	dbg_info("page realloc: %p", addr);
	return addr;
}

void page_free(void* page, size_t size){
	dbg_info("page free: %p", page);
	munmap(page, size);
}



