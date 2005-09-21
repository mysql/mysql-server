/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * Some parts of this code originally written by Adam Stubblefield
 * -- astubble@rice.edu
 *
 * $Id: crypto.c,v 1.31 2004/10/15 16:59:38 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/crypto.h"

/*
 * __crypto_region_init --
 *	Initialize crypto.
 */
int
__crypto_region_init(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop;
	CIPHER *cipher;
	DB_CIPHER *db_cipher;
	char *sh_passwd;
	int ret;

	db_cipher = dbenv->crypto_handle;

	ret = 0;
	infop = dbenv->reginfo;
	renv = infop->primary;
	MUTEX_LOCK(dbenv, &renv->mutex);
	if (renv->cipher_off == INVALID_ROFF) {
		if (!CRYPTO_ON(dbenv))
			goto err;
		if (!F_ISSET(infop, REGION_CREATE)) {
			__db_err(dbenv,
    "Joining non-encrypted environment with encryption key");
			ret = EINVAL;
			goto err;
		}
		if (F_ISSET(db_cipher, CIPHER_ANY)) {
			__db_err(dbenv, "Encryption algorithm not supplied");
			ret = EINVAL;
			goto err;
		}
		/*
		 * Must create the shared information.  We need:
		 * Shared cipher information that contains the passwd.
		 * After we copy the passwd, we smash and free the one in the
		 * dbenv.
		 */
		if ((ret = __db_shalloc(
		    infop, sizeof(CIPHER), MUTEX_ALIGN, &cipher)) != 0)
			goto err;
		memset(cipher, 0, sizeof(*cipher));
		if ((ret = __db_shalloc(
		    infop, dbenv->passwd_len, 0, &sh_passwd)) != 0) {
			__db_shalloc_free(infop, cipher);
			goto err;
		}
		memset(sh_passwd, 0, dbenv->passwd_len);
		cipher->passwd = R_OFFSET(infop, sh_passwd);
		cipher->passwd_len = dbenv->passwd_len;
		cipher->flags = db_cipher->alg;
		memcpy(sh_passwd, dbenv->passwd, cipher->passwd_len);
		renv->cipher_off = R_OFFSET(infop, cipher);
	} else {
		if (!CRYPTO_ON(dbenv)) {
			__db_err(dbenv,
    "Encrypted environment: no encryption key supplied");
			ret = EINVAL;
			goto err;
		}
		cipher = R_ADDR(infop, renv->cipher_off);
		sh_passwd = R_ADDR(infop, cipher->passwd);
		if ((cipher->passwd_len != dbenv->passwd_len) ||
		    memcmp(dbenv->passwd, sh_passwd, cipher->passwd_len) != 0) {
			__db_err(dbenv, "Invalid password");
			ret = EPERM;
			goto err;
		}
		if (!F_ISSET(db_cipher, CIPHER_ANY) &&
		    db_cipher->alg != cipher->flags) {
			__db_err(dbenv,
    "Environment encrypted using a different algorithm");
			ret = EINVAL;
			goto err;
		}
		if (F_ISSET(db_cipher, CIPHER_ANY))
			/*
			 * We have CIPHER_ANY and we are joining the
			 * existing env.  Setup our cipher structure
			 * for whatever algorithm this env has.
			 */
			if ((ret = __crypto_algsetup(dbenv, db_cipher,
			    cipher->flags, 0)) != 0)
				goto err;
	}
	MUTEX_UNLOCK(dbenv, &renv->mutex);
	ret = db_cipher->init(dbenv, db_cipher);

	/*
	 * On success, no matter if we allocated it or are using the
	 * already existing one, we are done with the passwd in the dbenv.
	 * We smash N-1 bytes so that we don't overwrite the nul.
	 */
	memset(dbenv->passwd, 0xff, dbenv->passwd_len-1);
	__os_free(dbenv, dbenv->passwd);
	dbenv->passwd = NULL;
	dbenv->passwd_len = 0;

	if (0) {
err:		MUTEX_UNLOCK(dbenv, &renv->mutex);
	}
	return (ret);
}

/*
 * __crypto_dbenv_close --
 *	Crypto-specific destruction of DB_ENV structure.
 *
 * PUBLIC: int __crypto_dbenv_close __P((DB_ENV *));
 */
