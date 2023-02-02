#ifndef __NOTSTD_CORE_MEMORY_H__
#define __NOTSTD_CORE_MEMORY_H__

#include <stdint.h>
#include <stddef.h>
#include <notstd/core/compiler.h>

//is not threads safe

//calcolate header size
#define MEM_HEADER_SIZE(NAME,EXTEND) (48 + ROUND_UP(NAME, sizeof(uintptr_t)) + ROUND_UP(NAME, sizeof(uintptr_t)))

//mem_alloc flags
//allocate in page
#define MEM_FLAG_PAGE     0x01
//are shared
#define MEM_FLAG_SHARED   0x02
//are virtual address(see mem_alloc doc)
#define MEM_FLAG_VIRTUAL  0x04

//mem_protect flags
//protect memory for read
#define MEM_PROTECT_READ  0x01
//protect memory for writing
#define MEM_PROTECT_WRITE 0x02

//raii attribute for cleanup and free memory allocated with mem_alloc
#define __free __cleanup(mem_free_raii)

//macro for simple use mem_alloc,
//T is type, int, char, etc
//C is count, the numbers of object to allocate
//S is Size of object to be allocate
//ADDR is original address of memory
//M is memory allocated with mem_alloc
//
//macro type safe for allocate multiple type of object, examples array of 12 int
//int* array = MANY(int, 12);
#define MANY(T,C)           (T*)mem_alloc(sizeof(T)*(C), 0, 0, NULL, 0, NULL)
//same MANY but only for one object
#define NEW(T)              MANY(T,1)
//same NEW but allocate in PAGE
#define PAGE(S)             mem_alloc((S), 0, MEM_FLAG_PAGE, NULL, 0, NULL)
//same PAGE but are shared
#define SHARED(S)           mem_alloc((S), 0, MEM_FLAG_SHARED, NULL, 0, NULL)
//same SHARED but memory have a name for attach from others process
#define NAMED(S, NAME, PRV) mem_alloc((S), 0, MEM_FLAG_SHARED, NAME, PRV, NULL)
//create an header memory inside other memory
#define VIRTUAL(T,C,ADDR)     (T*)mem_alloc(sizeof(T)*(C), MEM_FLAG_VIRTUAL, NULL, 0, ADDR)
//same mem_free but set ptr to null
#define DELETE(M)           ({ mem_free(M); (M)=NULL; NULL; })
//realloc type safe
#define RESIZE(T,M,C)       (T*)mem_realloc((M), sizeof(T)*(C))

//classic way for swap any value
#define swap(A,B) ({ typeof(A) tmp = A; (A) = (B); (B) = tmp; })

//callback type for cleanup object
typedef void (*mcleanup_f)(void*);

//protection object
typedef struct mProtect{
	void* raw;
	size_t size;
	int key;
}mProtect_s;

//superblock object
typedef struct superblocks superblocks_t;

/**************/
/*** page.c ***/
/**************/

#ifndef PAGE_IMPLEMENT
//get size of page, fill in __ctor with OS_PAGE_SIZE
extern size_t PAGE_SIZE;
#endif

//os abstraction for allocate memory in page, you know as mmap 
//this is automatic called in __ctor core not need you
void page_begin(void);
//allocate memory size in page
__malloc void* page_alloc(size_t size);
//same but shared
__malloc void* page_shared(size_t size);
//attach to memory named
__malloc void* page_attach(size_t size, const char* name);
//create a memory named
__malloc void* page_named(size_t size, const char* name, unsigned prv);
//realloc a page
__malloc void* page_realloc(void* page, size_t size, size_t newsize);
//free a page
void page_free(void* page, size_t size);

/************/
/* memory.c */
/************/

