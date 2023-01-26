#include <notstd/core.h>
#include <notstd/mth.h>
#include <notstd/vector.h>
#include <notstd/rbhash.h>
#include <notstd/delay.h>

#define FILE_NAME_A "/home/vbextreme/Project/Training/password/2151220-passwords.txt"
#define FILE_COUNT_A 2151220UL

#define FILE_NAME_B "/home/vbextreme/Project/Training/password/crackstation-human-only.txt" //sobig

#define FILE_NAME_C "/home/vbextreme/Project/Training/password/10-million-password-list-top-1000000.txt"
#define FILE_COUNT_C 999999UL

#define FILE_NAME_D "/home/vbextreme/Project/Training/password/test-password.txt"
#define FILE_COUNT_D 100UL

#define FILE_NAME_E "/usr/share/dict/cracklib-small"
#define FILE_COUNT_E 54763UL

#define FILE_TEST       FILE_NAME_E
#define FILE_TEST_COUNT FILE_COUNT_E

__private rbhash_f hfn[] = {
	hash_one_at_a_time,
	hash_fasthash,
	hash_kr,
	hash_sedgewicks,
	hash_sobel,
	hash_weinberger,
	hash_elf,
	hash_sdbm,
	hash_bernstein,
	hash_knuth,
	hash_partow,
	hash_murmur_oaat64,
	hash_murmur_oaat32
};

__private const char* hname[] = {
	"one_at_a_time",
	"fasthash",
	"kr",
	"sedgewicks",
	"sobel",
	"weinberger",
	"elf",
	"sdbm",
	"bernstein",
	"knuth",
	"partow",
	"murmur_oaat64",
	"murmur_oaat32"
};

__private char** load_data(__out size_t* max, const char* fname){
	fprintf(stderr,"loading: ");
	FILE* f = fopen(fname, "r");
	if( !f ) die("on open file: %m");

	if( max ) *max = 0;
	char buf[4096];
	char** vl = VECTOR(char*, 4096);

	size_t n = 0;
	while( fgets(buf, 4096, f) ){
		size_t len = strlen(buf);
		if( max && len > *max ) *max = len;
		if( buf[len-1] == '\n' ) buf[--len] = 0;
		char* line = MANY(char, len+1);
		memcpy(line, buf, len+1);
		vector_push(&vl, &line);
		mem_gift(line, vl);
		printf("\r(%lu)%s                                                              ", n, line);
		++n;
	}

	printf("\rloading: ok                                                  \n");
	fclose(f);
	return vl;
}

//value from 0.95 to 1.05 good result
__private double hash_chi_square(unsigned keyscount, unsigned bucketcount, unsigned* table){
	double n = keyscount;
	double m = bucketcount;
	double sum = 0;
	double b;
	for( unsigned j = 0; j < bucketcount; ++j ){
		b = table[j];
		sum += (b * (b + 1.0)) / 2.0;
	}
	double pa = n / (2.0 * m);
	double pb = n + 2.0 * m - 1;
	return sum / (pa * pb);
}

__private double hash_chi_square_goodness(double chi, double maxchi){
	chi -= 0.95;
	maxchi -= 0.95;
	return 100.0 - ((chi * 100.0)/maxchi);
}

typedef struct htest{
	const char* name;
	double pvalue;
	double time;
	double   medcollisions;
	unsigned maxcollisions;
	unsigned mincollisions;
	unsigned bucketempty;
	unsigned bucketcount;
	unsigned totalscase;
}htest_s;

__private int htcmp(const void* A, const void* B){
	const htest_s* a = A;
	const htest_s* b = B;
	if( mth_approx_eq(a->pvalue, b->pvalue, 0.000000001) ) return 0;
	return a->pvalue > b->pvalue ? 1 : -1;
}

#define NCHI FILE_TEST_COUNT
#define PCHI 20
#define ADDCHI ((NCHI*PCHI)/100)
#define CHI_MAX (NCHI+ADDCHI)
#define HT_NORM(H) ((H)%CHI_MAX)

