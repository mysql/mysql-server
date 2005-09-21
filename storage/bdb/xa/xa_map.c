/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: xa_map.c,v 11.25 2004/10/15 16:59:46 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/txn.h"

/*
 * This file contains all the mapping information that we need to support
 * the DB/XA interface.
 */

/*
 * __db_rmid_to_env
 *	Return the environment associated with a given XA rmid.
 *
 * PUBLIC: int __db_rmid_to_env __P((int rmid, DB_ENV **envp));
 */
int
__db_rmid_to_env(rmid, dbenvp)
	int rmid;
	DB_ENV **dbenvp;
{
	DB_ENV *dbenv;

	dbenv = TAILQ_FIRST(&DB_GLOBAL(db_envq));
	if (dbenv != NULL && dbenv->xa_rmid == rmid) {
		*dbenvp = dbenv;
		return (0);
	}

	/*
	 * When we map an rmid, move that environment to be the first one in
	 * the list of environments, so we acquire the correct environment
	 * in DB->open.
	 */
	for (; dbenv != NULL; dbenv = TAILQ_NEXT(dbenv, links))
		if (dbenv->xa_rmid == rmid) {
			TAILQ_REMOVE(&DB_GLOBAL(db_envq), dbenv, links);
			TAILQ_INSERT_HEAD(&DB_GLOBAL(db_envq), dbenv, links);
			*dbenvp = dbenv;
			return (0);
		}

	return (1);
}

/*
 * __db_xid_to_txn
 *	Return the txn that corresponds to this XID.
 *
 * PUBLIC: int __db_xid_to_txn __P((DB_ENV *, XID *, roff_t *));
 */
int
__db_xid_to_txn(dbenv, xid, offp)
	DB_ENV *dbenv;
	XID *xid;
	roff_t *offp;
{
	struct __txn_detail *td;

	return (__txn_map_gid(dbenv, (u_int8_t *)xid->data, &td, offp));
}

/*
 * __db_map_rmid
 *	Create a mapping between the specified rmid and environment.
 *
 * PUBLIC: int __db_map_rmid __P((int, DB_ENV *));
 */
int
__db_map_rmid(rmid, dbenv)
	int rmid;
	DB_ENV *dbenv;
{
	dbenv->xa_rmid = rmid;
	TAILQ_INSERT_TAIL(&DB_GLOBAL(db_envq), dbenv, links);
	return (0);
}

/*
 * __db_unmap_rmid
 *	Destroy the mapping for the given rmid.
 *
 * PUBLIC: int __db_unmap_rmid __P((int));
 */
int
__db_unmap_rmid(rmid)
	int rmid;
{
	DB_ENV *e;

	for (e = TAILQ_FIRST(&DB_GLOBAL(db_envq));
	    e->xa_rmid != rmid;
	    e = TAILQ_NEXT(e, links))
	    ;

	if (e == NULL)
		return (EINVAL);

	TAILQ_REMOVE(&DB_GLOBAL(db_envq), e, links);
	return (0);
}

/*
 * __db_map_xid
 *	Create a mapping between this XID and the transaction at
 *	"off" in the shared region.
 *
 * PUBLIC: int __db_map_xid __P((DB_ENV *, XID *, size_t));
 */
int
__db_map_xid(dbenv, xid, off)
	DB_ENV *dbenv;
	XID *xid;
	size_t off;
{
	REGINFO *infop;
	TXN_DETAIL *td;

	infop = &((DB_TXNMGR *)dbenv->tx_handle)->reginfo;
	td = R_ADDR(infop, off);

	R_LOCK(dbenv, infop);
	memcpy(td->xid, xid->data, XIDDATASIZE);
	td->bqual = (u_int32_t)xid->bqual_length;
	td->gtrid = (u_int32_t)xid->gtrid_length;
	td->format = (int32_t)xid->formatID;
	R_UNLOCK(dbenv, infop);

	return (0);
}

/*
 * __db_unmap_xid
 *	Destroy the mapping for the specified XID.
 *
 * PUBLIC: void __db_unmap_xid __P((DB_ENV *, XID *, size_t));
 */

void
__db_unmap_xid(dbenv, xid, off)
	DB_ENV *dbenv;
	XID *xid;
	size_t off;
{
	TXN_DETAIL *td;

	COMPQUIET(xid, NULL);

	td = R_ADDR(&((DB_TXNMGR *)dbenv->tx_handle)->reginfo, off);
	memset(td->xid, 0, sizeof(td->xid));
}
