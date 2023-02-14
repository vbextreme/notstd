#include <notstd/threads.h>
#include <notstd/vector.h>
#include <notstd/futex.h>

#include <pthread.h>
#include <sys/eventfd.h>
#include <sched.h>

/********************/
/*** generic lock ***/
/********************/

glock_s* glock_ctor(glock_s* gl, int val, int sharedProcess, glockWait_f w){
	gl->futex   = val;
	gl->private = sharedProcess ? 0 : FUTEX_PRIVATE_FLAG;
	gl->wait    = w;
	gl->state   = 0;
	gl->ctx     = NULL;
	return gl;
}

void glock_wait(glock_s* gl, int value){
	unsigned const op = FUTEX_WAIT | gl->private;
	futex(&gl->futex, op, value, NULL, NULL, 0);
}

void glock_wake(glock_s* gl){
	unsigned const op = FUTEX_WAKE | gl->private;
    futex(&gl->futex, op, 1, NULL, NULL, 0);
}

void glock_broadcast(glock_s* gl){
	unsigned const op = FUTEX_WAKE | gl->private;
    futex(&gl->futex, op, INT_MAX, NULL, NULL, 0);
}

glock_s* glock_anyof(glock_s** gls, unsigned count){
	futexWaitv_s fws[FUTEX_WAITV_MAX];
	if( count > 128 ) die("thr_anyof can wait only to 128 futex");
	
	for( size_t i = 0; i < count; ++i ){
		fws[i].uaddr = (uintptr_t)&gls[i]->futex;
		fws[i].flags = gls[i]->private | FUTEX_32;
		fws[i].__reserved = 0;
		int val;
		if( gls[i]->wait(gls[i], &val) ) return gls[i];
		fws[i].val   = val;
	}
	
	while(1){
		int ret = futex_waitv(fws, count, 0);
		if( ret == -EAGAIN ){
			for( size_t i = 0; i < count; ++i ){
				int val;
				if( gls[i]->wait(gls[i], &val) ) return gls[i];
				fws[i].val = val;
			}
		}
		else{
			int val;
			if( gls[ret]->wait(gls[ret], &val) ) return gls[ret];
			fws[ret].val = val;
		}
	}
}

__private void fws_remove(futexWaitv_s* fws, unsigned id, unsigned* count){
	--(*count);
	if( id == *count ) return;
	memmove(&fws[id], &fws[id+1], *count);
}

void glock_waitv(glock_s** gls, unsigned count){
	futexWaitv_s fws[FUTEX_WAITV_MAX];
	if( count > 128 ) die("thr_anyof can wait only to 128 futex");
	
	unsigned setted = 0;
	for( unsigned i = 0; i < count; ++i ){
		fws[setted].uaddr = (uintptr_t)&gls[i]->futex;
		fws[setted].flags = gls[i]->private | FUTEX_32;
		fws[setted].__reserved = 0;
		int val;
		if( !gls[i]->wait(gls[setted], &val) ){
			fws[setted].val   = val;
			++setted;
		}
	}
	
	while( setted ){
		int ret = futex_waitv(fws, count, 0);
		if( ret == -EAGAIN ){
			unsigned i = 0;
			while( i < setted ){
				int val;
				if( gls[i]->wait(gls[i], &val) ){
					fws_remove(fws, i, &setted);
				}
				else{
					fws[i].val = val;
					++i;
				}
			}
		}
		else{
			int val;
			if( gls[ret]->wait(gls[ret], &val) ){
				fws_remove(fws, ret, &setted);
			}
			else{
				fws[ret].val = val;
			}
		}
	}
}

void glock_await(glock_s* gl){
	int value;
	while( !gl->wait(gl, &value) ){
		glock_wait(gl, value);
	}
}

/*************/
/*** mutex ***/
/*************/