void uc_chi_square_hash(){
	htest_s test[sizeof_vector(hname)];
	__free unsigned* ht = MANY(unsigned, CHI_MAX);
	__free char** words = load_data(NULL, FILE_TEST);
	const size_t tests = sizeof_vector(hname);
	double worstchi = 0.0;

	fprintf(stderr, "calcolate χ² for (%lu words) hash functions, with %lu bukets: \n", vector_count(&words), CHI_MAX);
	for( size_t t = 0; t < tests; ++t ){
		test[t].name          = hname[t];
		test[t].bucketcount   = CHI_MAX;
		test[t].bucketempty   = 0;
		test[t].maxcollisions = 0;
		test[t].mincollisions = ~0U;
		test[t].totalscase    = vector_count(&words);
		test[t].pvalue        = 0.0;
		test[t].time          = 0;
		test[t].medcollisions = 0.0;
		mem_zero(ht);
		
		double start = time_sec();
		foreach_vector(words, i){
			uint64_t h = hfn[t](words[i], strlen(words[i]));
			++ht[HT_NORM(h)];
		}
		test[t].time = time_sec() - start;
	
		for( unsigned i = 0; i < CHI_MAX; ++i ){
			if( ht[i] > test[t].maxcollisions ) test[t].maxcollisions = ht[i];
			if( ht[i] < test[t].mincollisions ) test[t].mincollisions = ht[i];
			if( !ht[i] ){
				++test[t].bucketempty;
			}
			else{
				test[t].medcollisions += ht[i];
			}
		}
		
		double used = test[t].bucketcount - test[t].bucketempty;
		test[t].medcollisions /= used;
		test[t].pvalue = hash_chi_square(test[t].totalscase, CHI_MAX, ht);
		if( test[t].pvalue > worstchi ) worstchi = test[t].pvalue;
	}

	qsort(test, tests, sizeof(htest_s), htcmp);
	for( size_t t = 0; t < tests; ++t ){
		fprintf(stderr, "%13s: χ²:%.8f collision max:%4u min:%2u med:%.4f empty:%4u(%.2f%%) time:%.4fs goodness: %.4f%%\n", 
			test[t].name, 
			test[t].pvalue, 
			test[t].maxcollisions,
			test[t].mincollisions,
			test[t].medcollisions,
			test[t].bucketempty,
			test[t].bucketempty * 100.0 / test[t].bucketcount,
			test[t].time,
			hash_chi_square_goodness(test[t].pvalue, worstchi)
		);
	}
}

__private int run_hash(char** vdata, size_t max, const char* fnname, rbhash_f fnh){
	const size_t lines = vector_count(&vdata);
	__free rbhash_t* rbh = rbhash_new(4096, 10, max, fnh);
	
	double st = time_sec();	
	for( size_t i = 0; i < lines; ++i ){
		rbhash_add(rbh, vdata[i], strlen(vdata[i]), vdata[i]);
	}
	double en = time_sec();
	for( size_t i = 0; i < lines; ++i ){
		char* str = rbhash_find(rbh, vdata[i], strlen(vdata[i]));
		if( !str || strcmp(str, vdata[i]) ){
			return -1;
		}
	}

	printf("%13s: totalTime:%.5fs collision:%lu/%lu(%.2f%%) distance:%lu\n", 
		fnname, en-st, 
		rbhash_collision(rbh), rbhash_bucket_count(rbh), (rbhash_collision(rbh)*100.0)/rbhash_bucket_count(rbh), 
		rbhash_distance_max(rbh)
	);
	return 0;
}

void uc_rbhash(void){
	size_t maxlen;
	__free char** words = load_data(&maxlen, FILE_TEST);
	const size_t tests = sizeof_vector(hname);
	for( unsigned i = 0; i < tests; ++i ){
		if( run_hash(words, maxlen, hname[i], hfn[i]) ) die("%s fail", hname[i]);
	}
}