//all notstd use always this memory manager
//
//allocate memory
//size, how many memory want in bytes
//extend, size for extra headers info
//flasg, where && how allocate memory
//name, you can add string name to memory but probably is only used with MEM_FLAG_SHARED for non anonymous shared memory
//prv, used only for privileges of shared memory
//virt, is address of memory you need transform, for examples if you need to use with stack object:
//
//#define N 10
//uint8_t memraw(MEM_HEADER_SIZE(0,0)+sizeof(int)*N);
//int* stackarray = VIRTUAL(int, N, memraw);
//
//now can use all mem_* function on stackarray, is helpfull when add different type of object in datastructre
//mem_alloc alway return an valid address otherwise die
__malloc void* mem_alloc(size_t size, unsigned extend, unsigned flags, const char* name, unsigned prv, void* virt);

//nothing special, simple realloc  if called on VIRTUAL die, same mem_alloc always return a valid address, otherwise die
void* mem_realloc(void* mem, size_t size);

//borrowed memory to others memory, in big software it happens that multiple datastructure share same object.
//in this situations is to hard decide who, where and when deallocate object, this function try to solve problem
//you can tell memory which has been loaned to others and need to kwnow when free.
//i think an exaples is more expressive, try to allocate 2 matrix of string that shared same string for eache element
//classic way:
//#define N 10
//#define NUMMAX 32
//char** matrixA = malloc(sizeof(char*)*N);
//char** matrixB = malloc(sizeof(char*)*N);
//for( unsigned i = 0; i < N; ++i ){
//	char* str = malloc(sizeof(char)*NUMMAX);
//	sprintf(str, "%u", i);
//	matrixA[i] = str;
//	matrixB[i] = str;
//}
//at this point each line of matrix A && B share same ptr, you need to decide how free memory, event if its simples in this case
//down in the real world things never are.
//with borrowed is more simple see this:
//#define N 10
//#define NUMMAX 32
//char** matrixA = MANY(char*, N);
//char** matrixB = MANY(char*, N);
//for( unsigned i = 0; i < N; ++i ){
//	char* str = MANY(char, NUMNAX);
//	sprintf(str, "%u", i);
//	matrixA[i] = str;
//	mem_borrowed(str, matrixA);
//	matrixB[i] = str;
//	mem_borrowed(str, matrixB);
//	mem_free(str); // i can, i need, free because not more used str allocated with MANY, i have borrowed to matrixA and B
//}
//
//mem_free(matrixA); //release all memory used for matrix A and give back memory
//mem_free(matrixB)l //release all memory used, at this point matrixB are owner of string and automatic free all
//
//same code but more elegant:
//#define N 10
//#define NUMMAX 32
//void foo(){
//	__free char** matrixA = MANY(char*, N);
//	__free char** matrixB = MANY(char*, N);
//	for( unsigned i = 0; i < N; ++i ){
//		__free char* str = MANY(char, NUMNAX);
//		sprintf(str, "%u", i);
//		matrixA[i] = mem_borrowed(str, matrixA);
//		matrixB[i] = mem_borrowed(str, matrixB);
//	}
//}
//you can really forget about deallocated will do it by itsel
void* mem_borrowed(void* child, void* parent);

//free gift your memory, lol
//this works same borrowed but you dont need to free memory
//void foo(){
//	__free char** matrixA = MANY(char*, N);
//	for( unsigned i = 0; i < N; ++i ){
//		matrixA[i] = mem_gift(MANY(char*,NUMMAX), matrixA);
//		sprintf(matrixA[i], "%u", i);
//	}
//}
//warning, if you not borrowed or gift a memory, the parent memory can't free your child, for this used gift in this exaples
void* mem_gift(void* child, void* parent);

