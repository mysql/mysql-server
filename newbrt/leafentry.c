#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "brttypes.h"
#include "crc.h"
#include "leafentry.h"
#include "memory.h"
#include "toku_assert.h"
#include "log.h"
#include "wbuf.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct leafentry {
    enum typ_tag tag; // Delete this later
    char state;
    char contents[0];
} __attribute__((packed));

struct contents_committed {
    u_int32_t keylen;
    u_int32_t vallen;
    char data[0];
} __attribute__((packed));

struct contents_both {
    TXNID xid;
    u_int32_t keylen;
    u_int32_t committed_vallen;
    u_int32_t prov_vallen;
    char data[0];
} __attribute__((packed));

struct contents_provdelorpair { // The PROVDEL or PROVPAIR cases
    TXNID xid;
    u_int32_t keylen;
    u_int32_t vallen;
    char data[0];
} __attribute__((packed));

enum le_state get_le_state(LEAFENTRY le) {
    return le->state;
}
void* get_le_contents(LEAFENTRY le) {
    return &le->contents[0];
}
enum typ_tag get_le_tag(LEAFENTRY le) {
    return le->tag;
}

u_int32_t committed_keylen (void*cev) {
    struct contents_committed *ce=cev;
    return ce->keylen;
}

void* committed_key (void*cev) {
    struct contents_committed *ce=cev;
    return &ce->data[0];
}

u_int32_t committed_vallen (struct contents_committed *ce) {
    return ce->vallen;
}

void* committed_val (struct contents_committed *ce) {
    return &ce->data[ce->keylen];
}

TXNID both_xid (struct contents_both *ce) {
    return ce->xid;
}

u_int32_t both_keylen (struct contents_both *ce) {
    return ce->keylen;
}

u_int32_t both_committed_vallen (struct contents_both *ce) {
    return ce->committed_vallen;
}

u_int32_t both_prov_vallen (struct contents_both *ce) {
    return ce->prov_vallen;
}

void* both_key (struct contents_both *ce) {
    return &ce->data[0];
}

void* both_committed_val (struct contents_both *ce) {
    return &ce->data[ce->keylen];
}

void* both_prov_val (struct contents_both*ce) {
    return &ce->data[ce->keylen+ce->committed_vallen];
}

TXNID provdelorpair_xid (struct contents_provdelorpair *ce) {
    return ce->xid;
}

u_int32_t provdelorpair_keylen (struct contents_provdelorpair *ce) {
    return ce->keylen;
}

u_int32_t provdelorpair_vallen (struct contents_provdelorpair *ce) {
    return ce->vallen;
}

void* provdelorpair_key (struct contents_provdelorpair *ce) {
    return &ce->data[0];
}

void* provdelorpair_val (struct contents_provdelorpair *ce) {
    return &ce->data[ce->keylen];
}

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

int le_committed (u_int32_t klen, void* kval, u_int32_t dlen, void* dval, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result) {
    struct contents_committed *ce;
    LEAFENTRY le;
    size_t size = sizeof(*le)+sizeof(*ce)+klen+dlen;
    le=toku_malloc(size);
    le->tag  = TYP_LEAFENTRY;
    le->state= LE_COMMITTED;
    ce=(struct contents_committed*)&le->contents[0];
    ce->keylen = klen;
    ce->vallen = dlen;
    memcpy(&ce->data[0], kval, (size_t)klen);
    memcpy(&ce->data[klen], dval, (size_t)dlen);
    *resultsize=size;
    *disksize  = 1 + 4 + 4 + klen + dlen;
    *result=le;
    return 0;
}
int le_both (TXNID xid, u_int32_t klen, void* kval, u_int32_t clen, void* cval, u_int32_t plen, void* pval,
	     u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result) {
    struct contents_both *ce;
    LEAFENTRY le;
    size_t size = sizeof(*le)+sizeof(*ce)+klen+plen+clen;
    le=toku_malloc(size);
    le->tag  = TYP_LEAFENTRY;
    le->state= LE_BOTH;
    ce=(struct contents_both*)&le->contents[0];
    ce->xid    = xid;
    ce->keylen = klen;
    ce->committed_vallen = clen;
    ce->prov_vallen = plen;
    memcpy(&ce->data[0],         kval, (size_t)klen);
    memcpy(&ce->data[klen],      cval, (size_t)clen);
    memcpy(&ce->data[klen+clen], pval, (size_t)plen);
    *resultsize=size;
    *disksize  = 1 + 8 + 4*3 + klen + clen + plen;
    *result=le;
    return 0;
    
}
int le_provdel  (TXNID xid, u_int32_t klen, void* kval, u_int32_t dlen, void* dval, 
		 u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result) {
    struct contents_provdelorpair *ce;
    LEAFENTRY le;
    size_t size = sizeof(*le)+sizeof(*ce)+klen+dlen;
    le=toku_malloc(size);
    le->tag  = TYP_LEAFENTRY;
    le->state= LE_PROVDEL;
    ce=(struct contents_provdelorpair*)&le->contents[0];
    ce->xid    = xid;
    ce->keylen = klen;
    ce->vallen = dlen;
    memcpy(&ce->data[0], kval, (size_t)klen);
    memcpy(&ce->data[klen], dval, (size_t)dlen);
    *memsize=size;
    *disksize  = 1 + 4 + 4 + 8 + klen + dlen;
    *result=le;
    return 0;
}
int le_provpair (TXNID xid, u_int32_t klen, void* kval, u_int32_t dlen, void* dval, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result) {
    struct contents_provdelorpair *ce;
    LEAFENTRY le;
    size_t size = sizeof(*le)+sizeof(*ce)+klen+dlen;
    le=toku_malloc(size);
    le->tag  = TYP_LEAFENTRY;
    le->state= LE_PROVPAIR;
    ce=(struct contents_provdelorpair*)&le->contents[0];
    ce->xid    = xid;
    ce->keylen = klen;
    ce->vallen = dlen;
    memcpy(&ce->data[0], kval, (size_t)klen);
    memcpy(&ce->data[klen], dval, (size_t)dlen);
    *resultsize=size;
    *disksize  = 1 + 4 + 4 + 8 + klen + dlen;
    *result=le;
    return 0;
}

