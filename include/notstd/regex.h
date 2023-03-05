#ifndef __NOTSTD_CORE_REGEX_H__
#define __NOTSTD_CORE_REGEX_H__

#include <notstd/core.h>
#include <notstd/utf8.h>
#include <notstd/dict.h>

#define FSM_STATE_FLAG_NGREEDY 0x01
#define FSM_STATE_FLAG_REVERSE 0x02
#define FSM_STATE_FLAG_CAPTURE 0x04

#define FSM_CAPTURE_NAME_MAX   27

#define FSM_STRING_MAX         31

#define FSM_FLAG_START 0x01
#define FSM_FLAG_END   0x02

typedef enum {
	FSM_STATE_ASCII,
	FSM_STATE_UNICODE,
	FSM_STATE_STRING,
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

typedef struct fsmCapture{
	const utf8_t* start;
	unsigned len;
}fsmCapture_s;

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
	};
}fsmState_s;

typedef struct fsm{
	const char* err;
	dict_t*               cap;
	fsmState_s*           state;
	unsigned flags;
}fsm_s;

int fsm_exec(fsm_s* fsm, const utf8_t* txt, unsigned len);
fsm_s* fsm_ctor(fsm_s* fsm);
const utf8_t* fsm_build(fsm_s* fsm, const utf8_t* regex);









#endif
