#ifndef BRTTYPES_H
#define BRTTYPES_H
#include <sys/types.h>
#define _XOPEN_SOURCE 500
#define _FILE_OFFSET_BITS 64

typedef struct brt *BRT;
struct brt_header;
struct wbuf;

typedef unsigned int ITEMLEN;
typedef const void *bytevec;
//typedef const void *bytevec;

typedef long long DISKOFF;  /* Offset in a disk. -1 is the NULL pointer. */
typedef u_int64_t TXNID;

typedef struct {
    int len;
    char *data;
} BYTESTRING;

/* Make the LSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_lsn { u_int64_t lsn; } LSN;
#define ZERO_LSN ((LSN){0})

/* Make the FILEID a struct for the same reason. */
typedef struct __toku_fileid { u_int32_t fileid; } FILENUM;

typedef enum __toku_bool { FALSE=0, TRUE=1} BOOL;

typedef struct tokulogger *TOKULOGGER;
#define NULL_LOGGER ((TOKULOGGER)0)
typedef struct tokutxn    *TOKUTXN;
#define NULL_TXN ((TOKUTXN)0)

// The data that appears in the log to encode a brtheader. */
typedef struct loggedbrtheader {
    u_int32_t size;
    u_int32_t flags;
    u_int32_t nodesize;
    DISKOFF   freelist;
    DISKOFF   unused_memory;
    u_int32_t n_named_roots;
    union {
	struct {
	    char **names;
	    DISKOFF *roots;
	} many;
	struct {
	    DISKOFF   root;
	} one;
    } u;
} LOGGEDBRTHEADER; 

#endif
