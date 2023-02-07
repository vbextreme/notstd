#include <notstd/bipbuffer.h>

__private const char* DATA = "`1234567890-=qwertyuiop[]asdfghjkl;'zxcvbnm,./~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:ZXCVBNM<>?";
__private unsigned AVDATA;

__private unsigned fkfill(bipbuffer_t* bb, unsigned len){
	const unsigned max = strlen(DATA);
	if( AVDATA >= max ) return 0;
	unsigned nr = 0;


	while( len && AVDATA < max ){
		unsigned available;
		char* fill = bipbuffer_fill(bb, &available);
		if( !available ) break;
		//dbg_info("available in fill %u", available);
		if( available > max - AVDATA ) available = max-AVDATA;
		//dbg_info("device size: %u", max - AVDATA);
		if( available > len ) available = len;
		//dbg_info("fill %u in buffer", available);
		memcpy(fill, &DATA[AVDATA], available);
		bipbuffer_fill_commit(bb, available);
		len -= available;
		nr += available;
		AVDATA += available;
		//dbg_info("remain %u", len);
	}

	return nr;
}

__private unsigned fkread(bipbuffer_t* bb, void* buf, unsigned len){
	unsigned nr = 0;
	char* rd;
	unsigned available;
	while( len && (rd=bipbuffer_read(bb, &available)) && available ){
		//dbg_info("available in ready %u", available);
		if( available > len ) available = len;
		//dbg_info("write %u in buffer", available);
		memcpy(buf, rd, available);
		bipbuffer_read_commit(bb, available);
		len -= available;
		nr += available;
		//dbg_info("remain %u", len);
	}
	return nr;
}

void uc_bipbuffer(void){
	dbg_info("start");

	__free bipbuffer_t* lb = bipbuffer_new(sizeof(char), 64);
	while( fkfill(lb, 16) ){
		char buf[32];
		unsigned nr = fkread(lb, buf, 4);
		buf[nr] = 0;
		puts(buf);
	}
	//dbg_info("end of fill");
	unsigned nr;
	char buf[32];
	while( (nr=fkread(lb, buf, 12)) ){
		buf[nr] = 0;
		puts(buf);
	}

}
