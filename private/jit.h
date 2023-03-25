#ifndef __PRIVATE_NOTSTD_JIT_H__
#define __PRIVATE_NOTSTD_JIT_H__

#include <notstd/core.h>
#include <notstd/vector.h>
#include <notstd/utf8.h>

#include <jit/jit.h>

#define JIT_STK_MAX 32
#define JIT_LBL_MAX 16
#define JIT_WHL_MAX 16
#define JIT_CIF_MAX 16

typedef struct cosif{
	jit_label_t next;
	jit_label_t endif;
}cosif_s;

typedef struct jitfunction_s{
	jit_function_t fn;
	jit_label_t    label[JIT_LBL_MAX];
	jit_label_t    lwhile[JIT_LBL_MAX];
	jit_value_t    stk[JIT_STK_MAX];
	unsigned       stkcount;
	unsigned       lblcount;
	unsigned       whlcount;
}jitfunction_s;

typedef struct jit{
	jit_context_t  ctx;
	jitfunction_s* stkfn;
	jitfunction_s* current;
}jit_s;

typedef jit_context_t  _ctx;
typedef jit_value_t    _value;
typedef jit_type_t     _type;
typedef jit_function_t _fn;
typedef jit_label_t    _lbl;
typedef jit_type_t     _type;

typedef _value(*_if_f)(_fn, _value, _value);

#define _eq jit_insn_eq
#define _ne jit_insn_ne
#define _lt jit_insn_lt
#define _gt jit_insn_gt
#define _le jit_insn_le
#define _ge jit_insn_ge

#define _char  jit_type_sbyte
#define _uchar jit_type_ubyte
#define _int   jit_type_int
#define _uint  jit_type_uint
#define _long  jit_type_long
#define _ulong jit_type_ulong
#define _void  jit_type
#define _pvoid jit_type_void_ptr
#define _ucs4  _uint
#define _utf8  _uchar

#define _lbl_undef jit_label_undefined

unsigned jitasinit;
_type _putf8;
_type _str;
_type _dict;
_type _proto_i_pv_pv;
_type _proto_u_pu8;
_type _proto_ucs4_pu8;
_type _proto_i_pu8_pu8_u;

UNSAFE_BEGIN("-Wunused-function");

__private void _ctor(jit_s* self){
	self->ctx = jit_context_create();
	iassert(self->ctx);
	self->stkfn = VECTOR(jitfunction_s, 4);
	self->current = NULL;

	if( !jitasinit ){
		_putf8 = jit_type_create_pointer(_utf8, 1);
		_str   = jit_type_create_pointer(_char, 1);
		_dict  = jit_type_create_pointer(_pvoid, 1);
		_proto_i_pv_pv     = jit_type_create_signature(jit_abi_cdecl, _int,  (_type[]){_pvoid, _pvoid} , 2, 1);
		_proto_u_pu8       = jit_type_create_signature(jit_abi_cdecl, _uint, (_type[]){_putf8} , 1, 1);
		_proto_ucs4_pu8    = jit_type_create_signature(jit_abi_cdecl, _ucs4, (_type[]){_putf8} , 1, 1);
		_proto_i_pu8_pu8_u = jit_type_create_signature(jit_abi_cdecl, _int,  (_type[]){_putf8, _putf8, _uint}, 3, 1);
		jitasinit = 1;
	}
}

__private void _dtor(jit_s* self){
	mem_free(self->stkfn);
	jit_context_destroy(self->ctx);
}

__private void _begin(jit_s* self){
	jit_context_build_start(self->ctx);
}

__private void _end(jit_s* self){
	jit_context_build_end(self->ctx);
}

__private void _push(jit_s* self, _value val){
	iassert(self->current->stkcount < JIT_STK_MAX);
	self->current->stk[self->current->stkcount++] = val;
}

__private _value _pop(jit_s* self){
	iassert(self->current->stkcount);
	return self->current->stk[--self->current->stkcount];
}

__private _type _pointer(__unused jit_s* self, _type type){
	return jit_type_create_pointer(type, 1);
}

__private _type _signature(__unused jit_s* self, _type ret, _type* args, unsigned countargs){
	return jit_type_create_signature(jit_abi_cdecl, ret, args , countargs, 1);
}

__private void _function(jit_s* self, _type signature, unsigned nargs){
	self->current = vector_push(&self->stkfn, NULL);
	self->current->fn = jit_function_create(self->ctx, signature);
	for( unsigned i = 0; i < nargs; ++i ){
		_push(self, jit_value_get_param(self->current->fn, i));
	}
}

