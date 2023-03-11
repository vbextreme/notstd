#include <notstd/dict.h>
#include <notstd/regex.h>
#include <notstd/map.h>

__private void testing_build(const char* rx, const char* err, int show){
	dbg_warning("try /%s/", rx);
	__free regex_s* rex = regex(U8(rx));
	if( regex_error(rex) != err ) die("fsm return '%s' but aspected '%s'", regex_error(rex), err);
	if( show ) regex_error_show(rex);
}

__private const char* big_sequences(void){
	static char bs[4096];
	unsigned n = 0;
	char* s = bs;
	s += sprintf(s, "ciao[");
	for( unsigned i = 1; i < 2048 && n < 300; i += 4, ++n ){
		s += sprintf(s, "\\u%u-\\u%u", i, i+1);
	}
	s += sprintf(s, "]");
	dbg_info("size=%lu n=%u", s-bs, n);
	return bs;
}

__private void test_build(void){
	testing_build("ciao[", FSM_ERR_MISSING_SEQUENCES, 0);
	testing_build("ciao[\\up]", FSM_ERR_INVALID_UTF8, 0);
	testing_build(big_sequences(), FSM_ERR_TO_MANY_SEQUENCES, 0);
	testing_build("ciao[a-z", FSM_ERR_UNTERMINATED_SEQUENCES, 0);
	testing_build("ciao[]", FSM_ERR_ASPECTED_SEQUENCES, 0);
	testing_build("ciao[a-z]{", FSM_ERR_UNTERMINATED_QUANTIFIERS, 0);
	testing_build("ciao[a-z]{1,2", FSM_ERR_UNTERMINATED_QUANTIFIERS, 0);
	testing_build("ciao\\up", FSM_ERR_INVALID_UTF8, 0);
	testing_build("ciao\\<a123456789b123456789c123456789>", FSM_ERR_CAPTURE_NAME_TO_LONG, 0);
	testing_build("ciao\\<abc", FSM_ERR_CAPTURE_NAME_NOT_ENDING, 0);
	testing_build("ciao\\0123456789101234566789201234567893012345678940", FSM_ERR_CAPTURE_NUMBERS_INVALID, 0);
	testing_build("ciao(<abc:)", FSM_ERR_CAPTURE_NAME_NOT_ENDING, 0);
	testing_build("ciao(<a123456789b123456789c123456789>:)", FSM_ERR_CAPTURE_NAME_TO_LONG, 0);
	testing_build("ciao(abcde", FSM_ERR_GROUP_UNTERMINATED, 0);
	testing_build("(ciao)abc)de", FSM_ERR_GROUP_NOT_OPENED, 0);
	testing_build("(oab)c)de", FSM_ERR_GROUP_NOT_OPENED, 0);
}

__private void show_capture(const utf8_t* rx, const utf8_t* txt, dict_t* cap){
	if( rx ) printf("/%s/\n", rx);
	if( txt ) puts((const char*)txt);
	if( !cap ){
		puts("unmatch\n");
		return;
	}

	dictPair_s* pair;
	foreach(dict, cap, pair, 0, dict_count(cap)){
		if( pair->value.type != G_OBJ ) continue;
		putchar('(');
		g_print(pair->key);
		putchar(')');
		capture_s* c = pair->value.obj;
		printf("%.*s\n", c->len, c->start);
	}
	puts("");
}

__private void testing_match(const char* rx, const char* txt, unsigned flags, int show){
	__free regex_s* rex = regex(U8(rx));
	if( regex_error(rex) ){
		regex_error_show(rex);
		die("wrong regex");
	}

	unsigned n = 2;
	dict_t* cap;
	while( (cap=match(rex, U8(txt), flags)) ){
		if( show ) show_capture(U8(rx), U8(txt), cap);
		DELETE(cap);
		--n;
		if( !n ) die("");
	}
}

__private void test_match(void){
	testing_match("[a-z]+", "ciao mondo ", MATCH_FULL_TEXT | MATCH_CONTINUE_AFTER, 1);
}

int main(){
	test_build();
	//test_match();
	return 0;
}
