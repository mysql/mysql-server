/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_vrfyutil.c,v 11.11 2000/11/28 21:36:04 bostic Exp $
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_vrfyutil.c,v 11.11 2000/11/28 21:36:04 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_verify.h"
#include "db_ext.h"

static int __db_vrfy_pgset_iinc __P((DB *, db_pgno_t, int));

/*
 * __db_vrfy_dbinfo_create --
 *	Allocate and initialize a VRFY_DBINFO structure.
 *
 * PUBLIC: int __db_vrfy_dbinfo_create
 * PUBLIC:     __P((DB_ENV *, u_int32_t, VRFY_DBINFO **));
 */
int
__db_vrfy_dbinfo_create (dbenv, pgsize, vdpp)
	DB_ENV *dbenv;
	u_int32_t pgsize;
	VRFY_DBINFO **vdpp;
{
	DB *cdbp, *pgdbp, *pgset;
	VRFY_DBINFO *vdp;
	int ret;

	vdp = NULL;
	cdbp = pgdbp = pgset = NULL;

	if ((ret = __os_calloc(NULL,
	    1, sizeof(VRFY_DBINFO), (void **)&vdp)) != 0)
		goto err;

	if ((ret = db_create(&cdbp, dbenv, 0)) != 0)
		goto err;

	if ((ret = cdbp->set_flags(cdbp, DB_DUP | DB_DUPSORT)) != 0)
		goto err;

	if ((ret = cdbp->set_pagesize(cdbp, pgsize)) != 0)
		goto err;

	if ((ret =
	    cdbp->open(cdbp, NULL, NULL, DB_BTREE, DB_CREATE, 0600)) != 0)
		goto err;

	if ((ret = db_create(&pgdbp, dbenv, 0)) != 0)
		goto err;

	if ((ret = pgdbp->set_pagesize(pgdbp, pgsize)) != 0)
		goto err;

	if ((ret =
	    pgdbp->open(pgdbp, NULL, NULL, DB_BTREE, DB_CREATE, 0600)) != 0)
		goto err;

	if ((ret = __db_vrfy_pgset(dbenv, pgsize, &pgset)) != 0)
		goto err;

	LIST_INIT(&vdp->subdbs);
	LIST_INIT(&vdp->activepips);

	vdp->cdbp = cdbp;
	vdp->pgdbp = pgdbp;
	vdp->pgset = pgset;
	*vdpp = vdp;
	return (0);

err:	if (cdbp != NULL)
		(void)cdbp->close(cdbp, 0);
	if (pgdbp != NULL)
		(void)pgdbp->close(pgdbp, 0);
	if (vdp != NULL)
		__os_free(vdp, sizeof(VRFY_DBINFO));
	return (ret);
}

/*
 * __db_vrfy_dbinfo_destroy --
 *	Destructor for VRFY_DBINFO.  Destroys VRFY_PAGEINFOs and deallocates
 *	structure.
 *
 * PUBLIC: int __db_vrfy_dbinfo_destroy __P((VRFY_DBINFO *));
 */