__private _fn _endfunction(jit_s* self){
	jit_function_compile(self->current->fn);

	_fn ret = self->current->fn;
	vector_pop(&self->stkfn, NULL);
	unsigned count = vector_count(&self->stkfn);
	if( count )	self->current = &self->stkfn[count-1];

	return ret;
}

__private _value _variable(jit_s* self, _type type){
	return jit_value_create(self->current->fn, type);
}

__private _value _constant_inum(jit_s* self, _type type, unsigned long value){
	return jit_value_create_nint_constant(self->current->fn, type, value);
}

__private _value _constant_lnum(jit_s* self, _type type, unsigned long value){
	return jit_value_create_long_constant(self->current->fn, type, value);
}

__private _value _constant_ptr(jit_s* self, _type type, const void* value){
	return jit_value_create_nint_constant(self->current->fn, type, (jit_nint)value);
}

__private void _store(jit_s* self, _value dst, _value src){
	jit_insn_store(self->current->fn, dst, src);
}

__private _value _load(jit_s* self, _value src){
	return jit_insn_load(self->current->fn, src);
}

__private _value _deref(jit_s* self, _type type, _value src){
	return jit_insn_load_relative(self->current->fn, src, 0, type);
}

__private void _ref(jit_s* self, _value dst, _value src){
	jit_insn_store_relative(self->current->fn, dst, 0, src);
}

__private _value _call(jit_s* self, _type signature, _fn fn, _value* args, unsigned count){
	return jit_insn_call(self->current->fn, NULL, fn, signature, args, count, JIT_CALL_NOTHROW);
}

__private _value _call_native(jit_s* self, _type signature, void* fn, _value* args, unsigned count){
	return jit_insn_call_native(self->current->fn, NULL, fn, signature, args, count, JIT_CALL_NOTHROW);
}

__private void _call_jit(__unused jit_s* self, void* ret, _fn fn, void* args){
	jit_function_apply(fn, args, ret);
}

__private void _label(jit_s* self, _lbl* label){
	jit_insn_label(self->current->fn, label);
}

__private void _return(jit_s* self, _value v){
	jit_insn_return(self->current->fn, v);
}

__private void _goto(jit_s* self, _lbl* label){
	jit_insn_branch(self->current->fn, label);
}

__private unsigned __if__ctor__(jit_s* self){
	iassert( self->current->lblcount + 3 < JIT_LBL_MAX );
	self->current->lblcount += 3;
	unsigned l = self->current->lblcount;
	self->current->label[l-1] = jit_label_undefined;
	self->current->label[l-2] = jit_label_undefined;
	self->current->label[l-3] = jit_label_undefined;
	return l;
}

__private void _if(jit_s* self, _value a, _if_f cmp, jit_value_t b){
	unsigned l = __if__ctor__(self);
	_value test = cmp(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
}

__private void _if_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	unsigned l = __if__ctor__(self);
	_value test = cmp1(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
	test = cmp2(self->current->fn, c, d);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
}

__private void _if_not_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	unsigned l = __if__ctor__(self);
	_value test = cmp1(self->current->fn, a, b);
	jit_insn_branch_if(self->current->fn, test, &self->current->label[l-2]);
	test = cmp2(self->current->fn, c, d);
	jit_insn_branch_if(self->current->fn, test, &self->current->label[l-2]);
}

__private void _if_or(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	unsigned l = __if__ctor__(self);
	_lbl tmp = _lbl_undef;
	_value test = cmp1(self->current->fn, a, b);
	jit_insn_branch_if(self->current->fn, test, &tmp);
	test = cmp2(self->current->fn, c, d);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
	_label(self, &tmp);
}

__private void _else(jit_s* self){
	iassert(self->current->lblcount >= 3);
	unsigned l = self->current->lblcount;
	_goto(self, &self->current->label[l-3]);
	_label(self, &self->current->label[l-2]);
	self->current->label[l-2] = jit_label_undefined;
	self->current->label[l-1] = jit_label_undefined + 1;
}

__private void _elif(jit_s* self, _value a, _if_f cmp, _value b){
	_else(self);
	unsigned l = self->current->lblcount;
	_value test = cmp(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
}

__private void _elif_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	_else(self);
	unsigned l = self->current->lblcount;
	_value test = cmp1(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
	test = cmp2(self->current->fn, c, d);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->label[l-2]);
}

__private void _endif(jit_s* self){
	iassert(self->current->lblcount >= 3);
	unsigned l = self->current->lblcount;
	if( self->current->label[l-1] == jit_label_undefined ){
		_label(self, &self->current->label[l-2]);
	}
	else{
		_label(self, &self->current->label[l-3]);
	}
	self->current->lblcount -= 3;
}

