#include <toku_portability.h>
#include <string.h>

#include "test.h"
#include "brttypes.h"
#include "includes.h"
#include "ule.h"

enum {MAX_SIZE = 256};

static void
verify_ule_equal(ULE a, ULE b) {
    assert(a->num_uxrs > 0);
    assert(a->num_uxrs <= MAX_TRANSACTION_RECORDS);
    assert(a->num_uxrs == b->num_uxrs);
    assert(a->keylen   == b->keylen);
    assert(memcmp(a->keyp, b->keyp, a->keylen) == 0);
    u_int32_t i;
    for (i = 0; i < a->num_uxrs; i++) {
        assert(a->uxrs[i].type == b->uxrs[i].type);
        assert(a->uxrs[i].xid  == b->uxrs[i].xid);
        if (a->uxrs[i].type == XR_INSERT) {
            assert(a->uxrs[i].vallen  == b->uxrs[i].vallen);
            assert(memcmp(a->uxrs[i].valp, b->uxrs[i].valp, a->uxrs[i].vallen) == 0);
        }
    }
}

static void
fillrandom(u_int8_t buf[MAX_SIZE], u_int32_t length) {
    assert(length < MAX_SIZE);
    u_int32_t i;
    for (i = 0; i < length; i++) {
        buf[i] = random() & 0xFF;
    } 
}

static void
test_le_offset_is(LEAFENTRY le, void *field, size_t expected_offset) {
    size_t le_address    = (size_t) le;
    size_t field_address = (size_t) field;
    assert(field_address >= le_address);
    size_t actual_offset = field_address - le_address;
    assert(actual_offset == expected_offset);
}

//Fixed offsets in a packed leafentry.
enum {
    LE_OFFSET_NUM      = 0,
    LE_OFFSET_KEYLEN   = 1+LE_OFFSET_NUM,
    LE_OFFSET_VALLEN   = 4+LE_OFFSET_KEYLEN, //Vallen of innermost insert record
    LE_OFFSET_VARIABLE = 4+LE_OFFSET_VALLEN
};

static void
test_le_fixed_offsets (void) {
    LEAFENTRY XMALLOC(le);
    test_le_offset_is(le, &le->num_xrs,                    LE_OFFSET_NUM);
    test_le_offset_is(le, &le->keylen,                     LE_OFFSET_KEYLEN);
    test_le_offset_is(le, &le->innermost_inserted_vallen,  LE_OFFSET_VALLEN);
    toku_free(le);
}

//Fixed offsets in a leafentry with no uncommitted transaction records.
//(Note, there is no type required.) 
enum {
    LE_COMMITTED_OFFSET_KEY    = LE_OFFSET_VARIABLE
};

static void
test_le_committed_offsets (void) {
    LEAFENTRY XMALLOC(le);
    test_le_offset_is(le, &le->u.comm.key_val, LE_COMMITTED_OFFSET_KEY);
    toku_free(le);
}

//Fixed offsets in a leafentry with uncommitted transaction records.
enum {
    LE_PROVISIONAL_OFFSET_TYPE   = LE_OFFSET_VARIABLE, //Type of innermost record
    LE_PROVISIONAL_OFFSET_XID    = 1+LE_PROVISIONAL_OFFSET_TYPE, //XID of outermost noncommitted record
    LE_PROVISIONAL_OFFSET_KEY    = 8+LE_PROVISIONAL_OFFSET_XID
};

static void
test_le_provisional_offsets (void) {
    LEAFENTRY XMALLOC(le);
    test_le_offset_is(le, &le->u.prov.innermost_type,            LE_PROVISIONAL_OFFSET_TYPE);
    test_le_offset_is(le, &le->u.prov.xid_outermost_uncommitted, LE_PROVISIONAL_OFFSET_XID);
    test_le_offset_is(le, &le->u.prov.key_val_xrs,               LE_PROVISIONAL_OFFSET_KEY);
    toku_free(le);
}

//We use a packed struct to represent a leafentry.
//We want to make sure the compiler correctly represents the offsets.
//This test verifies all offsets in a packed leafentry correspond to the required memory format.
static void
test_le_offsets (void) {
    test_le_fixed_offsets();
    test_le_committed_offsets();
    test_le_provisional_offsets();
}

