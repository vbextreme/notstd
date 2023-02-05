#include <notstd/map.h>

void* map(map_f mfn, void* arg, iterate_f ifn, void* it){
	void* a;
	while( (a=ifn(it)) && !mfn(a, arg) );
	return it;
}
