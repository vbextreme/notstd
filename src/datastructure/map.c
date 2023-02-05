#include <notstd/map.h>

void mapg(map_f mfn, void* arg, iterate_f ifn, iterator_f nit, void* obj, unsigned off, unsigned count){
	__free void* it = nit(obj, off, count);
	void* el;
	while( (el=ifn(it)) && !mfn(el, arg) );
}
