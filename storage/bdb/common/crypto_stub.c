/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: crypto_stub.c,v 1.4 2004/01/28 03:35:52 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __crypto_region_init --
 *	Initialize crypto.
 *
 *
 * !!!
 * We don't put this stub file in the crypto/ directory of the distribution
 * because that entire directory is removed for non-crypto distributions.
 *
 * PUBLIC: int __crypto_region_init __P((DB_ENV *));
 */
int
__crypto_region_init(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop;
	int ret;

	infop = dbenv->reginfo;
	renv = infop->primary;
	MUTEX_LOCK(dbenv, &renv->mutex);
	ret = !(renv->cipher_off == INVALID_ROFF);
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	if (ret == 0)
		return (0);

	__db_err(dbenv,
"Encrypted environment: library build did not include cryptography support");
	return (DB_OPNOTSUP);
}
