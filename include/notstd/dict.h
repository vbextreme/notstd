#ifndef __NOTSTD_CORE_DICT_H__
#define __NOTSTD_CORE_DICT_H__

#include <notstd/core.h>
#include <notstd/generics.h>

//foreach
//map
//pair

typedef struct dict dict_t;

dict_t* dict_new(void);
generic_s* dicti(dict_t* d, long key);
generic_s* dicts(dict_t* d, const char* key);
int dictsrm(dict_t* d, const char* key);
int dictirm(dict_t* d, long key);


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


#endif