int
__db_vrfy_dbinfo_destroy(vdp)
	VRFY_DBINFO *vdp;
{
	VRFY_CHILDINFO *c, *d;
	int t_ret, ret;

	ret = 0;

	for (c = LIST_FIRST(&vdp->subdbs); c != NULL; c = d) {
		d = LIST_NEXT(c, links);
		__os_free(c, 0);
	}

	if ((t_ret = vdp->pgdbp->close(vdp->pgdbp, 0)) != 0)
		ret = t_ret;

	if ((t_ret = vdp->cdbp->close(vdp->cdbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = vdp->pgset->close(vdp->pgset, 0)) != 0 && ret == 0)
		ret = t_ret;

	DB_ASSERT(LIST_FIRST(&vdp->activepips) == NULL);

	__os_free(vdp, sizeof(VRFY_DBINFO));
	return (ret);
}

/*
 * __db_vrfy_getpageinfo --
 *	Get a PAGEINFO structure for a given page, creating it if necessary.
 *
 * PUBLIC: int __db_vrfy_getpageinfo
 * PUBLIC:     __P((VRFY_DBINFO *, db_pgno_t, VRFY_PAGEINFO **));
 */
int
__db_vrfy_getpageinfo(vdp, pgno, pipp)
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	VRFY_PAGEINFO **pipp;
{
	DBT key, data;
	DB *pgdbp;
	VRFY_PAGEINFO *pip;
	int ret;

	/*
	 * We want a page info struct.  There are three places to get it from,
	 * in decreasing order of preference:
	 *
	 * 1. vdp->activepips.  If it's already "checked out", we're
	 *	already using it, we return the same exact structure with a
	 *	bumped refcount.  This is necessary because this code is
	 *	replacing array accesses, and it's common for f() to make some
	 *	changes to a pip, and then call g() and h() which each make
	 *	changes to the same pip.  vdps are never shared between threads
	 *	(they're never returned to the application), so this is safe.
	 * 2. The pgdbp.  It's not in memory, but it's in the database, so
	 *	get it, give it a refcount of 1, and stick it on activepips.
	 * 3. malloc.  It doesn't exist yet;  create it, then stick it on
	 *	activepips.  We'll put it in the database when we putpageinfo
	 *	later.
	 */

	/* Case 1. */
	for (pip = LIST_FIRST(&vdp->activepips); pip != NULL;
	    pip = LIST_NEXT(pip, links))
		if (pip->pgno == pgno)
			/* Found it. */
			goto found;

	/* Case 2. */
	pgdbp = vdp->pgdbp;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	F_SET(&data, DB_DBT_MALLOC);
	key.data = &pgno;
	key.size = sizeof(db_pgno_t);

	if ((ret = pgdbp->get(pgdbp, NULL, &key, &data, 0)) == 0) {
		/* Found it. */
		DB_ASSERT(data.size = sizeof(VRFY_PAGEINFO));
		pip = data.data;
		DB_ASSERT(pip->pi_refcount == 0);
		LIST_INSERT_HEAD(&vdp->activepips, pip, links);
		goto found;
	} else if (ret != DB_NOTFOUND)	/* Something nasty happened. */
		return (ret);

	/* Case 3 */
	if ((ret = __db_vrfy_pageinfo_create(&pip)) != 0)
		return (ret);

	LIST_INSERT_HEAD(&vdp->activepips, pip, links);
found:	pip->pi_refcount++;

	*pipp = pip;

	DB_ASSERT(pip->pi_refcount > 0);
	return (0);
}

/*
 * __db_vrfy_putpageinfo --
 *	Put back a VRFY_PAGEINFO that we're done with.
 *
 * PUBLIC: int __db_vrfy_putpageinfo __P((VRFY_DBINFO *, VRFY_PAGEINFO *));
 */
int
__db_vrfy_putpageinfo(vdp, pip)
	VRFY_DBINFO *vdp;
	VRFY_PAGEINFO *pip;
{
	DBT key, data;
	DB *pgdbp;
	VRFY_PAGEINFO *p;
	int ret;
#ifdef DIAGNOSTIC
	int found;

	found = 0;
#endif

	if (--pip->pi_refcount > 0)
		return (0);

	pgdbp = vdp->pgdbp;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = &pip->pgno;
	key.size = sizeof(db_pgno_t);
	data.data = pip;
	data.size = sizeof(VRFY_PAGEINFO);

	if ((ret = pgdbp->put(pgdbp, NULL, &key, &data, 0)) != 0)
		return (ret);

	for (p = LIST_FIRST(&vdp->activepips); p != NULL;
	    p = LIST_NEXT(p, links))
		if (p == pip) {
#ifdef DIAGNOSTIC
			found++;
#endif
			DB_ASSERT(p->pi_refcount == 0);
			LIST_REMOVE(p, links);
			break;
		}
#ifdef DIAGNOSTIC
	DB_ASSERT(found == 1);
#endif

	DB_ASSERT(pip->pi_refcount == 0);
	__os_free(pip, 0);
	return (0);
}

/*
 * __db_vrfy_pgset --
 *	Create a temporary database for the storing of sets of page numbers.
 *	(A mapping from page number to int, used by the *_meta2pgset functions,
 *	as well as for keeping track of which pages the verifier has seen.)
 *
 * PUBLIC: int __db_vrfy_pgset __P((DB_ENV *, u_int32_t, DB **));
 */
int
__db_vrfy_pgset(dbenv, pgsize, dbpp)
	DB_ENV *dbenv;
	u_int32_t pgsize;
	DB **dbpp;
{
	DB *dbp;
	int ret;

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		return (ret);
	if ((ret = dbp->set_pagesize(dbp, pgsize)) != 0)
		goto err;
	if ((ret = dbp->open(dbp, NULL, NULL, DB_BTREE, DB_CREATE, 0600)) == 0)
		*dbpp = dbp;
	else
err:		(void)dbp->close(dbp, 0);

	return (ret);
}

/*
 * __db_vrfy_pgset_get --
 *	Get the value associated in a page set with a given pgno.  Return
 *	a 0 value (and succeed) if we've never heard of this page.
 *
 * PUBLIC: int __db_vrfy_pgset_get __P((DB *, db_pgno_t, int *));
 */
int
__db_vrfy_pgset_get(dbp, pgno, valp)
	DB *dbp;
	db_pgno_t pgno;
	int *valp;
{
	DBT key, data;
	int ret, val;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);
	data.data = &val;
	data.ulen = sizeof(int);
	F_SET(&data, DB_DBT_USERMEM);

	if ((ret = dbp->get(dbp, NULL, &key, &data, 0)) == 0) {
		DB_ASSERT(data.size = sizeof(int));
		memcpy(&val, data.data, sizeof(int));
	} else if (ret == DB_NOTFOUND)
		val = 0;
	else
		return (ret);

	*valp = val;
	return (0);
}

