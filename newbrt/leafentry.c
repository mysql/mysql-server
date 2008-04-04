#include "brttypes.h"
#include "crc.h"
#include "leafentry.h"
#include "memory.h"
#include "toku_assert.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

enum le_state { LE_COMMITTED=1, // A committed pair.
		LE_BOTH,        // A committed pair and a provisional pair.
		LE_PROVDEL,     // A committed pair that has been provisionally deleted
		LE_PROVPAIR };  // No committed value, but a provisional pair.

struct leafentry {
    char state;
    char contents[0];
} __attribute__((packed));

struct contents_committed {
    u_int32_t keylen;
    u_int32_t vallen;
    char *data[0];
} __attribute__((packed));

struct contents_both {
    TXNID xid;
    u_int32_t keylen;
    u_int32_t committed_vallen;
    u_int32_t prov_vallen;
    char *data[0];
} __attribute__((packed));

struct contents_provdelorpair { // Te PROVDEL or PROVPAIR cases
    TXNID xid;
    u_int32_t keylen;
    u_int32_t vallen;
    char *data[0];
} __attribute__((packed));

static u_int32_t committed_keylen (void*cev) {
    struct contents_committed *ce=cev;
    return ce->keylen;
}

static void* committed_key (void*cev) {
    struct contents_committed *ce=cev;
    return &ce->data[0];
}

static u_int32_t committed_vallen (struct contents_committed *ce) {
    return ce->vallen;
}

static void* committed_val (struct contents_committed *ce) {
    return &ce->data[ce->keylen];
}

static TXNID both_xid (struct contents_both *ce) {
    return ce->xid;
}

static u_int32_t both_keylen (struct contents_both *ce) {
    return ce->keylen;
}

static u_int32_t both_committed_vallen (struct contents_both *ce) {
    return ce->committed_vallen;
}

static u_int32_t both_prov_vallen (struct contents_both *ce) {
    return ce->prov_vallen;
}

static void* both_key (struct contents_both *ce) {
    return &ce->data[0];
}

static void* both_committed_val (struct contents_both *ce) {
    return &ce->data[ce->keylen];
}

static void* both_prov_val (struct contents_both*ce) {
    return &ce->data[ce->keylen+ce->committed_vallen];
}

static TXNID provdelorpair_xid (struct contents_provdelorpair *ce) {
    return ce->xid;
}

static u_int32_t provdelorpair_keylen (struct contents_provdelorpair *ce) {
    return ce->keylen;
}

static u_int32_t provdelorpair_vallen (struct contents_provdelorpair *ce) {
    return ce->vallen;
}

static void* provdelorpair_key (struct contents_provdelorpair *ce) {
    return &ce->data[0];
}

static void* provdelorpair_val (struct contents_provdelorpair *ce) {
    return &ce->data[ce->keylen];
}


#define LESWITCHCALL(le,funname, ...) ({	\
  switch((enum le_state)((le)->state)) {	\
  case LE_COMMITTED: return funname ## _le_committed( committed_keylen((struct contents_committed*)&(le)->contents), \
						      committed_key((struct contents_committed*)&(le)->contents),    \
						      committed_vallen((struct contents_committed*)&(le)->contents), \
						      committed_val((struct contents_committed*)&(le)->contents), \
                                                      ## __VA_ARGS__);                   \
  case LE_BOTH:   return funname ## _le_both( both_xid((struct contents_both*)&(le)->contents),                 \
					      both_keylen((struct contents_both*)&(le)->contents),              \
					      both_key((struct contents_both*)&(le)->contents),                 \
					      both_committed_vallen((struct contents_both*)&(le)->contents),    \
					      both_committed_val((struct contents_both*)&(le)->contents),       \
					      both_prov_vallen((struct contents_both*)&(le)->contents),         \
					      both_prov_val((struct contents_both*)&(le)->contents), \
                                                      ## __VA_ARGS__);                   \
  case LE_PROVDEL: return funname ## _le_provdel ( provdelorpair_xid((struct contents_provdelorpair*)&(le)->contents), \
						  provdelorpair_keylen((struct contents_provdelorpair*)&(le)->contents), \
						  provdelorpair_key((struct contents_provdelorpair*)&(le)->contents),    \
						  provdelorpair_vallen((struct contents_provdelorpair*)&(le)->contents), \
						  provdelorpair_val((struct contents_provdelorpair*)&(le)->contents),    \
                                                      ## __VA_ARGS__);                   \
  case LE_PROVPAIR: return funname ## _le_provpair(provdelorpair_xid((struct contents_provdelorpair*)&(le)->contents),    \
						  provdelorpair_keylen((struct contents_provdelorpair*)&(le)->contents), \
						  provdelorpair_key((struct contents_provdelorpair*)&(le)->contents),    \
						  provdelorpair_vallen((struct contents_provdelorpair*)&(le)->contents), \
						  provdelorpair_val((struct contents_provdelorpair*)&(le)->contents),    \
                                                      ## __VA_ARGS__);                   \
  } abort(); })