__private int mutex_glock_event(glock_s* mtx, int* waitval){
	unsigned m;
	switch( mtx->state ){
		case 0:
			if( (m = __sync_val_compare_and_swap(&mtx->futex, 0, 1)) ){
				do{
					if( m == 2 || __sync_val_compare_and_swap(&mtx->futex, 1, 2) != 0){
						*waitval = 2;
						mtx->state = 1;
						return 0;
					}
				}while( (m = __sync_val_compare_and_swap(&mtx->futex, 0, 2)) );
			}
		return 1;
			
		case 1:
			while( (m = __sync_val_compare_and_swap(&mtx->futex, 0, 2)) ){
				if( m == 2 || __sync_val_compare_and_swap(&mtx->futex, 1, 2) != 0){
					*waitval = 2;
					return 0;
				}
			}
			mtx->state = 0;
		return 1;
	}

	die("wrong state");
	return 0;
}

glock_s* mutex_ctor(glock_s* mtx, int sharedProcess){
	return glock_ctor(mtx, 0, sharedProcess, mutex_glock_event);
}

int mutex_unlock(glock_s* mtx){
	if( __sync_fetch_and_sub(&mtx->futex, 1) != 1)  {
		mtx->futex = 0;
		glock_wake(mtx);
	}
	return 1;
}

int mutex_lock(glock_s* mtx){
	unsigned m;
	if( (m = __sync_val_compare_and_swap(&mtx->futex, 0, 1)) ){
		do{
			if( m == 2 || __sync_val_compare_and_swap(&mtx->futex, 1, 2) != 0){
				glock_wait(mtx, 2);
			}
		}while( (m = __sync_val_compare_and_swap(&mtx->futex, 0, 2)) );
	}
	return 1;
}

int mutex_trylock(glock_s* mtx){
	if( __sync_val_compare_and_swap(&mtx->futex, 0, 1) ){
		return -1;
	}
	return 0;
}

/*****************/
/*** semaphore ***/
/*****************/

__private int semaphore_glock_event(glock_s* sem, int* waitval){
	while( __sync_bool_compare_and_swap(&sem->futex, 0, 0) ){
		*waitval = 0;
		return 0;
	}
	__sync_fetch_and_sub(&sem->futex, 1);
	return 1;
}

glock_s* semaphore_ctor(glock_s* sem, int val, int sharedProcess){
	return glock_ctor(sem, val, sharedProcess, semaphore_glock_event);
}

void semaphore_post(glock_s* sem){
	if( __sync_fetch_and_add(&sem->futex, 1) == 0 ){
		glock_wake(sem);
	}
}

void semaphore_wait(glock_s* sem){
	while( __sync_bool_compare_and_swap(&sem->futex, 0, 0) ){
		glock_wait(sem, 0);
	}
	__sync_fetch_and_sub(&sem->futex, 1);
}

int semaphore_trywait(glock_s* sem){
	if( __sync_bool_compare_and_swap(&sem->futex, 0, 0) ){
		return -1;
	}
	__sync_fetch_and_sub(&sem->futex, 1);
	return 0;
}

/*************/
/*** event ***/
/*************/

__private int event_glock_event(glock_s* ev, int* waitval){
	while( !__sync_bool_compare_and_swap(&ev->futex, 1, 0) ){
		*waitval = 0;
		return 0;
	}
	return 1;
}


glock_s* event_ctor(glock_s* ev, int sharedProcess){
	return glock_ctor(ev, 0, sharedProcess, event_glock_event);
}

void event_raise(glock_s* ev){
	if( __sync_bool_compare_and_swap(&ev->futex, 0, 1) ){
		glock_wake(ev);
	}
}

void event_broadcast(glock_s* ev){
	if( __sync_bool_compare_and_swap(&ev->futex, 0, 1) ){
		glock_broadcast(ev);
	}
}

void event_wait(glock_s* ev){
	while( !__sync_bool_compare_and_swap(&ev->futex, 1, 0) ){
		glock_wait(ev, 0);
	}
}

void event_clear(glock_s* ev){
	ev->futex = 0;
}

int event_israised(glock_s* ev){
	int ret = __sync_bool_compare_and_swap(&ev->futex, 0, 0);
	return ret;
}

/***************/
/*** eventfd ***/
/***************/

