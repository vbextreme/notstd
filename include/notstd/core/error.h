#ifndef __NOTSTD_CORE_ERROR_H__
#define __NOTSTD_CORE_ERROR_H__

#include <errno.h>

#define ERR_ERRNO 0
#define ERR_CORE  1
#define ERR_TERM  2
#define ERR_GUI   3
#define ERR_USR   4
#define ERR_PAGE  4
#define ERR_MAX   64

#define ECORE_WRONG_ERROR   0

#define ERR(PAGE, NUM) (((PAGE)<<8)|(NUM))

#define ereturn(ERR) do{ errno = ERR; return -1; }while(0)
#define nreturn(ERR) do{ errno = ERR; return NULL; }while(0)

#define ERR_COLOR       "\033[31m"
#define ERR_COLOR_INFO  "\033[36m"
#define ERR_COLOR_RESET "\033[m"

#define err(FORMAT, arg...) do{\
	int __tty__ = isatty(fileno(stderr));\
	const char* __ce__ = __tty__ ? ERR_COLOR : "";\
	const char* __ci__ = __tty__ ? ERR_COLOR_INFO : "";\
	fprintf(stderr, "%serror:%s " FORMAT "\n", __ce__, __ci__, ## arg); \
	if( __tty__ ) fputs(ERR_COLOR_RESET, stderr);\
}while(0)

#define ern(FORMAT, arg...) do{\
	int __tty__ = isatty(fileno(stderr));\
	const char* __ce__ = __tty__ ? ERR_COLOR : "";\
	const char* __ci__ = __tty__ ? ERR_COLOR_INFO : "";\
	fprintf(stderr, "%serror:%s " FORMAT , __ce__, __ci__, ## arg);\
	if( __tty__ ) fputs(ERR_COLOR_RESET, stderr);\
	fprintf(stderr, " (%d)::%s\n", errno, err_str() );\
}while(0)


/*- stop software and exit arguments is same printf */
#if DBG_ENABLE > 0
#define die(FORMAT, arg...) do{\
	int __tty__ = isatty(fileno(stderr));\
	const char* __ce__ = __tty__ ? ERR_COLOR : "";\
	const char* __ci__ = __tty__ ? ERR_COLOR_INFO : "";\
	fprintf(stderr, "%sdie%s at %s(%u) " FORMAT "\n", __ce__, __ci__, __FILE__, __LINE__, ## arg);\
	if( __tty__ ) fputs(ERR_COLOR_RESET, stderr);\
	fflush(stderr);\
	fsync(3);\
	exit(1);\
}while(0)
#else
#define die(FORMAT, arg...) do{\
	int __tty__ = isatty(fileno(stderr));\
	const char* __ce__ = __tty__ ? ERR_COLOR : "";\
	const char* __ci__ = __tty__ ? ERR_COLOR_INFO : "";\
	fprintf(stderr, "%sdie%s " FORMAT "\n", __ce__, __ci__, ## arg);\
	if( __tty__ ) fputs(ERR_COLOR_RESET, stderr);\
	exit(1);\
}while(0)
#endif

int err_str_set(unsigned page, unsigned err, const char* str);
const char* err_str_num(int err);
const char* err_str(void);
void err_showline(const char* begin, const char* errs, int wordmode);

