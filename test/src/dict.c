#include <notstd/dict.h>

__private int pair(generic_s key, generic_s* value, __unused void* arg){
	g_print(key);
	fputs(": ", stdout);
	g_print(*value);
	putchar('\n');	
	return 0;
}

void uc_dict(void){
	dict_t* d = dict_new();
	
	*dict(d, "key") = GI("value");
	*dict(d, "key2") = GI("value2");
	*dict(d, 1) = GI("one");
	*dict(d, 2) = GI("two");
	*dict(d, 3) = GI("three");
	*dict(d, "float") = GI(4.2);
	*dict(d, "signed") = GI(4);
	dict(d, "unset value");

	map_dict(d, pair, NULL);
}
