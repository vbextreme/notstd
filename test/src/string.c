#include <notstd/dict.h>
#include <notstd/regex.h>
#include <notstd/map.h>
#include <notstd/delay.h>

//TODO not match last chars
/*
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
	testing_match("[a-z]+", "ciao mondo", MATCH_FULL_TEXT | MATCH_CONTINUE_AFTER, 1);
}
*/
struct um{
	unsigned va;
	unsigned nb;
}map[16] = {
	{0b00000000, 1},
	{0b00000001, 1},
	{0b00000010, 1},
	{0b00000011, 1},
	{0b00000100, 1},
	{0b00000101, 1},
	{0b00000110, 1},
	{0b00000111, 1},
	{0b00001000, 1},
	{0b00001001, 1},
	{0b00001010, 1},
	{0b00001011, 1},
	{0b00001100, 2},
	{0b00001101, 2},
	{0b00001110, 3},
	{0b00001111, 4}
};

unsigned fast_nb(utf8_t u){
	return ((0xE5000000 >> ((u>>4)<<1)) & 0x3)+1;
}

#define bench() time_cpu_us()
#define TEST_BENCH (UINT32_MAX/10)

void printmap(void){
	printf("unsigned UTF8_NB_MAP[256] = {");
	for( unsigned i = 0; i < 256; ++i ){
		printf(" %u,", utf8_codepoint_nb(i));
	}
	printf("}\n");
}

int main(){
	printmap();
	unsigned umask = 0;

	for( unsigned i = 0; i < 16; ++i ){
		if( map[i].va > 1 ){
			umask |= (map[i].nb) << (map[i].va<<1);
		}
	}

	for( unsigned i = 0; i < 16; ++i ){
		unsigned nb = fast_nb(i<<4);
		printf("%2u| %u | %u | %s \n", i, map[i].nb, nb, map[i].nb == nb ? "true" : "fail" );
	}

	printf("mask: 0x%X\n", umask);

	for( unsigned i = 0; i < 256; ++i ){
		unsigned ounb = utf8_codepoint_nb(i);
		unsigned nwnb = fast_nb(i);
		if( ounb != nwnb ) die("fail on value %d, old_nb(%u) new_nb(%u)", i, ounb, nwnb);
	}

	//exit(0);
	delay_t st = bench();
	for( unsigned i = 0; i < TEST_BENCH; ++i ){
		utf8_t* u8 = (utf8_t*)&i;
		unsigned nb = utf8_codepoint_nb(*u8);
		if( nb < 1 || nb > 4 ) die("fail on value %d, _nb(%u)", i, nb);
		if( map[(*u8)>>4].nb != nb ) die("org fail on value %d, _nb(%u), aspected %u", i, nb, map[*u8>>4].nb);
	}
	delay_t en = bench();
	printf("sta nnb: %lu\n", en-st);

	st = bench();
	for( unsigned i = 0; i < TEST_BENCH; ++i ){
		utf8_t* u8 = (utf8_t*)&i;
		unsigned nb = fast_nb(*u8);
		if( nb < 1 || nb > 4 ) die("fail on value %d, _nb(%u)", i, nb);
		if( map[(*u8)>>4].nb != nb ) die("org fail on value %d, _nb(%u), aspected %u", i, nb, map[*u8>>4].nb);
	}
	en = bench();
	printf("loc fnb: %lu\n", en-st);


	
	//__free regex_t* r = regex(U8("ciao(mondo) (come|va)* [sqen-ces]+"), 0);
	//regex_error_show(r);

	//test_build();
	//test_match();
	return 0;
}