static void
test_ule_packs_to_nothing (ULE ule) {
    size_t memsize;
    size_t disksize;
    LEAFENTRY le;
    int r = le_pack(ule,
                    &memsize, &disksize,
                    &le, NULL, NULL, NULL);
    assert(r==0);
    assert(le==NULL);
}

//A leafentry must contain at least one 'insert' (all deletes means the leafentry
//should not exist).
//Verify that 'le_pack' of any set of all deletes ends up not creating a leafentry.
static void
test_le_empty_packs_to_nothing (void) {
    ULE_S ule;

    int key = random(); //Arbitrary number
    //Set up defaults.
    ule.keylen       = sizeof(key);
    ule.keyp         = &key;
    ule.uxrs[0].type = XR_DELETE;
    ule.uxrs[0].xid  = 0;
    u_int8_t num_xrs;
    for (num_xrs = 1; num_xrs < MAX_TRANSACTION_RECORDS; num_xrs++) {
        if (num_xrs > 1) {
            ule.uxrs[num_xrs-1].type = XR_DELETE,
            ule.uxrs[num_xrs-1].xid  = ule.uxrs[num_xrs-2].xid + (random() % 32 + 1); //Abitrary number, xids must be strictly increasing
        }
        ule.num_uxrs = num_xrs;
        test_ule_packs_to_nothing(&ule);
        if (num_xrs > 2 && num_xrs % 4) {
            //Set some of them to placeholders instead of deletes
            ule.uxrs[num_xrs-2].type = XR_PLACEHOLDER;
        }
        test_ule_packs_to_nothing(&ule);
    }
}

static void
le_verify_accessors(LEAFENTRY le, ULE ule,
                    size_t pre_calculated_memsize,
                    size_t pre_calculated_disksize) {
    assert(le);
    assert(ule->num_uxrs > 0);
    assert(ule->num_uxrs <= MAX_TRANSACTION_RECORDS);
    assert(ule->uxrs[ule->num_uxrs-1].type != XR_PLACEHOLDER);
    //Extract expected values from ULE
    size_t memsize  = le_memsize_from_ule(ule);
    size_t disksize = le_memsize_from_ule(ule);

    void *latest_key        = ule->uxrs[ule->num_uxrs-1].type == XR_DELETE ? NULL : ule->keyp;
    u_int32_t latest_keylen = ule->uxrs[ule->num_uxrs-1].type == XR_DELETE ? 0    : ule->keylen;
    void *key               = ule->keyp;
    u_int32_t keylen        = ule->keylen;
    void *latest_val        = ule->uxrs[ule->num_uxrs-1].type == XR_DELETE ? NULL : ule->uxrs[ule->num_uxrs-1].valp;
    u_int32_t latest_vallen = ule->uxrs[ule->num_uxrs-1].type == XR_DELETE ? 0    : ule->uxrs[ule->num_uxrs-1].vallen;
    void *innermost_inserted_val;
    u_int32_t innermost_inserted_vallen;
    {
        int i;
        for (i = ule->num_uxrs - 1; i >= 0; i--) {
            if (ule->uxrs[i].type == XR_INSERT) {
                innermost_inserted_val    = ule->uxrs[i].valp;
                innermost_inserted_vallen = ule->uxrs[i].vallen;
                goto found_insert;
            }
        }
        assert(FALSE);
    }
found_insert:;
    TXNID outermost_uncommitted_xid = ule->num_uxrs == 1 ? 0 : ule->uxrs[1].xid;
    int   is_provdel = ule->uxrs[ule->num_uxrs-1].type == XR_DELETE;

    assert(le!=NULL);
    //Verify all accessors
    assert(memsize  == pre_calculated_memsize);
    assert(disksize == pre_calculated_disksize);
    assert(memsize  == disksize);
    assert(memsize  == leafentry_memsize(le));
    assert(disksize == leafentry_disksize(le));
    {
        u_int32_t test_keylen;
        void*     test_keyp = le_latest_key_and_len(le, &test_keylen);
        if (latest_key != NULL) assert(test_keyp != latest_key);
        assert(test_keylen == latest_keylen);
        assert(memcmp(test_keyp, latest_key, test_keylen) == 0);
        assert(le_latest_key(le)    == test_keyp);
        assert(le_latest_keylen(le) == test_keylen);
    }
    {
        u_int32_t test_keylen;
        void*     test_keyp = le_key_and_len(le, &test_keylen);
        if (key != NULL) assert(test_keyp != key);
        assert(test_keylen == keylen);
        assert(memcmp(test_keyp, key, test_keylen) == 0);
        assert(le_key(le)    == test_keyp);
        assert(le_keylen(le) == test_keylen);
    }
    {
        u_int32_t test_vallen;
        void*     test_valp = le_latest_val_and_len(le, &test_vallen);
        if (latest_val != NULL) assert(test_valp != latest_val);
        assert(test_vallen == latest_vallen);
        assert(memcmp(test_valp, latest_val, test_vallen) == 0);
        assert(le_latest_val(le)    == test_valp);
        assert(le_latest_vallen(le) == test_vallen);
    }
    {
        u_int32_t test_vallen;
        void*     test_valp = le_innermost_inserted_val_and_len(le, &test_vallen);
        if (innermost_inserted_val != NULL) assert(test_valp != innermost_inserted_val);
        assert(test_vallen == innermost_inserted_vallen);
        assert(memcmp(test_valp, innermost_inserted_val, test_vallen) == 0);
        assert(le_innermost_inserted_val(le)    == test_valp);
        assert(le_innermost_inserted_vallen(le) == test_vallen);
    }
    {
        assert(le_outermost_uncommitted_xid(le) == outermost_uncommitted_xid);
    }
    {
        assert((le_is_provdel(le)==0) == (is_provdel==0));
    }
}



