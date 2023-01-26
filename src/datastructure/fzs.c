#include "notstd/vector.h"
#include <notstd/fzs.h>

/* fork of*/
// MIT licensed.
// Copyright (c) 2015 Titus Wormer <tituswormer@gmail.com>
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

//vbextreme version, add weight, swap, find in classic
size_t fzs_case_weigth_levenshtein(const char *a, size_t lena, const char* b, size_t lenb){
	if( lena > lenb ){
		swap(a,b);
		swap(lena, lenb);
	}

	size_t score = 0;
	unsigned ia = 0;
	unsigned ib = 0;

	for(; ia < lena && ib < lenb; ++ia, ++ib ){
		int cha = tolower(a[ia]);
		int chb = tolower(b[ib]);
		if( cha != chb ){
			int cha1 = tolower(a[ia+1]);
			int chb1 = tolower(b[ib+1]);
			if( cha == chb1 ){
				if( cha1 == chb ) {
					score += 1;
					++ia;
					++ib;
				}
				else{
					++ib;
					score += 2;
				}
			}
			else if( chb == cha1 ){
				++ia;
				score += 2;
			}
			else{
				score += 3;
			}
		}
	}

	if( ia > lena ) ia = lena;
	if( ib > lenb ) ib = lenb;
	score += (lenb-lena) * 1;
	return score;
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

