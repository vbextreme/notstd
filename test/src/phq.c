#include <notstd/phq.h>

__private int qcmp(const void* a, const void* b){
	return *(const phqPriority_t*)b - *(const phqPriority_t*)a;
}

__private void test_phq_des(void){
	phqPriority_t lstp[] = {7, 2, 9, 1, 3, 8, 6};
	phqPriority_t tstp[] = {0, 0, 0, 0, 0, 0, 0};

	dbg_warning("test phq des");
	
	dbg_info("test insert");
	__free phq_t* q = phq_new(6, phq_cmp_des, 0);
	phq_dump(q);
	puts("");

	for( unsigned i = 0; i < sizeof_vector(lstp); ++i ){
		printf("insert %zu\n", lstp[i]);
		phq_insert(q, mem_gift(phq_element_new(lstp[i],NULL), q));
		phq_dump(q);
	}

	dbg_info("test pop");
	memcpy(tstp, lstp, sizeof(lstp));
	qsort(tstp, sizeof_vector(tstp), sizeof(phqPriority_t), qcmp); 
	
	unsigned it = 0;
	phqElement_t* e;
	while( (e=phq_pop(q)) ){
		if( it >= sizeof_vector(tstp) ) die("out of bounds");
		if( tstp[it] != phq_element_priority(e) ) die("wrong priority %zu != %zu", tstp[it], phq_element_priority(e));
		printf("[%2u]%2zu\n", it, phq_element_priority(e));
		++it;
		phq_dump(q);
		mem_free(mem_give(e, q));
	}

	dbg_info("test change priority 7 -> 77");
	phqElement_t* e7 = NULL;
	for( unsigned i = 0; i < sizeof_vector(lstp); ++i ){
		if( lstp[i] != 7 ){
			phq_insert(q, mem_gift(phq_element_new(lstp[i],NULL), q));
		}
		else{
			e7 = phq_element_new(lstp[i], NULL);
			phq_insert(q, mem_gift(e7, q));
		}
	}
	phq_dump(q);
	phq_change_priority(q, 77, e7);
	phq_dump(q);
	
	dbg_info("test remove 77");
	mem_free(mem_give(phq_remove(q, e7), q));
	phq_dump(q);
}

__private void test_phq_taw_asc(void){
	phqPriority_t lstp[] = {1, 2, 3, 4};

	dbg_warning("test phq taw asc");

	dbg_info("test insert");
	__free phq_t* q = phq_new(6, phq_cmp_taw_asc, 1);

	for( unsigned i = 0; i < sizeof_vector(lstp) / 2; ++i ){
		printf("insert %zu\n", lstp[i]);
		phq_insert(q, mem_gift(phq_element_new(lstp[i],NULL), q));
		phq_dump(q);
	}
	if( phq_taw_switch(q) ) die("taw_switch: %m");
	for( unsigned i = sizeof_vector(lstp) / 2; i < sizeof_vector(lstp) ;++i ){
		printf("insert tawned %zu\n", lstp[i]);
		phq_insert(q, mem_gift(phq_element_new(lstp[i],NULL), q));
		phq_dump(q);
	}

	dbg_info("test pop taw: %u", !!phq_taw_current(q));
	unsigned it = 0;
	phqElement_t* e;
	while( (e=phq_pop(q)) ){
		if( it >= sizeof_vector(lstp) ) die("out of bounds");
		printf("[%2u](%u)%2zu\n", it, !!phq_element_taw(e), phq_element_priority(e));
		++it;
		mem_free(mem_give(e, q));
	}

	dbg_info("test pop and insert taw");
	if( phq_taw_switch(q) ) die("taw_switch: %m");
	for( unsigned i = 0; i < sizeof_vector(lstp) / 2; ++i ){
		printf("insert %zu\n", lstp[i]);
		phq_insert(q, mem_gift(phq_element_new(lstp[i],NULL), q));
	}
	if( phq_taw_switch(q) ) die("taw_switch: %m");
	for( unsigned i = sizeof_vector(lstp) / 2; i < sizeof_vector(lstp) ;++i ){
		printf("insert tawned %zu\n", lstp[i]);
		phq_insert(q, mem_gift(phq_element_new(lstp[i],NULL), q));
	}

	phq_dump(q);
	e = phq_pop(q);
	printf("popped %zu\n", phq_element_priority(e));
	phq_insert(q, e);
	phq_dump(q);
	iassert(phq_taw_switch(q) && errno == EPERM);

}

void uc_phq(void){
	test_phq_des();
	test_phq_taw_asc();
}