static u_int32_t memsize_le_committed (u_int32_t keylen, void *key __attribute__((__unused__)),
				       u_int32_t vallen, void *val __attribute__((__unused__))) {
    return sizeof(struct leafentry) + sizeof(struct contents_committed) + keylen + vallen;
}

static u_int32_t memsize_le_both (TXNID txnid __attribute__((__unused__)),
				  u_int32_t klen, void *kval __attribute__((__unused__)),
				  u_int32_t clen, void *cval __attribute__((__unused__)),
				  u_int32_t plen, void *pval __attribute__((__unused__))) {
    return sizeof(struct leafentry) + sizeof(struct contents_both) + klen + clen + plen;
}

static u_int32_t memsize_le_provdel (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t clen, void *cval __attribute__((__unused__))) {
    return sizeof(struct leafentry) + sizeof(struct contents_provdelorpair) + klen + clen;
}

static u_int32_t memsize_le_provpair (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t plen, void *pval __attribute__((__unused__))) {
    return sizeof(struct leafentry) + sizeof(struct contents_provdelorpair) + klen + plen;
}

u_int32_t leafentry_memsize (LEAFENTRY le) {
    LESWITCHCALL(le, memsize);
}

static u_int32_t disksize_le_committed (u_int32_t keylen, void *key __attribute__((__unused__)),
				       u_int32_t vallen, void *val __attribute__((__unused__))) {
    return 1 + 4 + 4 + keylen + vallen;
}

static u_int32_t disksize_le_both (TXNID txnid __attribute__((__unused__)),
				  u_int32_t klen, void *kval __attribute__((__unused__)),
				  u_int32_t clen, void *cval __attribute__((__unused__)),
				  u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*3 + klen + clen + plen;
}

static u_int32_t disksize_le_provdel (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t clen, void *cval __attribute__((__unused__))) {
    return 1 + 8 + 4 + 4 + klen + clen;
}

static u_int32_t disksize_le_provpair (TXNID txnid __attribute__((__unused__)),
				       u_int32_t klen, void *kval __attribute__((__unused__)),
				       u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4 + 4 + klen + plen;
}

u_int32_t leafentry_disksize (LEAFENTRY le) {
    LESWITCHCALL(le, disksize);
}

u_int32_t toku_logsizeof_LEAFENTRY (LEAFENTRY le) {
    return leafentry_disksize(le);
}