/*
 * __db_vrfy_pgset_inc --
 *	Increment the value associated with a pgno by 1.
 *
 * PUBLIC: int __db_vrfy_pgset_inc __P((DB *, db_pgno_t));
 */
int
__db_vrfy_pgset_inc(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{

	return (__db_vrfy_pgset_iinc(dbp, pgno, 1));
}

/*
 * __db_vrfy_pgset_dec --
 *	Increment the value associated with a pgno by 1.
 *
 * PUBLIC: int __db_vrfy_pgset_dec __P((DB *, db_pgno_t));
 */
int
__db_vrfy_pgset_dec(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{

	return (__db_vrfy_pgset_iinc(dbp, pgno, -1));
}

/*
 * __db_vrfy_pgset_iinc --
 *	Increment the value associated with a pgno by i.
 *
 */
static int
__db_vrfy_pgset_iinc(dbp, pgno, i)
	DB *dbp;
	db_pgno_t pgno;
	int i;
{
	DBT key, data;
	int ret;
	int val;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	val = 0;

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);
	data.data = &val;
	data.ulen = sizeof(int);
	F_SET(&data, DB_DBT_USERMEM);

	if ((ret = dbp->get(dbp, NULL, &key, &data, 0)) == 0) {
		DB_ASSERT(data.size = sizeof(int));
		memcpy(&val, data.data, sizeof(int));
	} else if (ret != DB_NOTFOUND)
		return (ret);

	data.size = sizeof(int);
	val += i;

	return (dbp->put(dbp, NULL, &key, &data, 0));
}

/*
 * __db_vrfy_pgset_next --
 *	Given a cursor open in a pgset database, get the next page in the
 *	set.
 *
 * PUBLIC: int __db_vrfy_pgset_next __P((DBC *, db_pgno_t *));
 */
int
__db_vrfy_pgset_next(dbc, pgnop)
	DBC *dbc;
	db_pgno_t *pgnop;
{
	DBT key, data;
	db_pgno_t pgno;
	int ret;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	/* We don't care about the data, just the keys. */
	F_SET(&data, DB_DBT_USERMEM | DB_DBT_PARTIAL);
	F_SET(&key, DB_DBT_USERMEM);
	key.data = &pgno;
	key.ulen = sizeof(db_pgno_t);

	if ((ret = dbc->c_get(dbc, &key, &data, DB_NEXT)) != 0)
		return (ret);

	DB_ASSERT(key.size == sizeof(db_pgno_t));
	*pgnop = pgno;

	return (0);
}

/*
 * __db_vrfy_childcursor --
 *	Create a cursor to walk the child list with.  Returns with a nonzero
 *	final argument if the specified page has no children.
 *
 * PUBLIC: int __db_vrfy_childcursor __P((VRFY_DBINFO *, DBC **));
 */
int
__db_vrfy_childcursor(vdp, dbcp)
	VRFY_DBINFO *vdp;
	DBC **dbcp;
{
	DB *cdbp;
	DBC *dbc;
	int ret;

	cdbp = vdp->cdbp;

	if ((ret = cdbp->cursor(cdbp, NULL, &dbc, 0)) == 0)
		*dbcp = dbc;

	return (ret);
}

/*
 * __db_vrfy_childput --
 *	Add a child structure to the set for a given page.
 *
 * PUBLIC: int __db_vrfy_childput
 * PUBLIC:     __P((VRFY_DBINFO *, db_pgno_t, VRFY_CHILDINFO *));
 */
int
__db_vrfy_childput(vdp, pgno, cip)
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	VRFY_CHILDINFO *cip;
{
	DBT key, data;
	DB *cdbp;
	int ret;

	cdbp = vdp->cdbp;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);

	data.data = cip;
	data.size = sizeof(VRFY_CHILDINFO);

	/*
	 * Don't add duplicate (data) entries for a given child, and accept
	 * DB_KEYEXIST as a successful return;  we only need to verify
	 * each child once, even if a child (such as an overflow key) is
	 * multiply referenced.
	 */
	ret = cdbp->put(cdbp, NULL, &key, &data, DB_NODUPDATA);
	return (ret == DB_KEYEXIST ? 0 : ret);
}