int
__crypto_dbenv_close(dbenv)
	DB_ENV *dbenv;
{
	DB_CIPHER *db_cipher;
	int ret;

	ret = 0;
	db_cipher = dbenv->crypto_handle;
	if (dbenv->passwd != NULL) {
		memset(dbenv->passwd, 0xff, dbenv->passwd_len-1);
		__os_free(dbenv, dbenv->passwd);
		dbenv->passwd = NULL;
	}
	if (!CRYPTO_ON(dbenv))
		return (0);
	if (!F_ISSET(db_cipher, CIPHER_ANY))
		ret = db_cipher->close(dbenv, db_cipher->data);
	__os_free(dbenv, db_cipher);
	return (ret);
}

/*
 * __crypto_region_destroy --
 *	Destroy any system resources allocated in the primary region.
 *
 * PUBLIC: int __crypto_region_destroy __P((DB_ENV *));
 */
int
__crypto_region_destroy(dbenv)
	DB_ENV *dbenv;
{
	CIPHER *cipher;
	REGENV *renv;
	REGINFO *infop;

	infop = dbenv->reginfo;
	renv = infop->primary;
	if (renv->cipher_off != INVALID_ROFF) {
		cipher = R_ADDR(infop, renv->cipher_off);
		__db_shalloc_free(infop, R_ADDR(infop, cipher->passwd));
		__db_shalloc_free(infop, cipher);
	}
	return (0);
}

/*
 * __crypto_algsetup --
 *	Given a db_cipher structure and a valid algorithm flag, call
 * the specific algorithm setup function.
 *
 * PUBLIC: int __crypto_algsetup __P((DB_ENV *, DB_CIPHER *, u_int32_t, int));
 */
int
__crypto_algsetup(dbenv, db_cipher, alg, do_init)
	DB_ENV *dbenv;
	DB_CIPHER *db_cipher;
	u_int32_t alg;
	int do_init;
{
	int ret;

	ret = 0;
	if (!CRYPTO_ON(dbenv)) {
		__db_err(dbenv, "No cipher structure given");
		return (EINVAL);
	}
	F_CLR(db_cipher, CIPHER_ANY);
	switch (alg) {
	case CIPHER_AES:
		db_cipher->alg = CIPHER_AES;
		ret = __aes_setup(dbenv, db_cipher);
		break;
	default:
		__db_panic(dbenv, EINVAL);
		/* NOTREACHED */
	}
	if (do_init)
		ret = db_cipher->init(dbenv, db_cipher);
	return (ret);
}

/*
 * __crypto_decrypt_meta --
 *	Perform decryption on a metapage if needed.
 *
 * PUBLIC:  int __crypto_decrypt_meta __P((DB_ENV *, DB *, u_int8_t *, int));
 */
