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

struct contents_committed;
struct contents_both;
struct contents_provdelorpair;

u_int32_t leafentry_memsize (LEAFENTRY);

enum le_state get_le_state(LEAFENTRY);
void *get_le_contents(LEAFENTRY);
enum typ_tag get_le_tag(LEAFENTRY);

u_int32_t committed_keylen (void*cev);
void* committed_key (void*cev);
u_int32_t committed_vallen (struct contents_committed *ce);
void* committed_val (struct contents_committed *ce);
TXNID both_xid (struct contents_both *ce);
u_int32_t both_keylen (struct contents_both *ce);
u_int32_t both_committed_vallen (struct contents_both *ce);
u_int32_t both_prov_vallen (struct contents_both *ce);
void* both_key (struct contents_both *ce);
void* both_committed_val (struct contents_both *ce);
void* both_prov_val (struct contents_both*ce);
TXNID provdelorpair_xid (struct contents_provdelorpair *ce);
u_int32_t provdelorpair_keylen (struct contents_provdelorpair *ce);
u_int32_t provdelorpair_vallen (struct contents_provdelorpair *ce);
void* provdelorpair_key (struct contents_provdelorpair *ce);
void* provdelorpair_val (struct contents_provdelorpair *ce);

#define LESWITCHCALL(le,funname, ...) ({	\
  assert(get_le_tag(le)==TYP_LEAFENTRY);                                                                                  \
  switch(get_le_state(le)) {					\
  case LE_COMMITTED: return funname ## _le_committed( committed_keylen((struct contents_committed*)(get_le_contents(le))), \
						      committed_key((struct contents_committed*)(get_le_contents(le))),	\
						      committed_vallen((struct contents_committed*)(get_le_contents(le))), \
						      committed_val((struct contents_committed*)(get_le_contents(le))), \
                                                      ## __VA_ARGS__);                   \
  case LE_BOTH:   return funname ## _le_both( both_xid((struct contents_both*)(get_le_contents(le))),                 \
					      both_keylen((struct contents_both*)(get_le_contents(le))),              \
					      both_key((struct contents_both*)(get_le_contents(le))),                 \
					      both_committed_vallen((struct contents_both*)(get_le_contents(le))),    \
					      both_committed_val((struct contents_both*)(get_le_contents(le))),       \
					      both_prov_vallen((struct contents_both*)(get_le_contents(le))),         \
					      both_prov_val((struct contents_both*)(get_le_contents(le))), \
                                                      ## __VA_ARGS__);                   \
  case LE_PROVDEL: return funname ## _le_provdel ( provdelorpair_xid((struct contents_provdelorpair*)(get_le_contents(le))), \
						  provdelorpair_keylen((struct contents_provdelorpair*)(get_le_contents(le))), \
						  provdelorpair_key((struct contents_provdelorpair*)(get_le_contents(le))),    \
						  provdelorpair_vallen((struct contents_provdelorpair*)(get_le_contents(le))), \
						  provdelorpair_val((struct contents_provdelorpair*)(get_le_contents(le))),    \
                                                      ## __VA_ARGS__);                   \
  case LE_PROVPAIR: return funname ## _le_provpair(provdelorpair_xid((struct contents_provdelorpair*)(get_le_contents(le))),    \
						   provdelorpair_keylen((struct contents_provdelorpair*)(get_le_contents(le))), \
						   provdelorpair_key((struct contents_provdelorpair*)(get_le_contents(le))), \
						   provdelorpair_vallen((struct contents_provdelorpair*)(get_le_contents(le))), \
						   provdelorpair_val((struct contents_provdelorpair*)(get_le_contents(le))), \
                                                      ## __VA_ARGS__);                   \
  } abort(); })


u_int32_t leafentry_memsize (LEAFENTRY le); // the size of a leafentry in memory.
u_int32_t leafentry_disksize (LEAFENTRY le); // this is the same as logsizeof_LEAFENTRY.  The size of a leafentry on disk.
u_int32_t toku_logsizeof_LEAFENTRY(LEAFENTRY le);
void wbuf_LEAFENTRY(struct wbuf *w, LEAFENTRY le);
void rbuf_LEAFENTRY(struct rbuf *r, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *le);
int toku_fread_LEAFENTRY(FILE *f, LEAFENTRY *le, u_int32_t *crc, u_int32_t *len); // read a leafentry from a log
int toku_logprint_LEAFENTRY(FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *format); // read a leafentry from a log and then print it in human-readable form.
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

#endif

