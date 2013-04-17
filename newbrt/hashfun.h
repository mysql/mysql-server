#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// FNV Hash: From an idea sent by Glenn Fowler and Phong Vo to the IEEE POSIX 1003.2 committee.  Landon Curt Noll improved it.
// See: http://isthe.com/chongo/tech/comp/fnv/
static inline u_int32_t hash_key_extend(u_int32_t initial_hash,
                                        const unsigned char *key,
                                        size_t keylen) {
    size_t i;
    u_int32_t hash = initial_hash;
    for (i=0; i<keylen; i++, key++) {
        hash *= 16777619;
        // GCC 4.1.2 -O2 and -O3 translates the following shifts back into the multiply shown on the line above here.
        // So much for optimizing this multiplication...
        //hash += (hash<<1) + (hash<<4) + (hash<<7) + (hash<<8) + (hash<<24);
        hash ^= *key;
    }
    return hash;
}

static inline u_int32_t hash_key(const unsigned char *key, size_t keylen) {
    return hash_key_extend(0, key, keylen);
}

#if 0
static unsigned int hash_key (const char *key, ITEMLEN keylen) {
    /* From Sedgewick.  There are probably better hash functions. */
    unsigned int b    = 378551;
    unsigned int a    = 63689;
    unsigned int hash = 0;
    ITEMLEN i;
    for (i = 0; i < keylen; i++ ) {
	hash = hash * a + key[i];
	a *= b;
    }
    return hash;
}
#endif
