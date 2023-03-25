#include "jit/jit-type.h"
#include "notstd/utf8.h"
#include <notstd/regex.h>
#include <notstd/vector.h>
#include <notstd/str.h>
#include <jit/jit.h>
#include <jit.h>

/* perl regex:
	Escape	                                          - partiale
		\t tab - ok                                   - ok
		\n newline - ok                               - ok
		\r         - not support                      - not support
		\f form feed - not support                    - not support
		\a alarm (bell) - not support                 - not support
		\e escape                                     - not support
		\cK control char                              - not support
		\x{}, \x00  hexadecimal char                  - not support
		\N{name}    named Unicode char                - not support
		\N{U+263D}  Unicode character                 - supported with \U
		\o{}, \000  octal char                        - not support
		\l          lowercase next char               - not support
		\u          uppercase next char               - not support
		\L          lowercase until \E                - not support
		\U          uppercase until \E                - not support
		\Q          quote (disable) pattern \E        - not support
		\E          end either modification or quoted - not support
		\UXXXX \U+XXXX unicode                        - different in perl, perl use \U for other feature and \N{U+XXXX} for unicode

	Class                - partial
		[...] sequences  - ok
		\w [a-zA-Z_,.;]  - not support
		\W [^a-zA-Z_,.;] - not support
		\s [ \t]         - not support
		\S [^ \t]        - not support
		\d [0-9]         - not support
		\D [^0-9]        - not support
		others           - not support

	quantifier                - ok
		* 0, UINT32_t         - ok
		+ 1, UINT32_t         - ok
		? 0, 1                - ok
		{n} == n              - ok
		{n,} >= n, < UINT32_t - ok
		{,m} >= 0, < m        - ok
		{n,m} >= n, <= m      - ok
		quantifier? lazy      - ok

	backreferences            - partial
		\nnn                  - ok
		\g{name}              - ok
		\gN \g{N}             - not support
		\k                    - not support

	Extended Pattern                       - partial               
		(? ) check flags                   - ok
		(? (?)) sub flags                  - not support
		(?...:) flags from ? to :          - ok
		# comment                          - not support
		i case insensitive                 - not support
		: dont capture                     - ok
		^ dont inherit the flags           - not support
		| reset capture count              - ok
		= match but not include in capture - not support
		! is not and not include           - not support
		<= lockbehind                      - not support
		<! reverse lockbehind              - not support
		>                                  - not support
		<name> use capture name            - ok
		'name' use capture name            - ok
		{code} execute code                - not support
		(condition)                        - not support
		?{code}                            - not support
		&NAME                              - not support
*/

typedef struct state state_s;

typedef enum type{
	RX_STRING,
	RX_SINGLE,
	RX_SEQUENCES,
	RX_BACKREF,
	RX_GROUP
}type_e;

typedef struct quantifier{
	unsigned n;
	unsigned m;
	unsigned greedy;
}quantifier_s;

typedef struct range{
	ucs4_t start;
	ucs4_t end;
}range_s;

typedef struct sequences{
	uint32_t ascii[8];
	range_s* range;
	unsigned reverse;
	unsigned asciiset;
}sequences_s;

typedef struct backref{
	unsigned id;
	utf8_t* name;
}backref_s;

#define GROUP_FLAG_CAPTURE     0x01
#define GROUP_FLAG_COUNT_RESET 0x02
#define GROUP_FLAG_NAMED       0x04

typedef struct group{
	unsigned  flags;
	unsigned  id;
	utf8_t*   name;
	state_s** state;
}group_s;

struct state{
	type_e         type;
	quantifier_s   quantifier;
	jit_function_t fn;
	union {
		utf8_t*     string;
		ucs4_t      u;
		sequences_s seq;
		backref_s   backref;
		group_s     group;
	};
};

typedef struct rxch{
	ucs4_t code;
	uint32_t nb;
}rxch_s;

typedef struct jproto{
	_type state;
	_type brnamed;
	_type brindex;
}jproto_s;

struct regex{
	jit_s         jit;
	jproto_s      proto;
	state_s       state;
	const utf8_t* rx;
	const char*   err;
	const utf8_t* last;
	unsigned      flags;
};

__private rxch_s parse_get_ch(const utf8_t* u8, __out const char** err){
	rxch_s ch;
	unsigned escaped = 0;
	if( !*u8 ){
		ch.code = 0;
		ch.nb = 0;
		return ch;
	}

	if( *u8 == '\\' ){
		escaped = 1;
		++u8;
		switch( *u8 ){
			case 'n': ch.code = str_to_ucs4("\n"); ch.nb = 2; return ch;
			case 't': ch.code = str_to_ucs4("\t"); ch.nb = 2; return ch;
			case 'u':
			case 'U':{
				int e;
				const utf8_t* next;
				ch.code = utf8_toul(u8+1, &next, 16, &e);
				if( e ){
					*err = REGEX_ERR_INVALID_UTF_ESCAPE;
					ch.code = 0;
					ch.nb = UINT32_MAX;
				}
				else{
					ch.nb = next - u8;
				}
				return ch;
			}
		}
	}
	
	ch.code = utf8_to_ucs4(u8);
	ch.nb = utf8_codepoint_nb(*u8) + escaped;
	return ch;
}

__private int parse_quantifier(quantifier_s* q, const utf8_t** rxu8, const char** err){
	dbg_info("quantifiers");
	q->greedy = 1;

	const utf8_t* u8 = *rxu8;
	switch( *u8 ){
		default : q->n = 1; q->m = 1; break;
		case '?': q->n = 0, q->m = 1; ++u8; break;
		case '*': q->n = 0, q->m = UINT32_MAX; ++u8; break;
		case '+': q->n = 1, q->m = UINT32_MAX; ++u8; break;
		case '{':
			u8 = (const utf8_t*)str_skip_h((const char*)(u8+1));
			if( !*u8 || *u8 == '}' ){
				*err = REGEX_ERR_UNTERMINATED_QUANTIFIER;
				return -1;
			}

			if( *u8 == ',' ){
				q->n = 0;
			}
			else{
				const utf8_t* end;
				int e;
				q->n = utf8_toul(u8, &end, 10, &e);
				if( e ){
					*err = strerror(errno);
					*rxu8 = u8;
					return -1;
				}
				u8 = (const utf8_t*)str_skip_h((const char*)end);
			}
			
			if( *u8 == ',' ){
				u8 = (const utf8_t*)str_skip_h((const char*)(u8+1));
				if( *u8 == '}' ){
					q->m = UINT32_MAX;
				}
				else{
					const utf8_t* end;
					int e;
					q->m = utf8_toul(u8, &end, 10, &e);
					if( e ){
						*err = strerror(errno);
						*rxu8 = u8;
						return -1;
					}
					u8 = (const utf8_t*)str_skip_h((const char*)end);
				}
			}
			else if( *u8 == '}' ){
				q->m = q->n;
			}

			if( *u8 != '}' ){
				*err = REGEX_ERR_UNTERMINATED_QUANTIFIER;
				return -1;
			}
			++u8;
		break;
	}
	
	if( *u8 == '?' ){
		q->greedy = 0;
		++u8;
	}
	
	*rxu8 = u8;
	return 0;
}

state_s* state_ctor(state_s* s){
	s->fn = NULL;
	return s;
}

__private void _make_lazy(regex_t* rx, jit_s* self, _value c0, _value aDict, _value aU8, _value u8, _value match, _value qn, _fn lazy){
	if( !lazy ) return;
	_if(self, match, _ge, qn);
		_value ret = _call(self, rx->proto.state, lazy, (_value[]){aDict, aU8, _sub(self, u8, aU8)}, 3);
		_if(self, ret, _gt, c0);
			_return(self, _sub(self, u8, aU8));
		_endif(self);
	_endif(self);
}

/********************************************/
/********************************************/
/********************************************/
/********************************************/
/********************************************/
/*************** STATE STRING ***************/
/********************************************/
/********************************************/
/********************************************/
/********************************************/
/********************************************/

__private _fn make_fn_string_cmp(regex_t* rx, const utf8_t* str, unsigned len){
	jit_s* self = &rx->jit;
	
	_function(self, rx->proto.state, 3);
		_value aIndex = _pop(self);
		_value aU8 = _pop(self);
		//unused _value aDict
		_pop(self);

		_value cstr = _constant_ptr(self, _putf8, str);
		_value clen = _constant_inum(self, _uint, len);
		_value c0   = _constant_inum(self, _int, 0);
		_value ce   = _constant_inum(self, _long, -1);
		
		_value cmp  = _utf8_ncmp(self, cstr, _add(self, aU8, aIndex), clen);
		_if(self, cmp, _ne, c0);
			_return(self, _mul(self, aIndex, ce));
		_endif(self);
		_return(self, _add(self, aIndex, clen));
	return _endfunction(self);
}

__private state_s* state_string(regex_t* rx, state_s* s, const utf8_t** rxu8, const char** err){
	const utf8_t* u8 = *rxu8;

	unsigned size = 9;
	utf8_t* str = MANY(utf8_t, size);
	unsigned len  = 0;

	s->type = RX_STRING;

	while( *u8 && *u8 != '(' && *u8 != ')' && *u8 != '[' && *u8 != ']' && *u8 != '|' ){
		rxch_s ch = parse_get_ch(u8, err);
		u8 += ch.nb;
	
		if( *u8 == '?' || *u8 == '+' || *u8 == '*' || *u8 == '{' ) break;

		if( len >= size - 5 ){
			size *= 2;
			str = RESIZE(utf8_t, str, size);
		}

		if( ch.nb == UINT32_MAX ){
			*rxu8 = u8;
			mem_free(str);
			return NULL;
		}
		
		len += ucs4_to_utf8(ch.code, &str[len]);
	}

	str[len] = 0;
	str = RESIZE(utf8_t, str, len+1);
	
	s->string = str;
	mem_gift(str, rx);
	*rxu8 = u8;

	dbg_info("state string '%s'", s->string);
	return s;
}

/********************************************/
/********************************************/
/********************************************/
/********************************************/
/********************************************/
/*************** STATE SINGLE ***************/
/********************************************/
/********************************************/
/********************************************/
/********************************************/
/********************************************/