/*
 * __db_vrfy_ccset --
 *	Sets a cursor created with __db_vrfy_childcursor to the first
 *	child of the given pgno, and returns it in the third arg.
 *
 * PUBLIC: int __db_vrfy_ccset __P((DBC *, db_pgno_t, VRFY_CHILDINFO **));
 */
int
__db_vrfy_ccset(dbc, pgno, cipp)
	DBC *dbc;
	db_pgno_t pgno;
	VRFY_CHILDINFO **cipp;
{
	DBT key, data;
	int ret;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);

	if ((ret = dbc->c_get(dbc, &key, &data, DB_SET)) != 0)
		return (ret);

	DB_ASSERT(data.size == sizeof(VRFY_CHILDINFO));
	*cipp = (VRFY_CHILDINFO *)data.data;

	return (0);
}

/*
 * __db_vrfy_ccnext --
 *	Gets the next child of the given cursor created with
 *	__db_vrfy_childcursor, and returns it in the memory provided in the
 *	second arg.
 *
 * PUBLIC: int __db_vrfy_ccnext __P((DBC *, VRFY_CHILDINFO **));
 */
int
__db_vrfy_ccnext(dbc, cipp)
	DBC *dbc;
	VRFY_CHILDINFO **cipp;
{
	DBT key, data;
	int ret;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	if ((ret = dbc->c_get(dbc, &key, &data, DB_NEXT_DUP)) != 0)
		return (ret);

	DB_ASSERT(data.size == sizeof(VRFY_CHILDINFO));
	*cipp = (VRFY_CHILDINFO *)data.data;

	return (0);
}

