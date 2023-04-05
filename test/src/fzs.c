#include <notstd/core.h>
#include <notstd/fzs.h>
#include <notstd/vector.h>
#include <notstd/delay.h>
#include <notstd/str.h>

#include <sys/types.h>
#include <dirent.h>

__private fzs_s words[] = {
	{ "Smith"    , 0, 0 },
	{ "Johnson"  , 0, 0 },
	{ "Williams" , 0, 0 },
	{ "Welsh"    , 0, 0 },
	{ "Brown"    , 0, 0 },
	{ "Jones"    , 0, 0 },
	{ "Garcia"   , 0, 0 },
	{ "Miller"   , 0, 0 },
	{ "Davis"    , 0, 0 },
	{ "Rodriguez", 0, 0 },
	{ "Martinez" , 0, 0 },
	{ "Hernandez", 0, 0 },
	{ "Lopez"    , 0, 0 },
	{ "Gonzales" , 0, 0 },
	{ "Wilson"   , 0, 0 },
	{ "Anderson" , 0, 0 },
	{ "Thomas"   , 0, 0 },
	{ "Taylor"   , 0, 0 },
	{ "Moore"    , 0, 0 },
	{ "Jackson"  , 0, 0 },
	{ "Martin"   , 0, 0 },
	{ "Lee"      , 0, 0 },
	{ "Thompson" , 0, 0 },
	{ "White"    , 0, 0 },
	{ "Harris"   , 0, 0 },
	{ "Sanchez"  , 0, 0 },
	{ "Clark"    , 0, 0 },
	{ "Ramirez"  , 0, 0 },
	{ "Lewis"    , 0, 0 },
	{ "Robinson" , 0, 0 },
	{ "Walker"   , 0, 0 },
	{ "Young"    , 0, 0 },
	{ "Allen"    , 0, 0 },
	{ "King"     , 0, 0 },
	{ "Wright"   , 0, 0 },
	{ "Scott"    , 0, 0 },
	{ "Torres"   , 0, 0 }
};

__private const char* match[] = {
	"mon",
	"welldone",
	"torre",
	"a",
	"al",
	"all",
	"alle",
	"allen"
};

__private void pres(fzs_s* f){
	for( unsigned i = 0; i < 9; ++i ){
		printf("  [%u]%s", i, f[i].str);
	}
}

/*
__private void pres2(fzs_s* f){
	for( unsigned i = 0; i < 9; ++i ){
		printf("  [%u](%lu)%s\n", i, f[i].distance, f[i].str);
	}
}
*/

void uc_fzs(void){
	const size_t wordssize = sizeof_vector(words);
	const size_t matchsize = sizeof_vector(match);
	for( unsigned m = 0; m < matchsize; ++m ){
		printf("search: %s\n", match[m]);
		//fzs_qsort(words, wordssize, match[m], 0, fzs_case_damerau_levenshtein);
		fputs("  weight     : ",stdout);
		fzs_qsort(words, wordssize, match[m], 0, fzs_case_weigth_levenshtein);
		pres(words);
		puts("");
		fputs("  levenshtein: ",stdout);
		fzs_qsort(words, wordssize, match[m], 0, fzs_case_levenshtein);
		pres(words);
		puts("");
	}
}

__private char** allcmd(void){
	char** ret = VECTOR(char*, 128);
	const char* PATH = getenv("PATH");
	if( !PATH ) die("need enviroment PATH");
	const char* path;
	unsigned len;
	unsigned next = 0;
	while( (path=str_tok(PATH, ":", 0, &len, &next)) ){
		if( !len && len >= PATH_MAX - 1 ) continue;
		char norm[PATH_MAX];
		memcpy(norm, path, len);
		norm[len] = 0;
		dbg_info("open PATH:%s", norm);
		DIR* d = opendir(norm);
		if( !d ) continue;
		struct dirent* ent;
		while( (ent=readdir(d)) ){
			if( !strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || ent->d_type != DT_REG ) continue;
			unsigned l = strlen(ent->d_name);
			char* e = MANY(char, l+1);
			memcpy(e, ent->d_name, l+1);
			vector_push(&ret, &e);
			mem_gift(e, ret);
		}
		closedir(d);
	}
	return ret;
}

