#include <notstd/dict.h>
#include <notstd/regex.h>
#include <notstd/map.h>
#include <notstd/delay.h>

__private void testing_build(const char* rx, const char* err, int show){
	dbg_warning("try /%s/", rx);
	__free regex_t* rex = regex_build(U8(rx), 0);
	if( regex_error(rex) != err ) die("regex return '%s' but aspected '%s' %d", regex_error(rex), err, errno);
	if( show ){
		regex_error_show(rex);
		die("");
	}
}

__private void test_build(void){
	testing_build("ciao[", REGEX_ERR_UNTERMINATED_SEQUENCES, 0);
	testing_build("ciao[a-z", REGEX_ERR_UNTERMINATED_SEQUENCES, 0);
	testing_build("ciao[]", REGEX_ERR_ASPECTED_SEQUENCES, 0);
	testing_build("ciao]", REGEX_ERR_UNOPENED_SEQUENCES, 0);
	testing_build("ciao]mondo", REGEX_ERR_UNOPENED_SEQUENCES, 0);
	testing_build("c[ia]o]", REGEX_ERR_UNOPENED_SEQUENCES, 0);

	testing_build("ciao[a-z]{", REGEX_ERR_UNTERMINATED_QUANTIFIER, 0);
	testing_build("ciao[a-z]{1,2", REGEX_ERR_UNTERMINATED_QUANTIFIER, 0);
	testing_build("ciao[a-z]}", REGEX_ERR_UNOPENED_QUANTIFIERS, 0);
	testing_build("ciao[a-z]{1}}", REGEX_ERR_UNOPENED_QUANTIFIERS, 0);

	testing_build("ciao\\up", REGEX_ERR_INVALID_UTF_ESCAPE, 0);
	testing_build("ciao[\\up]", REGEX_ERR_INVALID_UTF_ESCAPE, 0);

	testing_build("ciao\\g{a123456789b123456789c123456789", REGEX_ERR_UNTERMINATED_GROUP_NAME, 0);
	testing_build("ciao\\ga1234567}", REGEX_ERR_UNOPENED_BACKREF_NAME, 0);

	testing_build("ciao\\0123456789101234566789201234567893012345678940", REGEX_ERR_NUMERICAL_OUT_OF_RANGE, 0);

	testing_build("(ciao)(abcde", REGEX_ERR_UNTERMINATED_GROUP, 0);
	testing_build("(ciao)abc)de", REGEX_ERR_UNOPENED_GROUP, 0);
	testing_build("ciao(?<abc)", REGEX_ERR_UNTERMINATED_GROUP_NAME, 0);
	testing_build("ciao(?')", REGEX_ERR_UNTERMINATED_GROUP_NAME, 0);
	
}


/*
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
int main(){
	/*	
	const utf8_t* rx = U8("ciao[ \\t](mondo)[ \\t]!?, (bye|good bye).*");
	__free regex_t* r = regex_build(rx, 0);
	if( regex_error(r) ){
		regex_error_show(r);
		return 1;
	}

	dict_t* cap = match_at(r, U8("ciao"));
	if( cap ) mem_free(cap);
	*/

	const utf8_t* rx = U8("[A-Z]+");
	regex_t* r = regex_build(rx, 0);
	if( regex_error(r) ){
		regex_error_show(r);
		return 1;
	}

	dict_t* cap = match_at(r, U8("CIao"), NULL);
	if( cap ){
		generic_s* m;
		if( (m=dict(cap, 0))->type == G_SUB ){
			printf("match[0](%u): %.*s\n", m->sub.size, m->sub.size, (char*)m->sub.start);
		}
		mem_free(cap);
	}
	mem_free(r);

	return 0;
	test_build();
	//test_match();
	return 0;
}
