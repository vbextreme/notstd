#include <notstd/dict.h>

__private int pair(generic_s key, generic_s* value, __unused void* arg){
	if( key.type == G_LONG ){
		printf("%ld: %s\n", key.l, value->lstr);
	}
	else{
		printf("%s: %s\n", key.lstr, value->lstr);
	}
	return 0;
}

void uc_dict(void){
	dict_t* d = dict_new();
	
	*dict(d, "key") = GI("value");
	*dict(d, "key2") = GI("value2");
	*dict(d, 1) = GI("one");
	*dict(d, 2) = GI("two");
	*dict(d, 3) = GI("three");

	map_dict(d, pair, NULL);
}
