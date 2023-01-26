#include <notstd/core.h>

#ifndef TEST_DATASTRUCTURE
#define TEST_DATASTRUCTURE 0xFF
#endif

const int MODE=TEST_DATASTRUCTURE;

void uc_vector();
void uc_ls();
void uc_ld();
void uc_chi_square_hash();
void uc_rbhash(void);
void uc_fzs(void);
void uc_fzs_inp(void);

int main(){
	if( MODE & 0x01 ) uc_vector();
	if( MODE & 0x02 ) uc_ls();
	if( MODE & 0x04 ) uc_ld();
	if( MODE & 0x08 ) uc_chi_square_hash();
	if( MODE & 0x10 ) uc_rbhash();
	if( MODE & 0x20 ) uc_fzs();
	if( MODE & 0x40 ) uc_fzs_inp();
}
