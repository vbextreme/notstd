#ifndef __NOTSTD_CORE_RBHASH_H__
#define __NOTSTD_CORE_RBHASH_H__

#include <notstd/core.h>

//murmur
//one_at_a_time
//fasthash

typedef uint64_t(*rbhash_f)(const void* name, size_t len);
typedef struct rbhash rbhash_t;

/*************/
/* hashalg.c */
/*************/

uint64_t hash_one_at_a_time(const void *key, size_t len);
uint64_t hash_fasthash(const void* key, size_t len);
uint64_t hash_kr(const void* key, size_t len);
uint64_t hash_sedgewicks(const void* key, size_t len);
uint64_t hash_sobel(const void* key, size_t len);
uint64_t hash_weinberger(const void* key, size_t len);
uint64_t hash_elf(const void* key, size_t len);
uint64_t hash_sdbm(const void* key, size_t len);
uint64_t hash_bernstein(const void* key, size_t len);
uint64_t hash_knuth(const void* key, size_t len);
uint64_t hash_partow(const void* key, size_t len);
uint64_t hash64_splitmix(const void* key, __unused size_t len); //only for key as 64 bit value (no strings)
uint64_t hash_murmur_oaat64(const void* key, const size_t len);
uint64_t hash_murmur_oaat32(const void* key, const size_t len);

/************/
/* rbhash.c */
/************/

rbhash_t* rbhash_new(size_t size, size_t min, size_t keysize, rbhash_f hashing);
int rbhash_addh(rbhash_t* rbh, uint64_t hash, const void* key, size_t len, void* data);
int rbhash_add(rbhash_t* rbh, const void* key, size_t len, void* data);
int rbhash_addu(rbhash_t* rbh, const void* key, size_t len, void* data);
void* rbhash_findh(rbhash_t* rbh, uint64_t hash, const void* key, size_t len);
void* rbhash_find(rbhash_t* rbh, const void* key, size_t len);
void* rbhash_findnx(rbhash_t* rbh, uint64_t hash, const void* key, size_t len, unsigned* scan);
void* rbhash_removeh(rbhash_t* rbh, uint64_t hash, const void* key, size_t len);
void* rbhash_remove(rbhash_t* ht, const void* key, size_t len);
size_t rbhash_bucket_used(rbhash_t* rbh);
size_t rbhash_bucket_count(rbhash_t* rbh);
size_t rbhash_collision(rbhash_t* rbh);
size_t rbhash_distance_max(rbhash_t* rbh);
void* rbhash_linear(rbhash_t* rbh, long* slot);


#endif
