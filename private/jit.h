#ifndef __PRIVATE_NOTSTD_JIT_H__
#define __PRIVATE_NOTSTD_JIT_H__

#include "jit/jit-insn.h"
#include "notstd/core/compiler.h"
#include <notstd/core.h>
#include <jit/jit.h>

#define JIT_STK_MAX 32
#define JIT_LBL_MAX 16
#define JIT_WHL_MAX 16

typedef struct jit{
	jit_context_t  ctx;
	jit_function_t fn;
	jit_label_t    label[JIT_LBL_MAX];
	jit_label_t    lwhile[JIT_LBL_MAX];
	jit_value_t    stk[JIT_STK_MAX];
	unsigned       stkcount;
	unsigned       lblcount;
	unsigned       whlcount;
}jit_s;

typedef jit_context_t  _ctx;
typedef jit_value_t    _value;
typedef jit_type_t     _type;
typedef jit_function_t _fn;
typedef jit_label_t    _lbl;

typedef _value(*_if_f)(_fn, _value, _value);

#define _eq jit_insn_eq
#define _ne jit_insn_ne
#define _lt jit_insn_lt
#define _gt jit_insn_gt
#define _le jit_insn_le
#define _ge jit_insn_ge

UNSAFE_BEGIN("-Wunused-function");

__private void _ctor(jit_s* self){
	self->ctx = jit_context_create();
	iassert(self->ctx);
	self->lblcount = 0;
	self->stkcount = 0;
	self->whlcount = 0;
}

__private void _dtor(jit_s* self){
	jit_context_destroy(self->ctx);
}

__private void _begin(jit_s* self){
	jit_context_build_start(self->ctx);
	self->stkcount = 0;
	self->lblcount = 0;
	self->whlcount = 0;
}

__private void _end(jit_s* self){
	jit_context_build_end(self->ctx);
}

__private void _build(jit_s* self){
	jit_function_compile(self->fn);
}

__private void _push(jit_s* self, _value val){
	iassert(self->stkcount < JIT_STK_MAX);
	self->stk[self->stkcount++] = val;
}

__private _value _pop(jit_s* self){
	iassert(self->stkcount);
	return self->stk[--self->stkcount];
}

__private _type _pointer(__unused jit_s* self, _type type){
	return jit_type_create_pointer(type, 1);
}

__private _type _signature(__unused jit_s* self, _type ret, _type* args, unsigned countargs){
	return jit_type_create_signature(jit_abi_cdecl, ret, args , countargs, 1);
}

__private void _function(jit_s* self, _type signature, unsigned nargs){
	self->fn = jit_function_create(self->ctx, signature);
	for( unsigned i = 0; i < nargs; ++i ){
		_push(self, jit_value_get_param(self->fn, i));
	}
}

__private _value _variable(jit_s* self, _type type){
	return jit_value_create(self->fn, type);
}

__private _value _constant_inum(jit_s* self, _type type, unsigned long value){
	return jit_value_create_nint_constant(self->fn, type, value);
}

__private _value _constant_lnum(jit_s* self, _type type, unsigned long value){
	return jit_value_create_long_constant(self->fn, type, value);
}

__private _value _constant_ptr(jit_s* self, _type type, const void* value){
	return jit_value_create_nint_constant(self->fn, type, (jit_nint)value);
}

__private void _store(jit_s* self, _value dst){
	jit_insn_store(self->fn, dst, _pop(self));
}

__private void _load(jit_s* self, _value src){
	_push(self, jit_insn_load(self->fn, src));
}

__private void _deref(jit_s* self, _type type, _value src, unsigned offset){
	_push(self, jit_insn_load_relative(self->fn, src, offset, type));
}

__private void _call(jit_s* self, _type signature, _fn fn, _value* args, unsigned count){
	_push(self, jit_insn_call(self->fn, NULL, fn, signature, args, count, JIT_CALL_NOTHROW));
}

__private void _call_native(jit_s* self, _type signature, void* fn, _value* args, unsigned count){
	_push(self, jit_insn_call_native(self->fn, NULL, fn, signature, args, count, JIT_CALL_NOTHROW));
}

__private void _call_jit(__unused jit_s* self, void* ret, _fn fn, void* args){
	jit_function_apply(fn, args, ret);
}

__private void _label(jit_s* self, _lbl* label){
	jit_insn_label(self->fn, label);
}

__private void _return(jit_s* self, _value v){
	jit_insn_return(self->fn, v);
}

__private void _goto(jit_s* self, _lbl* label){
	jit_insn_branch(self->fn, label);
}

