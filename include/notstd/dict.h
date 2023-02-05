#ifndef __NOTSTD_CORE_DICT_H__
#define __NOTSTD_CORE_DICT_H__

#include <notstd/core.h>
#include <notstd/generics.h>

typedef struct dictPair{
	generic_s key;
	generic_s value;
}dictPair_s;

typedef struct gpair{
	generic_s key;
	generic_s* value;
}gpair_s;

typedef struct dict dict_t;
typedef struct dictit dict_i;

typedef int(*dictMap_f)(gpair_s pair, void* arg);

dict_t* dict_new(void);
generic_s* dicti(dict_t* d, long key);
generic_s* dicts(dict_t* d, const char* key);
int dictsrm(dict_t* d, const char* key);
int dictirm(dict_t* d, long key);
unsigned long dict_count(dict_t* dic);

dict_i* dict_iterator(dict_t* dic, unsigned long offset, unsigned long count);
void* dict_iterate(void* IT);

#define dict(D,K) _Generic((K),\
	int: dicti,\
	long: dicti,\
	unsigned long: dicti,\
	unsigned int: dicti,\
	char*: dicts,\
	const char*: dicts\
)(D, K)

#define dictrm(D,K) _Generic((K),\
	int: dictirm,\
	long: dictirm,\
	unsigned long: dictirm,\
	unsigned int: dictirm,\
	char*: dictsrm,\
	const char*: dictsrm\
)(D, K)


//void map_dict(dict_t* d, dictMap_f fn, void* arg);
//gpair_s* dict_pair(dict_t* d);

//#define foreach_dict(D, KV, IT) for( gpair_s* KV = dict_pair(d); KV; DELETE(KV) ) for(size_t IT = 0; i < vector_count(&KV); ++IT)

#endif
