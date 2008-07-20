#ifndef LEAFENTRY_H
#define LEAFENTRY_H
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* In the past, leaves simply contained key-value pairs.
 * In this implementatoin, leaf values are more complex
 * They can contain a committed value:
 *   - Which can be "not-present",
 *   - Or a key-value pair.
 * They can contain a provisional value, which depends on whether a particular transaction commits or aborts.
 *   - A not-present value
 *   - Or a key-value pair.
 *   - Or there can be no provisional value at all (that is, the value doesn't depend on the transaction.)
 * Note that if both the provisional value and the committed value are not-present, then there is simply no entry in the leaf.
 * So let's enumerate the possibilities:
 *     committed pair                                 A committed pair unaffected by any incomplete transaction.
 *     committed pair and provisional pair            A committed pair to provisionally be replaced by a new pair.
 *     committed pair and provisional delete          A committed pair that will be deleted
 *     provisional pair                               No committed pair, but if a provisional pair to add.
 *
 * In the case of a committed pair and a provisional pair, the key is the same in both cases.  The value can be different.
 *
 * For DUPSORT databases, the key-value pair is everything, so we need only represent the key-value pair once.  So the cases are
 *    committed pair
 *    committed pair provisionally deleted
 *    provisional pair
 * The case of a committed pair and a provisional pair can be represented by a committed pair, since it doesn't matter whether the transction aborts or commits, the value is the same.
 */

#include "brttypes.h"
#include "rbuf.h"
#include "murmur.h"
#include <arpa/inet.h>

u_int32_t toku_le_crc(LEAFENTRY v);

int le_committed (u_int32_t klen, void* kval, u_int32_t dlen, void* dval, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result);
int le_both (TXNID xid, u_int32_t cklen, void* ckval, u_int32_t cdlen, void* cdval, u_int32_t pdlen, void* pdval,
	     u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result);
int le_provdel  (TXNID xid, u_int32_t klen, void* kval, u_int32_t dlen, void* dval,
		 u_int32_t *resultsize, u_int32_t *memsize, LEAFENTRY *result);
int le_provpair (TXNID xid, u_int32_t klen, void* kval, u_int32_t dlen, void* dval,
		 u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result);

enum le_state { LE_COMMITTED=1, // A committed pair.
		LE_BOTH,        // A committed pair and a provisional pair.
		LE_PROVDEL,     // A committed pair that has been provisionally deleted
		LE_PROVPAIR };  // No committed value, but a provisional pair.

u_int32_t leafentry_memsize (LEAFENTRY);

static inline enum le_state get_le_state(LEAFENTRY le) {
    return *(unsigned char *)le;
}

static inline void putint (unsigned char *p, u_int32_t i) {
#if 1
    *(u_int32_t*)p = htonl(i);
#else
    p[0]=(i>>24)&0xff;
    p[1]=(i>>16)&0xff;
    p[2]=(i>> 8)&0xff;
    p[3]=(i>> 0)&0xff;
#endif
}
static inline void putint64 (unsigned char *p, u_int64_t i) {
    putint(p, (u_int32_t)(i>>32));
    putint(p+4, (u_int32_t)(i&0xffffffff));
}
static inline u_int32_t getint (unsigned char *p) {
#if 1
    return ntohl(*(u_int32_t*)p);
#else
    return (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+(p[3]);
#endif
}
static inline u_int64_t getint64 (unsigned char *p) {
    return (((u_int64_t)getint(p))<<32) + getint(p+4);
}

#define LESWITCHCALL(le,funname, ...) ({	                                                                     \
  switch(get_le_state(le)) {					                                                     \
  case LE_COMMITTED: {                                                                                               \
    unsigned char* __klenaddr = 1+(unsigned char*)le;  u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4      + __klenaddr;                                                                 \
    unsigned char* __clenaddr = __klen + __kvaladdr;   u_int32_t __clen = getint(__clenaddr);                        \
    unsigned char* __cvaladdr = 4 + __clenaddr;                                                                      \
    return funname ## _le_committed(__klen, __kvaladdr, __clen, __cvaladdr, ## __VA_ARGS__); }                       \
  case LE_BOTH: {                                                                                                    \
    unsigned char* __xidaddr  = 1+(unsigned char*)le;  u_int64_t __xid  = getint64(__xidaddr);                       \
    unsigned char* __klenaddr = 8 + __xidaddr;         u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4 + __klenaddr;                                                                      \
    unsigned char* __clenaddr = __klen + __kvaladdr;   u_int32_t __clen = getint(__clenaddr);                        \
    unsigned char* __cvaladdr = 4 + __clenaddr;                                                                      \
    unsigned char* __plenaddr = __clen + __cvaladdr;   u_int32_t __plen = getint(__plenaddr);                        \
    unsigned char* __pvaladdr = 4 + __plenaddr;                                                                      \
    return funname ## _le_both(__xid, __klen, __kvaladdr, __clen, __cvaladdr, __plen, __pvaladdr, ## __VA_ARGS__); } \
  case LE_PROVDEL:  {                                                                                                \
    unsigned char* __xidaddr  = 1+(unsigned char*)le;  u_int64_t __xid  = getint64(__xidaddr);                       \
    unsigned char* __klenaddr = 8 + __xidaddr;         u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4 + __klenaddr;                                                                      \
    unsigned char* __dlenaddr = __klen + __kvaladdr;   u_int32_t __dlen = getint(__dlenaddr);                        \
    unsigned char* __dvaladdr = 4 + __dlenaddr;                                                                      \
    return funname ## _le_provdel(__xid, __klen, __kvaladdr, __dlen, __dvaladdr, ## __VA_ARGS__); }                  \
  case LE_PROVPAIR:  {                                                                                               \
    unsigned char* __xidaddr  = 1+(unsigned char*)le;  u_int64_t __xid  = getint64(__xidaddr);                       \
    unsigned char* __klenaddr = 8 + __xidaddr;         u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4 + __klenaddr;                                                                      \
    unsigned char* __plenaddr = __klen + __kvaladdr;   u_int32_t __plen = getint(__plenaddr);                        \
    unsigned char* __pvaladdr = 4 + __plenaddr;                                                                      \
    return funname ## _le_provpair(__xid, __klen, __kvaladdr, __plen, __pvaladdr, ## __VA_ARGS__); }                 \
  } abort(); })


u_int32_t leafentry_memsize (LEAFENTRY le); // the size of a leafentry in memory.
u_int32_t leafentry_disksize (LEAFENTRY le); // this is the same as logsizeof_LEAFENTRY.  The size of a leafentry on disk.
u_int32_t toku_logsizeof_LEAFENTRY(LEAFENTRY le);
void wbuf_LEAFENTRY(struct wbuf *w, LEAFENTRY le);
void rbuf_LEAFENTRY(struct rbuf *r, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *le);
int toku_fread_LEAFENTRY(FILE *f, LEAFENTRY *le, struct murmur *, u_int32_t *len); // read a leafentry from a log
int toku_logprint_LEAFENTRY(FILE *outf, FILE *inf, const char *fieldname, struct murmur *, u_int32_t *len, const char *format); // read a leafentry from a log and then print it in human-readable form.
void toku_free_LEAFENTRY(LEAFENTRY le);
int print_leafentry (FILE *outf, LEAFENTRY v); // Print a leafentry out in human-readable form.

int le_is_provdel(LEAFENTRY le); // Return true if it is a provisional delete.
void*     le_latest_key (LEAFENTRY le); // Return the latest key (return NULL for provisional deletes)
u_int32_t le_latest_keylen (LEAFENTRY le); // Return the latest keylen.
void*     le_latest_val (LEAFENTRY le); // Return the latest val (return NULL for provisional deletes)
u_int32_t le_latest_vallen (LEAFENTRY le); // Return the latest vallen.  Returns 0 for provisional deletes.

 // Return any key or value (even if it's only provisional)
void* le_any_key (LEAFENTRY le);
u_int32_t le_any_keylen (LEAFENTRY le);
void* le_any_val (LEAFENTRY le);
u_int32_t le_any_vallen (LEAFENTRY le);
u_int64_t le_any_xid (LEAFENTRY le);

#endif

