/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: xa_map.c,v 11.5 2000/11/30 00:58:46 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "txn.h"

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
__db_rmid_to_env(rmid, envp)
	int rmid;
	DB_ENV **envp;
{
	DB_ENV *env;

	env = TAILQ_FIRST(&DB_GLOBAL(db_envq));
	if (env != NULL && env->xa_rmid == rmid) {
		*envp = env;
		return (0);
	}

	/*
	 * When we map an rmid, move that environment to be the first one in
	 * the list of environments, so we acquire the correct environment
	 * in DB->open.
	 */
	for (; env != NULL; env = TAILQ_NEXT(env, links))
		if (env->xa_rmid == rmid) {
			TAILQ_REMOVE(&DB_GLOBAL(db_envq), env, links);
			TAILQ_INSERT_HEAD(&DB_GLOBAL(db_envq), env, links);
			*envp = env;
			return (0);
		}

	return (1);
}

/*
 * __db_xid_to_txn
 *	Return the txn that corresponds to this XID.
 *
 * PUBLIC: int __db_xid_to_txn __P((DB_ENV *, XID *, size_t *));
 */
int
__db_xid_to_txn(dbenv, xid, offp)
	DB_ENV *dbenv;
	XID *xid;
	size_t *offp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *tmr;
	struct __txn_detail *td;

	mgr = dbenv->tx_handle;
	tmr = mgr->reginfo.primary;

	/*
	 * Search the internal active transaction table to find the
	 * matching xid.  If this is a performance hit, then we
	 * can create a hash table, but I doubt it's worth it.
	 */
	R_LOCK(dbenv, &mgr->reginfo);
	for (td = SH_TAILQ_FIRST(&tmr->active_txn, __txn_detail);
	    td != NULL;
	    td = SH_TAILQ_NEXT(td, links, __txn_detail))
		if (memcmp(xid->data, td->xid, XIDDATASIZE) == 0)
			break;
	R_UNLOCK(dbenv, &mgr->reginfo);

	if (td == NULL)
		return (EINVAL);

	*offp = R_OFFSET(&mgr->reginfo, td);
	return (0);
}

/*
 * __db_map_rmid
 *	Create a mapping between the specified rmid and environment.
 *
 * PUBLIC: int __db_map_rmid __P((int, DB_ENV *));
 */
int
__db_map_rmid(rmid, env)
	int rmid;
	DB_ENV *env;
{
	env->xa_rmid = rmid;
	TAILQ_INSERT_TAIL(&DB_GLOBAL(db_envq), env, links);
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
	    e = TAILQ_NEXT(e, links));

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
__db_map_xid(env, xid, off)
	DB_ENV *env;
	XID *xid;
	size_t off;
{
	REGINFO *infop;
	TXN_DETAIL *td;

	infop = &((DB_TXNMGR *)env->tx_handle)->reginfo;
	td = (TXN_DETAIL *)R_ADDR(infop, off);

	R_LOCK(env, infop);
	memcpy(td->xid, xid->data, XIDDATASIZE);
	td->bqual = (u_int32_t)xid->bqual_length;
	td->gtrid = (u_int32_t)xid->gtrid_length;
	td->format = (int32_t)xid->formatID;
	R_UNLOCK(env, infop);

	return (0);
}

/*
 * __db_unmap_xid
 *	Destroy the mapping for the specified XID.
 *
 * PUBLIC: void __db_unmap_xid __P((DB_ENV *, XID *, size_t));
 */

void
__db_unmap_xid(env, xid, off)
	DB_ENV *env;
	XID *xid;
	size_t off;
{
	TXN_DETAIL *td;

	COMPQUIET(xid, NULL);

	td = (TXN_DETAIL *)R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo, off);
	memset(td->xid, 0, sizeof(td->xid));
}