/*
 * __db_vrfy_ccclose --
 *	Closes the cursor created with __db_vrfy_childcursor.
 *
 *	This doesn't actually do anything interesting now, but it's
 *	not inconceivable that we might change the internal database usage
 *	and keep the interfaces the same, and a function call here or there
 *	seldom hurts anyone.
 *
 * PUBLIC: int __db_vrfy_ccclose __P((DBC *));
 */
int
__db_vrfy_ccclose(dbc)
	DBC *dbc;
{

	return (dbc->c_close(dbc));
}

/*
 * __db_vrfy_pageinfo_create --
 *	Constructor for VRFY_PAGEINFO;  allocates and initializes.
 *
 * PUBLIC: int __db_vrfy_pageinfo_create __P((VRFY_PAGEINFO **));
 */
int
__db_vrfy_pageinfo_create(pgipp)
	VRFY_PAGEINFO **pgipp;
{
	VRFY_PAGEINFO *pgip;
	int ret;

	if ((ret = __os_calloc(NULL,
	    1, sizeof(VRFY_PAGEINFO), (void **)&pgip)) != 0)
		return (ret);

	DB_ASSERT(pgip->pi_refcount == 0);

	*pgipp = pgip;
	return (0);
}

/*
 * __db_salvage_init --
 *	Set up salvager database.
 *
 * PUBLIC: int  __db_salvage_init __P((VRFY_DBINFO *));
 */
int
__db_salvage_init(vdp)
	VRFY_DBINFO *vdp;
{
	DB *dbp;
	int ret;

	if ((ret = db_create(&dbp, NULL, 0)) != 0)
		return (ret);

	if ((ret = dbp->set_pagesize(dbp, 1024)) != 0)
		goto err;

	if ((ret = dbp->open(dbp, NULL, NULL, DB_BTREE, DB_CREATE, 0)) != 0)
		goto err;

	vdp->salvage_pages = dbp;
	return (0);

err:	(void)dbp->close(dbp, 0);
	return (ret);
}

/*
 * __db_salvage_destroy --
 *	Close salvager database.
 * PUBLIC: void  __db_salvage_destroy __P((VRFY_DBINFO *));
 */
void
__db_salvage_destroy(vdp)
	VRFY_DBINFO *vdp;
{
	(void)vdp->salvage_pages->close(vdp->salvage_pages, 0);
}

/*
 * __db_salvage_getnext --
 *	Get the next (first) unprinted page in the database of pages we need to
 *	print still.  Delete entries for any already-printed pages we encounter
 *	in this search, as well as the page we're returning.
 *
 * PUBLIC: int __db_salvage_getnext
 * PUBLIC:     __P((VRFY_DBINFO *, db_pgno_t *, u_int32_t *));
 */
