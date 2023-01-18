#ifndef __NOTSTD_CORE_H__
#define __NOTSTD_CORE_H__

#if defined __x86_64__
#	ifndef _GNU_SOURCE
#		define _GNU_SOURCE
#	endif
#	ifndef _XOPEN_SPURCE
#		define _XOPEN_SPURCE 600
#	endif
#	include <unistd.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <pthread.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <stdatomic.h>

#include <notstd/core/compiler.h>
#include <notstd/core/type.h>
#include <notstd/core/xmacro.h>
#include <notstd/core/mathmacro.h>
#include <notstd/core/debug.h>
#include <notstd/core/error.h>
#include <notstd/core/memory.h>


#endif
