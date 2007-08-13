// FNV Hash: From an idea sent by Glenn Fowler and Phong Vo to the IEEE POSIX 1003.2 committee.  Landon Curt Noll improved it.
// See: http://isthe.com/chongo/tech/comp/fnv/
static inline unsigned int hash_key (const unsigned char *key, unsigned long keylen) {
    unsigned long i;
    unsigned int hash=0;
    for (i=0; i<keylen; i++, key++) {
	hash *= 16777619;
	hash ^= *key;
    }
    return hash;
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
