#include <notstd/map.h>
#include <notstd/rbtree.h>
#include <notstd/mth.h>
#include <notstd/vector.h>

__private int cmp(const void* a, const void* b){
	return (int)(uintptr_t)a - (int)(uintptr_t)b;
}

__private int mapp(void* data, __unused void* arg){
	printf("%d\n", (int)(uintptr_t)(rbtree_node_data(data)));
	return 0;
}
#define N 8

void uc_rbtree(){
	__free rbtree_t* t = rbtree_new(cmp);
	__free int* val = VECTOR(int,N*2);
	dbg_info("insert");
	for( unsigned i = 0; i < N; ++i ){
		int num = mth_random(256);
		*(int*)vector_push(&val, NULL) = num;
		rbtree_insert(t, mem_gift(rbtree_node_new((void*)(uintptr_t)num), t));
		printf("insert: %d\n", num);
	}

	puts("iterate");
	map(rbtree_inorder, t, mapp, NULL, 0, 0);
	
	dbg_info("search");
	for( unsigned i = 0; i < N; ++i ){
		if( !rbtree_find(t, (void*)(uintptr_t)val[i]) ) die("try find element %d but not exists", val[i]);
	}

	dbg_info("delete");
	rbtNode_t* node = rbtree_find(t, (void*)(uintptr_t)val[N/2]);
	mem_free(mem_give(rbtree_remove(t, node), t));

	if( rbtree_find(t, (void*)(uintptr_t)val[N/2]) ) die("find element but element is removed");

}
