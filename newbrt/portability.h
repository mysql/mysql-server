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

//Define chmod
/*
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
*/
#define chmod(a,b) (void)0 /* Remove temporarily till compatibility layer exists */

#define FAKE_WINDOWS_STUBS
#ifdef FAKE_WINDOWS_STUBS

typedef size_t ssize_t;
#define PRId64 "lld"
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
