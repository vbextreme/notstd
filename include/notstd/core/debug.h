#ifndef __NOTSTD_CORE_DEBUG_H__
#define __NOTSTD_CORE_DEBUG_H__

#ifndef DBG_OUTPUT
#	define DBG_OUTPUT stderr
#endif

#ifdef DBG_COLOR
#	define DBG_COLOR_INFO    "\033[36m"
#	define DBG_COLOR_WARNING "\033[93m"
#	define DBG_COLOR_ERROR   "\033[31m"
#	define DBG_COLOR_FAIL    "\033[91m"
#	define DBG_COLOR_RESET   "\033[m"
#else
#	define DBG_COLOR_INFO    ""
#	define DBG_COLOR_WARNING ""
#	define DBG_COLOR_ERROR   ""
#	define DBG_COLOR_FAIL    ""
#	define DBG_COLOR_RESET   ""
#endif

#ifndef DBG_INFO
#	define DBG_INFO    "info"
#endif
#ifndef DBG_WARNING
#	define DBG_WARNING "warning"
#endif
#ifndef DBG_ERROR
#	define DBG_ERROR   "error"
#endif
#ifndef DBG_FAIL
#	define DBG_FAIL    "fail"
#endif

#ifndef DBG_LVL_FAIL
#	define DBG_LVL_FAIL    1
#endif
#ifndef DBG_LVL_ERROR
#	define DBG_LVL_ERROR   2
#endif
#ifndef DBG_LVL_WARNING
#	define DBG_LVL_WARNING 3
#endif
#ifndef DBG_LVL_INFO
#	define DBG_LVL_INFO    4
#endif

#if DBG_ENABLE >= 1
#	if DBG_SHORTPATH
#		define dbg_shortpath(PATH) ({\
			size_t len = strlen(PATH);\
			const char* begin = PATH;\
			const char* path = &begin[len-1];\
			while( *path != '/' && path > begin ) --path;\
			if( *path == '/' ) ++path;\
			path;\
		})
#		define DBG_SHORTPATH_PREC "16"
#	else
#		define dbg_shortpath(PATH) PATH
#		define DBG_SHORTPATH_PREC
#	endif

/*- dbg log on DBG_OUTPUT, default stderr
 * @>0 'display type of dbg'
 * @>1 'if tty set type with color DBG_COLOR_'
 * @>2 'same printf'
 * @>3 'same printf'
 */
#	define dbg(TYPE, COLOR, FORMAT, arg...) do{\
		int __tty__ = isatty(fileno(DBG_OUTPUT));\
		const char* __co__ = __tty__ ? COLOR : "" ;\
		const char* __re__ = __tty__ ? DBG_COLOR_RESET : "";\
		fprintf(DBG_OUTPUT, "%s%s%s %" DBG_SHORTPATH_PREC "s[%4u]:{0x%lX} %s():: " FORMAT "\n",\
			__co__, TYPE, __re__,\
			dbg_shortpath(__FILE__),\
			__LINE__,\
			pthread_self(),\
			__FUNCTION__,\
			## arg\
		);\
		fflush(DBG_OUTPUT);\
	}while(0)

/*- print message fail and exit from application*/
	#define dbg_fail(FORMAT, arg...) do{ \
		dbg(DBG_FAIL, DBG_COLOR_FAIL, FORMAT, ## arg);\
		exit(1);\
	}while(0)
#else
	#define dbg(TYPE, FORMAT, arg...)
	#define dbg_fail(FORMAT, arg...) do{exit(1);}while(0)
#endif

#if DBG_ENABLE > DBG_LVL_FAIL
/*- print error message */
	#define dbg_error(FORMAT, arg...) dbg(DBG_ERROR, DBG_COLOR_ERROR, FORMAT, ## arg)
	#define dbg_errno(FORMAT, arg...) dbg_error(FORMAT "(%d):%s", ##arg, errno, err_str()) 
#else
	#define dbg_error(FORMAT, arg...)
	#define dbg_errno(FORMAT, arg...)
#endif

#if DBG_ENABLE > DBG_LVL_ERROR
/*- print warning message */
	#define dbg_warning(FORMAT, arg...) dbg(DBG_WARNING, DBG_COLOR_WARNING, FORMAT, ## arg)
#else
	#define dbg_warning(FORMAT, arg...)
#endif

#if DBG_ENABLE > DBG_LVL_WARNING
/*- print info */
	#define dbg_info(FORMAT, arg...) dbg(DBG_INFO, DBG_COLOR_INFO, FORMAT, ## arg)
#else
	#define dbg_info(FORMAT, arg...)
#endif
	
#if ASSERT_ENABLE == _Y_
/*- assert @>0 'expression' */
	#define iassert(C) do{ if ( !(C) ){fprintf(stderr,"assertion fail %s[%u]: %s(%s)\n", __FILE__, __LINE__, __FUNCTION__, #C); fflush(stderr); exit(0);}}while(0)
#else
	#define iassert(C) 
#endif

/*- static assert 
 * @>0 'expression' 
 * @>1 'message'
 */
#define iassert_static(EX, MSG) _Static_assert(EX, MSG)




#endif
