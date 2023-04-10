#include <notstd/str.h>
#include <notstd/vector.h>

char* str_dup(const char* src, size_t len){
	if( !len ) len=strlen(src);
	char* str = MANY(char, len + 1);
	memcpy(str, src, len);
	str[len] = 0;
	return str;
}

int str_ncmp(const char* a, size_t lenA, const char* b, size_t lenB){
	if( lenA != lenB ) return lenA-lenB;
	return strncmp(a,b, lenA);
}

char* str_cpy(char* dst, const char* src){
	size_t len = strlen(src);
	memcpy(dst, src, len+1);
	return &dst[len];
}

char* str_vprintf(const char* format, va_list va1, va_list va2){
	size_t len = vsnprintf(NULL, 0, format, va1);
	char* ret = MANY(char, len+1);
	vsprintf(ret, format, va2);
	return ret;
}

__printf(1,2) char* str_printf(const char* format, ...){
	va_list va1,va2;
	va_start(va1, format);
	va_start(va2, format);
	char* ret = str_vprintf(format, va1, va2);
	va_end(va1);
	va_end(va2);
	return ret;
}

const char* str_find(const char* str, const char* need){
	const char* ret = strstr(str, need);
	return ret ? ret : &str[strlen(str)];
}

const char* str_nfind(const char* str, const char* need, size_t max){
	const char* f = str;
	size_t len = str_len(need);
	size_t lr = max;
	while( (f=memchr(f, *need, lr)) ){
		if( !memcmp(f, need, len) )	return f;
		++f;
		lr = max - (f-str);
	}
	return str+max;	
}

const char* str_anyof(const char* str, const char* any){
	const char* ret = strpbrk(str, any);
	return ret ? ret : &str[strlen(str)];
}

const char* str_skip_h(const char* str){
	while( *str && (*str == ' ' || *str == '\t') ) ++str;
	return str;
}

const char* str_skip_hn(const char* str){
	while( *str && (*str == ' ' || *str == '\t' || *str == '\n') ) ++str;
	return str;
}

const char* str_next_line(const char* str){
	while( *str && *str != '\n' ) ++str;
	if( *str ) ++str;
	return str;
}

const char* str_end(const char* str){
	size_t len = strlen(str);
	return &str[len];
}

void str_swap(char* restrict a, char* restrict b){
	size_t la = strlen(a);
	size_t lb = strlen(b);
	memswap(a, la, b, lb);
}

void str_chomp(char* str){
	const ssize_t len = strlen(str);
	if( len > 0 && str[len-1] == '\n' ){
		str[len-1] = 0;
	}
}

void str_toupper(char* dst, const char* src){
	while( (*dst++=toupper(*src++)) );
}

void str_tolower(char* dst, const char* src){
	while( (*dst++=toupper(*src++)) );
}

void str_tr(char* str, const char* find, const char replace){
	while( (str=strpbrk(str,find)) ) *str++ = replace;
}

const char* str_tok(const char* str, const char* delimiter, int anydel, __out unsigned* len, __out unsigned* next){
	const char* begin = &str[*next];
	if( !*begin ) return begin;
	const char* sn = anydel ? str_anyof(begin, delimiter) : str_find(begin, delimiter);
	*len = sn - begin;
	*next += *sn ? *len + strlen(delimiter) : *len;
	return begin;
}

char** str_split(const char* str, const char* delimiter, int anydel){
	unsigned len  = 0;
	unsigned next = 0;
	const char* tok = NULL;
	char** vstr = VECTOR(char*, 8);
	while( *(tok=str_tok(str, delimiter, anydel, &len, &next)) ){
		char* dp = str_dup(tok, len);
		mem_gift(dp, vstr);
		vector_push(&vstr, &dp);
	}	
	return vstr;
}

void str_insch(char* dst, const char ch){
	size_t ld = strlen(dst);
	memmove(dst+1, dst, ld);
	*dst = ch;
}

void str_ins(char* dst, const char* restrict src, size_t len){
	size_t ld = strlen(dst);
	memmove(dst+len, dst, ld+1);
	memcpy(dst, src, len);
}

void str_del(char* dst, size_t len){
	size_t ld = strlen(dst);
	memmove(dst, &dst[len], (ld - len)+1);
}

char* quote_printable_decode(size_t *len, const char* str){
	size_t strsize = strlen(str);
	char* ret = MANY(char, strsize + 1);
	char* next = ret;
	while( *str ){
		if( *str != '=' ){
			*next++ = *str++;
		}
		else{
			++str;
			if( *str == '\r' ) ++str;
			if( *str == '\n' ){
				++str;
				continue;
			}	
			char val[3];
			val[0] = *str++;
			val[1] = *str++;
			val[2] = 0;
			*next++ = strtoul(val, NULL, 16);
		}
	}
	*next = 0;
	if( len ) *len = next - ret;
	return ret;
}

long str_tol(const char* str, const char** end, unsigned base, int* err){
	char* e;
	errno = 0;
	long ret = strtol(str, &e, base);
	if( errno || !e || str == e){
		if( err ) *err = 1;
		if( !errno ) errno = EINVAL;
		if( end ) *end = str;
		return 0;
	}
	if( err ) *err = 0;
	if( end ) *end = e;
	return ret;
}

unsigned long str_toul(const char* str, const char** end, unsigned base, int* err){
	char* e;
	errno = 0;
	unsigned long ret = strtoul(str, &e, base);
	if( errno || !e || str == e ){
		if( err ) *err = 1;
		if( !errno ) errno = EINVAL;
		if( end ) *end = str;
		return 0;
	}
	if( err ) *err = 0;
	if( end ) *end = e;
	return ret;
}

__private int escape_num(const char* str, unsigned* i, unsigned base){
	int e;
	const char* next;
	int code = str_toul(&str[*i], &next, base, &e);
	if( e ) return -1;
	*i = next - &str[*i];
	return code;
}

char* str_escape_decode(const char* str, unsigned len){
	if( !len ) len = strlen(str);
	char* out = MANY(char, len+1);
	unsigned i = 0;
	unsigned o = 0;
	unsigned q = 0;
	int ch;

	while( i < len ){
		if( str[i] == '\\' ){
			++i;
			switch( str[i] ){
				case 't': out[o++] = '\t'; break;
				case 'n': out[o++] = '\n'; break;
				case 'r': out[o++] = '\r'; break;
				case 'f': out[o++] = '\f'; break;
				case 'a': out[o++] = '\a'; break;
				case 'e': out[o++] = 27;   break;
				case 'x':
					++i;
					if( str[i] == '{' ){ q = 1; ++i; }
					else{ q = 0; }
					ch = escape_num(str, &i, 16);
					if( ch == -1 ) goto ONERR;
					if( q ){
						if( str[i] == '}' ){ ++i; }
						else{ goto ONERR; }
					}
					if( ch > 127 ) goto ONERR;
					out[o++] = ch;
				break;

				case 'o':
					++i;
					if( str[i] != '{' ) goto ONERR;
					++i;
					ch = escape_num(str, &i, 8);
					if( str[i] != '}' ) goto ONERR;
					if( ch > 127 ) goto ONERR;
					out[o++] = ch;
				break;

				case '0':
					ch = escape_num(str, &i, 8);
					if( ch == -1 || ch > 127 ) goto ONERR;
					out[o++] = ch;
				break;

				default: out[o++] = str[i++]; break;
			}
		}
		else{
			out[o++] = str[i++];
		}
	}

	return out;
ONERR:
	mem_free(out);
	return NULL;
}