__private _fn make_fn_single_cmp(regex_t* rx, ucs4_t u, quantifier_s* quantifier, _fn lazy){
	jit_s* self = &rx->jit;
	
	_function(self, rx->proto.state, 3);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		_value aDict  = _pop(self);

		_value u4   = _constant_inum(self, _ucs4, u);
		_value cn   = _constant_inum(self, _uint, '\n');
		_value c0   = _constant_inum(self, _int, 0);
		_value c1   = _constant_inum(self, _int, 1);
		_value ce   = _constant_inum(self, _long, -1);
		_value qn   = _constant_inum(self, _uint, quantifier->n);
		_value qm   = _constant_inum(self, _uint, quantifier->m);
		
		_value match = _variable(self, _uint);
		_clr(self, match);
		_value u8 = _add(self, aU8, aIndex);

		_do(self);
			_make_lazy(rx, self, c0, aDict, aU8, u8, match, qn, lazy);

			_value ch = _utf8_to_ucs4(self, u8);
			_if(self, ch, _eq, u4);
				_store(self, match, _add(self, match, c1));

				_value nb = _utf8_codepoint_nb(self, _deref(self, _utf8, u8));
				_store(self, u8, _add(self, u8, nb));

				ch = _deref(self, _utf8, u8);
				if( rx->flags & REGEX_FLAG_DISALBLE_LINE ){
					_if(self, ch, _eq, c0);
				}
				else{
					_if_or(self, ch, _eq, c0, ch, _eq, cn);
				}
					_break(self);
				_endif(self);
			_else(self);
				_break(self);
			_endif(self);
		_end_while(self, match, _le, qm);
		
		_if(self, match, _lt, qn);
			_return(self, _mul(self, _sub(self, u8, aU8), ce));
		_endif(self);
		_return(self, _sub(self, u8, aU8));
	return _endfunction(self);
}

__private state_s* state_single(__unused regex_t* rx, state_s* s, const utf8_t** rxu8, rxch_s ch){
	s->type = RX_SINGLE;
	dbg_info("state single %u", ch.code);
	s->u = ch.code;
	*rxu8 += ch.nb;
	return s;
}

/***********************************************/
/***********************************************/
/***********************************************/
/***********************************************/
/***********************************************/
/*************** STATE SEQUENCES ***************/
/***********************************************/
/***********************************************/
/***********************************************/
/***********************************************/
/***********************************************/

__private _fn make_fn_sequences_cmp(regex_t* rx, sequences_s* seq, quantifier_s* quantifier, _fn lazy){
	jit_s* self = &rx->jit;
	
	_function(self, rx->proto.state, 3);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		_value aDict  = _pop(self);

		_value cn   = _constant_inum(self, _uint, '\n');
		_value c0   = _constant_inum(self, _int, 0);
		_value c1   = _constant_inum(self, _int, 1);
		_value c3   = _constant_inum(self, _int, 3);
		_value c4   = _constant_inum(self, _int, 4);
		_value c7   = _constant_inum(self, _int, 7);
		_value c256 = _constant_inum(self, _int, 256);
		_value ce   = _constant_inum(self, _long, -1);
		_value qn   = _constant_inum(self, _uint, quantifier->n);
		_value qm   = _constant_inum(self, _uint, quantifier->m);
		_value tb   = _constant_ptr(self, _ucs4, seq->ascii);
		_value vs[32];
		_value ve[32];
		
		unsigned countr = vector_count(&seq->range);
		if( countr >= 32 ) die("todo add support for more range");
		for( unsigned i = 0; i < countr; ++i ){
			vs[i] = _constant_inum(self, _ucs4, seq->range[i].start);
			ve[i] = _constant_inum(self, _ucs4, seq->range[i].end);
		}

		_value match = _variable(self, _uint);
		_clr(self, match);
		_value u8 = _add(self, aU8, aIndex);

		_do(self);
			_make_lazy(rx, self, c0, aDict, aU8, u8, match, qn, lazy);
			
			_value ch = _utf8_to_ucs4(self, u8);
			_lbl   nextmatch = _lbl_undef;

			if( seq->asciiset ){
				_if(self, ch, _lt, c256);
					// i = (tb[ch & 7]
					_value lut = _deref(self, _ucs4, _add(self, tb, _mul(self, _and(self, ch, c7), c4)));
					//ch >>= 3;
					_value num = _shr(self, ch, c3);
					// lut & (1<<ch)
					_if(self, _and(self, lut, _shl(self, c1, num)), _eq, c0);
						_break(self);
					_else(self);
						_goto(self, &nextmatch);
					_endif(self);
				_endif(self);
			}
			else{
				_if(self, ch, _lt, c256);
					_break(self);
				_endif(self);
			}
			
			if( countr > 0 ){
				_if_and(self, ch, _ge, vs[0], ch, _le, ve[0]);
					if( seq->reverse ){
						_break(self);
					}
					else{
						_goto(self, &nextmatch);
					}
				for( unsigned i = 1; i < countr; ++i ){
					_elif_and(self, ch, _ge, vs[i], ch, _le, ve[i]);
					if( seq->reverse ){
						_break(self);
					}
					else{
						_goto(self, &nextmatch);
					}
				}
				_endif(self);
			}	

			_label(self, &nextmatch);
			_store(self, match, _add(self, match, c1));
			_value nb = _utf8_codepoint_nb(self, _deref(self, _utf8, u8));
			_store(self, u8, _add(self, u8, nb));
			ch = _deref(self, _utf8, u8);
			if( rx->flags & REGEX_FLAG_DISALBLE_LINE ){
				_if(self, ch, _eq, c0);
			}
			else{
				_if_or(self, ch, _eq, c0, ch, _eq, cn);
			}
				_break(self);
			_endif(self);
		_end_while(self, match, _le, qm);
		
		_if(self, match, _lt, qn);
			_return(self, _mul(self, _sub(self, u8, aU8), ce));
		_endif(self);
		_return(self, _sub(self, u8, aU8));
	return _endfunction(self);
}

__private void seq_ctor(sequences_s* seq){
	memset(seq->ascii, 0, sizeof(seq->ascii));
	seq->range = VECTOR(range_s, 2);
	seq->asciiset = 0;
	seq->reverse  = 0;
}

__private void seq_add(sequences_s* seq, ucs4_t st, ucs4_t en){
	if( st < 256 ){
		if( en > 255 ){
			range_s* r = vector_push(&seq->range, NULL);
			r->start = 256;
			r->end   = en;
			en = 255;
		}
		for( ucs4_t a = st; a <= en; ++a ){
			unsigned i = a & 7;
			seq->ascii[i] |= 1 << (a>>3);
		}
		seq->asciiset = 1;
	}
	else{
		range_s* r = vector_push(&seq->range, NULL);
		r->start = st;
		r->end   = en;
	}
}

__private int range_cmp(const void* a, const void* b){ return ((range_s*)a)->start - ((range_s*)b)->start; }

__private void seq_merge(sequences_s* seq, unsigned disableline){	
	if( seq->reverse && seq->asciiset ){
		for( unsigned i = 0; i < 8; ++i ){
			seq->ascii[i] = ~seq->ascii[i];
		}
	}

	if( !disableline ){
		unsigned i = '\n' & 0x7;
		seq->ascii[i] &= ~( 1 << ('n'>>3));
	}

	vector_qsort(&seq->range, range_cmp);
	unsigned i = 0;
	while( i < vector_count(&seq->range) - 1 ){
		range_s* r[2] = { &seq->range[i], &seq->range[i+1] };
		if( r[1]->start <= r[0]->end + 1 ){
			if( r[0]->end < r[1]->end ) r[0]->end = r[1]->end;
			vector_remove(&seq->range, i+1, 1);
			continue;
		}
		++i;
	}
}

__private state_s* state_dot(regex_t* rx, state_s* s, const utf8_t** rxu8){
	dbg_info("state dot");

	s->type = RX_SEQUENCES;
	seq_ctor(&s->seq);

	if( rx->flags & REGEX_FLAG_DISALBLE_LINE ){
		seq_add(&s->seq, 1, UINT32_MAX);
	}
	else{
		seq_add(&s->seq, 1, '\n'-1);
		seq_add(&s->seq, '\n'+1, UINT32_MAX);
	}
	
	*rxu8 += 1;
	return s;
}

__private state_s* state_sequences(regex_t* rx, state_s* s, const utf8_t** rxu8, const char** err){
	dbg_info("state sequences");

	s->type = RX_SEQUENCES;
	seq_ctor(&s->seq);

	const utf8_t* u8 = *rxu8;
	++u8;

	if( !*u8 ){
		mem_free(s->seq.range);
		*err = REGEX_ERR_UNTERMINATED_SEQUENCES;
		return NULL;
	}
	
	if( *u8 == '^' ){
		s->seq.reverse = 1;
		++u8;
	}

	while( *u8 && *u8 != ']' ){
		ucs4_t st;
		ucs4_t en;
		rxch_s ch = parse_get_ch(u8, err);
		if( ch.nb == UINT32_MAX ){
			mem_free(s->seq.range);
			*rxu8 = u8;
			return NULL;
		}
		en = st = ch.code;
		u8 += ch.nb;

		if( u8[0] == '-' && u8[1] != ']' ){
			++u8;
			rxch_s ch = parse_get_ch(u8, err);
			if( ch.nb == UINT32_MAX ){
				mem_free(s->seq.range);
				*rxu8 = u8;
				return NULL;
			}
			en = ch.code;
			u8 += ch.nb;
		}

		seq_add(&s->seq, st, en);
	}

	if( *u8 != ']' ){
		mem_free(s->seq.range);
		*err = REGEX_ERR_UNTERMINATED_SEQUENCES;
		return NULL;
	}
	
	if( !vector_count(&s->seq.range) ){
		mem_free(s->seq.range);
		*err = REGEX_ERR_ASPECTED_SEQUENCES;
		return NULL;
	}

	seq_merge(&s->seq, rx->flags & REGEX_FLAG_DISALBLE_LINE);

	mem_gift(s->seq.range, rx);
	*rxu8 = u8+1;
	return s;
}

/*********************************************/
/*********************************************/
/*********************************************/
/*********************************************/
/*********************************************/
/*************** STATE BACKREF ***************/
/*********************************************/
/*********************************************/
/*********************************************/
/*********************************************/
/*********************************************/