int event_fd(long val, int nonblock){
	int fd = eventfd(val, 0);
	if( fd == -1 ) die("eventfd error:%m");
	if( nonblock ){
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
	return fd;
}

long event_fd_read(int fd){
	uint64_t val;
	if( read(fd, &val, sizeof(uint64_t)) ) return -1;
	return val;
}

void event_fd_write(int fd, uint64_t val){
	write(fd, &val, sizeof(uint64_t));
}

/**************/
/*** thread ***/
/**************/

typedef struct thr{
	pthread_t id;
    pthread_attr_t attr;
	thr_e state;
	glock_s evstop;
	thr_f fn;
	void* arg;
	void* ret;
}thr_t;

__private cpu_set_t* thr_setcpu(unsigned mcpu){
	static cpu_set_t cpu;
	CPU_ZERO(&cpu);
	if (mcpu == 0 ) return &cpu;
	unsigned s;
	while ( (s = mcpu % 10) ){
		CPU_SET(s - 1,&cpu);
		mcpu /= 10;
	}
	return &cpu;
}

__private void* pthr_wrap(void* ctx){
	thr_t* t = ctx;
	mem_acquire_write(t){
		t->state = THR_STATE_RUN;
	}
	
	t->fn(t, t->arg);

	mem_acquire_write(t){
		t->state = THR_STATE_STOP;
	}

	event_broadcast(&t->evstop);
	return t->ret;
}

__private void thr_dtor(thr_t* thr){
	thr_stop(thr);
	pthread_attr_destroy(&thr->attr);
	free(thr);
}

__private int thr_glock_event(glock_s* gl, int* waitval){
	if( !event_glock_event(gl, waitval) ) return 0;
	thr_t* t = gl->ctx;
	thr_e  s = THR_STATE_RUN;
	mem_acquire_read(t){
		s = t->state;
	}
	return s == THR_STATE_STOP ? 1 : 0;
}

thr_t* thr_new(thr_f fn, void* arg, unsigned stackSize, unsigned oncpu, int detach){
	thr_t* thr = NEW(thr_t);
	mem_cleanup(thr, (mcleanup_f)thr_dtor);
	event_ctor(&thr->evstop, 0);
	thr->evstop.wait = thr_glock_event;
	thr->evstop.ctx = thr;
	thr->state = THR_STATE_RUN;
	thr->fn = fn;
	thr->arg = arg;
	thr->ret = NULL;

	pthread_attr_init(&thr->attr);
    if ( stackSize > 0 && pthread_attr_setstacksize(&thr->attr, stackSize) ) die("pthread stack size");
	thr_cpu_set(thr, oncpu);
	if( pthread_attr_setdetachstate(&thr->attr, detach ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE) ) die("pthread detach state");
	if( pthread_create(&thr->id, &thr->attr, pthr_wrap, thr) ) die("pthread create");

	return thr;
}

void thr_cpu_set(thr_t* thr, unsigned cpu){
	if( cpu > 0 ){
		cpu_set_t* ncpu = thr_setcpu(cpu);
		if( pthread_attr_setaffinity_np(&thr->attr, CPU_SETSIZE, ncpu) ) die("pthread set affinity");
	}
}

void thr_wait(thr_t* thr){
	glock_await(&thr->evstop);
}

thr_e thr_check(thr_t* thr){
	thr_e state = THR_STATE_RUN;
	mem_acquire_read(thr){
		state = thr->state;
	}
	return state;
}

void thr_waitv(thr_t** thr, unsigned count){
	for( unsigned i = 0; i < count; ++i ){
		thr_wait(thr[i]);
	}
}

thr_t* thr_anyof(thr_t** thr, unsigned count){
	if( count > GLOCK_MAX_WAITABLE ) die("to many waitable glock");
	glock_s* gth[GLOCK_MAX_WAITABLE];
	for( unsigned i = 0; i < count; ++i ){
		gth[i] = &thr[i]->evstop;
	}
	glock_s* g = glock_anyof(gth, count);
	return g->ctx;
}

void thr_stop(thr_t* thr){
	mem_acquire_write(thr){
		if( thr->state != THR_STATE_STOP ){
			if( pthread_cancel(thr->id) ) die("pthread cancel");
			thr->state = THR_STATE_STOP;
			event_broadcast(&thr->evstop);
		}
	}
}

void thr_yield(void){
	sched_yield();
}

unsigned long thr_id(thr_t* thr){
	return thr->id;
}

void thr_retval(thr_t* thr, void* val){
	thr->ret = val;
}

