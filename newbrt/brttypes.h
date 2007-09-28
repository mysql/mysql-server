#ifndef BRTTYPES_H
#define BRTTYPES_H
#define _XOPEN_SOURCE 500
#define _FILE_OFFSET_BITS 64
typedef unsigned int ITEMLEN;
typedef const void *bytevec;
//typedef const void *bytevec;

typedef long long diskoff;  /* Offset in a disk. -1 is the NULL pointer. */
typedef long long TXNID;

#endif