static u_int32_t crc_uint32_t (u_int32_t crc, u_int32_t v) {
    u_int32_t i = htonl(v);
    return toku_crc32(crc, &i, 4);
}

static u_int32_t crc_uint64_t (u_int32_t crc, u_int64_t v) {
    crc = crc_uint32_t (crc, (u_int32_t)(v>>32));
    crc = crc_uint32_t (crc, (u_int32_t)(v&0xffffffff));
    return crc;
}

static u_int32_t crc_dbt (u_int32_t crc, u_int32_t len, void *data) {
    crc = crc_uint32_t(crc, len);
    crc = toku_crc32(crc, data, len);
    return crc;
}

static u_int32_t crc_le_committed(u_int32_t keylen, void *key, u_int32_t vallen, void *val, u_int32_t crc) {
    crc = crc_dbt(crc, keylen, key);
    crc = crc_dbt(crc, vallen, val);
    return crc;
}

static u_int32_t crc_le_both(TXNID xid, u_int32_t keylen, void *key, u_int32_t cvallen, void *cval, u_int32_t pvallen, void *pval, u_int32_t crc) {
    crc = crc_uint64_t(crc, xid);
    crc = crc_dbt(crc, keylen, key);
    crc = crc_dbt(crc, cvallen, cval);
    crc = crc_dbt(crc, pvallen, pval);
    return crc;
}

static u_int32_t crc_le_provdel(TXNID xid, u_int32_t keylen, void *key, u_int32_t vallen, void *val, u_int32_t crc) {
    crc = crc_uint64_t(crc, xid);
    crc = crc_dbt(crc, keylen, key);
    crc = crc_dbt(crc, vallen, val);
    return crc;
}

static u_int32_t crc_le_provpair(TXNID xid, u_int32_t keylen, void *key, u_int32_t vallen, void *val, u_int32_t crc) {
    return crc_le_provdel(xid, keylen, key, vallen, val, crc);
}

u_int32_t toku_le_crc(LEAFENTRY v) {
    u_int32_t crc = toku_null_crc;
    crc = toku_crc32(crc, &v->state, 1);
    LESWITCHCALL(v, crc, crc);
}

int toku_gpma_compress_kvspace (GPMA pma, struct mempool *memp) {
    if (toku_mempool_get_frag_size(memp) == 0)
        return -1;
    void *newmem = toku_malloc(memp->size);
    if (newmem == 0)
        return -2;
    struct mempool new_kvspace;
    toku_mempool_init(&new_kvspace, newmem, memp->size);
    GPMA_ITERATE(pma, idx, len, data,
		 ({
		     void *newdata = toku_mempool_malloc(&new_kvspace, (size_t)len, 4);
		     assert(newdata);
		     memcpy(newdata, data, (size_t)len);
		     toku_gpma_set_at_index(pma, idx, len, newdata);
		     // toku_verify_gpma(pma);
		 }));
    toku_free(memp->base);
    *memp = new_kvspace;
    // toku_verify_gpma(pma);
    return 0;
}


void *mempool_malloc_from_gpma(GPMA pma, struct mempool *mp, size_t size) {
    void *v = toku_mempool_malloc(mp, size, 4);
    if (v==0) {
	if (0 == toku_gpma_compress_kvspace(pma, mp)) {
	    v = toku_mempool_malloc(mp, size, 4);
	    assert(v);
	}
    }
    return v;
}

int le_committed (ITEMLEN klen, bytevec kval, ITEMLEN dlen, bytevec dval, GPMA pma, struct mempool *mp, LEAFENTRY *result) {
    struct contents_committed *ce;
    LEAFENTRY le=mempool_malloc_from_gpma(pma, mp, sizeof(*le)+sizeof(*ce)+klen+dlen);
    le->state=LE_COMMITTED;
    ce=(struct contents_committed*)&le->contents[0];
    ce->keylen = klen;
    ce->vallen = dlen;
    memcpy(&ce->data[0], kval, (size_t)klen);
    memcpy(&ce->data[klen], dval, (size_t)dlen);
    *result=le;
    return 0;
}
int le_both (ITEMLEN cklen, bytevec ckval, ITEMLEN cdlen, bytevec cdval, ITEMLEN pdlen, bytevec pdval,
	     struct mempool *mp, LEAFENTRY *result);
int le_provdel  (ITEMLEN klen, bytevec kval, ITEMLEN dlen, bytevec dval, struct mempool *mp, LEAFENTRY *result);
int le_provpair (ITEMLEN klen, bytevec kval, ITEMLEN dlen, bytevec dval, struct mempool *mp, LEAFENTRY *result);
