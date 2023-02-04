#include <notstd/core.h>

#ifndef TEST_DATASTRUCTURE
#define TEST_DATASTRUCTURE 0xFFFF
#endif

const unsigned MODE=TEST_DATASTRUCTURE;

void uc_vector();
void uc_ls();
void uc_ld();
void uc_chi_square_hash();
void uc_rbhash(void);
void uc_fzs(void);
void uc_fzs_inp(void);
void uc_fzs_benchmark(void);
void uc_phq(void);
void uc_rbtree();
void uc_dict(void);
void uc_trie(void);

int main(){
	if( MODE & 0x0001 ) uc_vector();
	if( MODE & 0x0002 ) uc_ls();
	if( MODE & 0x0004 ) uc_ld();
	if( MODE & 0x0008 ) uc_chi_square_hash();
	if( MODE & 0x0010 ) uc_rbhash();
	if( MODE & 0x0020 ) uc_fzs();
	if( MODE & 0x0040 ) uc_fzs_inp();
	if( MODE & 0x0080 ) uc_fzs_benchmark();
	if( MODE & 0x0100 ) uc_phq();
	if( MODE & 0x0200 ) uc_rbtree();
	if( MODE & 0x0400 ) uc_dict();
	if( MODE & 0x0800 ) uc_trie();
	return 0;
}