int
__crypto_decrypt_meta(dbenv, dbp, mbuf, do_metachk)
	DB_ENV *dbenv;
	DB *dbp;
	u_int8_t *mbuf;
	int do_metachk;
{
	DB_CIPHER *db_cipher;
	DB dummydb;
	DBMETA *meta;
	size_t pg_off;
	int ret;
	u_int8_t *iv;

	/*
	 * If we weren't given a dbp, we just want to decrypt the page
	 * on behalf of some internal subsystem, not on behalf of a user
	 * with a dbp.  Therefore, set up a dummy dbp so that the call
	 * to P_OVERHEAD below works.
	 */
	if (dbp == NULL) {
		memset(&dummydb, 0, sizeof(DB));
		dbp = &dummydb;
	}
	/*
	 * Meta-pages may be encrypted for DBMETASIZE bytes.  If
	 * we have a non-zero IV (that is written after encryption)
	 * then we decrypt (or error if the user isn't set up for
	 * security).  We guarantee that the IV space on non-encrypted
	 * pages will be zero and a zero-IV is illegal for encryption.
	 * Therefore any non-zero IV means an encrypted database.
	 * This basically checks the passwd on the file
	 * if we cannot find a good magic number.
	 * We walk through all the algorithms we know about attempting
	 * to decrypt (and possibly byteswap).
	 *
	 * !!!
	 * All method meta pages have the IV and checksum at the
	 * exact same location, but not in DBMETA, use BTMETA.
	 */
	ret = 0;
	meta = (DBMETA *)mbuf;
	if (meta->encrypt_alg != 0) {
		db_cipher = (DB_CIPHER *)dbenv->crypto_handle;
		if (!F_ISSET(dbp, DB_AM_ENCRYPT)) {
			if (!CRYPTO_ON(dbenv)) {
				__db_err(dbenv,
    "Encrypted database: no encryption flag specified");
				return (EINVAL);
			}
			/*
			 * User has a correct, secure env, but has
			 * encountered a database in that env that is
			 * secure, but user didn't dbp->set_flags.  Since
			 * it is existing, use encryption if it is that
			 * way already.
			 */
			F_SET(dbp, DB_AM_ENCRYPT|DB_AM_CHKSUM);
		}
		/*
		 * This was checked in set_flags when DB_AM_ENCRYPT was set.
		 * So it better still be true here.
		 */
		DB_ASSERT(CRYPTO_ON(dbenv));
		if (!F_ISSET(db_cipher, CIPHER_ANY) &&
		    meta->encrypt_alg != db_cipher->alg) {
			__db_err(dbenv,
			    "Database encrypted using a different algorithm");
			return (EINVAL);
		}
		DB_ASSERT(F_ISSET(dbp, DB_AM_CHKSUM));
		iv = ((BTMETA *)mbuf)->iv;
		/*
		 * For ALL pages, we do not encrypt the beginning
		 * of the page that contains overhead information.
		 * This is true of meta and all other pages.
		 */
		pg_off = P_OVERHEAD(dbp);
alg_retry:
		/*
		 * If they asked for a specific algorithm, then
		 * use it.  Otherwise walk through those we know.
		 */
		if (!F_ISSET(db_cipher, CIPHER_ANY)) {
			if (do_metachk && (ret = db_cipher->decrypt(dbenv,
			    db_cipher->data, iv, mbuf + pg_off,
			    DBMETASIZE - pg_off)))
				return (ret);
			if (((BTMETA *)meta)->crypto_magic !=
			    meta->magic) {
				__db_err(dbenv, "Invalid password");
				return (EINVAL);
			}
			/*
			 * Success here.  The algorithm asked for and the one
			 * on the file match.  We've just decrypted the meta
			 * page and checked the magic numbers.  They match,
			 * indicating the password is right.  All is right
			 * with the world.
			 */
			return (0);
		}
		/*
		 * If we get here, CIPHER_ANY must be set.
		 */
		ret = __crypto_algsetup(dbenv, db_cipher, meta->encrypt_alg, 1);
		goto alg_retry;
	} else if (F_ISSET(dbp, DB_AM_ENCRYPT)) {
		/*
		 * They gave us a passwd, but the database is not
		 * encrypted.  This is an error.  We do NOT want to
		 * silently allow them to write data in the clear when
		 * the user set up and expects encrypted data.
		 *
		 * This covers at least the following scenario.
		 * 1.  User creates and sets up an encrypted database.
		 * 2.  Attacker cannot read the actual data in the database
		 * because it is encrypted, but can remove/replace the file
		 * with an empty, unencrypted database file.
		 * 3.  User sets encryption and we get to this code now.
		 * If we allowed the file to be used in the clear since
		 * it is that way on disk, the user would unsuspectingly
		 * write sensitive data in the clear.
		 * 4.  Attacker reads data that user thought was encrypted.
		 *
		 * Therefore, asking for encryption with a database that
		 * was not encrypted is an error.
		 */
		__db_err(dbenv,
		    "Unencrypted database with a supplied encryption key");
		return (EINVAL);
	}
	return (ret);
}

/*
 * __crypto_set_passwd --
 *	Get the password from the shared region; and set it in a new
 * environment handle.  Use this to duplicate environment handles.
 *
 * PUBLIC: int __crypto_set_passwd __P((DB_ENV *, DB_ENV *));
 */
int
__crypto_set_passwd(dbenv_src, dbenv_dest)
	DB_ENV *dbenv_src, *dbenv_dest;
{
	CIPHER *cipher;
	REGENV *renv;
	REGINFO *infop;
	char *sh_passwd;
	int ret;

	ret = 0;
	infop = dbenv_src->reginfo;
	renv = infop->primary;

	DB_ASSERT(CRYPTO_ON(dbenv_src));

	cipher = R_ADDR(infop, renv->cipher_off);
	sh_passwd = R_ADDR(infop, cipher->passwd);
	return (__dbenv_set_encrypt(dbenv_dest, sh_passwd, DB_ENCRYPT_AES));
}