static void
test_le_pack_committed (void) {
    ULE_S ule;

    u_int8_t key[MAX_SIZE];
    u_int8_t val[MAX_SIZE];
    u_int32_t keysize;
    u_int32_t valsize;
    for (keysize = 0; keysize < MAX_SIZE; keysize += (random() % MAX_SIZE) + 1) {
        for (valsize = 0; valsize < MAX_SIZE; valsize += (random() % MAX_SIZE) + 1) {
            fillrandom(key, keysize);
            fillrandom(val, valsize);

            ule.num_uxrs       = 1;
            ule.keylen         = keysize;
            ule.keyp           = key;
            ule.uxrs[0].type   = XR_INSERT;
            ule.uxrs[0].xid    = 0;
            ule.uxrs[0].valp   = val;
            ule.uxrs[0].vallen = valsize;

            size_t memsize;
            size_t disksize;
            LEAFENTRY le;
            int r = le_pack(&ule,
                            &memsize, &disksize,
                            &le, NULL, NULL, NULL);
            assert(r==0);
            assert(le!=NULL);
            le_verify_accessors(le, &ule, memsize, disksize);
            ULE_S tmp_ule;
            le_unpack(&tmp_ule, le);
            verify_ule_equal(&ule, &tmp_ule);
            LEAFENTRY tmp_le;
            size_t    tmp_memsize;
            size_t    tmp_disksize;
            r = le_pack(&tmp_ule,
                        &tmp_memsize, &tmp_disksize,
                        &tmp_le, NULL, NULL, NULL);
            assert(r==0);
            assert(tmp_memsize == memsize);
            assert(tmp_disksize == disksize);
            assert(memcmp(le, tmp_le, memsize) == 0);

            toku_free(tmp_le);
            toku_free(le);
        }
    }
}