__private void _do(jit_s* self){
	iassert( self->current->whlcount < JIT_WHL_MAX-2 );
	unsigned lp = self->current->whlcount++;
	unsigned le = self->current->whlcount++;
	self->current->lwhile[lp] = jit_label_undefined;
	self->current->lwhile[le] = jit_label_undefined;
	_label(self, &self->current->lwhile[lp]);
}

__private void _end_while(jit_s* self, _value a, _if_f cmp, _value b){
	iassert(self->current->whlcount >= 2);
	unsigned le = --self->current->whlcount;
	unsigned lp = --self->current->whlcount;
	_value test = cmp(self->current->fn, a, b);
	jit_insn_branch_if(self->current->fn, test, &self->current->lwhile[lp]);
	_label(self, &self->current->lwhile[le]);
}

__private void _end_while_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	iassert(self->current->whlcount >= 2);
	unsigned le = --self->current->whlcount;
	unsigned lp = --self->current->whlcount;
	_value test = cmp1(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->lwhile[le]);
	test = cmp2(self->current->fn, c, d);
	jit_insn_branch_if(self->current->fn, test, &self->current->lwhile[lp]);
	_label(self, &self->current->lwhile[le]);
}

__private void _while(jit_s* self, _value a, _if_f cmp, _value b){
	iassert( self->current->lblcount < JIT_WHL_MAX - 2);
	unsigned lp = self->current->whlcount++;
	unsigned le = self->current->whlcount++;
	self->current->lwhile[lp] = jit_label_undefined;
	self->current->lwhile[le] = jit_label_undefined;
	_label(self, &self->current->lwhile[lp]);
	_value test = cmp(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->lwhile[le]);	
}

__private void _while_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	iassert( self->current->lblcount < JIT_WHL_MAX - 2);
	unsigned lp = self->current->whlcount++;
	unsigned le = self->current->whlcount++;
	self->current->lwhile[lp] = jit_label_undefined;
	self->current->lwhile[le] = jit_label_undefined;
	_label(self, &self->current->lwhile[lp]);
	_value test = cmp1(self->current->fn, a, b);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->lwhile[le]);	
	test = cmp2(self->current->fn, c, d);
	jit_insn_branch_if_not(self->current->fn, test, &self->current->lwhile[le]);	
}

__private void _loop(jit_s* self){
	iassert(self->current->whlcount>=2);
	unsigned le = --self->current->whlcount;
	unsigned lp = --self->current->whlcount;
	_goto(self, &self->current->lwhile[lp]);
	_label(self, &self->current->lwhile[le]);
}

__private void _break(jit_s* self){
	iassert(self->current->whlcount>=2);
	unsigned le = self->current->whlcount - 1;
	_goto(self, &self->current->lwhile[le]);
}

__private void _continue(jit_s* self){
	iassert(self->current->whlcount>=2);
	unsigned lp = self->current->whlcount - 2;
	_goto(self, &self->current->lwhile[lp]);
}

__private _value _add(jit_s* self, _value a, _value b){
	return jit_insn_add(self->current->fn, a, b);
}

__private _value _sub(jit_s* self, _value a, _value b){
	return jit_insn_sub(self->current->fn, a, b);
}

__private _value _and(jit_s* self, _value a, _value b){
	return jit_insn_and(self->current->fn, a, b);
}

__private _value _xor(jit_s* self, _value a, _value b){
	return jit_insn_xor(self->current->fn, a, b);
}

__private _value _mul(jit_s* self, _value a, _value b){
	return jit_insn_mul(self->current->fn, a, b);
}

__private _value _shl(jit_s* self, _value a, _value b){
	return jit_insn_shl(self->current->fn, a, b);
}

__private _value _shr(jit_s* self, _value a, _value b){
	return jit_insn_shr(self->current->fn, a, b);
}

__private void _clr(jit_s* self, _value a){
	_store(self, a, _xor(self, a, a));
}

__private _value _memcmp(jit_s* self, _value a, _value b, _value size){
	return _call_native(self, _proto_i_pv_pv, memcmp, (_value[]){a,b,size}, 3);
}

__private _value _utf8_codepoint_nb(jit_s* self, _value a){
	return _call_native(self, _proto_u_pu8, utf8_codepoint_nb, (_value[]){a}, 1);
}

__private _value _utf8_to_ucs4(jit_s* self, _value a){
	return _call_native(self, _proto_ucs4_pu8, utf8_to_ucs4, (_value[]){a}, 1);
}

__private _value _utf8_ncmp(jit_s* self, _value a, _value b, _value l){
	return _call_native(self, _proto_i_pu8_pu8_u, utf8_ncmp, (_value[]){a,b,l}, 3);
}

UNSAFE_END;

#endif
