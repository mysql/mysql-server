
#ifndef NDBGLOBAL_H
#define NDBGLOBAL_H

#include <my_global.h>

/** signal & SIG_PIPE */
#include <my_alarm.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define NDB_WIN32
#else
#undef NDB_WIN32
#endif

#include <m_string.h>
#include <m_ctype.h>
#include <ndb_types.h>
#include <ctype.h>
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <sys/param.h>
#ifdef HAVE_SYS_STAT_H
  #if defined(__cplusplus) && defined(_APP32_64BIT_OFF_T) && defined(_INCLUDE_AES_SOURCE)
    #undef _INCLUDE_AES_SOURCE
    #include <sys/stat.h>
    #define _INCLUDE_AES_SOURCE
  #else
    #include <sys/stat.h>
  #endif
#endif
#include <sys/resource.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef NDB_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#define DIR_SEPARATOR "\\"
#define PATH_MAX 256

#pragma warning(disable: 4503 4786)
#else

#define DIR_SEPARATOR "/"

#endif

static const char table_name_separator =  '/';

#ifdef NDB_VC98
#define STATIC_CONST(x) enum { x }
#else
#define STATIC_CONST(x) static const Uint32 x
#endif

#ifdef  __cplusplus
#include <new>
#endif

#ifdef  __cplusplus
extern "C" {
#endif
	
#include <assert.h>

/* call in main() - does not return on error */
extern int ndb_init(void);

#ifndef HAVE_STRDUP
extern char * strdup(const char *s);
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy (char *dst, const char *src, size_t dst_sz);
#endif

#ifndef HAVE_STRLCAT
extern size_t strlcat (char *dst, const char *src, size_t dst_sz);
#endif

#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifdef SCO

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#endif /* SCO */

#ifdef  __cplusplus
}
#endif

#endif
