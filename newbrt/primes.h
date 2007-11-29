#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Return the smallest prime >= 2^(idx+1)
 * Only works for idx<30 */
int toku_get_prime (unsigned int idx);
void toku_test_primes(void);
