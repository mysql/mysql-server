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

#include "mempool.h"
#include "brttypes.h"
#include "gpma.h"

typedef struct leafentry *LEAFENTRY;

u_int32_t le_crc(LEAFENTRY v);

int le_committed (ITEMLEN klen, bytevec kval, ITEMLEN dlen, bytevec dval, GPMA pma, struct mempool *mp, LEAFENTRY *result);
int le_both (ITEMLEN cklen, bytevec ckval, ITEMLEN cdlen, bytevec cdval, ITEMLEN pdlen, bytevec pdval,
	     struct mempool *mp, LEAFENTRY *result);
int le_provdel  (ITEMLEN klen, bytevec kval, ITEMLEN dlen, bytevec dval, struct mempool *mp, LEAFENTRY *result);
int le_provpair (ITEMLEN klen, bytevec kval, ITEMLEN dlen, bytevec dval, struct mempool *mp, LEAFENTRY *result);

int toku_gpma_compress_kvspace (GPMA pma, struct mempool *memp);
void *mempool_malloc_from_gpma(GPMA pma, struct mempool *mp, u_int32_t size);

#endif

