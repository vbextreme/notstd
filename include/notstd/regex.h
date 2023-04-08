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
#define REGEX_ERR_UNOPENED_BACKREF_NAME   "unopened back references name"
#define REGEX_ERR_UNTERMINATED_SEQUENCES  "untermited sequences"
#define REGEX_ERR_UNTERMINATED_GROUP      "untermited group"
#define REGEX_ERR_UNTERMINATED_QUANTIFIER "unterminated quantifiers"
#define REGEX_ERR_UNTERMINATED_REGEX      "unterminated regex"
#define REGEX_ERR_INVALID_QUANTIFIER      "invalid quantifier"
#define REGEX_ERR_ASPECTED_SEQUENCES      "aspected sequences"
#define REGEX_ERR_ASPECTED_STRING         "aspected string"
#define REGEX_ERR_ASPECTED_REGEX          "aspected regex"
#define REGEX_ERR_UNTERMINATED_GROUP_NAME "unterminated group name"
#define REGEX_ERR_NUMERICAL_OUT_OF_RANGE  strerror(ERANGE)
#define REGEX_ERR_CANT_START_WITH_BACKREF "regex can't start with back references"
#define REGEX_ERR_UNSUPPORTED_FLAG        "unsupported flag"

typedef struct regex regex_t;

regex_t* regexu(const utf8_t* regstr);
regex_t* regex(const char* regstr);
const char* regex_error(regex_t* rex);
void regex_error_show(regex_t* rex);
const utf8_t* regex_get(regex_t* rx);

dict_t* match_at(regex_t* rx, const utf8_t** str);
dict_t* matchu(regex_t* rx, const utf8_t** str);
dict_t* match(regex_t* rx, const char** str);

#endif
