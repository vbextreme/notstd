#include <notstd/dict.h>
#include <notstd/map.h>

__private int pair(void* KV, __unused void* arg){
	dictPair_s* kv = KV;
	g_print(kv->key);
	fputs(": ", stdout);
	g_print(kv->value);
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

	puts("map");
	map(dict, d, pair, NULL, 0, 0);

	puts("foreach");
	dictPair_s* kv;
	foreach(dict, d, kv, 0, 0){
		pair(kv, NULL);
	}
}