__private void _if(jit_s* self, _value a, _if_f cmp, jit_value_t b){
	iassert( self->lblcount < JIT_LBL_MAX );
	unsigned l = self->lblcount++;
	self->label[l] = jit_label_undefined;
	_value test = cmp(self->fn, a, b);
	jit_insn_branch_if_not(self->fn, test, &self->label[l]);
}

__private void _if_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	iassert( self->lblcount < JIT_LBL_MAX );
	unsigned l = self->lblcount++;
	self->label[l] = jit_label_undefined;
	_value test = cmp1(self->fn, a, b);
	jit_insn_branch_if_not(self->fn, test, &self->label[l]);
	test = cmp2(self->fn, c, d);
	jit_insn_branch_if_not(self->fn, test, &self->label[l]);
}

__private void _if_not_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	iassert( self->lblcount < JIT_LBL_MAX );
	unsigned l = self->lblcount++;
	self->label[l] = jit_label_undefined;
	_value test = cmp1(self->fn, a, b);
	jit_insn_branch_if(self->fn, test, &self->label[l]);
	test = cmp2(self->fn, c, d);
	jit_insn_branch_if(self->fn, test, &self->label[l]);
}

__private void _else(jit_s* self){
	iassert(self->lblcount);
	_label(self, &self->label[--self->lblcount]);
}

__private void _endif(jit_s* self){
	iassert(self->lblcount);
	_label(self, &self->label[--self->lblcount]);
}

__private void _do(jit_s* self){
	iassert( self->whlcount < JIT_WHL_MAX-2 );
	unsigned lp = self->whlcount++;
	unsigned le = self->whlcount++;
	self->lwhile[lp] = jit_label_undefined;
	self->lwhile[le] = jit_label_undefined;
	_label(self, &self->lwhile[lp]);
}

__private void _endwhile(jit_s* self, _value a, _if_f cmp, _value b){
	iassert(self->whlcount >= 2);
	unsigned le = --self->whlcount;
	unsigned lp = --self->whlcount;
	_value test = cmp(self->fn, a, b);
	jit_insn_branch_if(self->fn, test, &self->lwhile[lp]);
	_label(self, &self->lwhile[le]);
}

__private void _endwhile_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	iassert(self->whlcount >= 2);
	unsigned le = --self->whlcount;
	unsigned lp = --self->whlcount;
	_value test = cmp1(self->fn, a, b);
	jit_insn_branch_if_not(self->fn, test, &self->lwhile[le]);
	test = cmp2(self->fn, c, d);
	jit_insn_branch_if(self->fn, test, &self->lwhile[lp]);
	_label(self, &self->lwhile[le]);
}

__private void _while(jit_s* self, _value a, _if_f cmp, _value b){
	iassert( self->lblcount < JIT_WHL_MAX - 2);
	unsigned lp = self->whlcount++;
	unsigned le = self->whlcount++;
	self->lwhile[lp] = jit_label_undefined;
	self->lwhile[le] = jit_label_undefined;
	_label(self, &self->lwhile[lp]);
	_value test = cmp(self->fn, a, b);
	jit_insn_branch_if_not(self->fn, test, &self->lwhile[le]);	
}

__private void _while_and(jit_s* self, _value a, _if_f cmp1, _value b, _value c, _if_f cmp2, _value d){
	iassert( self->lblcount < JIT_WHL_MAX - 2);
	unsigned lp = self->whlcount++;
	unsigned le = self->whlcount++;
	self->lwhile[lp] = jit_label_undefined;
	self->lwhile[le] = jit_label_undefined;
	_label(self, &self->lwhile[lp]);
	_value test = cmp1(self->fn, a, b);
	jit_insn_branch_if_not(self->fn, test, &self->lwhile[le]);	
	test = cmp2(self->fn, c, d);
	jit_insn_branch_if_not(self->fn, test, &self->lwhile[le]);	
}

__private void _loop(jit_s* self){
	iassert(self->whlcount>=2);
	unsigned le = --self->whlcount;
	unsigned lp = --self->whlcount;
	_goto(self, &self->lwhile[lp]);
	_label(self, &self->lwhile[le]);
}

__private void _break(jit_s* self){
	iassert(self->whlcount>=2);
	unsigned le = self->whlcount - 1;
	_goto(self, &self->lwhile[le]);
}

__private void _continue(jit_s* self){
	iassert(self->whlcount>=2);
	unsigned lp = self->whlcount - 2;
	_goto(self, &self->lwhile[lp]);
}

__private void _add(jit_s* self, _value a, _value b){
	_push(self, jit_insn_add(self->fn, a, b));
}

__private void _sub(jit_s* self, _value a, _value b){
	_push(self, jit_insn_sub(self->fn, a, b));
}

__private void _xor(jit_s* self, _value a, _value b){
	_push(self, jit_insn_xor(self->fn, a, b));
}

UNSAFE_END;


#endif