__private long backref_named_cmp(const char* name, dict_t* dic, const utf8_t* u8, unsigned index){
	generic_s* g = dict(dic, name);
	if( g->type == G_UNSET ) return (long)index * -1;
	if( strncmp(g->sub.start, (const char*)(u8+index), g->sub.size) ) return (long)index *-1;
	return index + g->sub.size;
}

__private long backref_index_cmp(unsigned id, dict_t* dic, const utf8_t* u8, unsigned index){
	generic_s* g = dict(dic, id);
	if( g->type == G_UNSET ) return (long)index * -1;
	if( strncmp(g->sub.start, (const char*)(u8+index), g->sub.size) ) return (long)index *-1;
	return index + g->sub.size;
}

__private _fn make_fn_backref_cmp(regex_t* rx, backref_s* br, quantifier_s* quantifier, _fn lazy){
	jit_s* self = &rx->jit;
	
	_function(self, rx->proto.state, 3);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		_value aDict  = _pop(self);

		_value cn   = _constant_inum(self, _uint, '\n');
		_value c0   = _constant_inum(self, _int, 0);
		_value c1   = _constant_inum(self, _int, 1);
		_value ce   = _constant_inum(self, _long, -1);
		_value qn   = _constant_inum(self, _uint, quantifier->n);
		_value qm   = _constant_inum(self, _uint, quantifier->m);
		_value bn   = _constant_ptr(self, _char, br->name);
		_value bi   = _constant_inum(self, _uint, br->id);

		_value match = _variable(self, _uint);
		_clr(self, match);
		_value u8 = _add(self, aU8, aIndex);

		_do(self);
			_make_lazy(rx, self, c0, aDict, aU8, u8, match, qn, lazy);

			_value ret;
			if( br->name ){
				ret = _call_native(self, rx->proto.brnamed, backref_named_cmp, (_value[]){ bn, aDict, u8, aIndex}, 4);
			}
			else{
				ret = _call_native(self, rx->proto.brnamed, backref_index_cmp, (_value[]){ bi, aDict, u8, aIndex}, 4);
			}
			_if(self, ret, _le, c0);
				_break(self);
			_endif(self);

			_store(self, match, _add(self, match, c1));
			u8 = _add(self, aU8, ret);
			
			_value ch = _deref(self, _utf8, u8);
			if( rx->flags & REGEX_FLAG_DISALBLE_LINE ){
				_if(self, ch, _eq, c0);
			}
			else{
				_if_or(self, ch, _eq, c0, ch, _eq, cn);
			}
				_break(self);
			_endif(self);
		_end_while(self, match, _le, qm);
		
		_if(self, match, _lt, qn);
			_return(self, _mul(self, _sub(self, u8, aU8), ce));
		_endif(self);
		_return(self, _sub(self, u8, aU8));
	return _endfunction(self);
}

__private state_s* state_backreferences(regex_t* rx, state_s* s, const utf8_t** rxu8, const char** err){
	dbg_info("state backreferences");

	s->type = RX_BACKREF;
	s->backref.id   = 0;
	s->backref.name = NULL;

	++(*rxu8);
	if( **rxu8 == 'g' ){
		++(*rxu8);
		if( **rxu8 != '{' ){
			*err = REGEX_ERR_UNOPENED_BACKREF_NAME;
			return NULL;
		}
		const utf8_t* n = *rxu8+1;
		const utf8_t* u8 = n;
		while( *u8 && *u8 != '}' ) ++u8;
		if( *u8 != '}' ){
			*err = REGEX_ERR_UNTERMINATED_GROUP_NAME;
			return NULL;
		}
		*rxu8 = u8+1;
		s->backref.name = utf8_dup(n, u8-n);
		mem_gift(s->backref.name, rx);
	}
	else{
		int e;
		s->backref.id = utf8_toul(*rxu8, rxu8, 10, &e);
		if( e ){
			*err = strerror(errno);
			return NULL;
		}
	}

	return s;
}

/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/
/*************** STATE GROUP ***************/
/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/

__private _fn make_fn_group_cmp(regex_t* rx, group_s* grp, quantifier_s* quantifier, _fn lazy){
	jit_s* self = &rx->jit;
	
	_function(self, rx->proto.state, 3);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		_value aDict  = _pop(self);

		_value cn   = _constant_inum(self, _uint, '\n');
		_value c0   = _constant_inum(self, _int, 0);
		_value c1   = _constant_inum(self, _int, 1);
		_value ce   = _constant_inum(self, _long, -1);
		_value qn   = _constant_inum(self, _uint, quantifier->n);
		_value qm   = _constant_inum(self, _uint, quantifier->m);
		_value ret;

		_value match = _variable(self, _uint);
		_clr(self, match);

		_value u8 = _add(self, aU8, aIndex);

		_do(self);
			_make_lazy(rx, self, c0, aDict, aU8, u8, match, qn, lazy);
			unsigned orCount = vector_count(&grp->state);
			for( unsigned ior = 0; ior < orCount; ++ior ){	
				unsigned stCount = vector_count(&grp->state[0]);
				for( unsigned si = 0; si < stCount; ++si ){
					ret = _call(self, rx->proto.state, grp->state[0][si].fn, (_value[]){aDict, aU8, aIndex}, 3);
					_if(self, ret, _le, c0);
						//TODO goto OR || break
					_endif(self);
					_store(self, aIndex, ret);
				}
				//TODO set nextor
			}

			_store(self, match, _add(self, match, c1));
			u8 = _add(self, aU8, aIndex);
			
			_value ch = _deref(self, _utf8, u8);
			if( rx->flags & REGEX_FLAG_DISALBLE_LINE ){
				_if(self, ch, _eq, c0);
			}
			else{
				_if_or(self, ch, _eq, c0, ch, _eq, cn);
			}
				_break(self);
			_endif(self);
		_end_while(self, match, _le, qm);
		
		_if(self, match, _lt, qn);
			_return(self, _mul(self, _sub(self, u8, aU8), ce));
		_endif(self);
		_return(self, _sub(self, u8, aU8));
	return _endfunction(self);
}

__private void make_reverse(regex_t*rx, state_s* state, unsigned count){
	state[count-1].quantifier.greedy = 1;
	while( count --> 0 ){
		_fn lazy = state[count].quantifier.greedy ? NULL : state[count+1].fn;

		switch( state[count].type ){
			case RX_SINGLE:
				state[count].fn = make_fn_single_cmp(rx, state[count].u, &state->quantifier, lazy);
			break;

			case RX_STRING:
				state[count].fn = make_fn_string_cmp(rx, state[count].string, utf8_bytes_count(state[count].string));
			break;

			case RX_BACKREF:

			break;

			case RX_SEQUENCES:

			break;

			case RX_GROUP:

			break;
		}
	}
}

__private int state_group_flags(const utf8_t** rxu8, utf8_t** name, unsigned* flags, const char** err){
	if( **rxu8 != '?' ) return 0;
	++(*rxu8);

	dbg_info("group flags");

	//check if is multi flags
	const utf8_t* ufs = *rxu8;
	const utf8_t* ufe = *rxu8;
	while( *ufe && *ufe != ')' && *ufe != ':' ) ++ufe;
	ufe = *ufe == ':' ? ufe+1 : ufs + 1;

	//parsing flags;
	const utf8_t* n = NULL;
	unsigned l = 0;

	while( ufs < ufe ){
		switch( *ufs ){
			case '|': *flags |= GROUP_FLAG_COUNT_RESET; break;
			case ':': *flags &= ~GROUP_FLAG_CAPTURE; break;
			case '\'': case '<':
				n = ++ufs;
				while( *ufs && *ufs != '>' && *ufs !='\'' ) ++ufs;
				if( *ufs != '<' && *ufs != '\'' ){
					*rxu8 = n;
					*err = REGEX_ERR_UNTERMINATED_GROUP_NAME;
					return -1;
				}
				*flags |= GROUP_FLAG_NAMED;
				l = ufs - n;
			break;

			default: break;
		}
		++ufs;
	}

	if( n ) *name = utf8_dup(n, l);
	
	return 0;
}

__private state_s* state_group(regex_t* rx, state_s* s, unsigned* grpcount, const utf8_t** rxu8, const char** err){
	s->type = RX_GROUP;
	unsigned flags;
	const utf8_t* u8 = *rxu8;
	unsigned gc = *grpcount;
	unsigned gs = gc;

	s->group.id   = gc;
	s->group.name = NULL;
	s->group.flags= GROUP_FLAG_CAPTURE;

	if( state_group_flags(rxu8, &s->group.name, &flags, err) ) return NULL;
	if( s->group.name ) mem_gift(s->group.name, rx);

	__free state_s** state = VECTOR(state_s*, 4);
	unsigned or = 0;
	state[0] = VECTOR(state_s, 4);
	state_s* current;

	while( *u8 ){
		dbg_info("parsing:'%s'", (char*)u8);
		switch( *u8 ){
			case '.':
				current = state_ctor(vector_push(&state[or], NULL));
				if( !state_dot(rx, current, &u8) ) goto ONERR;
				if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
			break;

			case '[': 
				current = state_ctor(vector_push(&state[or], NULL));
				if( !state_sequences(rx, current, &u8, err) ) goto ONERR;
				if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
			break;

			case '(': 
				current = state_ctor(vector_push(&state[or], NULL));
				++gc;
				if( !state_group(rx, current, &gc, &u8, err) ) goto ONERR;
				if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
			break;

			case '|':
				mem_gift(state[or++], state);
				state[or] = VECTOR(state_s, 4);
				if( s->group.flags & GROUP_FLAG_COUNT_RESET ){
					if( gc > *grpcount ) *grpcount = gc;
					gc = gs;
				}
			break;

			case ')': 
				if( gs == 0 ){
					*err = REGEX_ERR_UNTERMINATED_GROUP;
					goto ONERR;
				}
				if( !(s->group.flags & GROUP_FLAG_COUNT_RESET) ) *grpcount = gc;
				mem_gift(state[or++], state);
				s->group.state = state;
				*rxu8 = u8+1;
			return s;

			case ']': *err = REGEX_ERR_UNOPENED_SEQUENCES; goto ONERR;
			case '}': *err = REGEX_ERR_UNOPENED_QUANTIFIERS; goto ONERR;
	
			default :
				current = state_ctor(vector_push(&state[or], NULL));
				if( u8[0] == '\\' && ((u8[1] >= '0' && u8[1] <= '9') || (u8[1] == 'g')) ){
					if( !state_backreferences(rx, current, &u8, err) ) goto ONERR;
					if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
				}
				else{
					rxch_s ch   = parse_get_ch(u8, err);
					if( ch.nb == UINT32_MAX ) goto ONERR;
					rxch_s next = parse_get_ch(u8+ch.nb, err);
					if( next.code == 0 || next.code == '?' || next.code == '+' || next.code == '*' || next.code == '{' ){
						if( !state_single(rx, current, &u8, ch) ) goto ONERR;
						if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
					}
					else{
						if( !state_string(rx, current, &u8, err) ) goto ONERR;
					}
				}
			break;
			
		}
	}

	if( gs != 0 ){
		*err = REGEX_ERR_UNTERMINATED_GROUP;
		goto ONERR;
	}
	mem_gift(state[or++], state);
	s->group.state = state;
	*rxu8 = u8;
	return s;

ONERR:
	mem_gift(state[or++], state);
	*rxu8 = u8;
	DELETE(state);
	return NULL;
}