static void
test_le_pack_uncommitted (u_int8_t committed_type, u_int8_t prov_type, int num_placeholders) {
    ULE_S ule;

    u_int8_t key[MAX_SIZE];
    u_int8_t cval[MAX_SIZE];
    u_int8_t pval[MAX_SIZE];
    u_int32_t keysize;
    u_int32_t cvalsize;
    u_int32_t pvalsize;
    for (keysize = 0; keysize < MAX_SIZE; keysize += (random() % MAX_SIZE) + 1) {
        for (cvalsize = 0; cvalsize < MAX_SIZE; cvalsize += (random() % MAX_SIZE) + 1) {
            pvalsize = (cvalsize + random()) % MAX_SIZE;
            fillrandom(key, keysize);
            if (committed_type == XR_INSERT)
                fillrandom(cval, cvalsize);
            if (prov_type == XR_INSERT)
                fillrandom(pval, pvalsize);
            ule.uxrs[0].type   = committed_type;
            ule.uxrs[0].xid    = 0;
            ule.uxrs[0].vallen = cvalsize;
            ule.uxrs[0].valp   = cval;
            ule.keylen         = keysize;
            ule.keyp           = key;
            ule.num_uxrs       = 2 + num_placeholders;

            u_int8_t idx;
            for (idx = 1; idx <= num_placeholders; idx++) {
                ule.uxrs[idx].type = XR_PLACEHOLDER;
                ule.uxrs[idx].xid  = ule.uxrs[idx-1].xid + (random() % 32 + 1); //Abitrary number, xids must be strictly increasing
            }
            ule.uxrs[idx].xid  = ule.uxrs[idx-1].xid + (random() % 32 + 1); //Abitrary number, xids must be strictly increasing
            ule.uxrs[idx].type   = prov_type;
            ule.uxrs[idx].vallen = pvalsize;
            ule.uxrs[idx].valp   = pval;

            size_t memsize;
            size_t disksize;
            LEAFENTRY le;
            int r = le_pack(&ule,
                            &memsize, &disksize,
                            &le, NULL, NULL, NULL);
            assert(r==0);
            assert(le!=NULL);
            le_verify_accessors(le, &ule, memsize, disksize);
            ULE_S tmp_ule;
            le_unpack(&tmp_ule, le);
            verify_ule_equal(&ule, &tmp_ule);
            LEAFENTRY tmp_le;
            size_t    tmp_memsize;
            size_t    tmp_disksize;
            r = le_pack(&tmp_ule,
                        &tmp_memsize, &tmp_disksize,
                        &tmp_le, NULL, NULL, NULL);
            assert(r==0);
            assert(tmp_memsize == memsize);
            assert(tmp_disksize == disksize);
            assert(memcmp(le, tmp_le, memsize) == 0);

            toku_free(tmp_le);
            toku_free(le);
        }
    }
}

static void
test_le_pack_provpair (int num_placeholders) {
    test_le_pack_uncommitted(XR_DELETE, XR_INSERT, num_placeholders);
}

static void
test_le_pack_provdel (int num_placeholders) {
    test_le_pack_uncommitted(XR_INSERT, XR_DELETE, num_placeholders);
}

static void
test_le_pack_both (int num_placeholders) {
    test_le_pack_uncommitted(XR_INSERT, XR_INSERT, num_placeholders);
}

//Test of PACK
//  Committed leafentry
//      delete -> nothing (le_empty_packs_to_nothing)
//      insert
//          make key/val have diff lengths/content
//  Uncommitted
//      committed delete
//          followed by placeholder*, delete (le_empty_packs_to_nothing)
//          followed by placeholder*, insert
//      committed insert
//          followed by placeholder*, delete
//          followed by placeholder*, insert
//          
//  placeholder* is 0,1, or 2 placeholders
static void
test_le_pack (void) {
    test_le_empty_packs_to_nothing();
    test_le_pack_committed();
    int i;
    for (i = 0; i < 3; i++) {
        test_le_pack_provpair(i);
        test_le_pack_provdel(i);
        test_le_pack_both(i);
    }
}

//TODO: #1125 tests:
//      Will probably have to expose ULE_S definition
//            - Check memsize function is correct
//             - Assert == disksize (almost useless, but go ahead)
//            - Check standard accessors
//             - le_latest_key_and_len
//             - le_latest_key 
//             - le_latest_keylen
//             - le_latest_val_and_len
//             - le_latest_val 
//             - le_latest_vallen
//             - le_key_and_len
//             - le_key 
//             - le_keylen
//             - le_innermost_inserted_val_and_len
//             - le_innermost_inserted_val 
//             - le_innermost_inserted_vallen
//            - Check le_outermost_uncommitted_xid
//            - Check le_is_provdel
//            - Check unpack+pack memcmps equal
//            - Check exact memory expected (including size) for various leafentry types.
//            - Check apply_msg logic
//             - Known start, known expected.. various types.
//            - Go through test-leafentry10.c
//             - Verify we have tests for all analogous stuff.
//
//  PACK
//  UNPACK
//      verify pack+unpack is no-op
//      verify unpack+pack is no-op
//  accessors
//  Test apply_msg logic
//      i.e. start with LE, apply message
//          in parallel, construct the expected ULE manually, and pack that
//          Compare the two results
//  Test full_promote

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    srandom(7); //Arbitrary seed.
    test_le_offsets();
    test_le_pack();
    return 0;
}
