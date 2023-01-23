/* fork of*/
// MIT licensed.
// Copyright (c) 2015 Titus Wormer <tituswormer@gmail.com>

#include "notstd/vector.h"
#include <notstd/fzs.h>

// Returns a size_t, depicting the difference between `a` and `b`.
// See <https://en.wikipedia.org/wiki/Levenshtein_distance> for more information.
size_t fzs_levenshtein(const char *a, size_t lena, const char *b, size_t lenb){
	if( a == b ) return 0;
	if( lena == 0 ) return lenb;
	if( lenb == 0 ) return lena;
	
	size_t stkcache[4096];
	__free size_t* heapcache = NULL;
	size_t* cache = stkcache;

	if( lena > 4096 ){
		heapcache = MANY(size_t, lena);
		cache = heapcache;
	}

	size_t index = 0;
	size_t bIndex = 0;
	size_t distance;
	size_t bDistance;
	size_t result;
	char code;

	while( index < lena ){
	    cache[index] = index + 1;
		index++;
	}

	while( bIndex < lenb ){
		code = b[bIndex];
		result = distance = bIndex++;
		index = SIZE_MAX;
	    while( ++index < lena ){
			bDistance = code == a[index] ? distance : distance + 1;
			distance = cache[index];

			cache[index] = result = distance > result
				? bDistance > result
					? result + 1
					: bDistance
				: bDistance > distance
					? distance + 1
					: bDistance;
		}
	}
	
	return result;
}

size_t fzs_case_levenshtein(const char *a, size_t lena, const char *b, size_t lenb){
	if( a == b ) return 0;
	if( lena == 0 ) return lenb;
	if( lenb == 0 ) return lena;
	
	size_t stkcache[4096];
	__free size_t* heapcache = NULL;
	size_t* cache = stkcache;

	if( lena > 4096 ){
		heapcache = MANY(size_t, lena);
		cache = heapcache;
	}

	size_t index = 0;
	size_t bIndex = 0;
	size_t distance;
	size_t bDistance;
	size_t result;
	char code;

	while( index < lena ){
	    cache[index] = index + 1;
		index++;
	}

	while( bIndex < lenb ){
		code = b[bIndex];
		result = distance = bIndex++;
		index = SIZE_MAX;
	    while( ++index < lena ){
			bDistance = tolower(code) == tolower(a[index]) ? distance : distance + 1;
			distance = cache[index];

			cache[index] = result = distance > result
				? bDistance > result
					? result + 1
					: bDistance
				: bDistance > distance
					? distance + 1
					: bDistance;
		}
	}
	
	return result;
}

__private size_t levenstein_rec(const char* str1, unsigned len1, const char* str2, unsigned len2, unsigned x, unsigned y){
	if( x == len1+1 ) return len2-y+1;
	if( y == len2+1 ) return len1-x+1;
	if( str1[x-1] == str2[y-1] ) return levenstein_rec(str1, len1, str2, len2, x + 1, y + 1); 
	size_t m[3];
   	m[0] = levenstein_rec(str1, len1, str2, len2, x    , y + 1);
	m[1] = levenstein_rec(str1, len1, str2, len2, x + 1, y    );
	m[2] = levenstein_rec(str1, len1, str2, len2, x + 1, y + 1);
	
	if( m[0] < m[1] ){
		if( m[0] < m[2] ) return 1 + m[0];
		return 1 + m[2];
	}
	if( m[1] < m[2] ) return 1 + m[1];
	return m[2];
}

size_t fzs_levenshtein_rec(const char *a, size_t lena, const char *b, size_t lenb){
	return levenstein_rec(a, lena, b, lenb, 1, 1);
}

ssize_t fzs_vector_find(char** v, unsigned count, const char* str, unsigned lens, fzs_f fn){
	if( lens == 0 ) lens = strlen(str);
	size_t min = -1UL;
	ssize_t index = -1;
	for( unsigned i = 0; i < count; ++i ){
		size_t distance = fn(v[i], strlen(v[i]), str, lens);
		if( distance < min ){
		   	min =  distance;
			index = i;
		}
	}
	return index;
}

