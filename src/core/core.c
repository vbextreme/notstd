#include <notstd/core.h>

__ctor void notstd_begin(void){
	page_begin();
	//deadpoll_begin();
}

__dtor void notstd_end(void){
	//deadpoll_end();
}
