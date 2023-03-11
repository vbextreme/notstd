#include "notstd/generics.h"
#include "notstd/utf8.h"
#include <notstd/regex.h>
#include <notstd/vector.h>
#include <notstd/str.h>

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

	/*
	for( unsigned i = 0; i < bs->ucount; ++i ){
		dbg_info("[%u/%u] %u-%u", i, bs->ucount, bs->unico[i].start, bs->unico[i].end);
	}
	die("");
	*/
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