__private int parse_state(regex_t* rx, const utf8_t* regstr){
	rx->err  = NULL;
	rx->rx   = regstr;
	rx->last = regstr;
	unsigned gc = 0;
	return state_group(rx, &rx->state, &gc, &rx->last, &rx->err) ? 0 : -1;
}

__private int make_state(regex_t* rx){
	_ctor(&rx->jit);
	rx->proto.state = _signature(NULL, _long, (_type[]){_dict, _putf8, _uint}, 3);
	//TODO compilare gli stati, al contrario, rimuovere greedy dallo stato last
	return 0;
}

__private void rx_cleanup(void* mem){
	regex_t* rx = mem;
	_dtor(&rx->jit);
}

regex_t* regex_build(const utf8_t* regstr, unsigned flags){
	regex_t* rx = NEW(regex_t);
	mem_cleanup(rx, rx_cleanup);
	
	rx->flags   = flags;
	if( parse_state(rx, regstr) ) return rx;
	if( make_state(rx) ) return rx;
	return rx;
}

const char* regex_error(regex_t* rx){
	return rx->err;
}


















/*
__private state_s* state_group_end(state_s** state, const utf8_t** rxu8, const char** err){
	dbg_info("group end");
	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_GROUP_END;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->group.idgroup = -1;
	s->group.id = 0;
	s->group.flags = 0;
	s->group.name = NULL;

	unsigned count = vector_count(state);
	if( count == 1 ){
		dbg_info("regex contains only ), return error");
		*err = REGEX_ERR_UNOPENED_GROUP;
		return NULL;
	}
	--count;

	unsigned b = 1;
	while( b && count>0 ){
		--count;
		if( (*state)[count].type == RX_GROUP_END ){
			dbg_info("have group inside, increase b");
			++b;
		}
		else if( (*state)[count].type == RX_GROUP ){
			dbg_info("find open group decrease b");
			--b;
		}
	}

	if( b ){
		dbg_info("groups are not balanced");
		*err = REGEX_ERR_UNOPENED_GROUP;
		return NULL;
	}

	++(*rxu8);
	s->group.idgroup = count;
	return &(*state)[count];
}

__private state_s* state_group_or(state_s** state, const utf8_t** rxu8){
	dbg_info("or");
	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_GROUP_OR;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->group.idgroup = -1;
	s->group.id = 0;
	s->group.flags = 0;
	s->group.name = NULL;
	++(*rxu8);
	return s;
}

__private int parse_quantifier(quantifier_s* q, const utf8_t** rxu8, const char** err){
	dbg_info("quantifiers");
	q->greedy = 1;

	const utf8_t* u8 = *rxu8;
	switch( *u8 ){
		default : q->n = 1; q->m = 1; break;
		case '?': q->n = 0, q->m = 1; ++u8; break;
		case '*': q->n = 0, q->m = UINT32_MAX; ++u8; break;
		case '+': q->n = 1, q->m = UINT32_MAX; ++u8; break;
		case '{':
			u8 = (const utf8_t*)str_skip_h((const char*)(u8+1));
			if( !*u8 || *u8 == '}' ){
				*err = REGEX_ERR_UNTERMINATED_QUANTIFIER;
				return -1;
			}

			if( *u8 == ',' ){
				q->n = 0;
			}
			else{
				const utf8_t* end;
				int e;
				q->n = utf8_toul(u8, &end, 10, &e);
				if( e ){
					*err = strerror(errno);
					*rxu8 = u8;
					return -1;
				}
				u8 = (const utf8_t*)str_skip_h((const char*)end);
			}
			
			if( *u8 == ',' ){
				u8 = (const utf8_t*)str_skip_h((const char*)(u8+1));
				if( *u8 == '}' ){
					q->m = UINT32_MAX;
				}
				else{
					const utf8_t* end;
					int e;
					q->m = utf8_toul(u8, &end, 10, &e);
					if( e ){
						*err = strerror(errno);
						*rxu8 = u8;
						return -1;
					}
					u8 = (const utf8_t*)str_skip_h((const char*)end);
				}
			}
			else if( *u8 == '}' ){
				q->m = q->n;
			}

			if( *u8 != '}' ){
				*err = REGEX_ERR_UNTERMINATED_QUANTIFIER;
				return -1;
			}
			++u8;
		break;
	}
	
	if( *u8 == '?' ){
		q->greedy = 0;
		++u8;
	}
	
	*rxu8 = u8;
	return 0;
}

__private state_s* parse_state(const utf8_t** rxu8, const char** err){
	state_s* state = VECTOR(state_s, 4);
	const utf8_t* u8 = *rxu8;
	state_s* current = NULL;
	unsigned gb = 0;

	while( *u8 ){
		dbg_info("parsing:'%s'", (char*)u8);
		switch( *u8 ){
			case '[': if( !(current=state_sequences(&state, &u8, err)) ) goto ONERR; break;
			case ']': *err = REGEX_ERR_UNOPENED_SEQUENCES; goto ONERR;
	
			case '(': ++gb; if( !(current = state_group(&state, &u8, err)) ) goto ONERR; break;
			case ')': if( !(current = state_group_end(&state, &u8, err)) ) goto ONERR; --gb; break;
			case '|': current = state_group_or(&state, &u8); break;

			case '?': case '+': case '*': case '{':
				if( !current || current->type == RX_STRING ){
					*err = REGEX_ERR_INVALID_QUANTIFIER;
					goto ONERR;
				}
				if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
			break;
			case '}': *err = REGEX_ERR_UNOPENED_QUANTIFIERS; goto ONERR;
	
			case '.': current = state_dot(&state, &u8); break;

			default :{
				if( u8[0] == '\\' && ((u8[1] >= '0' && u8[1] <= '9') || (u8[1] == '<')) ){
					if( !(current = state_backtrack(&state, &u8, err)) ) goto ONERR;
					break;
				}	
				rxch_s ch   = parse_get_ch(u8, err);
				if( ch.nb == UINT32_MAX ) goto ONERR;
				rxch_s next = parse_get_ch(u8+ch.nb, err);
				if( next.code == 0 || next.code == '?' || next.code == '+' || next.code == '*' || next.code == '{' ){
					current = state_single(&state, &u8, ch);
					break;
				}
				if( !state_string(&state, &u8, err) ) goto ONERR;
				current = NULL;
				break;
			}
		}
	}

	if( gb ){
		*rxu8 = u8;
		*err = REGEX_ERR_UNTERMINATED_GROUP;
	}

	*rxu8 = u8;
	return state;

ONERR:
	*rxu8 = u8;
	DELETE(state);
	return NULL;
}

__private unsigned indicize_group_internal(state_s* state, unsigned i, unsigned* id, unsigned reset, unsigned internal){
	unsigned const count = vector_count(&state);
	if( i >= count ) return i;
	unsigned previd = *id;
	unsigned max = 0;

	while( i < count ){
		switch( state[i].type ){
			case RX_GROUP:
				dbg_info("indicize group %u", i);
				state[i].group.idgroup = (*id)++;
				i += indicize_group_internal(state, i+1, id, state[i].group.flags & GROUP_FLAG_COUNT_RESET, 1);
			continue;

			case RX_GROUP_OR:
				dbg_info("or group");
				if( reset ){
					if( *id > max ) max = *id;
					*id = previd;
				}
			break;

			case RX_GROUP_END:
				if( reset ) *id = max;
				if( internal ){
					dbg_info("internal group end");
					return i+1;
				}
				dbg_warning("this is realy?");
			break;

			default: break;
		}
		++i;
	}
	return i;
}

__private void dump_state(const utf8_t* rx, state_s* state){
	printf("dump(%lu):/%s/\n", vector_count(&state), (char*)rx);
	foreach_vector(state, i){
		printf("[%2lu|", i);

		switch( state[i].type ){
			
			case RX_SINGLE: printf("single:<%u>", state[i].single.u); break;
			
			case RX_STRING: printf("string:<%s>", state[i].string.str); break;

			case RX_BACKTRACK: 
				if( state[i].backtrack.name )
					printf("backtrack:<%s>", state[i].string.str);
				else
					printf("backtrack:<%s>", state[i].string.str);
			break;

			case RX_SEQUENCES: 
				printf("sequences:%d<", state[i].sequences.reverse);
				foreach_vector(state[i].sequences.range, j){
					printf("%u-%u,", state[i].sequences.range[j].start, state[i].sequences.range[j].end);
				}
				printf(">");
			break;

			case RX_QUANTIFIERS: printf("error state quantifiers not realy exists"); break;

			case RX_GROUP: 
				printf("group<");
				if( state[i].group.name )
					printf("%s", state[i].group.name);
				else
					printf("%u", state[i].group.id);
				printf(",%ld, 0x%X>", state[i].group.idgroup, state[i].group.flags);
			break;
			
			case RX_GROUP_OR:
				printf("or");
				printf("<%ld, 0x%X>", state[i].group.idgroup, state[i].group.flags);
			break;
			
			case RX_GROUP_END: 
				printf("end");
				printf("<%ld, 0x%X>", state[i].group.idgroup, state[i].group.flags);	
			break;
		}

		printf("{%u,%u}", state[i].quantifier.n, state[i].quantifier.m);
		if( !state[i].quantifier.greedy ) printf("?");
		printf("]\n");
	}
	puts("");
}
*/








