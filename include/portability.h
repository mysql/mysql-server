#ifndef TOKU_PORTABILITY_H
#define TOKU_PORTABILITY_H

#if defined __cplusplus
extern "C" {
#endif

// Portability layer
#define DEV_NULL_FILE "/dev/null"

#if defined(_MSC_VER)
// Microsoft compiler
#define TOKU_WINDOWS 1
#endif

#if defined(__INTEL_COMPILER)
// Intel compiler

#if defined(__ICL)
#define TOKU_WINDOWS 1
#endif

#undef DEV_NULL_FILE
#define DEV_NULL_FILE "NUL"

#endif

#if defined(TOKU_WINDOWS)
// Windows

//  ntohl and htonl are defined in winsock.h 
#include <winsock.h>
#include <direct.h>
#include <sys/types.h>
#include "stdint.h"
#include "inttypes.h"
#include "toku_pthread.h"
#include "unistd.h"
#include "misc.h"

#define UNUSED_WARNING(a) a=a /* To make up for missing attributes */

#if defined(__ICL)
#define __attribute__(x)      /* Nothing */
#endif

#elif defined(__INTEL_COMPILER)

#if defined(__ICC)
// Intel linux

#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>

#endif 

#elif defined(__GNUC__)
// GCC linux

#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
// Define ntohl using arpa/inet.h
#include <arpa/inet.h>
#include <sys/time.h>

#else

#error Not ICC and not GNUC.  What compiler?

#endif

#include "os.h"

#define UU(x) x __attribute__((__unused__))

#if defined __cplusplus
};
#endif

#endif
