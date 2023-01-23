#include "notstd/core/memory.h"
#include <notstd/rbhash.h>

typedef struct rbhElement{
	void* data;        /**< user data*/
	uint64_t hash;     /**< hash */
	uint32_t len;      /**< len of key*/
	uint32_t distance; /**< distance from hash*/
	char key[];        /**< flexible key*/
}rbhElement_s;

typedef struct rbhash{
	rbhElement_s* table; /**< hash table*/
	size_t size;            /**< total bucket of table*/
	size_t elementSize;     /**< sizeof rbhashElement*/
	size_t count;           /**< bucket used*/
	size_t pmin;            /**< percentage elements of free bucket*/
	size_t min;             /**< min elements of free bucket*/
	size_t maxdistance;     /**< max distance from hash*/
	size_t keySize;         /**< key size*/
	rbhash_f hashing;       /**< function calcolate hash*/
}rbhash_t;

#define rbhash_slot(HASH,SIZE) FAST_MOD_POW_TWO(HASH, SIZE)
#define rbhash_slot_next(SLOT, SIZE) (rbhash_slot((SLOT)+1,SIZE))
#define rbhash_element_slot(TABLE, ESZ, SLOT) ((rbhElement_s*)(ADDR(TABLE) + ((ESZ) * (SLOT))))
#define rbhash_element_next(TABLE, ESZ) ((rbhElement_s*)(ADDR(TABLE) + (ESZ)))
#define rbhash_minel(RBH) (((RBH)->size * (RBH)->pmin)/100)

rbhash_t* rbhash_new(size_t size, size_t min, size_t keysize, rbhash_f hashing){
	rbhash_t* rbh = NEW(rbhash_t);
	rbh->size = ROUND_UP_POW_TWO32(size);
	rbh->pmin = min;
	rbh->min  = rbh->size - rbhash_minel(rbh);
	rbh->hashing = hashing;
	rbh->count = 0;
	rbh->maxdistance = 0;
	rbh->keySize = keysize;
	rbh->elementSize = ROUND_UP(sizeof(rbhElement_s), sizeof(void*));
	rbh->elementSize = ROUND_UP(rbh->elementSize + keysize, sizeof(void*));
	iassert((rbh->elementSize % sizeof(void*)) == 0);
	rbh->table = mem_gift(mem_alloc(rbh->elementSize * (rbh->size+1), 0, 0, NULL, 0, NULL), rbh);

	rbhElement_s* table = rbh->table;
	for( size_t i = 0; i < rbh->size; ++i, table = rbhash_element_next(table, rbh->elementSize)){
		table->len = 0;
	}
	return rbh;
}

__private void rbhash_swapdown(rbhElement_s* table, const size_t size, const size_t esize, size_t* maxdistance, rbhElement_s* nw){
	uint64_t bucket = rbhash_slot(nw->hash, size);
	rbhElement_s* tbl = rbhash_element_slot(table, esize, bucket);
	size_t i = 0;
	while( i < size && tbl->len != 0 ){
		if(  nw->distance > tbl->distance ){
			//dbg_info("swap %.*s(%p) -> %.*s(%p)", tbl->len, tbl->key, tbl->data, nw->len, nw->key, nw->data);
			memswap(tbl, sizeof(rbhElement_s) + tbl->len, nw, sizeof(rbhElement_s) + nw->len);
			//dbg_info("ok   %.*s(%p) <> %.*s(%p)", tbl->len, tbl->key, tbl->data, nw->len, nw->key, nw->data);
		}
		++nw->distance;
		bucket = rbhash_slot_next(bucket, size);
		tbl = rbhash_element_slot(table, esize, bucket);
		++i;
		if( nw->distance > *maxdistance ) *maxdistance = nw->distance;
	}
	if( tbl->len != 0 ) die("hash lose element, the hash table is to small");
	memcpy(tbl, nw, sizeof(rbhElement_s) + nw->len);
}

__private void rbhash_upsize(rbhash_t* rbh){
	if( rbh->count < rbh->min ) return;
	size_t newsize = rbh->size * 2 + rbhash_minel(rbh);
	newsize = ROUND_UP_POW_TWO32(newsize);

	rbhElement_s* newtable = mem_alloc(rbh->elementSize * (newsize+1), 0, 0, NULL, 0, NULL);
	rbhElement_s* table = newtable;
	for( size_t i = 0; i < newsize; ++i, table = rbhash_element_next(table,rbh->elementSize)){
		table->len = 0;
	}

	rbh->maxdistance = 0;
	table = rbh->table;
	for(size_t i = 0; i < rbh->size; ++i, table = rbhash_element_next(table,rbh->elementSize)){
		if( table->len == 0 ) continue;
		table->distance = 0;
		rbhash_swapdown(newtable, newsize, rbh->elementSize, &rbh->maxdistance, table);
	}
	mem_free(mem_give(rbh->table, rbh));
	rbh->table = mem_gift(newtable, rbh);
	rbh->size = newsize;
	rbh->min  = rbh->size - rbhash_minel(rbh);
}

int rbhash_addh(rbhash_t* rbh, uint64_t hash, const void* key, size_t len, void* data){
	if( len > rbh->keySize ){
		errno = EFBIG;
		return -1;
	}
	rbhElement_s* el = rbhash_element_slot(rbh->table, rbh->elementSize, rbh->size);
	el->data = data;
	el->distance = 0;
	el->hash = hash;
	el->len = len;
	memcpy(el->key, key, len);
	rbhash_swapdown(rbh->table, rbh->size, rbh->elementSize, &rbh->maxdistance, el);
	++rbh->count;
	rbhash_upsize(rbh);
	return 0;
}