/*
void test(void){
	jit_s jit;
	_ctor(&jit);
	_begin(&jit);

	_type sf = _signature(&jit, jit_type_int, (jit_type_t[]){jit_type_int}, 1);
	
	_function(&jit, sf, 1);
		_value a = _pop(&jit);
		_value b = _constant_inum(&jit, jit_type_int, 1);
		_add(&jit, a, b);
		_store(&jit, a);
		_fn tmp = jit.fn;

		_function(&jit, sf, 1);
			_value c = _pop(&jit);
			_value d = _constant_inum(&jit, jit_type_int, 1);
			_add(&jit, c, d);
			_store(&jit, c);
			_return(&jit, c);
		_build(&jit);

		_fn bu = jit.fn;
		jit.fn = tmp;

		_call(&jit, sf, bu, (_value[]){a}, 1);
		a = _pop(&jit);
		
		_return(&jit, a);
	_build(&jit);
	_end(&jit);

	int ret = 0;
	int arg = 8;
	_call_jit(&jit, &ret, jit.fn, (void*[]){&arg});
	printf("return %d\n", ret);


	_dtor(&jit);
}

*/


































/*
__private void make_ctor(regex_t* rx){
	_ctor(&rx->jit);
	_begin(&rx->jit);
	rx->type.utf8      = _pointer(&rx->jit, jit_type_ubyte);
	rx->type.str       = _pointer(&rx->jit, jit_type_sbyte);
	rx->type.dict      = jit_type_void_ptr;
	rx->proto.memcmp   = _signature(&rx->jit, jit_type_int,   (jit_type_t[]){jit_type_void_ptr, jit_type_void_ptr, jit_type_ulong}, 3);
	rx->proto.utf8nb   = _signature(&rx->jit, jit_type_ulong, (jit_type_t[]){jit_type_ubyte}, 1);
	rx->proto.utf8ucs4 = _signature(&rx->jit, jit_type_uint,  (jit_type_t[]){rx->type.utf8}, 1);
	rx->proto.state    = _signature(&rx->jit, jit_type_uint,  (jit_type_t[]){rx->type.dict, rx->type.utf8, jit_type_uint, jit_type_uint}, 4);
	rx->proto.btcheck  = _signature(&rx->jit, jit_type_uint,  (jit_type_t[]){
			rx->type.dict, 
			jit_type_uint, 
			rx->type.str,
		   	rx->type.utf8,
			jit_type_uint,
			jit_type_uint
		}, 
	6);
	_end(&rx->jit);
}

__private void make_dtor(regex_t* rx){
	_dtor(&rx->jit);
}





__private _fn make_fn_quantifier(regex_t* rx, quantifier_s* quantifier, _fn state){
	jit_s* self = &rx->jit;
	_begin(self);
	
	_function(self, rx->proto.state, 4);
		_value aEnd   = _pop(self);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		_value aDict  = _pop(self);

		_value c1 = _constant_inum(self, jit_type_uint, 1);
		_value ce = _constant_inum(self, jit_type_long, -1);
		_value cn = _constant_inum(self, jit_type_uint, quantifier->n);
		_value cm = _constant_inum(self, jit_type_uint, quantifier->m);

		//match = 0;
		_value match = _variable(self, jit_type_ulong);
		_xor(self, match, match);
		_store(self, match);

		//do{
		_do(self);
			//call statetest(dict, u8, index, end)
			_call(self, rx->proto.state, state, (_value[]){aDict, aU8, aIndex, aEnd}, 4);
			_value ret = _pop(self);

			//if not match break
			_if(self, ret, _eq, ce);
				_break(self);
			_endif(self);

			//++match
			_add(self, match, c1);
			_store(self, match);

			//aIndex = ret (end of match, unicode can have multubytes)
			_push(self, ret);
			_store(self, aIndex);
		
		//}while(match < m && index < end)
		_while_and(self, match, _le, cm, aIndex, _lt, aEnd);

		//if( match < n ) return err
		_if(self, match, _lt, cn);
			_return(self, ce);
		_else(self);
			_return(self, aIndex);
	
	_build(self);
	_end(self);

	return self->fn;
}


__private void rx_build_state(regex_t* rx, state_s* state){
	foreach_vector(state, i){
		if( !state[i].quantifier.greedy ) continue;
		switch( state[i].type ){
			case RX_STRING:
				state[i].quantifierfn = make_fn_string_cmp(rx, state[i].string.str, utf8_bytes_count(state[i].string.str));
				state[i].build = 1;
			break;
			
			case RX_SEQUENCES:
				state[i].statefn = make_fn_state_sequences_cmp(rx, &state[i].sequences);
				state[i].quantifierfn = make_fn_quantifier(rx, &state[i].quantifier, state[i].statefn);
				state[i].build = 1;
			break;

			case RX_SINGLE:
				state[i].statefn = make_fn_state_single_cmp(rx, state[i].single.u);
				state[i].quantifierfn = make_fn_quantifier(rx, &state[i].quantifier, state[i].statefn);
				state[i].build = 1;
			break;

			case RX_BACKTRACK:
				state[i].statefn = make_fn_state_backtrack_cmp(rx, &state[i].backtrack);
				state[i].quantifierfn = make_fn_quantifier(rx, &state[i].quantifier, state[i].statefn);
				state[i].build = 1;
			break;

			default: break;
		}
	}
}

__private void regex_jit(regex_t* rx, state_s* state){
	make_ctor(rx);
	rx_build_state(rx, state);
}
*/


//todo move state inside regex, cleanup call free state and free jit ctx
/*
regex_t* regex(const utf8_t* rx, unsigned flags){
	unsigned id = 1;
	regex_t* r = NEW(regex_t);
	r->rx = rx;
	r->err = NULL;
	r->last = rx;
	r->flags = flags;
	state_s* state = parse_state(&r->last, &r->err);
	if( !state ) return r;
	indicize_group_internal(state, 0, &id, 0, 0);

	dump_state(r->rx, state);

	mem_free(state);

	return r;
}

const char* regex_error(regex_t* rex){
	return rex->err;
}

void regex_error_show(regex_t* rex){
	if( !rex->err ) return;
	fprintf(stderr, "error: %s on building regex, at\n", rex->err);
	err_showline((const char*)rex->rx, (const char*)rex->last, 0);
}
*/