int toku_fread_LEAFENTRY(FILE *f, LEAFENTRY *le, u_int32_t *crc, u_int32_t *len) {
    u_int8_t state;
    int r = toku_fread_u_int8_t (f, &state, crc, len); if (r!=0) return r;
    TXNID xid;
    BYTESTRING a,b,c;
    u_int32_t memsize, disksize;
    switch ((enum le_state)state) {
    case LE_COMMITTED:
	r = toku_fread_BYTESTRING(f, &a, crc, len);  if (r!=0) return r;
	r = toku_fread_BYTESTRING(f, &b, crc, len);  if (r!=0) return r;
	r = le_committed(a.len, a.data, b.len, b.data,
			 &memsize, &disksize, le);
	toku_free_BYTESTRING(a);
	toku_free_BYTESTRING(b);
	return r;
    case LE_BOTH:
	r = toku_fread_TXNID(f, &xid, crc, len);
	r = toku_fread_BYTESTRING(f, &a, crc, len);  if (r!=0) return r;
	r = toku_fread_BYTESTRING(f, &b, crc, len);  if (r!=0) return r;
	r = toku_fread_BYTESTRING(f, &c, crc, len);  if (r!=0) return r;
	r = le_both(xid, a.len, a.data, b.len, b.data, c.len, c.data,
		    &memsize, &disksize, le);
	toku_free_BYTESTRING(a);
	toku_free_BYTESTRING(b);
	toku_free_BYTESTRING(c);
	return r;
    case LE_PROVDEL:
	r = toku_fread_TXNID(f, &xid, crc, len);
	r = toku_fread_BYTESTRING(f, &a, crc, len);  if (r!=0) return r;
	r = toku_fread_BYTESTRING(f, &b, crc, len);  if (r!=0) return r;
	r = le_provdel(xid, a.len, a.data, b.len, b.data,
		       &memsize, &disksize, le);
	toku_free_BYTESTRING(a);
	toku_free_BYTESTRING(b);
	return r;
    case LE_PROVPAIR:
	r = toku_fread_TXNID(f, &xid, crc, len);
	r = toku_fread_BYTESTRING(f, &a, crc, len);  if (r!=0) return r;
	r = toku_fread_BYTESTRING(f, &b, crc, len);  if (r!=0) return r;
	r = le_provpair(xid, a.len, a.data, b.len, b.data,
			&memsize, &disksize, le);
	toku_free_BYTESTRING(a);
	toku_free_BYTESTRING(b);
	return r;
    }
    return DB_BADFORMAT;
}

static int print_le_committed (u_int32_t keylen, void *key, u_int32_t vallen, void *val, FILE *outf) {
    fprintf(outf, "{C: ");
    toku_print_BYTESTRING(outf, keylen, key);
    toku_print_BYTESTRING(outf, vallen, val);
    fprintf(outf, "}");
    return 0;
}

static int print_le_both (TXNID xid, u_int32_t klen, void *kval, u_int32_t clen, void *cval, u_int32_t plen, void *pval, FILE *outf) {
    fprintf(outf, "{B: ");
    fprintf(outf, " xid=%" PRId64, xid);
    fprintf(outf, " key=");
    toku_print_BYTESTRING(outf, klen, kval);
    toku_print_BYTESTRING(outf, clen, cval);
    fprintf(outf, " provisional=");
    toku_print_BYTESTRING(outf, plen, pval);
    fprintf(outf, "}");
    return 0;
}

static int print_le_provdel (TXNID xid, u_int32_t klen, void *kval, u_int32_t clen, void *cval, FILE *outf) {
    fprintf(outf, "{D: ");
    fprintf(outf, " xid=%" PRId64, xid);
    fprintf(outf, " key=");
    toku_print_BYTESTRING(outf, klen, kval);
    fprintf(outf, " committed=");
    toku_print_BYTESTRING(outf, clen, cval);
    fprintf(outf, "}");
    return 0;
}

static int print_le_provpair (TXNID xid, u_int32_t klen, void *kval, u_int32_t plen, void *pval, FILE *outf) {
    fprintf(outf, "{P: ");
    fprintf(outf, " xid=%" PRId64, xid);
    fprintf(outf, " key=");
    toku_print_BYTESTRING(outf, klen, kval);
    fprintf(outf, " provisional=");
    toku_print_BYTESTRING(outf, plen, pval);
    fprintf(outf, "}");
    return 0;
}

int print_leafentry (FILE *outf, LEAFENTRY v) {
    if (!v) { printf("NULL"); return 0; }
    LESWITCHCALL(v, print, outf);
}

