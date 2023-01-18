#ifndef __NOTSTD_CORE_MEMORY_H__
#define __NOTSTD_CORE_MEMORY_H__

#include <stdint.h>
#include <stddef.h>
#include <notstd/core/compiler.h>

#define MEM_FLAG_PAGE     0x01
#define MEM_FLAG_SHARED   0x02
#define MEM_FLAG_VIRTUAL  0x04
#define MEM_PROTECT_READ  0x01
#define MEM_PROTECT_WRITE 0x02

#define __free __cleanup(mem_free_raii)

#define MANY(T,C)           (T*)mem_alloc(sizeof(T)*(C), 0, 0, NULL, 0, NULL)
#define NEW(T)              MANY(T,1)
#define PAGE(S)             mem_alloc((S), 0, MEM_FLAG_PAGE, NULL, 0, NULL)
#define SHARED(S)           mem_alloc((S), 0, MEM_FLAG_SHARED, NULL, 0, NULL)
#define NAMED(S, NAME, PRV) mem_alloc((S), 0, MEM_FLAG_SHARED, NAME, PRV, NULL)
#define VIRTUAL(T,C,ADDR)     (T*)mem_alloc(sizeof(T)*(C), MEM_FLAG_VIRTUAL, NULL, 0, ADDR)
#define DELETE(M)           do{ mem_free(M); (M)=NULL; }while(0)
#define RESIZE(T,A,C)       (T*)mem_realloc((A), sizeof(T)*(C))

#define swap(A,B) ({ typeof(A) tmp = A; (A) = (B); (B) = tmp; })

typedef void (*mcleanup_f)(void*);

typedef struct mProtect{
	void* raw;
	size_t size;
	int key;
}mProtect_s;

typedef struct superblocks superblocks_s;

/**************/
/*** page.c ***/
/**************/

#ifndef PAGE_IMPLEMENT
extern size_t PAGE_SIZE;
#endif

void page_begin(void);
__malloc void* page_alloc(size_t size);
__malloc void* page_shared(size_t size);
__malloc void* page_attach(size_t size, const char* name);
__malloc void* page_named(size_t size, const char* name, unsigned prv);
__malloc void* page_realloc(void* page, size_t size, size_t newsize);
void page_free(void* page, size_t size);

/************/
/* memory.c */
/************/
__malloc void* mem_alloc(size_t size, unsigned extend, unsigned flags, const char* name, unsigned prv, void* virt);
void* mem_realloc(void* mem, size_t size);
void* mem_link(void* child, void* parent);
void* mem_unlink(void* child, void* parent);
void mem_free(void* addr);
void mem_free_raii(void* addr);
size_t mem_size_raw(void* addr);
int mem_lock_read(void* addr);
int mem_lock_write(void* addr);
int mem_unlock(void* addr);
int mem_check(void* addr);
size_t mem_size(void* addr);
void mem_zero(void* addr);
void* mem_raw(void* addr);
const char* mem_name(void* addr);
void* mem_extend(void* addr);
void mem_cleanup(void* addr, mcleanup_f fn);
void mem_shared_unlink(void* addr, int mode);

superblocks_s* msb_new(unsigned sof, unsigned count, mcleanup_f clean);
__malloc void* msb_alloc(superblocks_s* sb);

#define mem_acquire_read(ADDR) for(int __acquire__ = mem_lock_read(ADDR); __acquire__; __acquire__ = 0, mem_unlock(ADDR) )
#define mem_acquire_write(ADDR) for(int __acquire__ = mem_lock_write(ADDR); __acquire__; __acquire__ = 0, mem_unlock(ADDR) )

/*************/
/* protect.c */
/*************/
mProtect_s* mem_protect_ctor(mProtect_s* mp, void* addr);
mProtect_s* mem_protect_dtor(mProtect_s* mp);
mProtect_s* mem_protect(mProtect_s* mp, unsigned mode);

/************/
/* extras.c */
/************/
int memswap(void* restrict a, size_t sizeA, void* restrict b, size_t sizeB);
int mem_swap(void* restrict a, void* restrict b);


#endif
