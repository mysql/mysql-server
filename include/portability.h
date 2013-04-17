#ifndef TOKU_PORTABILITY_H
#define TOKU_PORTABILITY_H

// Tokutek portability layer

#if defined __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) || (defined(__INTEL_COMPILER) && defined(__ICL))

#define TOKU_WINDOWS 1
#define DEV_NULL_FILE "NUL"

#else

#define TOKU_WINDOWS 0
#define DEV_NULL_FILE "/dev/null"

#endif

#if defined(TOKU_WINDOWS) && TOKU_WINDOWS
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
