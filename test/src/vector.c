#include <notstd/vector.h>
#include <notstd/mth.h>

int dcre(const void* A, const void* B){
	return *(const int*)(A) - *(const int*)(B);
}

void uc_vector(){
	mth_random_begin();
	__free int* arr = VECTOR(int, 4);
	for( unsigned i = 0; i < 16; ++i ){
		int r = mth_random(4096);
		vector_push(&arr, &r);
	}

	dbg_info("unsorted");
	foreach_vector(arr, i){
		dbg_info("%d", arr[i]);
	}
	dbg_info("");
	
	dbg_info("sort");
	vector_qsort(&arr, dcre);
	foreach_vector(arr, i){
		dbg_info("%d", arr[i]);
	}
	dbg_info("");

	dbg_info("shuffle");
	vector_shuffle(&arr, 0, vector_count(&arr)-1);
	foreach_vector(arr, i){
		dbg_info("%d", arr[i]);
	}
	dbg_info("");

	dbg_info("new vector");
	__free int* v = VECTOR(int, 10);
	for( unsigned i = 0; i < 16; ++i ){
		vector_push(&v, &i);
	}
	foreach_vector(v, it) printf("%d, ", v[it]);
	puts("");

	dbg_info("remove first element");
	vector_remove(&v, 0, 1);
	foreach_vector(v, it) printf("%d, ", v[it]);
	puts("");





}
