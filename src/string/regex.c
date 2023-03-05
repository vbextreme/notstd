#include "notstd/generics.h"
#include "notstd/utf8.h"
#include <notstd/regex.h>
#include <notstd/vector.h>
#include <notstd/str.h>

__private void capture(fsm_s* fsm, const char* name, unsigned id, const utf8_t* start, unsigned len){
   	generic_s* g;
	if( *name ){
		g = dict(fsm->cap, name);
	}
	else{
		g = dict(fsm->cap, id);
	}
	if( g->type == G_UNSET ){
		fsmCapture_s* cap = mem_gift(NEW(fsmCapture_s), fsm->cap);
		cap->start = NULL;
		cap->len   = 0;
		*g = GI(cap);
	}
	fsmCapture_s* cap = g->obj;
	cap->start = start;
	cap->len = len;
}

__private bool_t state_cmp_ascii(fsmState_s* state, ucs4_t u){
	if( u >= 256 ) return 0;
	const unsigned g = u & 3;
	u >>= 2;
	return state->ascii.chset[g] & (1 << u);
}

__private bool_t state_cmp_unicode(fsmState_s* state, ucs4_t u){
	if( u >= state->unicode.start && u <= state->unicode.end ){
		return state->flags & FSM_STATE_FLAG_REVERSE ? 0 : 1;
	}
	return state->flags & FSM_STATE_FLAG_REVERSE ? 1 : 0;
}

__private unsigned state_cmp_string(fsmState_s* state, const utf8_t* u){
	if( strncmp((const char*)state->string.value, (const char*)u, state->string.len) ){
		return state->string.len;
	}
	return 0;
}

__private uint32_t fsm_walk(fsm_s* fsm, const utf8_t* txt, uint32_t itxt, uint32_t endt, uint32_t istate);
__private uint32_t state_group(fsm_s* fsm, const utf8_t* txt, uint32_t itxt, uint32_t endt, uint32_t istate){
	uint32_t ptxt;
	while( istate ){
		fsmState_s* or = &fsm->state[istate];
		if( (ptxt=fsm_walk(fsm, txt, itxt, endt, or->next)) ) return ptxt;
		istate = or->group.next;
	}
	return 0;
}

__private uint32_t fsm_walk(fsm_s* fsm, const utf8_t* txt, uint32_t itxt, uint32_t endt, uint32_t istate){
	ucs4_t u;
	unsigned tmp;

	do{
		fsmState_s* state = &fsm->state[istate];
		unsigned match = 0;
		while( itxt < endt ){
			switch( state->type ){
				case FSM_STATE_ASCII:
					u = utf8_to_ucs4(&txt[itxt]);
					if( !state_cmp_ascii(state, u) ) goto ENDMATCH;
					itxt += utf8_codepoint_nb(txt[itxt]);
				break;

				case FSM_STATE_UNICODE:
					u = utf8_to_ucs4(&txt[itxt]);
					if( !state_cmp_unicode(state, u) ) goto ENDMATCH;
					itxt += utf8_codepoint_nb(txt[itxt]);
				break;

				case FSM_STATE_STRING:
					if( !(tmp=state_cmp_string(state, &txt[itxt])) ) goto ENDMATCH;
					itxt += tmp;
				break;

				case FSM_STATE_GROUP_START:
					if( (tmp=state_group(fsm, txt, itxt, endt, state->group.next)) ){
						if( state->flags & FSM_STATE_FLAG_CAPTURE ) capture(fsm, state->group.name, state->group.id, &txt[itxt], tmp - itxt);
						itxt += tmp;
					}
					else{
						goto ENDMATCH;
					}
				break;

				case FSM_STATE_GROUP_END:
					return itxt;
				break;
			}
			++match;

			if( match >= state->qn && match <= state->qm && state->flags & FSM_STATE_FLAG_NGREEDY ){
				if( (tmp=fsm_walk(fsm, txt, itxt, endt, state->next)) ){
					return itxt+tmp;
				}
			}
		}
		ENDMATCH:
		if( match < state->qn || match > state->qm ) return 0;
		istate = state->next;
	}while( istate && itxt < endt );
	
	return istate ? 0 : itxt;
}

