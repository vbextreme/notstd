#include <notstd/lbuffer.h>

__private const char* DATA = "`1234567890-=qwertyuiop[]asdfghjkl;'zxcvbnm,./~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:ZXCVBNM<>?";
__private unsigned AVDATA;

__private unsigned fkfill(lbuffer_t* lb, unsigned len){
	const unsigned max = strlen(DATA);
	if( AVDATA >= max ) return 0;
	unsigned nr = 0;


	while( len && AVDATA < max ){
		unsigned available;
		char* fill = lbuffer_fill(lb, &available);
		iassert(fill);
		//dbg_info("available in fill %u", available);
		if( available > max - AVDATA ) available = max-AVDATA;
		//dbg_info("device size: %u", max - AVDATA);
		if( available > len ) available = len;
		//dbg_info("fill %u in buffer", available);
		memcpy(fill, &DATA[AVDATA], available);
		lbuffer_fill_commit(lb, available);
		len -= available;
		nr += available;
		AVDATA += available;
		//dbg_info("remain %u", len);
	}
	return nr;
}

__private unsigned fkread(lbuffer_t* lb, void* buf, unsigned len){
	unsigned nr = 0;
	char* rd;
	unsigned available;
	while( len && (rd=lbuffer_read(lb, &available)) ){
		//dbg_info("available in ready %u", available);
		if( available > len ) available = len;
		//dbg_info("fill %u in buffer", available);
		memcpy(buf, rd, available);
		lbuffer_read_commit(lb, available);
		len -= available;
		nr += available;
		//dbg_info("remain %u", len);
	}
	return nr;
}

void uc_lbuffer(void){
	dbg_info("start");

	__free lbuffer_t* lb = lbuffer_new(8);
	while( fkfill(lb, 16) ){
		char buf[32];
		unsigned nr = fkread(lb, buf, 4);
		buf[nr] = 0;
		puts(buf);
	}

	unsigned nr;
	char buf[32];
	while( (nr=fkread(lb, buf, 12)) ){
		buf[nr] = 0;
		puts(buf);
	}

}