int rbhash_add(rbhash_t* rbh, const void* key, size_t len, void* data){
	return rbhash_addh(rbh, rbh->hashing(key, len), key, len, data);
}

int rbhash_addu(rbhash_t* rbh, const void* key, size_t len, void* data){
	if( rbhash_find(rbh, key, len) ) return -1;
	return rbhash_add(rbh, key, len, data);
}

__private long rbhash_find_bucket(rbhash_t* rbh, uint64_t hash, const void* key, size_t len, unsigned* prevscan){
	uint64_t slot = rbhash_slot(hash, rbh->size);
	size_t maxscan;
	if( prevscan ){
		if( *prevscan >= rbh->maxdistance + 1 ){
			errno = ESRCH;
			return -1; 
		}
		slot = rbhash_slot(slot+*prevscan, rbh->size);
		maxscan = (rbh->maxdistance + 1) - *prevscan;
	}
	else{
		maxscan = rbh->maxdistance + 1;
	}

	rbhElement_s* table = rbhash_element_slot(rbh->table, rbh->elementSize, slot);
	for( unsigned iscan = 0; iscan < maxscan; ++iscan ){
		//dbg_info("slot:%lu", slot);
		if( table->len == len && table->hash == hash && !memcmp(key, table->key, len) ){
			if( prevscan ) *prevscan += iscan;
			//dbg_info("onslot:%lu %.*s == %.*s", slot, (int)len, (char*)key, (int)len, (char*)table->key);
			return slot;
		}
		slot = rbhash_slot_next(slot, rbh->size);
		table = rbhash_element_slot(rbh->table, rbh->elementSize, slot);
	}
	dbg_error("ESRCH");
	errno = ESRCH;
	return -1;
}

__private rbhElement_s* rbhash_find_hash_raw(rbhash_t* rbh, uint64_t hash, const void* key, size_t len){
	long bucket;
	if( (bucket = rbhash_find_bucket(rbh, hash, key, len, NULL)) == -1 ) return NULL;
	return rbhash_element_slot(rbh->table, rbh->elementSize, bucket);
}

void* rbhash_findh(rbhash_t* rbh, uint64_t hash, const void* key, size_t len){
	rbhElement_s* el = rbhash_find_hash_raw(rbh, hash, key, len);
	return el ? el->data : NULL;
}

void* rbhash_find(rbhash_t* rbh, const void* key, size_t len){
	return rbhash_findh(rbh, rbh->hashing(key, len), key, len);
}

void* rbhash_findnx(rbhash_t* rbh, uint64_t hash, const void* key, size_t len, unsigned* scan){
	long bucket;
	if( (bucket = rbhash_find_bucket(rbh, hash, key, len, scan)) == -1 ) return NULL;
	return rbhash_element_slot(rbh->table, rbh->elementSize, bucket);

}

__private void rbhash_swapup(rbhElement_s* table, size_t size, size_t esize, size_t bucket){
	size_t bucketfit = bucket;
	bucket = rbhash_slot_next(bucket, size);
	rbhElement_s* tbl = rbhash_element_slot(table, esize, bucket);

	while( tbl->len != 0 && tbl->distance ){
		rbhElement_s* tblfit = rbhash_element_slot(table, esize, bucketfit);
		memcpy(tblfit, tbl, esize);
		tbl->len = 0;
		tbl->data = NULL;
		bucketfit = bucket;
		bucket = rbhash_slot_next(bucket, size);
		table = rbhash_element_slot(table, esize, bucket);
	}
}

void* rbhash_removeh(rbhash_t* rbh, uint64_t hash, const void* key, size_t len){
	long bucket = rbhash_find_bucket(rbh, hash, key, len, NULL);
	if( bucket == -1 ) return NULL;
	rbhElement_s* el = rbhash_element_slot(rbh->table, rbh->elementSize, bucket);
	void* ret = el->data;
	el->len = 0;
	el->data = 0;
	rbhash_swapup(rbh->table, rbh->size, rbh->elementSize, bucket);
	--rbh->count;
	return ret;	
}

void* rbhash_remove(rbhash_t* ht, const void* key, size_t len){
	return rbhash_removeh(ht, ht->hashing(key, len), key, len);
}

size_t rbhash_bucket_used(rbhash_t* rbh){
	return rbh->count;
}

size_t rbhash_bucket_count(rbhash_t* rbh){
	return rbh->size;
}

size_t rbhash_collision(rbhash_t* rbh){
	rbhElement_s* tbl = rbh->table;
	size_t collision = 0;
	for(size_t i =0; i < rbh->size; ++i, tbl = rbhash_element_next(tbl,rbh->elementSize)){
		if( tbl->len != 0 && tbl->distance != 0 ) ++collision;
	}
	return collision;
}

size_t rbhash_distance_max(rbhash_t* rbh){
	return rbh->maxdistance;
}

void* rbhash_linear(rbhash_t* rbh, long* slot){
	if( *slot == -1 ) return NULL;
	while( *slot < (long)rbh->size){
		rbhElement_s* bucket = rbhash_element_slot(rbh->table, rbh->elementSize, *slot);
		++(*slot);
		if( bucket->len ) return bucket->data;
	}
	*slot = -1;
	return NULL;
}