//after borrowed memory, may want to release before free parent memory, this special case can't be use directly mem_free
//give remove reference to memory, and return this, you need free if not more use 
//
//char* imuglyman;
//
//void foo(){
//	__free char** matrixA = MANY(char*, N);
//	for( unsigned i = 0; i < N; ++i ){
//		matrixA[i] = mem_gift(MANY(char*,NUMMAX), matrixA);
//		sprintf(matrixA[i], "%u", i);
//	}
//  for( unsigned i = 0; i < N; ++i ){
//		if( !strcmp(matrixA[i], "666"){
//			imuglyman = mem_give(matrixA[i], matrixA);
//			matrixA[i] = NULL;
//		}
//	}
//}
//at this point only memory give arent free, "666" alive in global variable others are free
//
//anothers more complex examples, two matrix share same address, but one remove one object
//#define N 700
//#define NUMMAX 32
//void foo(){
//	__free char** matrixA = MANY(char*, N);
//	__free char** matrixB = MANY(char*, N);
//	for( unsigned i = 0; i < N; ++i ){
//		__free char* str = MANY(char, NUMNAX);
//		sprintf(str, "%u", i);
//		matrixA[i] = mem_borrowed(str, matrixA);
//		matrixB[i] = mem_borrowed(str, matrixB);
//	}
//  for( unsigned i = 0; i < N; ++i ){
//		if( !strcmp(matrixA[i], "666"){
//			mem_free(mem_give(matrixA[i], matrixA)); // matrix A not need more this object and decide to remove and destroy
//			matrixA[i] = NULL;
//		}
//	}
//	for( unsigned i = 0; i < N; ++i ){
//		if( !strcmp(matrixB[i], "666"){
//			puts(matrixB[i]); // matrix B can use object because exists
//		}
//	}
//}
//now all memory are automatic release
void* mem_give(void* child, void* parent);

//you know free? :)
void mem_free(void* addr);

//not use this, this is used for __free
void mem_free_raii(void* addr);

//return real memory size
size_t mem_size_raw(void* addr);

//lock memory for read, all threads can read but nobody can write
int mem_lock_read(void* addr);
//lock memory for writing, wait all threads stop reading, only one can write
int mem_lock_write(void* addr);
//unlock read/write
int mem_unlock(void* addr);
//wow macro for use lock, exaples:
//int *a = NEW(int);
//mem_acquire_read(a){
//	//now all threads can only reads
//	printf("%d", a);
//}
//mem_acquire_write(a){
//	//wait that nobody threads are reading
//	*a = 1;
//}
#define mem_acquire_read(ADDR) for(int __acquire__ = mem_lock_read(ADDR); __acquire__; __acquire__ = 0, mem_unlock(ADDR) )
#define mem_acquire_write(ADDR) for(int __acquire__ = mem_lock_write(ADDR); __acquire__; __acquire__ = 0, mem_unlock(ADDR) )

//simple check for validation memory exaples you can write
//iassert(mem_check(mem));
//to make sure it was allocated with mem_alloc(sizeof(double), 
int mem_check(void* addr);

//return size in byte of memory
size_t mem_size(void* addr);

//memset(0)
void mem_zero(void* addr);

//return raw address of memory
void* mem_raw(void* addr);

//return memory name
const char* mem_name(void* addr);

//extend your memory headers
//if have set size in mem_alloc extend, can get this section for all you want, exaples
//struct mystruct{
//	int a;
//};
//
//double* mem = mem_alloc(sizeof(double), sizeof(struct mystruct), 0, 0, 0, 0);
//struct mystruct* ex = mem_extend(mem);
//ex->a = 1;
void* mem_extend(void* addr);

//setup cleanup function, this is called before mem_free
//void clean(void* mem){
//	printf("after this free %p", mem);
//}
//
//int* a = NEW(int);
//mem_cleanup(a, clean);
void mem_cleanup(void* addr, mcleanup_f fn);

//unlink shared memory
void mem_shared_unlink(void* addr, int mode);

/*************/
/* protect.c */
/*************/

mProtect_s* mem_protect_ctor(mProtect_s* mp, void* addr);
mProtect_s* mem_protect_dtor(mProtect_s* mp);
mProtect_s* mem_protect(mProtect_s* mp, unsigned mode);

/************/
/* extras.c */
/************/

//swap memory region, size is how many memory you need to swap, warning size is not checked, -1 if !a||!b||!sizeA||!sizeB
int memswap(void* restrict a, size_t sizeA, void* restrict b, size_t sizeB);

//call memswap but check size, return same error of memswap + mem_size(a) < szb || mem_size(b) < sza
int mem_swap(void* restrict a, size_t sza, void* restrict b, size_t szb);


#endif
