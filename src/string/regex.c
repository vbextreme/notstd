#include "jit/jit-type.h"
#include <notstd/regex.h>
#include <notstd/vector.h>
#include <notstd/str.h>
#include <jit/jit.h>
#include <jit.h>

//TODO !greedy before state group

typedef enum type{
	RX_STRING,
	RX_SINGLE,
	RX_SEQUENCES,
	RX_QUANTIFIERS,
	RX_BACKTRACK,
	RX_GROUP,
	RX_GROUP_END,
	RX_GROUP_OR
}type_e;

typedef struct quantifier{
	unsigned n;
	unsigned m;
	unsigned greedy;
}quantifier_s;

typedef struct string{
	utf8_t* str;
}string_s;

typedef struct single{
	ucs4_t u;
}single_s;

typedef struct range{
	ucs4_t start;
	ucs4_t end;
}range_s;

typedef struct sequences{
	range_s* range;
	unsigned reverse;
}sequences_s;

typedef struct backtrack{
	unsigned id;
	char* name;
}backtrack_s;

#define GROUP_FLAG_CAPTURE     0x01
#define GROUP_FLAG_COUNT_RESET 0x02

typedef struct group{
	long idgroup;
	unsigned flags;
	unsigned id;
	char*    name;
}group_s;

typedef struct state{
	type_e       type;
	quantifier_s quantifier;
	jit_function_t statefn;
	jit_function_t quantifierfn;
	unsigned build;
	union{
		string_s    string;
		single_s    single;
		sequences_s sequences;
		group_s     group;
		backtrack_s backtrack;
	};
}state_s;

typedef struct rxch{
	ucs4_t code;
	uint32_t nb;
}rxch_s;

typedef struct jtype{
	_type utf8;
	_type str;
	_type dict;
}jtype_s;

typedef struct jproto{
	_type memcmp;
	_type utf8nb;
	_type utf8ucs4;
	_type state;
	_type btcheck;
}jproto_s;