/*
__private void captures(fsm_s* fsm, const char* name, unsigned id, const utf8_t* start, unsigned len){
   	generic_s* g = *name ? dict(fsm->cap, name) : dict(fsm->cap, id);
	if( g->type == G_UNSET ){
		capture_s* cap = mem_gift(NEW(capture_s), fsm->cap);
		cap->start = NULL;
		cap->len   = 0;
		*g = GI(cap);
	}
	capture_s* cap = g->obj;
	cap->start = start;
	cap->len = len;
}

__private bool_t state_cmp_ascii(fsmState_s* state, ucs4_t u){
	if( u >= 256 ) return 0;
	dbg_info("cmp ascii");
	const unsigned g = u & 3;
	u >>= 2;
	return state->ascii.chset[g] & (1 << u);
}

__private bool_t state_cmp_unicode(fsmState_s* state, ucs4_t u){
	dbg_info("cmp unicode");
	if( u >= state->unicode.start && u <= state->unicode.end ){
		return state->flags & FSM_STATE_FLAG_REVERSE ? 0 : 1;
	}
	return state->flags & FSM_STATE_FLAG_REVERSE ? 1 : 0;
}

__private unsigned state_cmp_string(fsmState_s* state, const utf8_t* u){
	dbg_info("cmp string %.*s == %.*s", state->string.len, state->string.value, state->string.len, u);
	return !strncmp((const char*)state->string.value, (const char*)u, state->string.len) ? state->string.len : 0;
}

__private unsigned state_cmp_backtrack(fsm_s* fsm, fsmState_s* state, const utf8_t* u){
	dbg_info("backtracking string compare");	
	generic_s* g = state->backtrack.name[0] ? dict(fsm->cap, state->backtrack.name) : dict(fsm->cap, (int)state->backtrack.id);
	if( g->type == G_UNSET ) return 0;
	capture_s* cap = g->obj;
	if( cap->len == 0 ) return 0;
	return !strncmp((const char*)cap->start, (const char*)u, cap->len) ? cap->len : 0;
}

__private uint32_t fsm_walk(fsm_s* fsm, const utf8_t* txt, uint32_t itxt, uint32_t endt, uint32_t istate);
__private uint32_t state_group(fsm_s* fsm, const utf8_t* txt, uint32_t itxt, uint32_t endt, uint32_t istate){
	uint32_t ptxt;
	while( istate ){
		fsmState_s* or = &fsm->state[istate];
		iassert( or->type == FSM_STATE_GROUP_START );
		dbg_info("or");
		if( (ptxt=fsm_walk(fsm, txt, itxt, endt, or->next)) ) return ptxt;
		istate = or->group.next;
	}
	dbg_info("group fail");
	return 0;
}

__private uint32_t fsm_walk(fsm_s* fsm, const utf8_t* txt, uint32_t itxt, uint32_t endt, uint32_t istate){
	ucs4_t u;
	unsigned tmp;

	dbg_info("walk state %u from %u -> %u", istate, itxt, endt);

	do{
		fsmState_s* state = &fsm->state[istate];
		unsigned match = 0;
		while( itxt < endt ){
			switch( state->type ){
				case FSM_STATE_ASCII:
					dbg_info("ascii");
					u = utf8_to_ucs4(&txt[itxt]);
					if( !state_cmp_ascii(state, u) ) goto ENDMATCH;
					itxt += utf8_codepoint_nb(txt[itxt]);
				break;

				case FSM_STATE_UNICODE:
					dbg_info("unicode");
					u = utf8_to_ucs4(&txt[itxt]);
					if( !state_cmp_unicode(state, u) ) goto ENDMATCH;
					itxt += utf8_codepoint_nb(txt[itxt]);
				break;

				case FSM_STATE_STRING:
					dbg_info("string");
					if( !(tmp=state_cmp_string(state, &txt[itxt])) ) goto ENDMATCH;
					itxt += tmp;
				break;

				case FSM_STATE_BACKTRACK:
					dbg_info("backtrack");
					if( !(tmp=state_cmp_backtrack(fsm, state, &txt[itxt])) ) goto ENDMATCH;
					itxt += tmp;
				break;

				case FSM_STATE_GROUP_START:
					dbg_info("group");
					if( (tmp=state_group(fsm, txt, itxt, endt, state->group.next)) ){
						if( state->flags & FSM_STATE_FLAG_CAPTURE ) captures(fsm, state->group.name, state->group.id, &txt[itxt], tmp - itxt);
						itxt += tmp;
					}
					else{
						goto ENDMATCH;
					}
				break;

				case FSM_STATE_GROUP_END:
					dbg_info("end group");
					return itxt;
				break;
			}
			dbg_info("match");
			++match;

			if( match >= state->qn && match <= state->qm && state->flags & FSM_STATE_FLAG_NGREEDY ){
				if( (tmp=fsm_walk(fsm, txt, itxt, endt, state->next)) ){
					return itxt+tmp;
				}
			}
		}
		ENDMATCH:
		if( !match && !state->qn && !state->qm ) die("");
		dbg_info("match %u <= %u <= %u", state->qn, match, state->qm);
		if( match < state->qn || match > state->qm ) return 0;
		istate = state->next;
	}while( istate && itxt < endt );
	
	return istate ? 0 : itxt;
}

int fsm_exec(fsm_s* fsm, const utf8_t* txt, unsigned len){
	dbg_info("");
	unsigned itxt = 0;
	long ret = 0;

	fsm->cap = dict_new();

	if( fsm->flags & FSM_FLAG_START ){
		dbg_info("need start with");
		ret = fsm_walk(fsm, txt, itxt, len, 0) ? 0 : -1;
	}
	else{
		dbg_info("search match[0]");
		while( itxt < len && !(ret=fsm_walk(fsm, txt, itxt, len, 0)) ){
			++itxt;
			dbg_warning("search match[%u]%.*s", itxt, len-itxt, &txt[itxt]);
		}
	}

	if( !ret ){
		DELETE(fsm->cap);
		return -1;
	}
	
	if( fsm->flags & FSM_FLAG_END && ret != len ){
		DELETE(fsm->cap);
		return -1;
	}

	return 0;
}

fsm_s* fsm_ctor(fsm_s* fsm){
	fsm->err   = NULL;
	fsm->state = NULL;
	fsm->flags = 0;
	fsm->cap   = NULL;
	return fsm;
}

fsm_s* fsm_dtor(fsm_s* fsm){
	if( fsm->state ) DELETE(fsm->state);
	return fsm;
}









#define BSEQ_MAX 256

typedef struct bseq{
	fsmStateUnicode_s ascii[BSEQ_MAX];
	fsmStateUnicode_s unico[BSEQ_MAX];
	unsigned acount;
	unsigned ucount;
	unsigned reverse;
}bseq_s;

__private fsmState_s* state_get(fsm_s* fsm, unsigned id){
	return &fsm->state[id];
}

__private unsigned state_new(fsm_s* fsm, fsmStateType_e type){
	unsigned id = fsm->bcstate;
	fsmState_s* state = vector_push(&fsm->state, NULL);
	state->type  = type;
	state->next  = ++fsm->bcstate;
	state->flags = 0;
	state->qn    = 0;
	state->qm    = 0;
	return id;
}

__private void state_ascii_uset(fsmState_s* state, ucs4_t st, ucs4_t en){
	for( ucs4_t u4 = st; u4 <= en; ++u4 ){
		unsigned m = u4 & 0x3;
		unsigned b = u4 >> 2;
		dbg_info("set [%u] bit: %u", m, b);
		state->ascii.chset[m] |= 1 << b;
	}
}

__private void state_ascii_reverse(fsmState_s* state){
	for( unsigned i = 0; i < 4; ++i ){
		state->ascii.chset[i] = ~state->ascii.chset[i];
	}
}

__private const utf8_t* u8seq(const utf8_t* u8, ucs4_t* unicode){
	if( *u8 == '\\' ){
		++u8;
		switch( *u8 ){
			case 'n': *unicode = str_to_ucs4("\n"); return u8+1;
			case 't': *unicode = str_to_ucs4("\t"); return u8+1;
			case 'u':
			case 'U':{
				int err;
				*unicode = utf8_toul(u8+1, &u8, 16, &err);
				if( err ) return NULL;
				return u8;
			}
		}
	}
	
	*unicode = utf8_to_ucs4(u8);
	return u8 + utf8_codepoint_nb(*u8);
}

__private int bseq_add(bseq_s* bs, ucs4_t st, ucs4_t en){
	if( st > en ) swap(st,en);
	if( en < 256 ){
		if( bs->acount >= BSEQ_MAX ) return -1;
		bs->ascii[bs->acount].start = st;
		bs->ascii[bs->acount].end = en;
		++bs->acount;
	}
	else{
		if( bs->ucount >= BSEQ_MAX ) return -1;
		bs->unico[bs->ucount].start = st;
		bs->unico[bs->ucount].end = en;
		++bs->ucount;
	}
	return 0;
}

__private int bseq_cmp(const void* a, const void* b){ return ((fsmStateUnicode_s*)a)->start - ((fsmStateUnicode_s*)b)->start; }


__private void bseq_merge(bseq_s* bs){
	if( !bs->ucount ) return;

	qsort(bs->unico, bs->ucount, sizeof(fsmStateUnicode_s), bseq_cmp);

	unsigned count = bs->ucount;
	unsigned i = 0;
	while( i < count-1 ){
		dbg_info("try merge %u-%u with %u-%u", bs->unico[i].start, bs->unico[i].end, bs->unico[i+1].start, bs->unico[i+1].end);
		if( bs->unico[i+1].start <= bs->unico[i].end + 1 ){
			dbg_info("\tmerge: .s(%u) .e(%d) + .s(%u) .e(%d)", bs->unico[i].start, bs->unico[i].end, bs->unico[i+1].start, bs->unico[i+1].end);
			if( bs->unico[i].end < bs->unico[i+1].end ) bs->unico[i].end = bs->unico[i+1].end;
			dbg_info("\t.s(%u) .e(%d)", bs->unico[i].start, bs->unico[i].end);

			if( i+2 < count ){
				memmove(&bs->unico[i+1], &bs->unico[i+2], sizeof(fsmStateUnicode_s) *(count - (i+1)));
			}
			--count;
			continue;
		}
		++i;
	}
	bs->ucount = count;

	
	//for( unsigned i = 0; i < bs->ucount; ++i ){
	//	dbg_info("[%u/%u] %u-%u", i, bs->ucount, bs->unico[i].start, bs->unico[i].end);
	//}
	//die("");
}

__private const utf8_t* build_sequences_get(fsm_s* fsm, const utf8_t* regex, bseq_s* bs){
	const utf8_t* stsq = regex-1;
	bs->reverse = 0;
	bs->acount  = 0;
	bs->ucount  = 0;

	if( !*regex ){
		fsm->err = FSM_ERR_MISSING_SEQUENCES;
		return regex-1;
	}
	
	if( *regex == '^' ){
		dbg_info("is reverse");
		bs->reverse = 1;
		++regex;
	}

	while( *regex && *regex != ']' ){
		ucs4_t st;
		ucs4_t en;
		const utf8_t* stored = regex;
		regex = u8seq(regex, &st);
		if( !regex ){
			fsm->err = FSM_ERR_INVALID_UTF8;
			return stored;
		}
		en = st;
		if( *regex == '-' && *(regex+1) != ']' ){
			stored = regex;
			regex = u8seq(regex+1, &en);
			if( !regex ){
				fsm->err = FSM_ERR_INVALID_UTF8;
				return stored;
			}	
		}
		dbg_info("add sequences: %u-%u", st, en);
		if( bseq_add(bs, st, en) ){
			fsm->err = FSM_ERR_TO_MANY_SEQUENCES;
			return regex;
		}
	}

	if( *regex != ']' ){
		fsm->err = FSM_ERR_UNTERMINATED_SEQUENCES;
		return stsq;
	}
	if( bs->acount + bs->ucount == 0 ){
		fsm->err = FSM_ERR_ASPECTED_SEQUENCES;
		return regex;
	}
	regex = utf8_codepoint_next(regex);

	bseq_merge(bs);

	return regex;
}

__private const utf8_t* quantifiers(fsm_s* fsm, const utf8_t* regex, unsigned* qn, unsigned* qm, unsigned* greedy){
	*greedy = 1;
	const utf8_t* begin = regex;
	switch( *regex ){
		default : *qn = 1; *qm = 1; return regex;
		case '?': *qn = 0, *qm = 1; ++regex; break;
		case '*': *qn = 0, *qm = UINT16_MAX; ++regex; break;
		case '+': *qn = 1, *qm = UINT16_MAX; ++regex; break;
		case '{':
			regex = (const utf8_t*)str_skip_h((const char*)(regex+1));
			if( !*regex || *regex == '}' ){
				fsm->err = FSM_ERR_UNTERMINATED_QUANTIFIERS;
				return begin;
			}

			if( *regex == ',' ){
				*qn = 0;
			}
			else{
				const utf8_t* end;
				int e;
				*qn = utf8_toul(regex, &end, 10, &e);
				if( e ){
					fsm->err = strerror(errno);
					return regex;
				}
				regex = (const utf8_t*)str_skip_h((const char*)end);
			}
			
			regex = (const utf8_t*)str_skip_h((const char*)(regex+1));
			if( *regex == ',' ){
				regex = (const utf8_t*)str_skip_h((const char*)(regex+1));
				if( *regex == '}' ){
					*qm = UINT16_MAX;
				}
				else{
					const utf8_t* end;
					int e;
					*qm = str_toul((const char*)regex, (const char**)&end, 10, &e);
					if( e ){
						fsm->err = strerror(errno);
						return regex;
					}
					regex = (const utf8_t*)str_skip_h((const char*)end);
				}
			}
			else if( *regex == '}' ){
				*qm = *qn;
			}

			if( *regex != '}' ){
				fsm->err = FSM_ERR_UNTERMINATED_QUANTIFIERS;
				return begin;
			}
			++regex;
		break;
	}
	
	if( *regex == '?' ){
		*greedy = 0;
		++regex;
	}
	
	return regex;
}

__private unsigned build_state_sequences_ascii(fsm_s* fsm, bseq_s* bs, unsigned qn, unsigned qm, unsigned greedy){
	dbg_info("create sequences ascii(%d)", bs->acount);
	unsigned idstate = state_new(fsm, FSM_STATE_ASCII);
	fsmState_s* state = state_get(fsm, idstate);
	state->qm = qm;
	state->qn = qn;
	if( !greedy ) state->flags |= FSM_STATE_FLAG_NGREEDY;
	for( unsigned i = 0; i < bs->acount; ++i ){
		dbg_info("set %d-%d", bs->ascii[i].start, bs->ascii[i].end);
		state_ascii_uset(state, bs->ascii[i].start, bs->ascii[i].end);
	}
	if( bs->reverse ) state_ascii_reverse(state);
	return idstate;
}

__private unsigned build_state_sequences_unicode(fsm_s* fsm, bseq_s* bs, unsigned qn, unsigned qm, unsigned greedy){
	dbg_info("create sequences unicode");
	if( !bs->acount && bs->ucount == 1 ){
		unsigned idstate = state_new(fsm, FSM_STATE_UNICODE);
		fsmState_s* unic = state_get(fsm, idstate);
		unic->unicode.start = bs->unico[0].start;
		unic->unicode.end   = bs->unico[0].end;
		if( bs->reverse ) unic->flags |= FSM_STATE_FLAG_REVERSE;
		dbg_info("create single unicode sequences: %u-%u", unic->unicode.start, unic->unicode.end);
		return idstate;
	}

	dbg_info("create sequences group");
	unsigned idgrpstart = state_new(fsm, FSM_STATE_GROUP_START);
	state_get(fsm,idgrpstart)->group.next = state_get(fsm, idgrpstart)->next;
	uint32_t* ornext = NULL;

	if( bs->acount ){
		dbg_info("or ascii");
		unsigned idgrporstart = state_new(fsm, FSM_STATE_GROUP_START);
		build_state_sequences_ascii(fsm, bs, qn, qm, greedy);
		unsigned idgrporend = state_new(fsm, FSM_STATE_GROUP_END);
		state_get(fsm,idgrporstart)->group.next = state_get(fsm, idgrporend)->next;
		state_get(fsm, idgrporend)->next = 0;
		ornext = &state_get(fsm, idgrporstart)->group.next;
	}

	for( unsigned i = 0; i < bs->ucount; ++i ){
		unsigned idgrporstart = state_new(fsm, FSM_STATE_GROUP_START);

		fsmState_s* unic = state_get(fsm, state_new(fsm, FSM_STATE_UNICODE));
		unic->unicode.start = bs->unico[i].start;
		unic->unicode.end   = bs->unico[i].end;
		dbg_info("or unicode: %u-%u", unic->unicode.start, unic->unicode.end);
		if( bs->reverse ) unic->flags |= FSM_STATE_FLAG_REVERSE;
		
		unsigned idgrporend = state_new(fsm, FSM_STATE_GROUP_END);
		state_get(fsm,idgrporstart)->group.next = state_get(fsm, idgrporend)->next;
		state_get(fsm, idgrporend)->next = 0;
		ornext = &state_get(fsm, idgrporstart)->group.next;
	}
	
	iassert(ornext);
	*ornext = 0;
	dbg_info("group sequences end");

	unsigned idgrpend = state_new(fsm, FSM_STATE_GROUP_END);
	state_get(fsm,idgrpstart)->next = state_get(fsm, idgrpend)->next;

	return idgrpstart;
}

__private const utf8_t* build_sequences(fsm_s* fsm, const utf8_t* regex){
	unsigned qm;
	unsigned qn;
	unsigned greedy;
	bseq_s bs;

	dbg_info("build sequences");

	regex = build_sequences_get(fsm, regex, &bs);
	if( fsm->err ) return regex;

	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;
	dbg_info("sequences quantifiers: %u %u", qn, qm);

	if( bs.ucount ){
		build_state_sequences_unicode(fsm, &bs, qn, qm, greedy);
	}
	else{
		build_state_sequences_ascii(fsm, &bs, qn, qm, greedy);
	}

	return regex;
}

__private const utf8_t* build_char(fsm_s* fsm, const utf8_t* regex){
	ucs4_t ch;
	if( *regex == '\\' ){
		++regex;
		switch( *regex ){
			case 'n': ch = utf8_to_ucs4(U8("\n")); ++regex; break;
			case 't': ch = utf8_to_ucs4(U8("\t")); ++regex; break;
			default : ch = utf8_to_ucs4(regex); regex += utf8_codepoint_nb(*regex); break;
		}
	}
	else{
		ch = utf8_to_ucs4(regex); 
		regex += utf8_codepoint_nb(*regex);
	}
	
	unsigned qm;
	unsigned qn;
	unsigned greedy;
	bseq_s bs = {
		.acount = 1,
		.ucount = 0,
		.reverse = 0,
		.ascii[0].start = ch,
		.ascii[0].end   = ch
	};
	
	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;
	dbg_info("sequences quantifiers: %u %u", qn, qm);
	build_state_sequences_ascii(fsm, &bs, qn, qm, greedy);
	return regex;
}

__private const utf8_t* build_unicode(fsm_s* fsm, const utf8_t* regex){
	ucs4_t ch;
	if( *regex == '\\' ){
		++regex;
		switch( *regex ){
			case 'n': ch = utf8_to_ucs4(U8("\n")); ++regex; break;
			case 't': ch = utf8_to_ucs4(U8("\t")); ++regex; break;
			default : ch = utf8_to_ucs4(regex); regex += utf8_codepoint_nb(*regex); break;
		}
	}
	else{
		ch = utf8_to_ucs4(regex); 
		regex += utf8_codepoint_nb(*regex);
	}
	
	unsigned qm;
	unsigned qn;
	unsigned greedy;
	bseq_s bs = {
		.acount = 0,
		.ucount = 1,
		.reverse = 0,
		.unico[0].start = ch,
		.unico[0].end   = ch
	};
	
	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;
	dbg_info("sequences quantifiers: %u %u", qn, qm);
	build_state_sequences_unicode(fsm, &bs, qn, qm, greedy);
	return regex;
}

__private const utf8_t* build_string(fsm_s* fsm, const utf8_t* regex){
	__private const char* quant = "{?+*";
	__private const char* sep   = "[]().";
	
	dbg_info("creating string:");
	fsmState_s* state = state_get(fsm, state_new(fsm, FSM_STATE_STRING));
	state->qm = 1;
	state->qn = 1;
	state->string.len = 0;
	utf8_t* end = &state->string.value[0];

	while( *regex ){
		if( strchr(sep, *regex) ) goto ENDSTRING;
		
		if( *regex == '\\' ){
			switch( regex[1] ){
				case '<':
				case '0' ... '9':
				goto ENDSTRING;

				case 'n':
					if( strchr(quant, regex[2]) ) goto ENDSTRING;
					if( end - state->string.value >= FSM_STRING_MAX - 1 ) goto ENDSTRING;
					*end++ = '\n';
					regex += 2;
				continue;

				case 't':
					if( strchr(quant, regex[2]) ) goto ENDSTRING;
					if( end - state->string.value >= FSM_STRING_MAX - 1 ) goto ENDSTRING;
					*end++ = '\t';
					regex += 2;
				continue;

				case 'u':
				case 'U':{
					int err;
					const utf8_t* en;
					ucs4_t unicode = utf8_toul(&regex[2], &en, 16, &err);
					if( err ){
						fsm->err = FSM_ERR_INVALID_UTF8;
						return regex;
					}
					if( strchr(quant, *en) ) goto ENDSTRING;
					if( end - state->string.value >= FSM_STRING_MAX - (1+4) ) goto ENDSTRING;
					end += ucs4_to_utf8(unicode, end);
					regex = en;
					continue;
				}
			}

			unsigned nb = utf8_codepoint_nb(regex[1]);
			if( strchr(quant, regex[1+nb]) ) goto ENDSTRING;
			if( end - state->string.value >= FSM_STRING_MAX - (1+nb) ) goto ENDSTRING;
			end = utf8_chcp(end, regex, nb);
			regex += nb;
		}
		else{
			if( strchr(quant, regex[1]) ) goto ENDSTRING;
			unsigned nb = utf8_codepoint_nb(*regex);
			if( end - state->string.value >= FSM_STRING_MAX - (1+nb) ) goto ENDSTRING;
			end = utf8_chcp(end, regex, nb);
			regex += nb;
		}
	}

	if( end == state->string.value ) goto ONERR;

ENDSTRING:
	*end = 0;
	state->string.len = end - state->string.value;
	dbg_info("build string:'%.*s'", state->string.len, state->string.value);
	return regex;
ONERR:
	fsm->err = "internal error, unaspected state of string";
	return regex;
}

__private const utf8_t* build_dot(fsm_s* fsm, const utf8_t* regex){
	unsigned qn, qm, greedy;
	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;
	dbg_info("create dot: %d %d", qn, qm);
	fsmState_s* state = state_get(fsm, state_new(fsm, FSM_STATE_UNICODE));
	state->qm = qm;
	state->qn = qn;
	if( !greedy ) state->flags |= FSM_STATE_FLAG_NGREEDY;
	state->unicode.start = 0;
	state->unicode.end   = UINT32_MAX;

	return regex;
}

__private const utf8_t* build_backtrack(fsm_s* fsm, const utf8_t* regex){
	dbg_info("create backtrack:\"%s\"", regex);
	fsmState_s* state = state_get(fsm, state_new(fsm, FSM_STATE_BACKTRACK));
	++regex;

	if( *regex == '<' ){
		const utf8_t* stored = regex;
		unsigned l = 0;
		while( *regex && *regex != '>' ){
			if( l >= FSM_CAPTURE_NAME_MAX - 1 ){
				fsm->err = FSM_ERR_CAPTURE_NAME_TO_LONG;
				return regex;
			}
			state->backtrack.name[l++] = *regex++;
		}

		if( *regex != '>' ){
			fsm->err = FSM_ERR_CAPTURE_NAME_NOT_ENDING;
			return stored;
		}
		
		state->backtrack.name[l] = 0;
		state->backtrack.id = 0;
		++regex;
	}
	else{
		int err;
		state->backtrack.name[0] = 0;
		state->backtrack.id = utf8_toul(regex, &regex, 10, &err);
		if( err ){
			fsm->err = FSM_ERR_CAPTURE_NUMBERS_INVALID;
			return regex;
		}
	}

	unsigned qn, qm, greedy;
	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;
	state->qm = qm;
	state->qn = qn;
	if( !greedy ) state->flags |= FSM_STATE_FLAG_NGREEDY;
	dbg_info("backtrack quantifiers: %d %d", qn, qm);

	return regex;
}

__private const utf8_t* build_group(fsm_s* fsm, const utf8_t* regex, unsigned* gid, int virtual);

__private const utf8_t* build_or(fsm_s* fsm, const utf8_t* regex, unsigned* gid){
	while( *regex && !fsm->err ){
		switch( *regex ){
			case '(': ++(*gid); regex = build_group(fsm, regex, gid, 0); break;
			case '[': regex = build_sequences(fsm, regex+1); break;
			case '.': regex = build_dot(fsm, regex+1); break;
			case '|': dbg_info("is or"); return regex;
			case ')': dbg_info("is end grp"); return regex;

			case '\\':{
				if( (regex[1] >= '0' && regex[1] <= '9') || regex[1] == '<' ){
					regex = build_backtrack(fsm, regex);
					break;
				}
				
				const utf8_t* ckq = utf8_codepoint_next(&regex[1]);
				if( strchr("{+?*", *ckq) ){
					regex = utf8_to_ucs4(&regex[1]) < 256 ? build_char(fsm, regex) : build_unicode(fsm, regex);
					break;
				}

				regex = build_string(fsm, regex);
				break;
			}

			default:{
				const utf8_t* ckq = utf8_codepoint_next(regex);
				if( strchr("{+?*", *ckq) ){
					regex = utf8_to_ucs4(&regex[1]) < 256 ? build_char(fsm, regex) : build_unicode(fsm, regex);
				}
				else{
					regex = build_string(fsm, regex);
				}
				break;
			}
		}
	}
	return regex;
}

__private const utf8_t* build_group_parse_flags(fsm_s* fsm, const utf8_t* regex, int* rc, int* cap, const utf8_t** name, unsigned* len){
	const utf8_t* stored = regex;
	unsigned r      = 0;
	const utf8_t* n = NULL;
	unsigned l      = 0;
	unsigned c      = 1;

	while( *regex ){
		switch( *regex ){
			case '|': r = 1; break;
			case '!': c = 0; break;

			case '<':
				n = ++regex;
				while( *regex && *regex != '>' && *regex !=':' ) ++regex;
				l = regex - n;
				if( *regex == '>' ) ++regex;
			break;

			case ':':
				if( l ){
					if( l >= FSM_CAPTURE_NAME_MAX -1 ){
						fsm->err = FSM_ERR_CAPTURE_NAME_TO_LONG;
						return &n[FSM_CAPTURE_NAME_MAX];
					}
					if( n[l] != '>' ){
						fsm->err = FSM_ERR_CAPTURE_NAME_NOT_ENDING;
						return n-1;
					}
				}
				*cap  = c;
				*rc   = r;
				*name = n;
				*len  = l;
			return regex+1;

			default : return stored;
		}
	}

	return stored;
}

__private const utf8_t* build_group(fsm_s* fsm, const utf8_t* regex, unsigned* gid, int virtual){
	int cap        = 1;
	int resetCount = 0;
	const utf8_t* name = NULL;
	unsigned startgid = *gid;
	unsigned len = 0;
	unsigned greedy = 1;
	unsigned qn;
	unsigned qm;
	const utf8_t* begin = regex;

	if( !virtual ){
		++regex;
		regex = build_group_parse_flags(fsm, regex, &resetCount, &cap, &name, &len);
		if( fsm->err ) return regex;
	}
	
	unsigned idgrpstart = state_new(fsm, FSM_STATE_GROUP_START);
	fsmState_s* tmpState = state_get(fsm, idgrpstart);
	tmpState->group.next = tmpState->next;
	tmpState->group.id   = *gid;
	uint32_t* ornext = NULL;
	
	if( name && len ){
		memcpy(tmpState->group.name, name, len);
		tmpState->group.name[len] = 0;
		dbg_info("create new group <%s>", tmpState->group.name);
	}
	else{
		dbg_info("create new group %d", tmpState->group.id);
	}

	while( *regex && !fsm->err ){
		dbg_info("or state new");
		unsigned idgrporstart = state_new(fsm, FSM_STATE_GROUP_START);

		regex = build_or(fsm, regex, gid);
		if( fsm->err ) return regex;
	
		unsigned idgrporend = state_new(fsm, FSM_STATE_GROUP_END);

		state_get(fsm,idgrporstart)->group.next = state_get(fsm, idgrporend)->next;
		state_get(fsm, idgrporend)->next = 0;
		ornext = &state_get(fsm, idgrporstart)->group.next;
		
		dbg_info("or state end");

		if( *regex == ')' )	break;
		if( *regex == '|' ){
			if( resetCount ) *gid = startgid;
			++regex;
		}
	}
	if( fsm->err ) return regex;
	iassert(ornext);
	if( ornext ) *ornext = 0;

	if( *regex == ')' ){
		if( virtual ){
			fsm->err = FSM_ERR_GROUP_NOT_OPENED;
			return regex;
		}
		++regex;
	}
	else if( !virtual ){
		fsm->err = FSM_ERR_GROUP_UNTERMINATED;
		return begin;
	}

	unsigned idgrpend = state_new(fsm, FSM_STATE_GROUP_END);
	state_get(fsm, idgrpstart)->next = virtual ? 0 : state_get(fsm, idgrpend)->next;

	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	dbg_info("group end, quantifiers: %d %d", qn, qm);

	tmpState = state_get(fsm, idgrpstart);
	tmpState->qn = qn;
	tmpState->qm = qm;
	if( !greedy ) tmpState->flags |= FSM_STATE_FLAG_NGREEDY;
	if( cap     ) tmpState->flags |= FSM_STATE_FLAG_CAPTURE;

	return regex;
}

const utf8_t* fsm_build(fsm_s* fsm, const utf8_t* regex){
	fsm->state = VECTOR(fsmState_s, 8);
	unsigned gid = 0;
	return build_group(fsm, regex, &gid, 1);
}

__private void state_dump(unsigned istate, fsmState_s* state){
	printf("[%u]{", istate);
	switch( state->type ){
		case FSM_STATE_ASCII: printf(" a "); break;
		case FSM_STATE_UNICODE: printf(" u "); break;
		case FSM_STATE_GROUP_START: printf(" ( "); break;
		case FSM_STATE_GROUP_END: printf(" ) "); break;
		case FSM_STATE_STRING: printf(" \" "); break;
		case FSM_STATE_BACKTRACK: printf(" \\ "); break;
	}
	printf("{%u,%u}->%u}", state->qn, state->qm, state->next);
}

void fsm_dump(fsm_s* fsm, unsigned istate, unsigned sub){
	do{
		fsmState_s* state = &fsm->state[istate];
		for( unsigned i = 0; i < sub; ++i ) putchar(' ');
		state_dump(istate, state);
		putchar('\n');
		if( state->type == FSM_STATE_GROUP_START ){
			uint32_t os = state->group.next;
			while( os ){
				fsmState_s* or = &fsm->state[os];
				//printf("os:%u or:%u", os, or->next);
				fsm_dump(fsm, or->next, sub+1);
				os = or->group.next;
			}
		}
		if( istate && fsm->state[istate].next == istate ) die("next %u == istate %u", fsm->state[istate].next, istate);
		istate = fsm->state[istate].next;
	}while(istate);	
}



















regex_s* regex_ctor(regex_s* rex, const utf8_t* rx){
	rex->rx   = rx;
	fsm_ctor(&rex->fsm);
	rex->last = fsm_build(&rex->fsm, rex->rx);
	if( !rex->fsm.err ) rex->last = NULL;
	return rex;
}

regex_s* regex(const utf8_t* rx){
	regex_s* rex = NEW(regex_s);
	regex_ctor(rex, rx);
	mem_gift(rex->fsm.state, rex);
	return rex;
}

const char* regex_error(regex_s* rex){
	return rex->fsm.err;
}

const utf8_t* regex_error_at(regex_s* rex){
	return rex->last;
}

void regex_error_show(regex_s* rex){
	if( !rex->fsm.err ) return;
	fprintf(stderr, "error: %s on building regex, at\n", rex->fsm.err);
	err_showline((const char*)rex->rx, (const char*)rex->last, 0);
}

dict_t* match(regex_s* rex, const utf8_t* txt, unsigned flags){
	if( !rex->last ) rex->last = txt;

	if( flags & MATCH_FULL_TEXT ){
		rex->len = utf8_bytes_count(rex->last);
		dbg_error("search in: '%.*s'", rex->len, rex->last);
		if( fsm_exec(&rex->fsm, rex->last, rex->len) ){
			rex->last = NULL;
			return NULL;
		}
		generic_s* gm = dict(rex->fsm.cap, 0);
		iassert( gm->type == G_OBJ );
		iassert( gm->obj );
		capture_s* cap = gm->obj;

		if( flags & MATCH_CONTINUE_AFTER ){
			rex->last = cap->start + cap->len;
		}
		else{
			rex->last = cap->start + 1;
		}
		return rex->fsm.cap;
	}

	while( rex->last && *rex->last ){
		const utf8_t* le = utf8_line_end(rex->last);
		rex->len = le - rex->last;

		if( fsm_exec(&rex->fsm, rex->last, rex->len) ){
			if( !*le ){
				rex->last = NULL;
				return NULL;
			}
			rex->last = le+1;
			continue;
		}

		generic_s* gm = dict(rex->fsm.cap, 0);
		iassert( gm->type == G_OBJ );
		iassert( gm->obj );
		capture_s* cap = gm->obj;

		if( (rex->fsm.flags & FSM_FLAG_START) || flags & MATCH_ONE_PER_LINE ){
			rex->last = *le ? le+1 : le;
		}
		else if( flags & MATCH_CONTINUE_AFTER ){
			rex->last = cap->start + cap->len;
		}
		else{
			rex->last = cap->start + 1;
		}
		return rex->fsm.cap;
	}

	rex->last = NULL;
	return NULL;
}

capture_s* capture(dict_t* cap, unsigned id, const char* name){
	static capture_s empty = { .start = NULL, .len = 0 };
	generic_s* gc = name ? dict(cap, name) : dict(cap, id);
	if( gc->type != G_OBJ ) return &empty;
	return gc->obj;
}

*/
