#include <notstd/dict.h>
#include <notstd/str.h>
#include <notstd/regex.h>
#include <notstd/map.h>
#include <notstd/delay.h>

//todo test utf8_anyof think have some problems

/*
	test match:
		regextest/type
			/regex/flags
			>input, if not specified used last input
			[output]value
	examples:
		regextest/example
			/[a-z]+/
			>123 hello 456
			[0]hello
			/(?'n'[a-z]+)/
			[0]hello
			[n]hello
*/
	

__private void testing_build(const char* rx, const char* err, int show){
	dbg_warning("try %s", rx);
	__free regex_t* rex = regex(rx);
	if( regex_error(rex) != err ) die("regex return '%s' but aspected '%s' %d", regex_error(rex), err, errno);
	if( show ){
		regex_error_show(rex);
		die("");
	}
}

__private void test_build(void){
	testing_build("/ciao[/", REGEX_ERR_UNTERMINATED_SEQUENCES, 0);
	testing_build("/ciao[a-z/", REGEX_ERR_UNTERMINATED_SEQUENCES, 0);
	testing_build("/ciao[]/", REGEX_ERR_ASPECTED_SEQUENCES, 0);
	testing_build("/ciao]/", REGEX_ERR_UNOPENED_SEQUENCES, 0);
	testing_build("/ciao]mondo/", REGEX_ERR_UNOPENED_SEQUENCES, 0);
	testing_build("/c[ia]o]/", REGEX_ERR_UNOPENED_SEQUENCES, 0);

	testing_build("/ciao[a-z]{/", REGEX_ERR_UNTERMINATED_QUANTIFIER, 0);
	testing_build("/ciao[a-z]{1,2/", REGEX_ERR_UNTERMINATED_QUANTIFIER, 0);
	testing_build("/ciao[a-z]}/", REGEX_ERR_UNOPENED_QUANTIFIERS, 0);
	testing_build("/ciao[a-z]{1}}/", REGEX_ERR_UNOPENED_QUANTIFIERS, 0);

	testing_build("/ciao\\up/", REGEX_ERR_INVALID_UTF_ESCAPE, 0);
	testing_build("/ciao[\\up]/", REGEX_ERR_INVALID_UTF_ESCAPE, 0);

	testing_build("/ciao\\g{a123456789b123456789c123456789/", REGEX_ERR_UNTERMINATED_GROUP_NAME, 0);
	testing_build("/ciao\\ga1234567}/", REGEX_ERR_UNOPENED_BACKREF_NAME, 0);

	testing_build("/ciao\\0123456789101234566789201234567893012345678940/", REGEX_ERR_NUMERICAL_OUT_OF_RANGE, 0);

	testing_build("/(ciao)(abcde/", REGEX_ERR_UNTERMINATED_GROUP, 0);
	testing_build("/(ciao)abc)de/", REGEX_ERR_UNOPENED_GROUP, 0);
	testing_build("/ciao(?<abc)/", REGEX_ERR_UNTERMINATED_GROUP_NAME, 0);
	testing_build("/ciao(?')/", REGEX_ERR_UNTERMINATED_GROUP_NAME, 0);
}

typedef struct onmatch{
	char* test;
	char** results;
}onmatch_s;

typedef struct testmatch{
	FILE* f;
	unsigned line;
	char buf[4096];
	onmatch_s* onmatch;
	char** unmatch;
	regex_t* rex;
}testmatch_s;

__private int test_match_load(testmatch_s* tm){
	unsigned skipmode = 0;
	char buf[4096];
	while( fgets(buf, 4096, tm->f) ){
		++tm->line;
		//dbg_info("[%u]%s", tm->line, buf);
		const char* p = str_skip_h(buf);
		if( *p == '\n' ) continue;
		if( *p == '#' ) continue;
		if( *p == '@' ){
			if( !strcmp(p, "@STOP\n") ) return 0;
			die("error unknow directive @ at line %u", tm->line);
		}
		if( !strncmp(p, "???", 3) ){
			skipmode = 1;
			dbg_warning("%.*s", (int)strlen(p)-4, p+3);
			continue;
		}
		
	
		if( skipmode ){
			if( skipmode == 1 ){
				if( *p == '/' ) skipmode = 2; 
				continue;
			}
			else if( skipmode == 2){
				if( *p != '/' ) continue;
				skipmode = 0;
			}
		}
	
		if( *p != '/' ) die("unknow directive %c at line %u", *p, tm->line);
	
		char* e = str_chr(p, '\n');
		*e = 0;
		char* defrx = str_dup(p, 0);
		if( tm->rex ) DELETE(tm->rex);
		dbg_info("build regex %s", defrx);
		tm->rex = regex(defrx);
		mem_gift(defrx, tm->rex);
		if( regex_error(tm->rex) ){
			regex_error_show(tm->rex);
			die("regex error");
		}
		
		vector_clear(&tm->onmatch);
		vector_clear(&tm->unmatch);
		
		char* tmp = NULL;
		onmatch_s* cur = NULL;

		while( fgets(buf, 4096, tm->f) ){
			++tm->line;
			const char* l = buf;
			if( *l == '\n' ) break;
			if( *l == '/' ) die("internal error");
			l = str_skip_h(l);
			if( !strncmp(l, "<<<", 3) ){
				l = str_skip_h(l+3);
				if( tmp ) die("double input without match");
				tmp = str_dup(l, strlen(l)-1);
				mem_gift(tmp, tm->rex);
			}
			else if( *l == '[' ){
				++l;
				if( *l == '!' ){
					if( tmp ){
						vector_push(&tm->unmatch, &tmp);
						tmp = NULL;
					}
				}
				else{
					if( tmp ){
						cur = vector_push(&tm->onmatch, NULL);
						cur->test = tmp;
						cur->results = VECTOR(char*, 2);
						tmp = NULL;
					}
					int e;
					unsigned n = str_toul(l, &l, 10, &e);
					if( e ) die("error on get numbers: %m");
					if( *l != ']' ) die("wrong match delimiter");
					char* res = str_dup(l+2, strlen(l+2)-1);
					vector_push(&cur->results, &res);
					mem_gift(res, tm->rex);
					if( vector_count(&cur->results) != n+1 ) die("match results not allineated");
				}
			}
			else{
				die("unknow directive");
			}
		}

		foreach_vector(tm->onmatch, i){
			mem_gift(tm->onmatch[i].results, tm->rex);
		}
		return 0;
	}

	return -1;
}