//https://stackoverflow.com/questions/10727174/damerau-levenshtein-distance-edit-distance-with-transposition-c-implementation
#define d(i,j) dd[(i) * (m+2) + (j) ]
#define min(x,y) ((x) < (y) ? (x) : (y))
#define min3(a,b,c) ((a)< (b) ? min((a),(c)) : min((b),(c)))
#define min4(a,b,c,d) ((a)< (b) ? min3((a),(c),(d)) : min3((b),(c),(d)))
size_t fzs_damerau_levenshtein(const char *s, size_t lena, const char* t, size_t lenb){
	int n = lena;
	int m = lenb;
	int i, j, cost, i1, j1, DB;
	int infinity = n + m;
	int DA[256 * sizeof(int)];
	memset(DA, 0, sizeof(DA));
	__free int *dd = MANY(int, (n+2)*(m+2));
	d(0,0) = infinity;
	for(i = 0; i < n+1; i++){
		d(i+1,1) = i;
		d(i+1,0) = infinity;
	}
	for(j = 0; j<m+1; j++){
		d(1,j+1) = j;
		d(0,j+1) = infinity;
	}
	for(i = 1; i< n+1; i++){
		DB = 0;
		for(j = 1; j< m+1; j++){
			i1 = DA[(unsigned)t[j-1]];
			j1 = DB;
			cost = ((s[i-1]==t[j-1])?0:1);
			if(cost==0) DB = j;
			d(i+1,j+1) = min4(d(i,j)+cost, d(i+1,j) + 1, d(i,j+1)+1, d(i1,j1) + (i-i1-1) + 1 + (j-j1-1));
		}
		DA[(unsigned)s[i-1]] = i;
	}
	cost = d(n+1,m+1);
	return cost; 
}

size_t fzs_case_damerau_levenshtein(const char *s, size_t lena, const char* t, size_t lenb){
	int n = lena;
	int m = lenb;
	int i, j, cost, i1, j1, DB;
	int infinity = n + m;
	int DA[256 * sizeof(int)];
	memset(DA, 0, sizeof(DA));
	__free int *dd = MANY(int, (n+2)*(m+2));
	d(0,0) = infinity;
	for(i = 0; i < n+1; i++){
		d(i+1,1) = i;
		d(i+1,0) = infinity;
	}
	for(j = 0; j<m+1; j++){
		d(1,j+1) = j;
		d(0,j+1) = infinity;
	}
	for(i = 1; i< n+1; i++){
		DB = 0;
		for(j = 1; j< m+1; j++){
			i1 = DA[(unsigned)t[j-1]];
			j1 = DB;
			cost = ((tolower(s[i-1])==tolower(t[j-1]))?0:1);
			if(cost==0) DB = j;
			d(i+1,j+1) = min4(d(i,j)+cost, d(i+1,j) + 1, d(i,j+1)+1, d(i1,j1) + (i-i1-1) + 1 + (j-j1-1));
		}
		DA[(unsigned)s[i-1]] = i;
	}
	cost = d(n+1,m+1);
	return cost; 
}

#undef d
#undef min
#undef min2
#undef min3
#undef min4

__private int fzs_cmp(const void* A, const void* B){
	const fzs_s* a = (const fzs_s*)A;
	const fzs_s* b = (const fzs_s*)B;
	return a->distance - b->distance;
}

void fzs_qsort(fzs_s* fzse, unsigned count, const char* str, unsigned lens, fzs_f fn){
	iassert(fzse);
	iassert(str);

	if( lens == 0 ) lens = strlen(str);
	for( unsigned i = 0; i < count; ++i ){
		if( !fzse[i].len ) fzse[i].len = strlen(fzse[i].str);
		fzse[i].distance = fn(fzse[i].str, fzse[i].len, str, lens);
	}
	qsort(fzse, count, sizeof(fzs_s), fzs_cmp);
}

