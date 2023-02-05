#ifndef __NOTSTD_CORE_MAP_H__
#define __NOTSTD_CORE_MAP_H__

#include <notstd/core.h>

typedef int (map_f)(void* ctx, void* arg);
typedef void*(*iterate_f)(void* it);
typedef void*(*iterator_f)(void* t, unsigned off, unsigned count);

void mapg(map_f mfn, void* arg, iterate_f ifn, iterator_f nit, void* obj, unsigned off, unsigned count);

#define map(TYPE, OBJ, FN, ARG, OFF, COUNT) mapg(FN, ARG, TYPE ## _iterate, (iterator_f)TYPE ## _iterator, OBJ, OFF, COUNT)

#define foreach(TYPE, OBJ, VAR, OFF, COUNT) for( __free void* __it__ = TYPE ## _iterator(OBJ, OFF, COUNT); (VAR=TYPE ## _iterate(__it__));)




#endif
