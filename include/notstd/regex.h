#ifndef __NOTSTD_CORE_REGEX_H__
#define __NOTSTD_CORE_REGEX_H__

#include <notstd/core.h>
#include <notstd/utf8.h>
#include <notstd/dict.h>

#define REGEX_ERR_INTERNAL                "internal error"
#define REGEX_ERR_INVALID_UTF_ESCAPE      "invalid utf escape"
#define REGEX_ERR_UNOPENED_SEQUENCES      "unopened sequences"
#define REGEX_ERR_UNOPENED_GROUP          "unopened group"
#define REGEX_ERR_UNOPENED_QUANTIFIERS    "unopened quantifiers"
#define REGEX_ERR_UNOPENED_BACKREF_NAME   "unopened backreferences name"
#define REGEX_ERR_UNTERMINATED_SEQUENCES  "untermited sequences"
#define REGEX_ERR_UNTERMINATED_GROUP      "untermited group"
#define REGEX_ERR_UNTERMINATED_QUANTIFIER "unterminated quantifiers"
#define REGEX_ERR_INVALID_QUANTIFIER      "invalid quantifier"
#define REGEX_ERR_ASPECTED_SEQUENCES      "aspected sequences"
#define REGEX_ERR_UNTERMINATED_GROUP_NAME "unterminated group name"

#define REGEX_FLAG_DISALBLE_LINE 0x01

typedef struct regex regex_t;

regex_t* regex(const utf8_t* rx, unsigned flags);
const char* regex_error(regex_t* rex);
void regex_error_show(regex_t* rex);


/*
#define FSM_STATE_FLAG_NGREEDY 0x01
#define FSM_STATE_FLAG_REVERSE 0x02
#define FSM_STATE_FLAG_CAPTURE 0x04

#define FSM_CAPTURE_NAME_MAX   27

#define FSM_STRING_MAX         31

#define FSM_FLAG_START 0x01
#define FSM_FLAG_END   0x02

#define FSM_ERR_MISSING_SEQUENCES        "missing sequences after ["
#define FSM_ERR_INVALID_UTF8             "invalid utf8"
#define FSM_ERR_TO_MANY_SEQUENCES        "to many sequences"
#define FSM_ERR_UNTERMINATED_SEQUENCES   "untermited sequences, missing ]"
#define FSM_ERR_ASPECTED_SEQUENCES       "aspected sequences"
#define FSM_ERR_UNTERMINATED_QUANTIFIERS "unterminated quantifiers, missing }"
#define FSM_ERR_CAPTURE_NAME_TO_LONG     "capture name to long"
#define FSM_ERR_CAPTURE_NAME_NOT_ENDING  "capture not end with >"
#define FSM_ERR_CAPTURE_NUMBERS_INVALID  "capture invalid numbes"
#define FSM_ERR_GROUP_UNTERMINATED       "unterminated group"
#define FSM_ERR_GROUP_NOT_OPENED         "closed group without open"


typedef enum {
	FSM_STATE_ASCII,
	FSM_STATE_UNICODE,
	FSM_STATE_STRING,
	FSM_STATE_BACKTRACK,
	FSM_STATE_GROUP_START,
	FSM_STATE_GROUP_END,
}fsmStateType_e;

typedef struct fsmStateAscii{
	uint64_t chset[4];
}fsmStateAscii_s;

typedef struct fsmStateUnicode{
	ucs4_t start;
	ucs4_t end;
}fsmStateUnicode_s;

typedef struct fsmStateString{
	utf8_t  value[FSM_STRING_MAX];
	uint8_t len;
}fsmStateString_s;

typedef struct fsmStateGroup{
	uint32_t next;
	char     name[FSM_CAPTURE_NAME_MAX];
	uint8_t  id;
}fsmStateGroup_s;

typedef struct fsmStateDump{
	uint64_t dump[4];
}fsmStateDump_s;

typedef struct capture{
	const utf8_t* start;
	unsigned len;
}capture_s;


typedef struct fsmState{
	uint32_t next;
	uint16_t type;
	uint16_t flags;
	uint16_t qn;
	uint16_t qm;
	union{
		fsmStateAscii_s   ascii;
		fsmStateUnicode_s unicode;
		fsmStateString_s  string;
		fsmStateGroup_s   group;
		fsmStateDump_s    dump;
		fsmStateGroup_s   backtrack;
	};
}fsmState_s;

typedef struct fsm{
	const char* err;
	dict_t*     cap;
	fsmState_s* state;
	uint32_t    bcstate;
	unsigned flags;
}fsm_s;

typedef struct regex{
	fsm_s fsm;
	const utf8_t* rx;
	const utf8_t* last;
	unsigned len;
}regex_s;

int fsm_exec(fsm_s* fsm, const utf8_t* txt, unsigned len);
fsm_s* fsm_ctor(fsm_s* fsm);
fsm_s* fsm_dtor(fsm_s* fsm);
const utf8_t* fsm_build(fsm_s* fsm, const utf8_t* regex);

void fsm_dump(fsm_s* fsm, unsigned istate, unsigned sub);

#define MATCH_FULL_TEXT      0x01
#define MATCH_CONTINUE_AFTER 0x02
#define MATCH_ONE_PER_LINE   0x04

regex_s* regex_ctor(regex_s* rex, const utf8_t* rx);
regex_s* regex(const utf8_t* rx);
const char* regex_error(regex_s* rex);
const utf8_t* regex_error_at(regex_s* rex);
void regex_error_show(regex_s* rex);

dict_t* match(regex_s* rex, const utf8_t* txt, unsigned flags);
capture_s* capture(dict_t* cap, unsigned id, const char* name);

*/


#endif
