#include <notstd/threads.h>
#include <notstd/delay.h>
#include <notstd/vector.h>

typedef struct conf{
	delay_t ms;
	unsigned rp;
	const char* str;
	glock_s* glm;
	glock_s* gls;
	glock_s* gle;
}conf_s;

__private void async_print(__unused thr_t* thr, void* ctx){
	conf_s* conf = ctx;

	for( unsigned i = 0 ; i < conf->rp; ++i ){
		delay_ms(conf->ms);
		printf("> %s\n", conf->str);
	}

	treturn(thr, NULL);
}

__private void async_mutex(__unused thr_t* thr, void* ctx){
	conf_s* conf = ctx;
	dbg_info("lock mutex");
	mutex_lock(conf->glm);
	dbg_info("waitlock %lu", conf->ms);
	delay_ms(conf->ms);
	mutex_unlock(conf->glm);
	dbg_info("unlock");
}

__private void async_sem_push(__unused thr_t* thr, void* ctx){
	conf_s* conf = ctx;
	dbg_info("sem push inc %u", conf->rp);
	for(unsigned i = 0; i < conf->rp; ++i ){
		delay_ms(conf->ms);
		dbg_info("push");
		semaphore_post(conf->gls);
	}
}

__private void async_sem_pop(__unused thr_t* thr, void* ctx){
	conf_s* conf = ctx;
	dbg_info("sem pop inc %u", conf->rp);
	for(unsigned i = 0; i < conf->rp; ++i ){
		delay_ms(conf->ms);
		dbg_info("wait pop");
		semaphore_wait(conf->gls);
		dbg_info("popped");
	}
}

__private void async_event_raise(__unused thr_t* thr, void* ctx){
	conf_s* conf = ctx;
	delay_ms(conf->ms);
	dbg_info("raise");
	event_raise(conf->gle);
}

__private void async_event_wait(__unused thr_t* thr, void* ctx){
	conf_s* conf = ctx;
	delay_ms(conf->ms);
	dbg_info("wait event");
	event_wait(conf->gle);
	dbg_info("event raised");
}

int main(){
	glock_s mtx;
	mutex_ctor(&mtx, 0);
	glock_s sem;
	semaphore_ctor(&sem, 0, 0);
	glock_s ev;
	event_ctor(&ev, 0);

	conf_s conf[] = {
		{
			.ms  = 500,
			.rp  = 1,
			.str = "any examples",
			.glm = &mtx
		},
		{
			.ms  = 600,
			.rp  = 1,
			.str = "mutex",
			.glm = &mtx
		},
		{
			.ms  = 400,
			.rp  = 3,
			.str = "sem push",
			.gls = &sem
		},
		{
			.ms  = 600,
			.rp  = 3,
			.str = "sem pop",
			.gls = &sem
		},
		{
			.ms  = 700,
			.rp  = 0,
			.str = "ev raised",
			.gle = &ev
		},
		{
			.ms  = 1,
			.rp  = 0,
			.str = "ev wait",
			.gle = &ev
		}

	};
	
	thr_t* t[4];

	puts("print:");
	t[0] = START(async_print, &conf[0]);
	thr_wait(t[0]);
	mem_free(t[0]);
	puts("");

	puts("mutex:");
	t[0] = START(async_mutex, &conf[0]);
	t[1] = START(async_mutex, &conf[1]);
	thr_waitv(t, 2);
	mem_free(t[0]);
	mem_free(t[1]);
	puts("");

	puts("semaphore");
	t[0] = START(async_sem_push, &conf[2]);
	t[1] = START(async_sem_pop, &conf[3]);
	thr_waitv(t, 2);
	mem_free(t[0]);
	mem_free(t[1]);
	puts("");

	puts("event");
	t[0] = START(async_event_raise, &conf[4]);
	t[1] = START(async_event_wait, &conf[5]);
	thr_waitv(t, 2);
	mem_free(t[0]);
	mem_free(t[1]);
	puts("");
	
	puts("thread are stopped");

	__free thr_t** vthr = VECTOR(thr_t*, 10);

	for( unsigned i = 0; i < 4; ++i ){
		t[i] = START(async_print, &conf[i]);
		vector_push(&vthr, &t[i]);
	}

	while( vector_count(&vthr) ){
		thr_t* e = thr_anyof(vthr, vector_count(&vthr));
		printf("? %lX stopped\n", thr_id(e));
		for( unsigned i = 0; i < vector_count(&vthr); ++i ){
			if( vthr[i] == e ){
				vector_remove(&vthr, i, 1);
				break;
			}
		}
	}

	return 0;
}












