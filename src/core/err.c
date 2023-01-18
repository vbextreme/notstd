#undef DBG_ENABLE
#include <notstd/core.h>
#include <sys/ioctl.h>

__private const char* errors[ERR_PAGE][ERR_MAX] = {
	[ERR_CORE - 1] = {
		[ECORE_WRONG_ERROR] = "wrong error"
	}
};

int err_str_set(unsigned page, unsigned err, const char* str){
	if( page >= ERR_PAGE ) ereturn(ERR_WRONG_ERROR);
	if( err  >=	ERR_MAX  ) ereturn(ERR_WRONG_ERROR);
	errors[page][err] = str;
	return 0;
}

const char* err_str_num(int err){	
	if( err < 256 ) return strerror(errno);
	unsigned page = err >> 8;
	unsigned errr = err & (ERR_MAX - 1);
	if( page >= ERR_PAGE ) { page = ERR_CORE; errr = ECORE_WRONG_ERROR; }
	if( errr >=	ERR_MAX  ) { page = ERR_CORE; errr = ECORE_WRONG_ERROR; }
	return errors[page][errr];
}

const char* err_str(void){
	return err_str_num(errno);
}

__private unsigned termwidth(void){
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	return w.ws_col;
}

__private const char* err_startline(const char* begin, const char* str){
	while( str > begin && *str != '\n' ) --str;
	return *str == '\n' ? ++str : str;
}

__private unsigned err_countline(const char* begin, const char* line){
	unsigned count = 1;
	while( *(begin = strchrnul(begin, '\n')) && begin < line ){
		++begin;
		++count;
	}
	return count;
}

__private const char* stranyof(const char* str, const char* any){
	const char* ret = strpbrk(str, any);
	return ret ? ret : &str[strlen(str)];
}

__private const char* err_word(__out const char** end, const char* line, const char* str){
	const char* sep = " \t\n+~`!@#$%^&*()-=[]\\{}|;':\",./<>?";
	const char* ret = str;
	if( *strchrnul(sep, *str) ){
		*end = str+1;
		dbg_info("%c is sep", *str);
		return str;
	}

	while( ret > line && !*strchrnul(sep, *ret) ){
		--ret;
	}
	if( *strchrnul(sep, *ret) ) ++ret;
	*end = stranyof(str, sep);
	return ret;
}

void err_showline(const char* begin, const char* errs, int wordmode){
	int __tty__ = isatty(fileno(stderr));
	const char* __ce__ = __tty__ ? ERR_COLOR : "";\
	const char* __cr__ = __tty__ ? ERR_COLOR_RESET : "";\
	const char* line = err_startline(begin, errs);
	unsigned nline = *strchrnul(begin, '\n') ? err_countline(begin, line) : 0;
	unsigned width = termwidth();
	unsigned poserr = errs - line;
	const char* endWord = errs+1;
	const char* startWord = wordmode ? err_word(&endWord, line, errs) : errs;
	const char* wr = line;
	unsigned out = 0;
	unsigned po;

	if( nline > 0 )	fprintf(stderr, "at line: %u\n", nline);

	if( poserr > width - 1 ){
		wr = startWord;
		fputs("... ", stderr);
		out = 4;
	}
	else{
		while( wr != startWord ){
			fputc(*wr++, stderr);
			++out;
		}
	}
	po = out;

	fputs(__ce__, stderr);
	while( wr != endWord && out < width - 1 ){
		fputc(*wr++, stderr);
		++out;
	}
	fputs(__cr__, stderr);

	while( *wr && *wr != '\n' && out < width - 1 ){
		fputc(*wr++, stderr);
		++out;
	}

	fputc('\n', stderr);
	while( po-->0 ){
		fputc(' ', stderr);
	}
	fputs("^\n", stderr);
}