int fsm_exec(fsm_s* fsm, const utf8_t* txt, unsigned len){
	unsigned itxt = 0;
	long ret = 0;

	fsm->cap = dict_new();

	if( fsm->flags & FSM_FLAG_START ){
		ret = fsm_walk(fsm, txt, itxt, len, 0) ? 0 : -1;
	}
	else{
		while( itxt < len && !(ret=fsm_walk(fsm, txt, itxt, len, 0)) ){
			++itxt;
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












#define BSEQ_MAX 256

typedef struct bseq{
	fsmStateUnicode_s ascii[BSEQ_MAX];
	fsmStateUnicode_s unico[BSEQ_MAX];
	unsigned acount;
	unsigned ucount;
	unsigned reverse;
}bseq_s;

__private uint32_t build_state_next(fsm_s* fsm){
	return vector_count(&fsm->state);
}

__private uint32_t build_state_current(fsm_s* fsm){
	return build_state_next(fsm)-1;
}

__private fsmState_s* build_state_new(fsm_s* fsm, fsmStateType_e type){
	fsmState_s* state = vector_push(&fsm->state, NULL);
	state->type  = type;
	state->next  = build_state_current(fsm);
	state->flags = 0;
	state->qn    = 0;
	state->qm    = 0;
	return state;
}

__private void state_ascii_uset(fsmState_s* state, ucs4_t st, ucs4_t en){
	for( ucs4_t u4 = st; u4 <= en; ++u4 ){
		unsigned b = u4 & 0x3;
		u4 >>= 2;
		state->ascii.chset[b] |= 1 << u4;
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

__private void bseq_merge(bseq_s* bs){
	qsort(bs->unico, bs->ucount, sizeof(fsmStateUnicode_s), 
		_$(int, (const void* a, const void* b), { return ((fsmStateUnicode_s*)a)->start - ((fsmStateUnicode_s*)b)->start; })
	);

	unsigned count = bs->ucount;
	unsigned i = 0;
	while( i < count-1 ){
		if( bs->unico[i+1].start <= bs->unico[i].end + 1 ){
			if( bs->unico[i].end < bs->unico[i+1].end ) bs->unico[i].end = bs->unico[i+1].end;
			if( i+2 < count ){
				memmove(&bs->unico[i+1], &bs->unico[i+2], sizeof(fsmStateUnicode_s) *(count - (i+1)));
			}
			--count;
			continue;
		}
		++i;
	}
}

__private const utf8_t* build_sequences_get(fsm_s* fsm, const utf8_t* regex, bseq_s* bs){
	bs->reverse = 0;

	if( !*regex ){
		fsm->err = "missing sequences after [";
		return regex-1;
	}
	
	if( *regex == '^' ){
		bs->reverse = 1;
		++regex;
	}

	while( *regex && *regex != ']' ){
		ucs4_t st;
		ucs4_t en;
		regex = u8seq(regex, &st);
		if( !regex ){
			fsm->err = "invalid utf8";
			return regex;
		}
		en = st;
		if( *regex == '-' && *(regex+1) != ']' ){
			regex = u8seq(regex+1, &en);
			if( !regex ){
				fsm->err = "invalid utf8";
				return regex;
			}	
		}
		if( bseq_add(bs, st, en) ){
			fsm->err = "to many sequences";
			return regex;
		}
	}

	if( *regex != ']' ){
		fsm->err = "untermited sequences, missing ]";
		return regex;
	}
	if( bs->acount + bs->ucount == 0 ){
		fsm->err = "aspected sequences";
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
				fsm->err = "unsetted quantifiers";
				return begin;
			}

			if( *regex == ',' ){
				*qn = 0;
			}
			else{
				const utf8_t* end;
				int e;
				*qn = str_toul((const char*)regex, (const char**)&end, 10, &e);
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
				fsm->err = "unterminated quantifiers, missing }";
				return regex;
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

__private fsmState_s* build_state_sequences_ascii(fsm_s* fsm, bseq_s* bs, unsigned qn, unsigned qm, unsigned greedy){
	fsmState_s* state = build_state_new(fsm, FSM_STATE_ASCII);
	state->qm = qm;
	state->qn = qn;
	if( !greedy ) state->flags |= FSM_STATE_FLAG_NGREEDY;
	for( unsigned i = 0; i < bs->acount; ++i ){
		state_ascii_uset(state, bs->ascii[i].start, bs->ascii[i].end);
	}
	if( bs->reverse ) state_ascii_reverse(state);
	return state;
}

__private fsmState_s* build_state_sequences_unicode(fsm_s* fsm, bseq_s* bs, unsigned qn, unsigned qm, unsigned greedy){
	if( !bs->acount && bs->ucount == 1 ){
		fsmState_s* unic = build_state_new(fsm, FSM_STATE_UNICODE);
		unic->unicode.start = bs->unico[0].start;
		unic->unicode.end   = bs->unico[0].end;
		if( bs->reverse ) unic->flags |= FSM_STATE_FLAG_REVERSE;
		return unic;
	}

	fsmState_s* grpS = build_state_new(fsm, FSM_STATE_GROUP_START);
	grpS->group.next = grpS->next;
	uint32_t* ornext = &grpS->group.next;

	if( bs->acount ){
		*ornext = build_state_next(fsm);
		fsmState_s* grpOS = build_state_new(fsm, FSM_STATE_GROUP_START);
		build_state_sequences_ascii(fsm, bs, qn, qm, greedy);
		fsmState_s* grpOE = build_state_new(fsm, FSM_STATE_GROUP_END);
		grpOE->next = 0;
		grpOS->group.next = 0;
		ornext = &grpOS->group.next;
	}

	for( unsigned i = 0; i < bs->ucount; ++i ){
		*ornext = build_state_next(fsm);
		fsmState_s* grpOS = build_state_new(fsm, FSM_STATE_GROUP_START);
		fsmState_s* unic = build_state_new(fsm, FSM_STATE_UNICODE);
		unic->unicode.start = bs->unico[i].start;
		unic->unicode.end   = bs->unico[i].end;
		if( bs->reverse ) unic->flags |= FSM_STATE_FLAG_REVERSE;
		fsmState_s* grpOE = build_state_new(fsm, FSM_STATE_GROUP_END);
		grpOE->next = 0;
		grpOS->group.next = 0;
		ornext = &grpOS->group.next;
	}

	fsmState_s* grpE = build_state_new(fsm, FSM_STATE_GROUP_END);
	grpS->next = grpE->next;
	return grpS;
}

__private const utf8_t* build_sequences(fsm_s* fsm, const utf8_t* regex){
	unsigned qm;
	unsigned qn;
	unsigned greedy;
	bseq_s bs;

	regex = build_sequences_get(fsm, regex, &bs);
	if( fsm->err ) return regex;

	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;

	if( bs.ucount ){
		build_state_sequences_unicode(fsm, &bs, qn, qm, greedy);
	}
	else{
		build_state_sequences_ascii(fsm, &bs, qn, qm, greedy);
	}

	return regex;
}

__private const utf8_t* build_string(fsm_s* fsm, const utf8_t* regex){
	__private const char* quant = "{?+*";
	__private const char* sep   = "[(.";
	
	fsmState_s* state = build_state_new(fsm, FSM_STATE_STRING);
	state->qm = 1;
	state->qn = 1;
	state->string.len = 0;
	unsigned prev = 0;
	utf8_t* end = &state->string.value[0];

	while( *regex ){
		if( strchr(quant, *regex) ){
			if( !prev ) goto ONERR;
			regex -= prev;
			end -= prev;
			goto ENDSTRING;
		}
	
		if( strchr(sep, *regex) ) goto ENDSTRING;
		
		if( *regex == '\\' ){
			++regex;
			switch( *regex ){
				case 'n':
					*end++ = '\n';
					++regex;
				continue;

				case 't':
					*end++ = '\t';
					++regex;
				continue;

				case 'u':
				case 'U':{
					int err;
					ucs4_t unicode = utf8_toul(regex+1, &regex, 16, &err);
					if( err ){
						fsm->err = "invalid unicode";
						return regex;
					}
					if( end - state->string.value >= FSM_STRING_MAX - (1+4) ) goto ENDSTRING;
					end += ucs4_to_utf8(unicode, end);
					continue;
				}
			}
		}

		unsigned nb = utf8_codepoint_nb(*regex);
		if( end - state->string.value >= FSM_STRING_MAX - (1+nb) ) goto ENDSTRING;

		end = utf8_chcp(end, regex, nb);
		regex += nb;
		prev = nb;
	}

	if( end == state->string.value ) goto ONERR;

ENDSTRING:
	*end = 0;
	state->string.len = end - state->string.value;
	return regex;
ONERR:
	fsm->err = "internal error, unaspected state of string";
	return regex;
}

__private const utf8_t* build_dot(fsm_s* fsm, const utf8_t* regex){
	unsigned qn, qm, greedy;
	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	if( fsm->err ) return regex;
	
	fsmState_s* state = build_state_new(fsm, FSM_STATE_UNICODE);
	state->qm = qm;
	state->qn = qn;
	if( !greedy ) state->flags |= FSM_STATE_FLAG_NGREEDY;

	return regex;
}

__private const utf8_t* build_group(fsm_s* fsm, const utf8_t* regex, unsigned* gid);

__private const utf8_t* build_or(fsm_s* fsm, const utf8_t* regex, unsigned* gid){
	while( *regex && !fsm->err ){
		switch( *regex ){
			case '(': ++(*gid); regex = build_group(fsm, regex, gid); break;
			case '[': regex = build_sequences(fsm, regex+1); break;
			case '.': regex = build_dot(fsm, regex+1); break;
			default : regex = build_string(fsm, regex); break;
			case '|': return regex;
			case ')': return regex;
		}
	}
	return regex;
}

__private const utf8_t* build_group_parse_flags(const utf8_t* regex, int* rc, const utf8_t** name, unsigned* len){
	const utf8_t* stored = regex;
	unsigned r      = 0;
	const utf8_t* n = NULL;
	unsigned l      = 0;

	while( *regex ){
		switch( *regex ){
			case '|': r = 1; break;

			case '<':
				n = regex;
				while( *regex && *regex != '>' ) ++regex;
				if( *regex != '>' ) return stored;
				l = regex - n;
				++regex;
			break;

			case ':':
				*rc   = r;
				*name = n;
				*len  = l;
			return regex+1;

			default : return stored;
		}
	}

	return stored;
}

__private const utf8_t* build_group(fsm_s* fsm, const utf8_t* regex, unsigned* gid){
	int virtual = 1;
	int resetCount = 0;
	const utf8_t* name = NULL;
	unsigned startgid = *gid;
	unsigned len = 0;
	unsigned greedy = 1;
	unsigned qn;
	unsigned qm;

	if( *regex == '(' ){
		virtual = 0;
		++regex;
		regex = build_group_parse_flags(regex, &resetCount, &name, &len);
		if( len >= FSM_CAPTURE_NAME_MAX ){
			fsm->err = "capture name to long";
			return regex;
		}
	}
	
	fsmState_s* grpS = build_state_new(fsm, FSM_STATE_GROUP_START);
	grpS->group.next = grpS->next;
	uint32_t* ornext = &grpS->group.next;
	grpS->group.id   = *gid;
	if( name && len ){
		memcpy(grpS->group.name, name, len);
		grpS->group.name[len] = 0;
	}

	while( *regex && !fsm->err ){
		*ornext = build_state_next(fsm);
		fsmState_s* grpOS = build_state_new(fsm, FSM_STATE_GROUP_START);
	
		regex = build_or(fsm, regex, gid);
		if( fsm->err ) return regex;
	
		fsmState_s* grpOE = build_state_new(fsm, FSM_STATE_GROUP_END);
		grpOE->next = 0;
		grpOS->group.next = 0;
		ornext = &grpOS->group.next;

		if( *regex == ')' )	break;
		if( *regex == '|' ){
			if( resetCount ) *gid = startgid;
			++regex;
		}
	}

	if( fsm->err ) return regex;
	
	if( *regex == ')' ){
		++regex;
	}
	else if( *regex ){
		fsm->err = "unterminated group";
		return regex;
	}
	else if( !virtual ){
		fsm->err = "closed group without open";
		return regex;
	}

	fsmState_s* grpE = build_state_new(fsm, FSM_STATE_GROUP_END);
	grpS->next = grpE->next;

	regex = quantifiers(fsm, regex, &qn, &qm, &greedy);
	grpS->qn = qn;
	grpS->qm = qm;
	if( !greedy ) grpS->flags |= FSM_STATE_FLAG_NGREEDY;
	return regex;
}

const utf8_t* fsm_build(fsm_s* fsm, const utf8_t* regex){
	unsigned gid = 0;
	return build_group(fsm, regex, &gid);
}





























