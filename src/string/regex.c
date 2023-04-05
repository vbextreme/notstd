#include <notstd/utf8.h>
#include <notstd/regex.h>
#include <notstd/vector.h>
#include <notstd/str.h>

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

#define SEQ_ASCII_LUT      4
#define SEQ_ASCII_MAX      128
#define SEQ_ASCII_INDEX(U) ((U)>>5)
#define SEQ_ASCII_BIT(U)   (1<<((U)&31))

typedef struct sequences{
	uint32_t ascii[SEQ_ASCII_LUT];
	range_s* range;
	unsigned reverse;
	unsigned asciiset;
}sequences_s;

typedef struct backref{
	unsigned id;
	utf8_t* name;
}backref_s;

typedef struct string{
	utf8_t* str;
	size_t len;
}string_s;

#define GROUP_FLAG_CAPTURE     0x01
#define GROUP_FLAG_COUNT_RESET 0x02

typedef struct group{
	unsigned  flags;
	unsigned  id;
	utf8_t*   name;
	state_s** state;
}group_s;

typedef const utf8_t*(*state_f)(state_s*, dict_t*, const utf8_t*);

struct state{
	type_e          type;
	quantifier_s    quantifier;
	state_f         fn;
	state_s*        lazy;
	union {
		string_s    str;
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

struct regex{
	state_s       state;
	utf8_t*       ff;
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
				dbg_info("escaping unicode");
				++u8;
				if( *u8 == '+' ) ++u8;
				int e;
				const utf8_t* next;
				ch.code = utf8_toul(u8, &next, 16, &e);
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
	s->fn   = NULL;
	s->lazy = NULL;
	s->quantifier.n = 1;
	s->quantifier.m = 1;
	s->quantifier.greedy = 1;
	return s;
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

__private const utf8_t* string_cmp_fn(state_s* self, __unused dict_t* cap, const utf8_t* txt){
	return utf8_ncmp(self->str.str, txt, self->str.len) ? NULL : txt+self->str.len;
}

__private state_s* state_string(regex_t* rx, state_s* s, const utf8_t** rxu8, const char** err){
	const utf8_t* u8 = *rxu8;

	unsigned size = 9;
	utf8_t* str = MANY(utf8_t, size);
	unsigned len  = 0;

	s->type = RX_STRING;
	s->fn   = string_cmp_fn;

	while( *u8 && *u8 != '(' && *u8 != ')' && *u8 != '[' && *u8 != ']' && *u8 != '|' ){
		if( u8[0] == '\\' && (u8[1] == 'g' || (u8[1] >= '0' && u8[1] <= '9')) ) break;

		rxch_s ch = parse_get_ch(u8, err);
		if( ch.nb == UINT32_MAX ){
			mem_free(str);
			*rxu8 = u8;
			return NULL;
		}
		u8 += ch.nb;
	
		if( *u8 == '?' || *u8 == '+' || *u8 == '*' || *u8 == '{' ) break;

		if( len >= size - 5 ){
			size *= 2;
			str = RESIZE(utf8_t, str, size);
		}
		len += ucs4_to_utf8(ch.code, &str[len]);
	}

	str[len] = 0;
	str = RESIZE(utf8_t, str, len+1);
	
	s->str.str = str;
	s->str.len = len;
	mem_gift(str, rx);
	*rxu8 = u8;

	dbg_info("state string '%s'", s->str.str);
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

__private const utf8_t* single_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	unsigned match = 0;
	do{
		if( self->lazy && match >= self->quantifier.n && self->lazy->fn(self->lazy, cap, txt)) return txt;
		ucs4_t u = utf8_to_ucs4(txt);
		if( u != self->u ) break;
		++match;
		txt += utf8_codepoint_nb(*txt);
	}while( match <= self->quantifier.m );
	return match < self->quantifier.n ? NULL : txt;
}

__private state_s* state_single(__unused regex_t* rx, state_s* s, const utf8_t** rxu8, rxch_s ch){
	s->type = RX_SINGLE;
	s->fn   = single_cmp_fn;
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

__private const utf8_t* seq_ascii_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	unsigned match = 0;
	do{
		if( self->lazy && match >= self->quantifier.n &&  self->lazy->fn(self->lazy, cap, txt)) return txt;
		if( *txt >= SEQ_ASCII_MAX ) break;
		if( !(self->seq.ascii[SEQ_ASCII_INDEX(*txt)] & SEQ_ASCII_BIT(*txt)) ) break;
		++match;
		++txt;
	}while( match < self->quantifier.m );
	return match < self->quantifier.n ? NULL : txt;
}

__private const utf8_t* seq_range_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	unsigned match = 0;
	const unsigned count = vector_count(&self->seq.range);
	do{
		if( self->lazy && match >= self->quantifier.n &&  self->lazy->fn(self->lazy, cap, txt)) return txt;
		ucs4_t u = utf8_to_ucs4(txt);
		if( !u ) break;
		unsigned in = 0;
		for( unsigned i = 0; i < count; ++i ){
			if( u >= self->seq.range[i].start && u <= self->seq.range[i].end ){
				++in;
				if( !self->seq.reverse ) break;
			}
		}
		if( !in ) break;
		++match;
		txt += utf8_codepoint_nb(*txt);
	}while( match < self->quantifier.m );
	return match < self->quantifier.n ? NULL : txt;
}

__private const utf8_t* seq_ascii_range_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	unsigned match = 0;
	const unsigned count = vector_count(&self->seq.range);
	do{
		if( self->lazy && match >= self->quantifier.n &&  self->lazy->fn(self->lazy, cap, txt)) return txt;
		ucs4_t u = utf8_to_ucs4(txt);
		if( u < SEQ_ASCII_MAX ){
			if( !(self->seq.ascii[SEQ_ASCII_INDEX(*txt)] & SEQ_ASCII_BIT(*txt)) ) break;
		}
		else{
			unsigned in = 0;
			for( unsigned i = 0; i < count; ++i ){
				if( u >= self->seq.range[i].start && u <= self->seq.range[i].end ){
					++in;
					if( !self->seq.reverse ) break;
				}
			}
			if( !in ) break;
		}
		++match;
		txt += utf8_codepoint_nb(*txt);
	}while( match <= self->quantifier.m );
	return match < self->quantifier.n ? NULL : txt;
}

__private void seq_ctor(sequences_s* seq){
	memset(seq->ascii, 0, sizeof(seq->ascii));
	seq->range = VECTOR(range_s, 2);
	seq->asciiset = 0;
	seq->reverse  = 0;
}

__private void seq_add(sequences_s* seq, ucs4_t st, ucs4_t en){
	dbg_info("seq add %u-%u", st, en);
	if( st < SEQ_ASCII_MAX ){
		if( en >= SEQ_ASCII_MAX ){
			range_s* r = vector_push(&seq->range, NULL);
			r->start = SEQ_ASCII_MAX;
			r->end   = en;
			en = SEQ_ASCII_MAX-1;
		}
		for( ucs4_t a = st; a <= en; ++a ){
			unsigned i = SEQ_ASCII_INDEX(a);
			//dbg_info("[%u]|%u", i, SEQ_ASCII_BIT(a));
			seq->ascii[i] |= SEQ_ASCII_BIT(a);
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
		for( unsigned i = 0; i < SEQ_ASCII_LUT; ++i ){
			seq->ascii[i] = ~seq->ascii[i];
		}
	}

	if( !disableline ){
		unsigned i = SEQ_ASCII_INDEX('\n');
		seq->ascii[i] &= ~SEQ_ASCII_BIT('\n');
	}

	if( !vector_count(&seq->range) ) return;
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
	
	s->fn = seq_ascii_range_cmp_fn;
	mem_gift(s->seq.range, rx);
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
	
	if( !vector_count(&s->seq.range) && !s->seq.asciiset ){
		mem_free(s->seq.range);
		*err = REGEX_ERR_ASPECTED_SEQUENCES;
		return NULL;
	}

	seq_merge(&s->seq, rx->flags & REGEX_FLAG_DISALBLE_LINE);
	if( s->seq.asciiset ){
		s->fn = vector_count(&s->seq.range) ? seq_ascii_range_cmp_fn : seq_ascii_cmp_fn;
	}
	else{
		s->fn = seq_range_cmp_fn;
	}
	
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

__private const utf8_t* backref_index_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	generic_s* g = dict(cap, self->backref.id);
	if( g->type == G_UNSET ) return NULL;
	unsigned match = 0;
	do{
		if( self->lazy && match >= self->quantifier.n && self->lazy->fn(self->lazy, cap, txt)) return txt;
		if( strncmp(g->sub.start, (const char*)(txt), g->sub.size) ) return NULL;
		++match;
		txt += g->sub.size;
	}while( match <= self->quantifier.m );
	return match < self->quantifier.n ? NULL : txt;
}

__private const utf8_t* backref_named_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	generic_s* g = dict(cap, (const char*)self->backref.name);
	if( g->type == G_UNSET ) return NULL;
	unsigned match = 0;
	do{
		if( self->lazy && match >= self->quantifier.n &&  self->lazy->fn(self->lazy, cap, txt)) return txt;
		if( strncmp(g->sub.start, (const char*)(txt), g->sub.size) ) return NULL;
		++match;
		txt += g->sub.size;
	}while( match < self->quantifier.m );
	return match < self->quantifier.n ? NULL : txt;
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
		s->fn = backref_named_cmp_fn;
	}
	else{
		int e;
		s->backref.id = utf8_toul(*rxu8, rxu8, 10, &e);
		if( e ){
			*err = strerror(errno);
			return NULL;
		}
		s->fn = backref_index_cmp_fn;
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

__private const utf8_t* group_state_walk(state_s* s, const unsigned count, dict_t* cap, const utf8_t* txt){
	for( unsigned i = 0; i < count && txt; ++i ){
		txt = s[i].fn(&s[i], cap, txt);
	}
	return txt;
}

__private const utf8_t* group_state_or(state_s** s, const unsigned count, dict_t* cap, const utf8_t* txt){
	const utf8_t* begin = txt;
	for( unsigned or = 0; or < count; ++or ){
		txt = group_state_walk(s[or], vector_count(&s[or]), cap, begin);
		if( txt ) return txt;
	}
	return NULL;
}

__private const utf8_t* group_cmp_fn(state_s* self, dict_t* cap, const utf8_t* txt){
	unsigned match = 0;
	const unsigned orcount = vector_count(&self->group.state);
	const utf8_t* stmatch = NULL;
	const utf8_t* enmatch = txt;
	do{
		if( self->lazy && match >= self->quantifier.n &&  self->lazy->fn(self->lazy, cap, txt)) return txt;
		txt = group_state_or(self->group.state, orcount, cap, txt);
		if( !txt ) break;
		stmatch = enmatch;
		enmatch = txt;
		++match;
	}while( match <= self->quantifier.m );
	if( match < self->quantifier.n ) return NULL;
	if( stmatch ){
		if( self->group.flags & GROUP_FLAG_CAPTURE ){
			generic_s* gc = self->group.name ? dict(cap, (const char*)self->group.name) : dict(cap, self->group.id);
			gc->type = G_SUB;
			gc->sub.start = stmatch;
			gc->sub.size  = enmatch - stmatch;
		}
		return enmatch;
	}
	return NULL;
}

__private int state_group_flags(const utf8_t** rxu8, utf8_t** name, unsigned* flags, const char** err){
	if( **rxu8 != '?' ) return 0;
	++(*rxu8);

	dbg_info("group flags");

	//parsing flags;
	const utf8_t* n = NULL;

	while( **rxu8 ){
		dbg_info("check flags:%c", **rxu8);
		switch( **rxu8 ){
			case '|': *flags |= GROUP_FLAG_COUNT_RESET; break;
			case ':': *flags &= ~GROUP_FLAG_CAPTURE; break;
			case '\'': case '<':
				++(*rxu8);
				n = *rxu8;
				while( **rxu8 && **rxu8 != '>' && **rxu8 !='\'' ) ++(*rxu8);
				if( !**rxu8 ){
					*rxu8 = n-1;
					*err = REGEX_ERR_UNTERMINATED_GROUP_NAME;
					return -1;
				}
				*name = utf8_dup(n, *rxu8-n);
				dbg_info("flags name: %s", *name);
			break;

			default: return 0;
		}
		++(*rxu8);
	}
	
	*err = REGEX_ERR_UNTERMINATED_GROUP;
	return -1;
}

__private state_s* state_group(regex_t* rx, state_s* s, unsigned* grpcount, const utf8_t** rxu8, const char** err){
	s->type = RX_GROUP;
	unsigned flags;
	unsigned gc = *grpcount;
	unsigned gs = gc;
	const utf8_t* stgr = *rxu8;

	s->group.id   = gc;
	s->group.name = NULL;
	s->group.flags= GROUP_FLAG_CAPTURE;
	s->fn         = group_cmp_fn;

	if( state_group_flags(rxu8, &s->group.name, &flags, err) ) return NULL;
	if( s->group.name ) mem_gift(s->group.name, rx);
	const utf8_t* u8 = *rxu8;

	state_s** state = VECTOR(state_s*, 4);
	unsigned or = 0;
	state_s* tmp = VECTOR(state_s, 4);
	vector_push(&state, &tmp);
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
				++u8;
				if( !state_group(rx, current, &gc, &u8, err) ) goto ONERR;
				if( parse_quantifier(&current->quantifier, &u8, err) ) goto ONERR;
			break;

			case '|':
				mem_gift(state[or], rx);
				++or;
				tmp = VECTOR(state_s, 4);
				vector_push(&state, &tmp);
				if( s->group.flags & GROUP_FLAG_COUNT_RESET ){
					if( gc > *grpcount ) *grpcount = gc;
					gc = gs;
				}
				++u8;
			break;

			case ')': 
				if( gs == 0 ){
					*err = REGEX_ERR_UNOPENED_GROUP;
					goto ONERR;
				}
				if( !(s->group.flags & GROUP_FLAG_COUNT_RESET) ) *grpcount = gc;
				s->group.state = state;
				*rxu8 = u8+1;
				mem_gift(state[or], rx);
				mem_gift(state, rx);
			return s;

			case ']': *err = REGEX_ERR_UNOPENED_SEQUENCES; goto ONERR;
			case '}': *err = REGEX_ERR_UNOPENED_QUANTIFIERS; goto ONERR;
	
			default :
				current = state_ctor(vector_push(&state[or], NULL));
				if( u8[0] == '\\' && ((u8[1] >= '0' && u8[1] <= '9') || (u8[1] == 'g')) ){
					dbg_info("escape is a back references");
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
		u8 = stgr-1;
		goto ONERR;
	}
	s->group.state = state;
	*rxu8 = u8;
	mem_gift(state[or], rx);
	mem_gift(state, rx);
	return s;

ONERR:
	*rxu8 = u8;
	DELETE(state);
	return NULL;
}

/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/
/****************** REGEX ******************/
/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/
/*******************************************/

__private void dump_ch(ucs4_t ch){
	switch( ch ){
		case '\t': printf("\\t"); break;
		case '\n': printf("\\n"); break;
		case  0 ... 8: 
		case 11 ... 31:
		case 127 ... UINT32_MAX:
			printf("\\u%u", ch); 
		break;
		default: printf("%c", ch); break;
	}
}

__private int check_ascii(sequences_s* seq, ucs4_t ch){
	unsigned i = SEQ_ASCII_INDEX(ch);
	//dbg_info("(%u)[%u]&%u=%u", ch, i, SEQ_ASCII_BIT(ch), seq->ascii[i] & SEQ_ASCII_BIT(ch));
	return seq->ascii[i] & SEQ_ASCII_BIT(ch) ? 1 : 0;
}

__private void dump_seq(sequences_s* seq){
	printf("[");
	if( seq->asciiset ){
		unsigned a = 0;
		while( a < 128 ){
			while( a < 128 && !check_ascii(seq, a) ) ++a;
			if( a >= 128 ) break;
			unsigned st = a++;
			while( a < 128 && check_ascii(seq, a) ) ++a;
			unsigned en = a-1;
			dbg_info("range %u-%u", st, en);
			dump_ch(st);
			if( st != en ){
				putchar('-');
				dump_ch(en);
			}
		}
	}

	iassert(seq->range);
	foreach_vector(seq->range, i){
		dump_ch(seq->range[i].start);
		if( seq->range[i].end != seq->range[i].start ){
			putchar('-');
			dump_ch(seq->range[i].end);
		}
	}
	printf("]");
}

__private void dump_backref(backref_s* br){
	if( br->name ){
		printf("\\g{%s}", br->name);
	}
	else{
		printf("\\%u", br->id);
	}
}

__private void state_dump(state_s* state, unsigned count){
	iassert(count);
	for( unsigned i = 0 ; i < count; ++i ){
		switch( state[i].type ){
			case RX_SINGLE   : dump_ch(state[i].u); break;
			case RX_STRING   : printf("%s", state[i].str.str); break;
			case RX_BACKREF  : dump_backref(&state[i].backref); break;
			case RX_SEQUENCES: dump_seq(&state[i].seq); break;
			case RX_GROUP    :
				printf("(");
				unsigned nor = vector_count(&state[i].group.state);
				foreach_vector(state[i].group.state, ior){
					state_dump(state[i].group.state[ior], vector_count(&state[i].group.state[ior]));
					if( ior < nor-1 ) printf("|");
				}
				printf(")");
			break;
		}
		printf("{%u,%u}", state[i].quantifier.n, state[i].quantifier.m);
		if( !state[i].quantifier.greedy ) putchar('?');
	}
}

__private void dump_state(regex_t* rx){
	state_dump(&rx->state, 1);
	putchar('\n');
	printf("fast find: '");
	char* s = (char*)rx->ff;
	while( *s ){
		dump_ch(*s);
		++s;
	}
	printf("'\n");
}

__private int parse_state(regex_t* rx, const utf8_t* regstr){
	rx->err  = NULL;
	rx->rx   = regstr;
	rx->last = regstr;
	unsigned gc = 0;
	state_ctor(&rx->state);
	return state_group(rx, &rx->state, &gc, &rx->last, &rx->err) ? 0 : -1;
}

__private void set_lazy(state_s* s){
	const unsigned orcount = vector_count(&s->group.state);
	for( unsigned or = 0; or < orcount; ++or ){
		const unsigned count = vector_count(&s->group.state[or]);
		s->group.state[or][count-1].quantifier.greedy = 1;
		for( unsigned i = 0; i < count-1; ++i ){
			if( !s->group.state[or][i].quantifier.greedy ){
				s->group.state[or][i].lazy = &s->group.state[or][i];
			}
			if( s->group.state[or][i].type == RX_GROUP ) set_lazy(&s->group.state[or][i]);
		}	
	}
}

__private void vff_add(utf8_t** vff, utf8_t u){
	foreach_vector(*vff, i){
		if( (*vff)[i] == u ) return;
	}
	vector_push(vff, &u);
}

__private const char* parse_fast_find(utf8_t** vff, state_s* s){
	switch( s->type ){
		case RX_BACKREF: return REGEX_ERR_CANT_START_WITH_BACKREF;

		case RX_SINGLE:{
			utf8_t u[5];
			ucs4_to_utf8(s->u, u);
			vff_add(vff, u[0]);
			break;
		}

		case RX_STRING:
			vff_add(vff, s->str.str[0]);
		break;
					   
		case RX_SEQUENCES: 
			if( s->seq.asciiset ){
				for( unsigned i = 1; i < SEQ_ASCII_MAX; ++i ){
					if( s->seq.ascii[SEQ_ASCII_INDEX(i)] & SEQ_ASCII_BIT(i) ){
						vff_add(vff, i);
					}
				}
			}
			foreach_vector(s->seq.range, i){
				for( unsigned j = s->seq.range[i].start; j <= s->seq.range[i].end; ++j ){
					utf8_t u[5];
					ucs4_to_utf8(s->u, u);
					vff_add(vff, j);
				}
			}
		break;
		
		case RX_GROUP:
			foreach_vector(s->group.state, ior){
				const char* res = parse_fast_find(vff, &s->group.state[ior][0]);
				if( res ) return res;
			}
		break;
	}

	return NULL;
}

regex_t* regex(const utf8_t* regstr, unsigned flags){
	regex_t* rx = NEW(regex_t);
	rx->flags = flags;
	if( parse_state(rx, regstr) ) return rx;
	set_lazy(&rx->state);
	rx->ff  = VECTOR(utf8_t, 8);
	rx->err = parse_fast_find(&rx->ff, &rx->state);
	vff_add(&rx->ff, 0);
	mem_gift(rx->ff, rx);
	dump_state(rx);
	return rx;
}

const char* regex_error(regex_t* rx){
	return rx->err;
}

void regex_error_show(regex_t* rx){
	if( !rx->err ) return;
	fprintf(stderr, "error: %s on building regex, at\n", rx->err);
	err_showline((const char*)rx->rx, (const char*)rx->last, 0);
}

dict_t* match_at(regex_t* rx, const utf8_t* str, const utf8_t** next){
	if( rx->err ) return NULL;
	dict_t* cap = dict_new();
	const utf8_t* res = rx->state.fn(&rx->state, cap, str);
	if( next ) *next = res;
	return cap;
}

dict_t* match(regex_t* rx, const utf8_t* str, const utf8_t** next){
	if( rx->err ) return NULL;

	const utf8_t* f = str;
	dict_t* cap = dict_new();
	const utf8_t* res;

	while( *(f = utf8_anyof(f, rx->ff)) && !(res=rx->state.fn(&rx->state, cap, f)) );
	
	if( next ) *next = res;	
	return cap;
}







