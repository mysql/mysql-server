/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#ifndef KEY_VAL_H
#define KEY_VAL_H
//
//   Functions to create unique key/value pairs, row generators, checkers, ... for each of NUM_DBS
//

//   To use, during initialization:
//     generate_permute_tables();
//     r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
//

#if defined(__cilkplusplus) || defined (__cplusplus)
extern "C" {
#endif

enum {MAX_DBS=32};
enum {MAGIC=311};

//   a is the bit-wise permute table.  For DB[i], permute bits as described in a[i] using 'twiddle32'
// inv is the inverse bit-wise permute of a[].  To get the original value from a twiddled value, twiddle32 (again) with inv[]
int   a[MAX_DBS][32];
int inv[MAX_DBS][32];

// rotate right and left functions
static inline uint32_t UU() rotr32(const uint32_t x, const uint32_t num) {
    const uint32_t n = num % 32;
    return (x >> n) | ( x << (32 - n));
}
static inline uint64_t UU() rotr64(const uint64_t x, const uint64_t num) {
    const uint64_t n = num % 64;
    return ( x >> n ) | ( x << (64 - n));
}
static inline uint32_t UU() rotl32(const uint32_t x, const uint32_t num) {
    const uint32_t n = num % 32;
    return (x << n) | ( x >> (32 - n));
}
static inline uint64_t UU() rotl64(const uint64_t x, const uint64_t num) {
    const uint64_t n = num % 64;
    return ( x << n ) | ( x >> (64 - n));
}

static void UU() generate_permute_tables(void) {
    int i, j, tmp;
    for(int db=0;db<MAX_DBS;db++) {
        for(i=0;i<32;i++) {
            a[db][i] = i;
        }
        for(i=0;i<32;i++) {
            j = random() % (i + 1);
            tmp = a[db][j];
            a[db][j] = a[db][i];
            a[db][i] = tmp;
        }
//        if(db < NUM_DBS){ printf("a[%d] = ", db); for(i=0;i<32;i++) { printf("%2d ", a[db][i]); } printf("\n");}
        for(i=0;i<32;i++) {
            inv[db][a[db][i]] = i;
        }
    }
}

// permute bits of x based on permute table bitmap
static uint32_t UU() twiddle32(uint32_t x, int db)
{
    uint32_t b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << a[db][i];
    }
    return b;
}

// permute bits of x based on inverse permute table bitmap
static uint32_t UU() inv_twiddle32(uint32_t x, int db)
{
    uint32_t b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << inv[db][i];
    }
    return b;
}

// generate val from key, index
static uint32_t UU() generate_val(int key, int i) {
    return rotl32((key + MAGIC), i);
}
static uint32_t UU() pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}

// There is no handlerton in this test, so this function is a local replacement
// for the handlerton's generate_row_for_put().
static int UU() put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {

    src_db = src_db;

    uint32_t which = *(uint32_t*)dest_db->app_private;

    if ( which == 0 ) {
        if (dest_key->flags==DB_DBT_REALLOC) {
            if (dest_key->data) toku_free(dest_key->data);
            dest_key->flags = 0;
            dest_key->ulen  = 0;
        }
        if (dest_val->flags==DB_DBT_REALLOC) {
            if (dest_val->data) toku_free(dest_val->data);
            dest_val->flags = 0;
            dest_val->ulen  = 0;
        }
        dbt_init(dest_key, src_key->data, src_key->size);
        dbt_init(dest_val, src_val->data, src_val->size);
    }
    else {
        assert(dest_key->flags==DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(uint32_t)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(uint32_t));
            dest_key->ulen = sizeof(uint32_t);
        }
        assert(dest_val->flags==DB_DBT_REALLOC);
        if (dest_val->ulen < sizeof(uint32_t)) {
            dest_val->data = toku_xrealloc(dest_val->data, sizeof(uint32_t));
            dest_val->ulen = sizeof(uint32_t);
        }
        uint32_t *new_key = (uint32_t *)dest_key->data;
        uint32_t *new_val = (uint32_t *)dest_val->data;

        *new_key = twiddle32(*(uint32_t*)src_key->data, which);
        *new_val = generate_val(*(uint32_t*)src_key->data, which);

        dest_key->size = sizeof(uint32_t);
        dest_val->size = sizeof(uint32_t);
        //data is already set above
    }

//    printf("pmg : dest_key.data = %u, dest_val.data = %u \n", *(unsigned int*)dest_key->data, *(unsigned int*)dest_val->data);

    return 0;
}

static int UU() uint_cmp(const void *ap, const void *bp) {
    unsigned int an = *(unsigned int *)ap;
    unsigned int bn = *(unsigned int *)bp;
    if (an < bn) 
        return -1;
    if (an > bn)
        return +1;
    return 0;
}


#if defined(__cilkplusplus) || defined(__cplusplus)
} // extern "C"
#endif

#endif // KEY_VAL_H
