/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: hash_method.c,v 11.17 2004/01/28 03:36:11 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/hash.h"

static int __ham_set_h_ffactor __P((DB *, u_int32_t));
static int __ham_set_h_hash
	       __P((DB *, u_int32_t(*)(DB *, const void *, u_int32_t)));
static int __ham_set_h_nelem __P((DB *, u_int32_t));

/*
 * __ham_db_create --
 *	Hash specific initialization of the DB structure.
 *
 * PUBLIC: int __ham_db_create __P((DB *));
 */
int
__ham_db_create(dbp)
	DB *dbp;
{
	HASH *hashp;
	int ret;

	if ((ret = __os_malloc(dbp->dbenv,
	    sizeof(HASH), &dbp->h_internal)) != 0)
		return (ret);

	hashp = dbp->h_internal;

	hashp->h_nelem = 0;			/* Defaults. */
	hashp->h_ffactor = 0;
	hashp->h_hash = NULL;

	dbp->get_h_ffactor = __ham_get_h_ffactor;
	dbp->set_h_ffactor = __ham_set_h_ffactor;
	dbp->set_h_hash = __ham_set_h_hash;
	dbp->get_h_nelem = __ham_get_h_nelem;
	dbp->set_h_nelem = __ham_set_h_nelem;

	return (0);
}

/*
 * PUBLIC: int __ham_db_close __P((DB *));
 */
int
__ham_db_close(dbp)
	DB *dbp;
{
	if (dbp->h_internal == NULL)
		return (0);
	__os_free(dbp->dbenv, dbp->h_internal);
	dbp->h_internal = NULL;
	return (0);
}

/*
 * __ham_get_h_ffactor --
 *
 * PUBLIC: int __ham_get_h_ffactor __P((DB *, u_int32_t *));
 */
int
__ham_get_h_ffactor(dbp, h_ffactorp)
	DB *dbp;
	u_int32_t *h_ffactorp;
{
	HASH *hashp;

	hashp = dbp->h_internal;
	*h_ffactorp = hashp->h_ffactor;
	return (0);
}

/*
 * __ham_set_h_ffactor --
 *	Set the fill factor.
 */
static int
__ham_set_h_ffactor(dbp, h_ffactor)
	DB *dbp;
	u_int32_t h_ffactor;
{
	HASH *hashp;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_h_ffactor");
	DB_ILLEGAL_METHOD(dbp, DB_OK_HASH);

	hashp = dbp->h_internal;
	hashp->h_ffactor = h_ffactor;
	return (0);
}

/*
 * __ham_set_h_hash --
 *	Set the hash function.
 */
static int
__ham_set_h_hash(dbp, func)
	DB *dbp;
	u_int32_t (*func) __P((DB *, const void *, u_int32_t));
{
	HASH *hashp;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_h_hash");
	DB_ILLEGAL_METHOD(dbp, DB_OK_HASH);

	hashp = dbp->h_internal;
	hashp->h_hash = func;
	return (0);
}

/*
 * __db_get_h_nelem --
 *
 * PUBLIC: int __ham_get_h_nelem __P((DB *, u_int32_t *));
 */
int
__ham_get_h_nelem(dbp, h_nelemp)
	DB *dbp;
	u_int32_t *h_nelemp;
{
	HASH *hashp;

	DB_ILLEGAL_METHOD(dbp, DB_OK_HASH);

	hashp = dbp->h_internal;
	*h_nelemp = hashp->h_nelem;
	return (0);
}

/*
 * __ham_set_h_nelem --
 *	Set the table size.
 */
static int
__ham_set_h_nelem(dbp, h_nelem)
	DB *dbp;
	u_int32_t h_nelem;
{
	HASH *hashp;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_h_nelem");
	DB_ILLEGAL_METHOD(dbp, DB_OK_HASH);

	hashp = dbp->h_internal;
	hashp->h_nelem = h_nelem;
	return (0);
}