__private char* getinput(const char* prompt){
	fputs(prompt, stdout);
	fflush(stdout);
	char* in = MANY(char, 80);
	unsigned len = 0;
	while( fgets(&in[len], mem_size(in)-len, stdin) ){
		len = strlen(in);
		if( len && in[len-1] == '\n' ) break;
		in = RESIZE(char, in, mem_size(in) + 80);
	}
	iassert(len < mem_size(in)-1);
	iassert(len);
	if( !len ) len = 1;
	in[len-1] = 0;
	return in;
}

__private void reply(const char* prefix, fzs_s* cmd){
	fputs(prefix, stdout);
	for( unsigned i = 0; i < 5; ++i ){
		fputs("  ", stdout);
		fputs(cmd[i].str, stdout);
	}
	puts("");
}

__private int fs_inp_cmp(const char* prompt, const char* cmdexit, fzs_s* cmds){
	__free char* in = getinput(prompt);
	if( !strcmp(in, cmdexit) ) return 0;
	
	fzs_qsort(cmds, vector_count(&cmds), in, strlen(in), fzs_case_weigth_levenshtein);
	reply("  vbextreme:", cmds);
	
	fzs_qsort(cmds, vector_count(&cmds), in, strlen(in), fzs_case_levenshtein);
	reply("levenshtein:", cmds);
	
	fzs_qsort(cmds, vector_count(&cmds), in, strlen(in), fzs_case_damerau_levenshtein);
	reply("    damerau:", cmds);
	
	return 1;
}

__private fzs_s* fzscmd(char** cmds){
	fzs_s* v = VECTOR(fzs_s, vector_count(&cmds));
	vector_reserve(&v, vector_count(&cmds));
	foreach_vector(cmds, i){
		v[i].distance = 0;
		v[i].len = strlen(cmds[i]);
		v[i].str = mem_borrowed(cmds[i], v);
	}
	return v;
}

/*
__private void fasttest(const char* str, fzs_s* cmds){
	puts(str);
	fzs_qsort(cmds, vector_count(&cmds), str, strlen(str), fzs_case_vbx);
	reply(">>", cmds);
	pres2(cmds);
	puts("");

	fzs_qsort(cmds, vector_count(&cmds), str, strlen(str), fzs_case_levenshtein);
	reply("@@", cmds);
}
*/

void uc_fzs_inp(void){
	__free char** cmds  = allcmd();
	__free fzs_s* fzcmd = fzscmd(cmds);
	//fasttest("bash", fzcmd);
	//fasttest("hsab", fzcmd);
	//fasttest("cash", fzcmd);
	//fasttest("sl", fzcmd);
	//fasttest("objdump", fzcmd);
	//fasttest("ibjdimp", fzcmd);

	while( fs_inp_cmp("> ", "q", fzcmd) );
}

void uc_fzs_benchmark(void){
	__free char** cmds  = allcmd();
	__free fzs_s* fzcmd = fzscmd(cmds);
	delay_t start, vbx, lev, dam;
   	
	puts("benchmarks fzs:");

	fputs("weight: ", stdout);
	fflush(stdout);
	start = time_us();
	foreach_vector(cmds, it){
		fzs_qsort(fzcmd, vector_count(&fzcmd), cmds[it], strlen(cmds[it]), fzs_case_weigth_levenshtein);
	}
	vbx = time_us() - start;
	puts("successfull");
	
	fputs("levenshtein: ", stdout);
	fflush(stdout);
	start = time_us();
	foreach_vector(cmds, it){
		fzs_qsort(fzcmd, vector_count(&fzcmd), cmds[it], strlen(cmds[it]), fzs_case_levenshtein);
	}
	lev = time_us() - start;
	puts("successfull");

	fputs("damerau: ", stdout);
	fflush(stdout);
	start = time_us();
	foreach_vector(cmds, it){
		fzs_qsort(fzcmd, vector_count(&fzcmd), cmds[it], strlen(cmds[it]), fzs_case_damerau_levenshtein);
	}
	dam = time_us() - start;
	puts("successfull");

	printf("weight      : %fs\n", (double)vbx / 1000000.0);
	printf("levenshtein : %fs\n", (double)lev / 1000000.0);
	printf("damerau     : %fs\n", (double)dam / 1000000.0);
}





















