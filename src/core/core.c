#include <notstd/core.h>
#include <notstd/mth.h>

__ctor void notstd_begin(void){
	mth_random_begin();
	page_begin();
	//deadpoll_begin();
}

__dtor void notstd_end(void){
	//deadpoll_end();
}
