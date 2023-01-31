#ifndef __NOTSTD_CORE_GENERICS_H__
#define __NOTSTD_CORE_GENERICS_H__

#include <notstd/core.h>
#include <notstd/vector.h>

typedef enum { G_UNSET, G_CHAR, G_LONG, G_ULONG, G_FLOAT, G_LSTRING, G_STRING, G_VECTOR, G_OBJ } gtype_e;

typedef struct generic generic_s;

struct generic{
	gtype_e type;
	union {
		char c;
		long l;
		unsigned long ul;
		double f;
		char* str;
		const char* lstr;
		void* obj;
		generic_s* vec;
		uintptr_t assign;
	};
};

inline generic_s gi_unset(void) { return (generic_s){ .type = G_UNSET }; }
inline generic_s gi_char(char v) { return (generic_s){ .type = G_CHAR, .c = v}; }
inline generic_s gi_int8(int8_t v) { return (generic_s){ .type = G_LONG, .l = v}; }
inline generic_s gi_int16(int16_t v) { return (generic_s){ .type = G_LONG, .l = v}; }
inline generic_s gi_int32(int32_t v) { return (generic_s){ .type = G_LONG, .l = v}; }
inline generic_s gi_int64(int64_t v) { return (generic_s){ .type = G_LONG, .l = v}; }
inline generic_s gi_uint8(uint8_t v) { return (generic_s){ .type = G_ULONG, .ul = v}; }
inline generic_s gi_uint16(uint16_t v) { return (generic_s){ .type = G_ULONG, .ul = v}; }
inline generic_s gi_uint32(uint32_t v) { return (generic_s){ .type = G_ULONG, .ul = v}; }
inline generic_s gi_uint64(uint64_t v) { return (generic_s){ .type = G_ULONG, .ul = v}; }
inline generic_s gi_float(float v) { return (generic_s){ .type = G_FLOAT, .f = v}; }
inline generic_s gi_double(double v) { return (generic_s){ .type = G_FLOAT, .f = v}; }
inline generic_s gi_str(char* v) { return (generic_s){ .type = G_STRING, .str = v}; }
inline generic_s gi_lstr(const char* v) { return (generic_s){ .type = G_LSTRING, .lstr = v}; }
inline generic_s gi_obj(void* v) { return (generic_s){ .type = G_OBJ, .obj = v}; }
inline generic_s gi_vec(generic_s* v) { return (generic_s){ .type = G_VECTOR, .vec = v}; }

#define GI(VALUE) _Generic((VALUE),\
	char       : gi_char,\
	int8_t     : gi_int8,\
	int16_t    : gi_int16,\
	int32_t    : gi_int32,\
	int64_t    : gi_int64,\
	uint8_t    : gi_uint8,\
	uint16_t   : gi_uint16,\
	uint32_t   : gi_uint32,\
	uint64_t   : gi_uint64,\
	float      : gi_float,\
	double     : gi_double,\
	char*      : gi_str,\
	const char*: gi_lstr,\
	generic_s* : gi_vec,\
	default    : gi_obj\
)(VALUE)

inline int g_print(generic_s g){
	switch( g.type ){
		default: return printf("%p", g.obj);
		case G_UNSET: return printf("unset");
		case G_CHAR: return printf("%c", g.c);
		case G_LONG: return printf("%ld", g.l);
		case G_ULONG: return printf("%lu", g.ul);
		case G_FLOAT: return printf("%f", g.f);
		case G_LSTRING: return printf("%s", g.lstr);
		case G_STRING: return printf("%s", g.str);
		case G_VECTOR:{
			int ret = 0;
			printf("[ ");
			foreach_vector(g.vec, it){
				ret += g_print(g.vec[it]);
				putchar(',');
				putchar(' ');
			}
			printf("]");
			return ret;
		}
	}
}

inline int g_dump(generic_s g){
	switch( g.type ){
		default: return printf("pointer: %p", g.obj);
		case G_UNSET: return printf("value unset");
		case G_CHAR: return printf("char: '%c'", g.c);
		case G_LONG: return printf("long: %ld", g.l);
		case G_ULONG: return printf("ulong: %lu", g.ul);
		case G_FLOAT: return printf("float: %f", g.f);
		case G_LSTRING: return printf("literal: '%s'", g.lstr);
		case G_STRING: return printf("string: '%s'", g.str);
		case G_VECTOR:{
			int ret = 0;
			printf("[ ");
			foreach_vector(g.vec, it){
				ret += g_dump(g.vec[it]);
				putchar(',');
				putchar(' ');
			}
			printf("]");
			return ret;
		}
	}
}





#endif
