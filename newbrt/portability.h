#ifndef PORTABILITY_H
#define PORTABILITY_H

// Portability layer

#if defined(__INTEL_COMPILER)
#if !defined(__ICL) && !defined(__ICC)
#error Which intel compiler?
#endif
#if defined(__ICL) && defined(__ICC)
#error Cannot distinguish between windows and linux intel compiler
#endif

#if defined(__ICL)
//Windows Intel Compiler
#define TOKU_WINDOWS

//Define standard integer types.
typedef __int8              int8_t;
typedef unsigned __int8   u_int8_t;
typedef __int16             int16_t;
typedef unsigned __int16  u_int16_t;
typedef __int32             int32_t;
typedef unsigned __int32  u_int32_t;
typedef __int64             int64_t;
typedef unsigned __int64  u_int64_t;

//Define printf types.
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRId32 "d"
#define PRIu32 "u"

//Limits
#define INT8_MIN   _I8_MIN
#define INT8_MAX   _I8_MAX
#define UINT8_MAX  _UI8_MAX
#define INT16_MIN  _I16_MIN
#define INT16_MAX  _I16_MAX
#define UINT16_MAX _UI16_MAX
#define INT32_MIN  _I32_MIN
#define INT32_MAX  _I32_MAX
#define UINT32_MAX _UI32_MAX
#define INT64_MIN  _I64_MIN
#define INT64_MAX  _I64_MAX
#define UINT64_MAX _UI64_MAX

#define FAKE_WINDOWS_STUBS 0 // Disable these fakes.
#if FAKE_WINDOWS_STUBS == 1

//#define chmod(a,b) (void)0 /* Remove temporarily till compatibility layer exists */
//Define chmod
typedef int    mode_t;
static inline
int
chmod(const char *path, mode_t mode) {
    //TODO: Write API to support what we really need.
    //Linux version supports WRITE/EXECUTE/READ bits separately for user/group/world
    //windows _chmod supports WRITE/READ bits separately for ?? one type (user? world?)
        //See _chmod in sys/stat.h
        //Supports setting read/write mode (not separately for user/group/world)
    return 0;
}
#define S_IRUSR 0
#define S_IRGRP 0
#define S_IROTH 0

//Fake typedefs to skip warnings.
typedef size_t ssize_t;
typedef int    pthread_cond_t;
typedef void*  pthread_mutex_t;
struct timeval {
    int tv_sec;
    int tv_usec;
};

#endif //FAKE_WINDOWS_STUBS

#endif //Windows Intel Compiler

// Intel compiler.
//  Define ntohl using bswap.
//  Define __attribute__ to be null

#include <sys/types.h>

static inline
u_int32_t
ntohl(u_int32_t x) {
    return _bswap(x);
}
static inline
u_int32_t
htonl(u_int32_t x) {
    return _bswap(x);
}

#define __attribute__(x)

#elif defined __GNUC__

// Gcc:
//   Define ntohl using arpa/inet.h
#include <arpa/inet.h>
#else
#error Not ICC and not GNUC.  What compiler?
#endif


#endif