int toku_logprint_LEAFENTRY (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *format __attribute__((__unused__))) {
    LEAFENTRY v;
    int r = toku_fread_LEAFENTRY(inf, &v, crc, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    print_leafentry(outf, v);
    toku_free(v);
    return 0;
}

static int wbuf_le_committed (u_int32_t keylen, void *key, u_int32_t vallen, void *val, struct wbuf *w) {
    wbuf_bytes(w, key, keylen);
    wbuf_bytes(w, val, vallen);
    return 0;
}

static int wbuf_le_both (TXNID xid, u_int32_t klen, void *kval, u_int32_t clen, void *cval, u_int32_t plen, void *pval, struct wbuf *w) {
    wbuf_TXNID(w, xid);
    wbuf_bytes(w, kval, klen);
    wbuf_bytes(w, cval, clen);
    wbuf_bytes(w, pval, plen);
    return 0;
}

static int wbuf_le_provdel (TXNID xid, u_int32_t klen, void *kval, u_int32_t clen, void *cval, struct wbuf *w) {
    wbuf_TXNID(w, xid);
    wbuf_bytes(w, kval, klen);
    wbuf_bytes(w, cval, clen);
    return 0;
}
static int wbuf_le_provpair (TXNID xid, u_int32_t klen, void *kval, u_int32_t plen, void *pval, struct wbuf *w) {
    wbuf_TXNID(w, xid);
    wbuf_bytes(w, kval, klen);
    wbuf_bytes(w, pval, plen);
    return 0;
}

static int do_wbuf_le (struct wbuf *w, LEAFENTRY le) {
    LESWITCHCALL(le, wbuf, w);
}
void wbuf_LEAFENTRY(struct wbuf *w, LEAFENTRY le) {
    wbuf_char(w, (unsigned int)le->state);
    do_wbuf_le(w,le);
}

void rbuf_LEAFENTRY(struct rbuf *r, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *le) {
    enum le_state state = rbuf_char(r);
    switch (state) {
    case LE_COMMITTED: {
	//printf("%s:%d reading committed\n", __FILE__, __LINE__);
	bytevec key,  val;
	u_int32_t keylen, vallen;
	rbuf_bytes(r, &key, &keylen);
	rbuf_bytes(r, &val, &vallen);
	le_committed(keylen, (void*)key, vallen, (void*)val, resultsize, disksize, le);
	return;
    }
    case LE_BOTH: {
	//printf("%s:%d reading both\n", __FILE__, __LINE__);
	bytevec   kval, cval, pval;
	u_int32_t klen, clen, plen;
	TXNID xid = rbuf_ulonglong(r);
	rbuf_bytes(r, &kval, &klen);
	rbuf_bytes(r, &cval, &clen);
	rbuf_bytes(r, &pval, &plen);
	le_both(xid, klen, (void*)kval, clen, (void*)cval, plen, (void*)pval, resultsize, disksize, le);
	return;
    }
    case LE_PROVDEL: {
	//printf("%s:%d reading provdel\n", __FILE__, __LINE__);
	bytevec   kval, cval;
	u_int32_t klen, clen;
	TXNID xid = rbuf_ulonglong(r);
	rbuf_bytes(r, &kval, &klen);
	rbuf_bytes(r, &cval, &clen);
	le_provdel(xid, klen, (void*)kval, clen, (void*)cval, resultsize, disksize, le);
	return;
    }
    case LE_PROVPAIR: {
	//printf("%s:%d reading both\n", __FILE__, __LINE__);
	bytevec   kval, pval;
	u_int32_t klen, plen;
	TXNID xid = rbuf_ulonglong(r);
	rbuf_bytes(r, &kval, &klen);
	rbuf_bytes(r, &pval, &plen);
	le_provpair(xid, klen, (void*)kval, plen, (void*)pval, resultsize, disksize, le);
	return;
    }
    }
    assert(0);
}

// Use toku_free()
void toku_free_LEAFENTRY(LEAFENTRY le) {
    toku_free(le);
}

int le_is_provdel(LEAFENTRY le) {
    return le->state==LE_PROVDEL;
}

void* latest_key_le_committed (u_int32_t UU(keylen), void *key, u_int32_t UU(vallen), void *UU(val)) {
    return key;
}
void* latest_key_le_both (TXNID UU(xid), u_int32_t UU(klen), void *kval, u_int32_t UU(clen), void *UU(cval), u_int32_t UU(plen), void *UU(pval)) {
    return kval;
}
void* latest_key_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval)) {
    return 0; // for provisional delete, there is no *latest* key, so return NULL
}
void* latest_key_le_provpair (TXNID UU(xid), u_int32_t UU(klen), void *kval, u_int32_t UU(plen), void *UU(pval)) {
    return kval;
}
void* le_latest_key (LEAFENTRY le) {
    LESWITCHCALL(le, latest_key);
}