int
__db_salvage_getnext(vdp, pgnop, pgtypep)
	VRFY_DBINFO *vdp;
	db_pgno_t *pgnop;
	u_int32_t *pgtypep;
{
	DB *dbp;
	DBC *dbc;
	DBT key, data;
	int ret;
	u_int32_t pgtype;

	dbp = vdp->salvage_pages;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	if ((ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	while ((ret = dbc->c_get(dbc, &key, &data, DB_NEXT)) == 0) {
		DB_ASSERT(data.size == sizeof(u_int32_t));
		memcpy(&pgtype, data.data, sizeof(pgtype));

		if ((ret = dbc->c_del(dbc, 0)) != 0)
			goto err;
		if (pgtype != SALVAGE_IGNORE)
			goto found;
	}

	/* No more entries--ret probably equals DB_NOTFOUND. */

	if (0) {
found:		DB_ASSERT(key.size == sizeof(db_pgno_t));
		DB_ASSERT(data.size == sizeof(u_int32_t));

		*pgnop = *(db_pgno_t *)key.data;
		*pgtypep = *(u_int32_t *)data.data;
	}

err:	(void)dbc->c_close(dbc);
	return (ret);
}

/*
 * __db_salvage_isdone --
 *	Return whether or not the given pgno is already marked
 *	SALVAGE_IGNORE (meaning that we don't need to print it again).
 *
 *	Returns DB_KEYEXIST if it is marked, 0 if not, or another error on
 *	error.
 *
 * PUBLIC: int __db_salvage_isdone __P((VRFY_DBINFO *, db_pgno_t));
 */
int
__db_salvage_isdone(vdp, pgno)
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
{
	DBT key, data;
	DB *dbp;
	int ret;
	u_int32_t currtype;

	dbp = vdp->salvage_pages;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	currtype = SALVAGE_INVALID;
	data.data = &currtype;
	data.ulen = sizeof(u_int32_t);
	data.flags = DB_DBT_USERMEM;

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);

	/*
	 * Put an entry for this page, with pgno as key and type as data,
	 * unless it's already there and is marked done.
	 * If it's there and is marked anything else, that's fine--we
	 * want to mark it done.
	 */
	ret = dbp->get(dbp, NULL, &key, &data, 0);
	if (ret == 0) {
		/*
		 * The key's already here.  Check and see if it's already
		 * marked done.  If it is, return DB_KEYEXIST.  If it's not,
		 * return 0.
		 */
		if (currtype == SALVAGE_IGNORE)
			return (DB_KEYEXIST);
		else
			return (0);
	} else if (ret != DB_NOTFOUND)
		return (ret);

	/* The pgno is not yet marked anything; return 0. */
	return (0);
}

/*
 * __db_salvage_markdone --
 *	Mark as done a given page.
 *
 * PUBLIC: int __db_salvage_markdone __P((VRFY_DBINFO *, db_pgno_t));
 */
int
__db_salvage_markdone(vdp, pgno)
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
{
	DBT key, data;
	DB *dbp;
	int pgtype, ret;
	u_int32_t currtype;

	pgtype = SALVAGE_IGNORE;
	dbp = vdp->salvage_pages;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	currtype = SALVAGE_INVALID;
	data.data = &currtype;
	data.ulen = sizeof(u_int32_t);
	data.flags = DB_DBT_USERMEM;

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);

	/*
	 * Put an entry for this page, with pgno as key and type as data,
	 * unless it's already there and is marked done.
	 * If it's there and is marked anything else, that's fine--we
	 * want to mark it done, but db_salvage_isdone only lets
	 * us know if it's marked IGNORE.
	 *
	 * We don't want to return DB_KEYEXIST, though;  this will
	 * likely get passed up all the way and make no sense to the
	 * application.  Instead, use DB_VERIFY_BAD to indicate that
	 * we've seen this page already--it probably indicates a
	 * multiply-linked page.
	 */
	if ((ret = __db_salvage_isdone(vdp, pgno)) != 0)
		return (ret == DB_KEYEXIST ? DB_VERIFY_BAD : ret);

	data.size = sizeof(u_int32_t);
	data.data = &pgtype;

	return (dbp->put(dbp, NULL, &key, &data, 0));
}

/*
 * __db_salvage_markneeded --
 *	If it has not yet been printed, make note of the fact that a page
 *	must be dealt with later.
 *
 * PUBLIC: int __db_salvage_markneeded
 * PUBLIC:     __P((VRFY_DBINFO *, db_pgno_t, u_int32_t));
 */
int
__db_salvage_markneeded(vdp, pgno, pgtype)
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	u_int32_t pgtype;
{
	DB *dbp;
	DBT key, data;
	int ret;

	dbp = vdp->salvage_pages;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = &pgno;
	key.size = sizeof(db_pgno_t);

	data.data = &pgtype;
	data.size = sizeof(u_int32_t);

	/*
	 * Put an entry for this page, with pgno as key and type as data,
	 * unless it's already there, in which case it's presumably
	 * already been marked done.
	 */
	ret = dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE);
	return (ret == DB_KEYEXIST ? 0 : ret);
}
