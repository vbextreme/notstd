#include <notstd/dict.h>

void uc_dict(void){
	dict_t* d = dict_new();
	
	*dict(d, "key") = GI("value");
	*dict(d, "key2") = GI("value2");
	*dict(d, 1) = GI("one");
	*dict(d, 2) = GI("two");
	*dict(d, 3) = GI("three");

}