u_int32_t latest_keylen_le_committed (u_int32_t keylen, void *UU(key), u_int32_t UU(vallen), void *UU(val)) {
    return keylen;
}
u_int32_t latest_keylen_le_both (TXNID UU(xid), u_int32_t klen, void *UU(kval), u_int32_t UU(clen), void *UU(cval), u_int32_t UU(plen), void *UU(pval)) {
    return klen;
}
u_int32_t latest_keylen_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval)) {
    return 0; // for provisional delete, there is no *latest* key, so return 0.  What else can we do?
}
u_int32_t latest_keylen_le_provpair (TXNID UU(xid), u_int32_t klen, void *UU(kval), u_int32_t UU(plen), void *UU(pval)) {
    return klen;
}
u_int32_t le_latest_keylen (LEAFENTRY le) {
    LESWITCHCALL(le, latest_keylen);
}

void* latest_val_le_committed (u_int32_t UU(keylen), void *UU(key), u_int32_t UU(vallen), void *UU(val)) {
    return val;
}
void* latest_val_le_both (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval), u_int32_t UU(plen), void *pval) {
    return pval;
}
void* latest_val_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval)) {
    return 0; // for provisional delete, there is no *latest* key, so return NULL
}
void* latest_val_le_provpair (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(plen), void *pval) {
    return pval;
}
void* le_latest_val (LEAFENTRY le) {
    LESWITCHCALL(le, latest_val);
}

u_int32_t latest_vallen_le_committed (u_int32_t UU(keylen), void *UU(key), u_int32_t vallen, void *UU(val)) {
    return vallen;
}
u_int32_t latest_vallen_le_both (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval), u_int32_t plen, void *UU(pval)) {
    return plen;
}
u_int32_t latest_vallen_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval)) {
    return 0; // for provisional delete, there is no *latest* key, so return 0.  What else can we do?
}
u_int32_t latest_vallen_le_provpair (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t plen, void *UU(pval)) {
    return plen;
}
u_int32_t le_latest_vallen (LEAFENTRY le) {
    LESWITCHCALL(le, latest_vallen);
}

void* any_key_le_committed (u_int32_t UU(keylen), void *key, u_int32_t UU(vallen), void *UU(val)) {
    return key;
}
void* any_key_le_both (TXNID UU(xid), u_int32_t UU(klen), void *kval, u_int32_t UU(clen), void *UU(cval), u_int32_t UU(plen), void *UU(pval)) {
    return kval;
}
void* any_key_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *kval, u_int32_t UU(clen), void *UU(cval)) {
    return kval;
}
void* any_key_le_provpair (TXNID UU(xid), u_int32_t UU(klen), void *kval, u_int32_t UU(plen), void *UU(pval)) {
    return kval;
}
void* le_any_key (LEAFENTRY le) {
    LESWITCHCALL(le, any_key);
}

u_int32_t any_keylen_le_committed (u_int32_t keylen, void *UU(key), u_int32_t UU(vallen), void *UU(val)) {
    return keylen;
}
u_int32_t any_keylen_le_both (TXNID UU(xid), u_int32_t klen, void *UU(kval), u_int32_t UU(clen), void *UU(cval), u_int32_t UU(plen), void *UU(pval)) {
    return klen;
}
u_int32_t any_keylen_le_provdel (TXNID UU(xid), u_int32_t klen, void *UU(kval), u_int32_t UU(clen), void *UU(cval)) {
    return klen;
}
u_int32_t any_keylen_le_provpair (TXNID UU(xid), u_int32_t klen, void *UU(kval), u_int32_t UU(plen), void *UU(pval)) {
    return klen;
}
u_int32_t le_any_keylen (LEAFENTRY le) {
    LESWITCHCALL(le, any_keylen);
}

void* any_val_le_committed (u_int32_t UU(keylen), void *UU(key), u_int32_t UU(vallen), void *UU(val)) {
    return val;
}
void* any_val_le_both (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *cval, u_int32_t UU(plen), void *UU(pval)) {
    return cval;
}
void* any_val_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *cval) {
    return cval;
}
void* any_val_le_provpair (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(plen), void *pval) {
    return pval;
}
void* le_any_val (LEAFENTRY le) {
    LESWITCHCALL(le, any_val);
}

u_int32_t any_vallen_le_committed (u_int32_t UU(keylen), void *UU(key), u_int32_t vallen, void *UU(val)) {
    return vallen;
}
u_int32_t any_vallen_le_both (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t UU(clen), void *UU(cval), u_int32_t plen, void *UU(pval)) {
    return plen;
}
u_int32_t any_vallen_le_provdel (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t clen, void *UU(cval)) {
    return clen; // for provisional delete, there is no *any* key, so return 0.  What else can we do?
}
u_int32_t any_vallen_le_provpair (TXNID UU(xid), u_int32_t UU(klen), void *UU(kval), u_int32_t plen, void *UU(pval)) {
    return plen;
}
u_int32_t le_any_vallen (LEAFENTRY le) {
    LESWITCHCALL(le, any_vallen);
}