struct regex{
	jit_s         jit;
	jtype_s       type;
	jproto_s      proto;
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

__private state_s* state_string(state_s** state, const utf8_t** rxu8, const char** err){
	const utf8_t* u8 = *rxu8;

	utf8_t* str = MANY(utf8_t, 8);
	unsigned size = 4;
	unsigned len  = 0;

	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_STRING;
	s->quantifier.m = 1;
	s->quantifier.n = 1;
	s->quantifier.greedy = 1;

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
	
	s->string.str = str;
	dbg_info("gift string(%s)", str);
	mem_gift(str, *state);
	*rxu8 = u8;
	return s;
}

__private state_s* state_single(state_s** state, const utf8_t** rxu8, rxch_s ch){
	dbg_info("single");
	state_s* s = vector_push(state, NULL);
	s->type = RX_SINGLE;
	s->build = 0;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->single.u = ch.code;
	*rxu8 += ch.nb;
	return s;
}

__private state_s* state_dot(state_s** state, const utf8_t** rxu8){
	dbg_info("dot");
	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_SEQUENCES;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->sequences.reverse = 0;
	s->sequences.range = VECTOR(range_s, 2);
	dbg_info("gift range");
	mem_gift(s->sequences.range, *state);
	range_s* r = vector_push(&s->sequences.range, NULL);
	r->start = 1;
	r->end   = UINT32_MAX;
	*rxu8 += 1;
	return s;
}

__private state_s* state_backtrack(state_s** state, const utf8_t** rxu8, const char** err){
	dbg_info("backtrack");
	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_BACKTRACK;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->backtrack.id = 0;
	s->backtrack.name = NULL;

	++(*rxu8);
	if( **rxu8 == '<' ){
		const utf8_t* n = *rxu8+1;
		const utf8_t* u8 = n;
		while( *u8 && *u8 != '>' ) ++u8;
		if( *u8 != '>' ){
			*err = REGEX_ERR_UNTERMINATED_GROUP_NAME;
			return NULL;
		}
		*rxu8 = u8+1;
		s->backtrack.name = str_dup((const char*)n, u8-n);
		dbg_info("gift backtrack name");
		mem_gift(s->backtrack.name, *state);
	}
	else{
		int e;
		s->backtrack.id = utf8_toul(*rxu8, rxu8, 10, &e);
		if( e ){
			*err = strerror(errno);
			return NULL;
		}
	}

	return s;
}
__private int range_cmp(const void* a, const void* b){ return ((range_s*)a)->start - ((range_s*)b)->start; }

__private state_s* state_sequences(state_s** state, const utf8_t** rxu8, const char** err){
	dbg_info("sequences");
	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_SEQUENCES;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->sequences.reverse = 0;
	s->sequences.range = VECTOR(range_s, 2);

	const utf8_t* u8 = *rxu8;
	++u8;

	if( !*u8 ){
		mem_gift(s->sequences.range, *state);
		*err = REGEX_ERR_UNTERMINATED_SEQUENCES;
		return NULL;
	}
	
	if( *u8 == '^' ){
		s->sequences.reverse = 1;
		++u8;
	}

	while( *u8 && *u8 != ']' ){
		ucs4_t st;
		ucs4_t en;
		rxch_s ch = parse_get_ch(u8, err);
		if( ch.nb == UINT32_MAX ){
			mem_gift(s->sequences.range, *state);
			*rxu8 = u8;
			return NULL;
		}
		
		en = st = ch.code;
		u8 += ch.nb;

		if( u8[0] == '-' && u8[1] != ']' ){
			++u8;
			rxch_s ch = parse_get_ch(u8, err);
			if( ch.nb == UINT32_MAX ){
				mem_gift(s->sequences.range, *state);
				*rxu8 = u8;
				return NULL;
			}
			en = ch.code;
			u8 += ch.nb;
		}

		range_s* r = vector_push(&s->sequences.range, NULL);
		r->start = st;
		r->end   = en;
	}

	if( *u8 != ']' ){
		mem_gift(s->sequences.range, *state);
		*err = REGEX_ERR_UNTERMINATED_SEQUENCES;
		return NULL;
	}
	
	if( !vector_count(&s->sequences.range) ){
		mem_gift(s->sequences.range, *state);
		*err = REGEX_ERR_ASPECTED_SEQUENCES;
		return NULL;
	}

	//merge sequences
	vector_qsort(&s->sequences.range, range_cmp);
	unsigned i = 0;
	while( i < vector_count(&s->sequences.range) - 1 ){
		range_s* r[2] = { &s->sequences.range[i], &s->sequences.range[i+1] };
		if( r[1]->start <= r[0]->end + 1 ){
			if( r[0]->end < r[1]->end ) r[0]->end = r[1]->end;
			vector_remove(&s->sequences.range, i+1, 1);
			continue;
		}
		++i;
	}

	mem_gift(s->sequences.range, *state);
	*rxu8 = u8+1;
	return s;
}

__private int state_group_flags(const utf8_t** rxu8, char** name, unsigned* flags, const char** err){
	dbg_info("group flags");
	const utf8_t* u8= *rxu8;
	unsigned r      = 0;
	const utf8_t* n = NULL;
	unsigned l      = 0;
	unsigned c      = 1;
	
	*flags = GROUP_FLAG_CAPTURE;
	*name  = NULL;

	while( *u8 ){
		switch( *u8 ){
			case '|': r = 1; break;
			case '!': c = 0; break;

			case '<':
				n = ++u8;
				while( *u8 && *u8 != '>' && *u8 !=':' ) ++u8;
				if( *u8 == ':' ){
					*rxu8 = n;
					*err = REGEX_ERR_UNTERMINATED_GROUP_NAME;
					return -1;
				}
				l = u8 - n;
				if( *u8 != '>' ) return 0;
				++u8;
			break;

			case ':':
				if( l ) *name = str_dup((char*)n, l);
				if( r ) *flags |= GROUP_FLAG_COUNT_RESET;
				if( !c ) *flags &= ~GROUP_FLAG_CAPTURE;
				*rxu8 = u8+1;
			return 0;

			default : return 0;
		}
	}

	return 0;
}

__private state_s* state_group(state_s** state, const utf8_t** rxu8, const char** err){
	dbg_info("group");
	state_s* s = vector_push(state, NULL);
	s->build = 0;
	s->type = RX_GROUP;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	s->group.idgroup = -1;
	s->group.id = 0;
	s->group.flags = 0;
	s->group.name = NULL;

	++(*rxu8);

	if( state_group_flags(rxu8, &s->group.name, &s->group.flags, err) ) return NULL;
	if( s->group.name ){
		dbg_info("gift group name");
		mem_gift(s->group.name, *state);
	}

	return s;
}

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















































__private long bt_check(dict_t* cap, unsigned id, const char* name, const utf8_t* u8, unsigned index, unsigned end){
	generic_s* gcap = name ? dict(cap,	name) : dict(cap, id);
	if( gcap->type != G_SUB ) return -1;
	if( gcap->sub.size > end - index ) return -1;
	u8 += index;
	if( memcmp(u8, gcap->sub.start, gcap->sub.size) ) return -1;
	return index + gcap->sub.size;
}

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

__private _fn make_fn_string_cmp(regex_t* rx, const utf8_t* str, unsigned len){
	jit_s* self = &rx->jit;
	_begin(self);
	
	_function(self, rx->proto.state, 4);
		_value aEnd   = _pop(self);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		//unused _value aDict
		_pop(self);

		_value cstr = _constant_ptr(self, rx->type.utf8, str);
		_value clen = _constant_inum(self, jit_type_uint, len);
		_value c0   = _constant_inum(self, jit_type_int, 0);
		_value ce   = _constant_inum(self, jit_type_long, -1);

		_sub(self, aEnd, aIndex);
		_if(self, _pop(self), _lt, clen);
			_return(self, ce);
		_endif(self);

		//u8 += index
		_add(self, aU8, aIndex);
	
		//memcmp(u8, str, len)
		_call_native(self, rx->proto.memcmp, memcmp, (jit_value_t[]){_pop(self), cstr, clen}, 3);
	
		//return memcmp == 0 ? aIndex+len : -1
		_if(self, _pop(self), _eq, c0);
			_add(self, aIndex, clen);
			_return(self, _pop(self));
		_else(self);
			_return(self, ce);

	_build(self);
	_end(self);

	return self->fn;
}

__private _fn make_fn_state_single_cmp(regex_t* rx, ucs4_t val){
	jit_s* self = &rx->jit;
	_begin(self);
	
	_function(self, rx->proto.state, 4);
		//unused _value aEnd   = 
		_pop(self);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		//unused _value aDict  = 
		_pop(self);

		_value ce = _constant_inum(self, jit_type_long, -1);

		//u8 += index
		_add(self, aU8, aIndex);
		_value u8 = _pop(self);

		// ch = utf8_to_ucs4(u8)
		_deref(self, jit_type_uint, u8, 0);
		_call_native(self, rx->proto.utf8ucs4, utf8_to_ucs4, (_value[]){_pop(self)}, 1);
		_value ch = _pop(self);

		_value only = _constant_inum(self, jit_type_uint, val);
		_if(self, ch, _eq, only);
			//index += utf8_codebpoint_nb(*u8)
			_call_native(self, rx->proto.utf8nb, utf8_codepoint_nb, (_value[]){u8}, 1);
			_add(self, aIndex, _pop(self));
			_return(self, _pop(self));
		_else(self);
			_return(self, ce);
	
	_build(self);
	_end(self);

	return self->fn;
}

__private _fn make_fn_state_sequences_cmp(regex_t* rx, sequences_s* seq){
	jit_s* self = &rx->jit;
	_begin(self);
	
	_function(self, rx->proto.state, 4);
		//unused _value aEnd   = 
		_pop(self);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		//unused _value aDict  = 
		_pop(self);

		_value ce = _constant_inum(self, jit_type_long, -1);

		//u8 += index
		_add(self, aU8, aIndex);
		_value u8 = _pop(self);

		// ch = utf8_to_ucs4(u8)
		_deref(self, jit_type_uint, u8, 0);
		_call_native(self, rx->proto.utf8ucs4, utf8_to_ucs4, (_value[]){_pop(self)}, 1);
		_value ch = _pop(self);

		foreach_vector(seq->range, i){
			_value st = _constant_inum(self, jit_type_uint, seq->range[i].start);
			_value en = _constant_inum(self, jit_type_uint, seq->range[i].end);
			if( seq->reverse ) _if_not_and(self, ch, _ge, st, ch, _le, en);
			else _if_and(self, ch, _ge, st, ch, _le, en);
				//index += utf8_codebpoint_nb(*u8)
				_call_native(self, rx->proto.utf8nb, utf8_codepoint_nb, (_value[]){u8}, 1);
				_add(self, aIndex, _pop(self));
				_return(self, _pop(self));
			_else(self);
		}
		_return(self, ce);
	
	_build(self);
	_end(self);

	return self->fn;
}

__private _fn make_fn_state_backtrack_cmp(regex_t* rx, backtrack_s* bt){
	jit_s* self = &rx->jit;
	_begin(self);
	
	_function(self, rx->proto.state, 4);
		_value aEnd   = _pop(self);
		_value aIndex = _pop(self);
		_value aU8    = _pop(self);
		_value aDict  = _pop(self);

		_value ce = _constant_inum(self, jit_type_long, -1);
		_value bi = _constant_inum(self, jit_type_uint, bt->id);
		_value bn = _constant_ptr(self, rx->type.str, bt->name);

		//bt_check(dict, id, name, u8, index, end)
		_call_native(self, rx->proto.btcheck, bt_check, (_value[]){aDict, bi, bn, aU8, aIndex, aEnd}, 6);
		_value ret = _pop(self);
		
		_if(self, ret, _eq, ce);
			_return(self, ce);
		_else(self);
			_push(self, ret);
			_store(self, aIndex);
			_return(self, aIndex);
	
	_build(self);
	_end(self);

	return self->fn;
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

//todo move state inside regex, cleanup call free state and free jit ctx
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
/*
	rjit_s jit;
	jt_ctor(&jit);

	const utf8_t* literal = U8("hello");
	jit_function_t mc = jt_make_fn_string_cmp(&jit, literal, utf8_bytes_count(literal));

	const utf8_t* txt = U8("hello world hello");
	unsigned long index = 3;
	jit_long ret = 0;
	_call_jit(&jit, &ret, mc, (void*[]){&txt, &index} );
	printf("fn return:%ld\n", ret);

	jt_dtor(&jit);
	die("OK");
	return NULL;
*/
}

const char* regex_error(regex_t* rex){
	return rex->err;
}

void regex_error_show(regex_t* rex){
	if( !rex->err ) return;
	fprintf(stderr, "error: %s on building regex, at\n", rex->err);
	err_showline((const char*)rex->rx, (const char*)rex->last, 0);
}




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
