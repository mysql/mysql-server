/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * Some parts of this code originally written by Adam Stubblefield,
 * -- astubble@rice.edu.
 *
 * $Id: hmac.c,v 1.27 2004/01/28 03:36:11 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/db_page.h"	/* for hash.h only */
#include "dbinc/hash.h"
#include "dbinc/hmac.h"

#define	HMAC_OUTPUT_SIZE	20
#define	HMAC_BLOCK_SIZE	64

static void __db_hmac __P((u_int8_t *, u_int8_t *, size_t, u_int8_t *));

/*
 * !!!
 * All of these functions use a ctx structure on the stack.  The __db_SHA1Init
 * call does not initialize the 64-byte buffer portion of it.  The
 * underlying SHA1 functions will properly pad the buffer if the data length
 * is less than 64-bytes, so there isn't a chance of reading uninitialized
 * memory.  Although it would be cleaner to do a memset(ctx.buffer, 0, 64)
 * we do not want to incur that penalty if we don't have to for performance.
 */

/*
 * __db_hmac --
 *	Do a hashed MAC.
 */
static void
__db_hmac(k, data, data_len, mac)
	u_int8_t *k, *data, *mac;
	size_t data_len;
{
	SHA1_CTX ctx;
	u_int8_t key[HMAC_BLOCK_SIZE];
	u_int8_t ipad[HMAC_BLOCK_SIZE];
	u_int8_t opad[HMAC_BLOCK_SIZE];
	u_int8_t tmp[HMAC_OUTPUT_SIZE];
	int i;

	memset(key, 0x00, HMAC_BLOCK_SIZE);
	memset(ipad, 0x36, HMAC_BLOCK_SIZE);
	memset(opad, 0x5C, HMAC_BLOCK_SIZE);

	memcpy(key, k, HMAC_OUTPUT_SIZE);

	for (i = 0; i < HMAC_BLOCK_SIZE; i++) {
		ipad[i] ^= key[i];
		opad[i] ^= key[i];
	}

	__db_SHA1Init(&ctx);
	__db_SHA1Update(&ctx, ipad, HMAC_BLOCK_SIZE);
	__db_SHA1Update(&ctx, data, data_len);
	__db_SHA1Final(tmp, &ctx);
	__db_SHA1Init(&ctx);
	__db_SHA1Update(&ctx, opad, HMAC_BLOCK_SIZE);
	__db_SHA1Update(&ctx, tmp, HMAC_OUTPUT_SIZE);
	__db_SHA1Final(mac, &ctx);
	return;
}

/*
 * __db_chksum --
 *	Create a MAC/SHA1 checksum.
 *
 * PUBLIC: void __db_chksum __P((u_int8_t *, size_t, u_int8_t *, u_int8_t *));
 */
void
__db_chksum(data, data_len, mac_key, store)
	u_int8_t *data;
	size_t data_len;
	u_int8_t *mac_key;
	u_int8_t *store;
{
	int sumlen;
	u_int32_t hash4;
	u_int8_t tmp[DB_MAC_KEY];

	/*
	 * Since the checksum might be on a page of data we are checksumming
	 * we might be overwriting after checksumming, we zero-out the
	 * checksum value so that we can have a known value there when
	 * we verify the checksum.
	 */
	if (mac_key == NULL)
		sumlen = sizeof(u_int32_t);
	else
		sumlen = DB_MAC_KEY;
	memset(store, 0, sumlen);
	if (mac_key == NULL) {
		/* Just a hash, no MAC */
		hash4 = __ham_func4(NULL, data, (u_int32_t)data_len);
		memcpy(store, &hash4, sumlen);
	} else {
		memset(tmp, 0, DB_MAC_KEY);
		__db_hmac(mac_key, data, data_len, tmp);
		memcpy(store, tmp, sumlen);
	}
	return;
}
/*
 * __db_derive_mac --
 *	Create a MAC/SHA1 key.
 *
 * PUBLIC: void __db_derive_mac __P((u_int8_t *, size_t, u_int8_t *));
 */
void
__db_derive_mac(passwd, plen, mac_key)
	u_int8_t *passwd;
	size_t plen;
	u_int8_t *mac_key;
{
	SHA1_CTX ctx;

	/* Compute the MAC key. mac_key must be 20 bytes. */
	__db_SHA1Init(&ctx);
	__db_SHA1Update(&ctx, passwd, plen);
	__db_SHA1Update(&ctx, (u_int8_t *)DB_MAC_MAGIC, strlen(DB_MAC_MAGIC));
	__db_SHA1Update(&ctx, passwd, plen);
	__db_SHA1Final(mac_key, &ctx);

	return;
}

/*
 * __db_check_chksum --
 *	Verify a checksum.
 *
 *	Return 0 on success, >0 (errno) on error, -1 on checksum mismatch.
 *
 * PUBLIC: int __db_check_chksum __P((DB_ENV *,
 * PUBLIC:     DB_CIPHER *, u_int8_t *, void *, size_t, int));
 */
int
__db_check_chksum(dbenv, db_cipher, chksum, data, data_len, is_hmac)
	DB_ENV *dbenv;
	DB_CIPHER *db_cipher;
	u_int8_t *chksum;
	void *data;
	size_t data_len;
	int is_hmac;
{
	int ret;
	size_t sum_len;
	u_int32_t hash4;
	u_int8_t *mac_key, old[DB_MAC_KEY], new[DB_MAC_KEY];

	/*
	 * If we are just doing checksumming and not encryption, then checksum
	 * is 4 bytes.  Otherwise, it is DB_MAC_KEY size.  Check for illegal
	 * combinations of crypto/non-crypto checksums.
	 */
	if (is_hmac == 0) {
		if (db_cipher != NULL) {
			__db_err(dbenv,
    "Unencrypted checksum with a supplied encryption key");
			return (EINVAL);
		}
		sum_len = sizeof(u_int32_t);
		mac_key = NULL;
	} else {
		if (db_cipher == NULL) {
			__db_err(dbenv,
    "Encrypted checksum: no encryption key specified");
			return (EINVAL);
		}
		sum_len = DB_MAC_KEY;
		mac_key = db_cipher->mac_key;
	}

	/*
	 * !!!
	 * Since the checksum might be on the page, we need to have known data
	 * there so that we can generate the same original checksum.  We zero
	 * it out, just like we do in __db_chksum above.
	 */
	memcpy(old, chksum, sum_len);
	memset(chksum, 0, sum_len);
	if (mac_key == NULL) {
		/* Just a hash, no MAC */
		hash4 = __ham_func4(NULL, data, (u_int32_t)data_len);
		ret = memcmp((u_int32_t *)old, &hash4, sum_len) ? -1 : 0;
	} else {
		__db_hmac(mac_key, data, data_len, new);
		ret = memcmp(old, new, sum_len) ? -1 : 0;
	}

	return (ret);
}