typedef enum{
	ERR_ARG_LIST_TOO_LONG                               = E2BIG,
	ERR_PERMISSION_DENIED                               = EACCES,
	ERR_ADDRESS_ALREADY_IN_USE                          = EADDRINUSE,
	ERR_ADDRESS_NOT_AVAILABLE                           = EADDRNOTAVAIL,
	ERR_ADDRESS_FAMILY_NOT_SUPPORTED                    = EAFNOSUPPORT,
	ERR_RESOURCE_TEMPORARILY_UNAVAILABLE                = EAGAIN,
	ERR_CONNECTION_ALREADY_IN_PROGRESS                  = EALREADY,
	ERR_INVALID_EXCHANGE                                = EBADE,
	ERR_BAD_FILE_DESCRIPTOR                             = EBADF,
	ERR_FILE_DESCRIPTOR_IN_BAD_STATE                    = EBADFD,
	ERR_BAD_MESSAGE                                     = EBADMSG,
	ERR_INVALID_REQUEST_DESCRIPTR                       = EBADR,	
	ERR_INVALID_REQUEST_CODE                            = EBADRQC,
	ERR_INVALID_SLOT                                    = EBADSLT,
	ERR_DEVICE_OR_RESOURCE_BUSY                         = EBUSY,
	ERR_OPERATION_CANCELED                              = ECANCELED,
	ERR_NO_CHILD_PROCESSES                              = ECHILD,
	ERR_CHANNEL_NUMBER_OUT_OF_RANGE                     = ECHRNG,
	ERR_COMMUNICATION_ERROR_ON_SEND                     = ECOMM,
	ERR_CONNECTION_ABORTED                              = ECONNABORTED,
	ERR_CONNECTION_REFUSED                              = ECONNREFUSED,
	ERR_CONNECTION_RESET                                = ECONNRESET,
	ERR_RESOURCE_DEADLOCK_AVOIDED                       = EDEADLK,
	ERR_ON_MOST_ARCHITECTURES                           = EDEADLOCK,
	ERR_DESTINATION_ADDRESS_REQUIRED                    = EDESTADDRREQ,
	ERR_MATHEMATICS_ARGUMENT_OUT_OF_DOMAIN_OF_FUNCTION  = EDOM,
	ERR_DISK_QUOTA_EXCEEDED                             = EDQUOT,
	ERR_FILE_EXISTS                                     = EEXIST,
	ERR_BAD_ADDRESS                                     = EFAULT,
	ERR_FILE_TOO_LARGE                                  = EFBIG,
	ERR_HOST_IS_DOWN                                    = EHOSTDOWN,
	ERR_HOST_IS_UNREACHABLE                             = EHOSTUNREACH,
	ERR_MEMORY_PAGE_HAS_HARDWARE_ERROR                  = EHWPOISON,
	ERR_IDENTIFIER_REMOVED                              = EIDRM,
	ERR_INVALID_OR_INCOMPLETE_MULTIBYTE_OR_WIDE_CHAR    = EILSEQ,
	ERR_OPERATION_IN_PROGRESS                           = EINPROGRESS,
	ERR_INTERRUPTED_FUNCTION_CALL                       = EINTR,
	ERR_INVALID_ARGUMENT                                = EINVAL,
	ERR_IO_ERROR                                        = EIO,
	ERR_SOCKET_IS_CONNECTED                             = EISCONN,
	ERR_IS_A_DIRECTORY                                  = EISDIR,
	ERR_IS_A_NAMED_TYPE_FILE                            = EISNAM,
	ERR_KEY_HAS_EXPIRED                                 = EKEYEXPIRED,
	ERR_KEY_WAS_REJECTED_BY_SERVICE                     = EKEYREJECTED,
	ERR_KEY_HAS_BEEN_REVOKED                            = EKEYREVOKED,
	ERR_LEVEL2_HALTED                                   = EL2HLT,
	ERR_LEVEL2_NOT_SYNCHRONIZED                         = EL2NSYNC,
	ERR_LEVEL3_HALTED                                   = EL3HLT,
	ERR_LEVEL3_RESET                                    = EL3RST,
	ERR_CANNOT_ACCESS_A_NEEDED_SHARED_LIBRARY           = ELIBACC,
	ERR_ACCESSING_A_CORRUPTED_SHARED_LIBRARY            = ELIBBAD,
	ERR_ATTEMPTING_TO_LINK_IN_TOO_MANY_SHARED_LIBRARIES = ELIBMAX,
	ERR_LIB_SECTION_CORRUPTED                           = ELIBSCN,
	ERR_CANNOT_EXEC_A_SHARED_LIBRARY_DIRECTLY           = ELIBEXEC,
	ERR_TOO_MANY_LEVELS_OF_SYMBOLIC_LINKS               = ELOOP,
	ERR_WRONG_MEDIUM_TYPE                               = EMEDIUMTYPE,
	ERR_TOO_MANY_OPEN_FILES                             = EMFILE,
	ERR_TOO_MANY_LINKS                                  = EMLINK,
	ERR_MESSAGE_TOO_LONG                                = EMSGSIZE,
	ERR_MULTIHOP_ATTEMPTED                              = EMULTIHOP,
	ERR_FILENAME_TOO_LONG                               = ENAMETOOLONG,
	ERR_NETWORK_IS_DOWN                                 = ENETDOWN,
	ERR_CONNECTION_ABORTED_BY_NETWORK                   = ENETRESET,
	ERR_NETWORK_UNREACHABLE                             = ENETUNREACH,
	ERR_TOO_MANY_OPEN_FILES_IN_SYSTEM                   = ENFILE,
	ERR_NO_ANODE                                        = ENOANO,
	ERR_NO_BUFFER_SPACE_AVAILABLE                       = ENOBUFS,
	ERR_NO_MESSAGE_IS_AVAILABLE_ON_THE_STREAM_HEAD_READ_QUEUE  = ENODATA,
	ERR_NO_SUCH_DEVICE                                  = ENODEV,
	ERR_NO_SUCH_FILE_OR_DIRECTORY                       = ENOENT,
	ERR_EXEC_FORMAT_ERROR                               = ENOEXEC,
	ERR_REQUIRED_KEY_NOT_AVAILABLE                      = ENOKEY,
	ERR_NO_LOCKS_AVAILABLE                              = ENOLCK,
	ERR_LINK_HAS_BEEN_SEVERED                           = ENOLINK,
	ERR_NO_MEDIUM_FOUND                                 = ENOMEDIUM,
	ERR_NOT_ENOUGH_SPACE                                = ENOMEM,
	ERR_NO_MESSAGE_OF_THE_DESIRED_TYPE                  = ENOMSG,
	ERR_MACHINE_IS_NOT_ON_THE_NETWORK                   = ENONET,
	ERR_PACKAGE_NOT_INSTALLED                           = ENOPKG,
	ERR_PROTOCOL_NOT_AVAILABLE                          = ENOPROTOOPT,
	ERR_NO_SPACE_LEFT_ON_DEVICE                         = ENOSPC,
	ERR_NO_STREAM_RESOURCES                             = ENOSR,
	ERR_NOT_A_STREAM                                    = ENOSTR,
	ERR_FUNCTION_NOT_IMPLEMENTED                        = ENOSYS,
	ERR_BLOCK_DEVICE_REQUIRED                           = ENOTBLK,
	ERR_THE_SOCKET_IS_NOT_CONNECTED                     = ENOTCONN,
	ERR_NOT_A_DIRECTORY                                 = ENOTDIR,
	ERR_DIRECTORY_NOT_EMPTY                             = ENOTEMPTY,
	ERR_STATE_NOT_RECOVERABLE                           = ENOTRECOVERABLE,
	ERR_NOT_A_SOCKET                                    = ENOTSOCK,
	ERR_OPERATION_NOT_SUPPORTED                         = ENOTSUP,
	ERR_INAPPROPRIATE_IO_CONTROL_OPERATION              = ENOTTY,
	ERR_NAME_NOT_UNIQUE_ON_NETWORK                      = ENOTUNIQ,
	ERR_NO_SUCH_DEVICE_OR_ADDRESS                       = ENXIO,
	ERR_OPERATION_NOT_SUPPORTED_ON_SOCKET               = EOPNOTSUPP,
	ERR_VALUE_TOO_LARGE_TO_BE_STORED_IN_DATA_TYPE       = EOVERFLOW,
	ERR_OWNER_DIED                                      = EOWNERDEAD,
	ERR_OPERATION_NOT_PERMITTED                         = EPERM,
	ERR_PROTOCOL_FAMILY_NOT_SUPPORTED                   = EPFNOSUPPORT,
	ERR_BROKEN_PIPE                                     = EPIPE,
	ERR_PROTOCOL_ERROR                                  = EPROTO,
	ERR_PROTOCOL_NOT_SUPPORTED                          = EPROTONOSUPPORT,
	ERR_PROTOCOL_WRONG_TYPE_FOR_SOCKET                  = EPROTOTYPE,
	ERR_RESULT_TOO_LARGE                                = ERANGE,
	ERR_REMOTE_ADDRESS_CHANGED                          = EREMCHG,
	ERR_OBJECT_IS_REMOTE                                = EREMOTE,
	ERR_REMOTE_IO_ERR                                   = EREMOTEIO,
	ERR_INTERRUPTED_SYSTEM_CALL_SHOULD_BE_RESTARTED     = ERESTART,
	ERR_OPERATION_NOT_POSSIBLE_DUE_TO_RF                = ERFKILL,
	ERR_READ                                            = EROFS,
	ERR_CANNOT_SEND_AFTER_TRANSPORT_ENDPOINT_SHUTDOWN   = ESHUTDOWN,
	ERR_INVALID_SEEK                                    = ESPIPE,
	ERR_SOCKET_TYPE_NOT_SUPPORTED                       = ESOCKTNOSUPPORT,
	ERR_NO_SUCH_PROCESS                                 = ESRCH,
	ERR_STALE_FILE_HANDLE                               = ESTALE,
	ERR_STREAMS_PIPE_ERROR                              = ESTRPIPE,
	ERR_TIMER_EXPIRED                                   = ETIME,
	ERR_CONNECTION_TIMED_OUT                            = ETIMEDOUT,
	ERR_TOO_MANY_REFERENCES                             = ETOOMANYREFS,
	ERR_TEXT_FILE_BUSY                                  = ETXTBSY,
	ERR_STRUCTURE_NEEDS_CLEANING                        = EUCLEAN,
	ERR_PROTOCOL_DRIVER_NOT_ATTACHED                    = EUNATCH,
	ERR_TOO_MANY_USERS                                  = EUSERS,
	ERR_OPERATION_WOULD_BLOCK                           = EWOULDBLOCK,
	ERR_IMPROPER_LINK                                   = EXDEV,
	ERR_EXCHANGE_FULL                                   = EXFULL,

	ERR_WRONG_ERROR                                     = ERR(ERR_CORE, ECORE_WRONG_ERROR),
}err_e;


#endif
