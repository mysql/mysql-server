#ifdef __powerpc__

#define CONSTANTS .crc32c_constants
#define SHORT_CONSTANTS .crc32c_short_constants
#define BARRETT_CONSTANTS .crc32c_barrett_constants

#include "crc32c_constants.h"

#define __F __crc32c_vpmsum

#include "crc32.iS"

#endif

