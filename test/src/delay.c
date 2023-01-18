#include <notstd/delay.h>

int uc_delay(){
	delay_t start, end;
	delay_t t;

	t=1;
	dbg_info("ns delay %lus", t);
	start = time_ns();
	delay_ns(SECTONS(t));
	end   = time_ns();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-SECTONS(t));
	
	t=32;
	dbg_info("ns delay %lums", t);
	start = time_ns();
	delay_ns(MSTONS(t));
	end   = time_ns();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-MSTONS(t));
	
	t=77;
	dbg_info("ns delay %luus", t);
	start = time_ns();
	delay_ns(USTONS(t));
	end   = time_ns();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-USTONS(t));
	
	t=500;
	dbg_info("ns delay %luns", t);
	start = time_ns();
	delay_ns(t);
	end   = time_ns();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-t);
	
	t=1;
	dbg_info("us delay %lus", t);
	start = time_us();
	delay_us(SECTOUS(t));
	end   = time_us();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-SECTOUS(t));
	
	t=32;
	dbg_info("us delay %lums", t);
	start = time_us();
	delay_us(MSTOUS(t));
	end   = time_us();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-MSTOUS(t));
	
	t=500;
	dbg_info("us delay %luus", t);
	start = time_us();
	delay_us(t);
	end   = time_us();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-t);

	t=1;
	dbg_info("ms delay %lus", t);
	start = time_ms();
	delay_ms(SECTOMS(t));
	end   = time_ms();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-SECTOMS(t));
	
	t=500;
	dbg_info("ms delay %lums", t);
	start = time_ms();
	delay_ms(t);
	end   = time_ms();
	dbg_info("\tdelay: %lu jitter: %lu", end-start, (end-start)-t);

	return 0;
}

int main(){
	uc_delay();
	return 0;
}
