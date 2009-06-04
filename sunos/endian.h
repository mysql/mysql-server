#ifndef _TOKU_ENDIAN_H
#define _TOKU_ENDIAN_H

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321

#if __x86_64__ || __i386__
    #define __BYTE_ORDER __LITTLE_ENDIAN
#else
    #error
#endif

#endif
