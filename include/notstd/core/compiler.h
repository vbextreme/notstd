#ifndef __NOTSTD_CORE_COMPILER_H__
#define __NOTSTD_CORE_COMPILER_H__

#define __error(STR) DO_PRAGMA(GCC error STR)
#define __warning(STR) DO_PRAGMA(GCC warning STR)

#ifdef __unused
#undef __unused
#endif
#ifdef __packed
#undef __packed
#endif
#ifdef __const
#undef __const
#endif
#ifdef __weak
#undef __weak
#endif

#define __out
#define __atomic _Atomic
#define __private static
#define __unused            __attribute__((unused))
#define __cleanup(FNC)      __attribute__((__cleanup__(FNC)))
#define __printf(FRMT,VA)   __attribute__((format(printf, FRMT, VA)))
#define __scanf(FRMT,VA)   __attribute__((format(scanf, FRMT, VA)))
#define __const             __attribute__((const))
#define __packed            __attribute__((packed))
#define __weak              __attribute__((weak))
#define __noinline          __attribute__((noinline))
#define __sectiona(SEC, CS) __attribute__((section(SEC "." EXPAND_STRING(CS))))
#define __aligneda(N)       __attribute__((aligned(4)))
#define __noreturn          __attribute__((noreturn))
#define __constructor       __attribute__((constructor))
#define __destructor        __attribute__((destructor))
#define __isaligned(V,A)    __builtin_assume_aligned(V,A)
#define __malloc            __attribute__((malloc))
#define __cpu_init()        __builtin_cpu_init()
#define __resolver(NAME)    __attribute__((ifunc(#NAME)))
#define __constructor_priority(P) __attribute__((constructor(P)))
#define __destructor_priority(P)  __attribute__((destructor(P)))

#define _TOR_PRIORITY        101
#define _TOR_PRIORITY_CORE   64

#define __ctor         __constructor_priority(_TOR_PRIORITY + _TOR_PRIORITY_CORE)
#define __ctor_prio(N) __constructor_priority(_TOR_PRIORITY + _TOR_PRIORITY_CORE + (N))

#define __dtor      __destructor_priority(_TOR_PRIORITY + _TOR_PRIORITY_CORE)
#define __dtor_prio(N) __destructor_priority(_TOR_PRIORITY + _TOR_PRIORITY_CORE + (N))

#if defined OMP_ENABLE && !defined __clang__
	#define __parallef DO_PRAGMA(omp parallel for)
	#define __parallefc(Z) DO_PRAGMA(omp parallel for collapse Z)
#else
	#define __parallef
	#define __parallefc(Z)
#endif

#ifdef __cplusplus
#	define __extern_begin extern "C" {
#	define __extern_end }
#else
#	define __extern_begin
#	define __extern_end
#endif

#define DO_PRAGMA(DOP) _Pragma(#DOP)
#define UNSAFE_BEGIN(FLAGS) DO_PRAGMA(GCC diagnostic push); DO_PRAGMA(GCC diagnostic ignored FLAGS)
#define UNSAFE_END DO_PRAGMA(GCC diagnostic pop)

#define EXPAND_STRING(ES) #ES

#define ADDR(VAR) ((uintptr_t)(VAR))
#define ADDRTO(VAR, SO, I) ( ADDR(VAR) + ((SO)*(I)))

#define OS_PAGE_SIZE sysconf(_SC_PAGESIZE)

#define sizeof_vector(V) (sizeof(V) / sizeof(V[0]))

#define forever() for(;;)

#define cpu_relax() __builtin_ia32_pause()

#endif
