#include "notstd/core/memory.h"
#include <notstd/core.h>
#include <notstd/delay.h>
#include <sys/wait.h>

typedef struct testext{
	int ex;
}testext_s;

int ut_new(void){
	dbg_info("test simple memory");
	
	dbg_info("extend memory");
	char* strA = mem_alloc(1024, sizeof(testext_s), 0, 0, 0, 0);
	strcpy(strA, "hello");
	testext_s* ext = mem_extend(strA);
	ext->ex = 123;

	dbg_info("page memory");
	char* strB = mem_alloc(1024, 0, MEM_FLAG_PAGE, 0, 0, 0);
	strcpy(strB, "hello");

	dbg_info("stack memory");
	char strCstk[1024];
	char* strC = mem_alloc(1024, 0, MEM_FLAG_VIRTUAL, 0, 0, strCstk);
	strcpy(strC, "hello");

	if( strcmp(strA,strB) || strcmp(strA, strC) || strcmp(strB, strC) ) die("mem test fail");
	
	dbg_info("free extended");
	mem_free(strA);
	dbg_info("free paging");
	mem_free(strB);
	dbg_info("free stacked");
	mem_free(strC);
	return 0;
}

int ut_raii(void){
	dbg_info("test raii");
	__free char* strB = MANY(char, 128);
	strcpy(strB, "hello");
	return 0;
}

__private char* scp(__out char* restrict d, const char* restrict s){
	while( (*d++ = *s++) );
	return --d;
}

__noreturn void child_readwrite(char* mem, delay_t ms, unsigned p){
	char n[12];
	char rp[128] = {0};
	sprintf(n, "%u ", p);
	char* a = rp;
	for( unsigned i = 0; i < p; ++i){
		a = scp(a, n);
	}

	mem_acquire_read(mem){
		write(1,   n, strlen(n));
		write(1, mem, strlen(mem));
		delay_ms(ms);
	}
	mem_acquire_write(mem){
		sprintf(mem, "child %s\n", rp);
		delay_ms(ms);
	}
	mem_acquire_read(mem){
		write(1,   n, strlen(n));
		write(1, mem, strlen(mem));
		delay_ms(ms);
	}
	exit(0);
}

#define NC 9
int ut_lock(void){
	__free char* mem = mem_alloc(2048, 0, MEM_FLAG_SHARED, 0, 0, 0);
	strcpy(mem, "hello world!\n");
	
	pid_t p[NC];
	for( unsigned i = 0; i < NC; ++i ){
		switch( (p[i] = fork()) ){
			default: break;
			case -1: die("fail fork"); break;
			case 0 : child_readwrite(mem, 214*(i+1), i); break;
		}
	}

	for( unsigned i = 0; i < NC; ++i ){
		if( waitpid(p[i], NULL, 0) != p[i] ){
			dbg_error("waitpid: %m");
		}
	}

	return 0;
}

void uc_swap(void){
	__free char* a = MANY(char, 80);
	__free char* b = MANY(char, 80);
	strcpy(a, "hello");
	strcpy(b, "world");

	dbg_info("%s swap %s", a, b);
	mem_swap(a, strlen(a), b, strlen(b));
	dbg_info("%s <> %s", a, b);
}

int main(){
	ut_new();
	ut_raii();
	ut_lock();
	uc_swap();
	return 0;
}