__private void test_match_open(testmatch_s* tm, const char* path){
	tm->onmatch = VECTOR(onmatch_s, 2);
	tm->unmatch = VECTOR(char*, 2);
	tm->line = 0;
	tm->rex  = NULL;
	tm->f = fopen(path, "r");
	if( !tm->f ) die("unable to open file %s, error:%m", path);
}

__private void test_match_complete(testmatch_s* tm){
	DELETE(tm->rex);
}

__private void test_match_try(testmatch_s* tm){
	const char* rx = (const char*)regex_get(tm->rex);
	printf("test %s: ", rx);

	foreach_vector(tm->onmatch, i){
		const char* txt = tm->onmatch[i].test;
		dbg_info("test match: '%s'", txt);
		dict_t* cap = match(tm->rex, &txt);
		if( !cap ) die("test '%s' aspected any results but return no match", tm->onmatch[i].test);

		foreach_vector(tm->onmatch[i].results, r){
			generic_s* g = dict(cap, r);
			if( g->type != G_SUB ) 
				die("test '%s' aspected [%lu] '%s' but return unset", tm->onmatch[i].test, r, tm->onmatch[i].results[r]);
			if( strlen(tm->onmatch[i].results[r]) != g->sub.size ) 
				die("test '%s' aspected [%lu] '%s' but return '%.*s'", tm->onmatch[i].test, r, tm->onmatch[i].results[r], g->sub.size, (char*)g->sub.start);
			if( strncmp(tm->onmatch[i].results[r], g->sub.start, g->sub.size) ) 
				die("test '%s' aspected [%lu] '%s' but return '%.*s'", tm->onmatch[i].test, r, tm->onmatch[i].results[r], g->sub.size, (char*)g->sub.start);
		}
		if( dict_count(cap) != vector_count(&tm->onmatch[i].results) )
			die("ascpected %lu cap but get %lu cap", vector_count(&tm->onmatch[i].results), dict_count(cap));
		mem_free(cap);
	}

	foreach_vector(tm->unmatch, i){
		const char* txt = tm->unmatch[i];
		dbg_info("test unmatch: '%s'", txt);
		dict_t* cap = match(tm->rex, &txt);
		if( cap ){
			generic_s* g = dict(cap,0);
			if( g->type != G_SUB ){
				die("test '%s' aspected unmatch but return unset", tm->unmatch[i]);
			}
			else{
				die("test '%s' aspected unmatch but return '%.*s'", tm->unmatch[i], g->sub.size, (char*)g->sub.start);
			}
			mem_free(cap);
		}
	}

	printf("successfull\n");
}

__private void test_match(const char* path){
	testmatch_s tm;
	test_match_open(&tm, path);
	while( !test_match_load(&tm) ){
		test_match_try(&tm);
		test_match_complete(&tm);
	}
	mem_free(tm.onmatch);
	mem_free(tm.unmatch);
	fclose(tm.f);
}

int main(){
	test_build();
	test_match("../test/data/regex/test.generic");

	/*
	const utf8_t* rx = U8("void (?'fn'_[a-z]+)");
	__free regex_t* r = regex(rx, 0);
	if( regex_error(r) ){
		regex_error_show(r);
		return 1;
	}

	__free dict_t* cap = match(r, U8("void _boo() {}"), NULL);
	dictPair_s* kv;
	foreach(dict, cap, kv, 0, dict_count(cap)){
		if( kv->value.type == G_SUB ){
			printf("match[");
			g_print(kv->key);
			printf("](%u): %.*s\n", kv->value.sub.size, kv->value.sub.size, (char*)kv->value.sub.start);
		}
	}
	*/
	//return 0;
	//test_match();
	return 0;
}
