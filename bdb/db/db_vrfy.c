/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_vrfy.c,v 1.53 2001/01/11 18:19:51 bostic Exp $
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_vrfy.c,v 1.53 2001/01/11 18:19:51 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_swap.h"
#include "db_verify.h"
#include "db_ext.h"
#include "btree.h"
#include "hash.h"
#include "qam.h"

static int  __db_guesspgsize __P((DB_ENV *, DB_FH *));
static int  __db_is_valid_magicno __P((u_int32_t, DBTYPE *));
static int  __db_is_valid_pagetype __P((u_int32_t));
static int  __db_meta2pgset
		__P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t, DB *));
static int  __db_salvage_subdbs
		__P((DB *, VRFY_DBINFO *, void *,
		int(*)(void *, const void *), u_int32_t, int *));
static int  __db_salvage_unknowns
		__P((DB *, VRFY_DBINFO *, void *,
		int (*)(void *, const void *), u_int32_t));
static int  __db_vrfy_common
		__P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t, u_int32_t));
static int  __db_vrfy_freelist __P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t));
static int  __db_vrfy_invalid
		__P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t, u_int32_t));
static int  __db_vrfy_orderchkonly __P((DB *,
		VRFY_DBINFO *, const char *, const char *, u_int32_t));
static int  __db_vrfy_pagezero __P((DB *, VRFY_DBINFO *, DB_FH *, u_int32_t));
static int  __db_vrfy_subdbs
		__P((DB *, VRFY_DBINFO *, const char *, u_int32_t));
static int  __db_vrfy_structure
		__P((DB *, VRFY_DBINFO *, const char *, db_pgno_t, u_int32_t));
static int  __db_vrfy_walkpages
		__P((DB *, VRFY_DBINFO *, void *, int (*)(void *, const void *),
		u_int32_t));

/*
 * This is the code for DB->verify, the DB database consistency checker.
 * For now, it checks all subdatabases in a database, and verifies
 * everything it knows how to (i.e. it's all-or-nothing, and one can't
 * check only for a subset of possible problems).
 */

/*
 * __db_verify --
 *	Walk the entire file page-by-page, either verifying with or without
 *	dumping in db_dump -d format, or DB_SALVAGE-ing whatever key/data
 *	pairs can be found and dumping them in standard (db_load-ready)
 *	dump format.
 *
 *	(Salvaging isn't really a verification operation, but we put it
 *	here anyway because it requires essentially identical top-level
 *	code.)
 *
 *	flags may be 0, DB_NOORDERCHK, DB_ORDERCHKONLY, or DB_SALVAGE
 *	(and optionally DB_AGGRESSIVE).
 *
 *	__db_verify itself is simply a wrapper to __db_verify_internal,
 *	which lets us pass appropriate equivalents to FILE * in from the
 *	non-C APIs.
 *
 * PUBLIC: int __db_verify
 * PUBLIC:     __P((DB *, const char *, const char *, FILE *, u_int32_t));
 */
int
__db_verify(dbp, file, database, outfile, flags)
	DB *dbp;
	const char *file, *database;
	FILE *outfile;
	u_int32_t flags;
{

	return (__db_verify_internal(dbp,
	    file, database, outfile, __db_verify_callback, flags));
}

/*
 * __db_verify_callback --
 *	Callback function for using pr_* functions from C.
 *
 * PUBLIC: int  __db_verify_callback __P((void *, const void *));
 */
int
__db_verify_callback(handle, str_arg)
	void *handle;
	const void *str_arg;
{
	char *str;
	FILE *f;

	str = (char *)str_arg;
	f = (FILE *)handle;

	if (fprintf(f, "%s", str) != (int)strlen(str))
		return (EIO);

	return (0);
}

/*
 * __db_verify_internal --
 *	Inner meat of __db_verify.
 *
 * PUBLIC: int __db_verify_internal __P((DB *, const char *,
 * PUBLIC:     const char *, void *, int (*)(void *, const void *), u_int32_t));
 */
int
__db_verify_internal(dbp_orig, name, subdb, handle, callback, flags)
	DB *dbp_orig;
	const char *name, *subdb;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	DB_FH fh, *fhp;
	PAGE *h;
	VRFY_DBINFO *vdp;
	db_pgno_t last;
	int has, ret, isbad;
	char *real_name;

	dbenv = dbp_orig->dbenv;
	vdp = NULL;
	real_name = NULL;
	ret = isbad = 0;

	memset(&fh, 0, sizeof(fh));
	fhp = &fh;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_AFTER_OPEN(dbp_orig, "verify");

#define	OKFLAGS (DB_AGGRESSIVE | DB_NOORDERCHK | DB_ORDERCHKONLY | DB_SALVAGE)
	if ((ret = __db_fchk(dbenv, "DB->verify", flags, OKFLAGS)) != 0)
		return (ret);

	/*
	 * DB_SALVAGE is mutually exclusive with the other flags except
	 * DB_AGGRESSIVE.
	 */
	if (LF_ISSET(DB_SALVAGE) &&
	    (flags & ~DB_AGGRESSIVE) != DB_SALVAGE)
		return (__db_ferr(dbenv, "__db_verify", 1));

	if (LF_ISSET(DB_ORDERCHKONLY) && flags != DB_ORDERCHKONLY)
		return (__db_ferr(dbenv, "__db_verify", 1));

	if (LF_ISSET(DB_ORDERCHKONLY) && subdb == NULL) {
		__db_err(dbenv, "DB_ORDERCHKONLY requires a database name");
		return (EINVAL);
	}

	/*
	 * Forbid working in an environment that uses transactions or
	 * locking;  we're going to be looking at the file freely,
	 * and while we're not going to modify it, we aren't obeying
	 * locking conventions either.
	 */
	if (TXN_ON(dbenv) || LOCKING_ON(dbenv) || LOGGING_ON(dbenv)) {
		dbp_orig->errx(dbp_orig,
	    "verify may not be used with transactions, logging, or locking");
		return (EINVAL);
		/* NOTREACHED */
	}

	/* Create a dbp to use internally, which we can close at our leisure. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;

	F_SET(dbp, DB_AM_VERIFYING);

	/* Copy the supplied pagesize, which we use if the file one is bogus. */
	if (dbp_orig->pgsize >= DB_MIN_PGSIZE &&
	    dbp_orig->pgsize <= DB_MAX_PGSIZE)
		dbp->set_pagesize(dbp, dbp_orig->pgsize);

	/* Copy the feedback function, if present, and initialize it. */
	if (!LF_ISSET(DB_SALVAGE) && dbp_orig->db_feedback != NULL) {
		dbp->set_feedback(dbp, dbp_orig->db_feedback);
		dbp->db_feedback(dbp, DB_VERIFY, 0);
	}

	/*
	 * Copy the comparison and hashing functions.  Note that
	 * even if the database is not a hash or btree, the respective
	 * internal structures will have been initialized.
	 */
	if (dbp_orig->dup_compare != NULL &&
	    (ret = dbp->set_dup_compare(dbp, dbp_orig->dup_compare)) != 0)
		goto err;
	if (((BTREE *)dbp_orig->bt_internal)->bt_compare != NULL &&
	    (ret = dbp->set_bt_compare(dbp,
	    ((BTREE *)dbp_orig->bt_internal)->bt_compare)) != 0)
		goto err;
	if (((HASH *)dbp_orig->h_internal)->h_hash != NULL &&
	    (ret = dbp->set_h_hash(dbp,
	    ((HASH *)dbp_orig->h_internal)->h_hash)) != 0)
		goto err;

	/*
	 * We don't know how large the cache is, and if the database
	 * in question uses a small page size--which we don't know
	 * yet!--it may be uncomfortably small for the default page
	 * size [#2143].  However, the things we need temporary
	 * databases for in dbinfo are largely tiny, so using a
	 * 1024-byte pagesize is probably not going to be a big hit,
	 * and will make us fit better into small spaces.
	 */
	if ((ret = __db_vrfy_dbinfo_create(dbenv, 1024, &vdp)) != 0)
		goto err;

	/* Find the real name of the file. */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, name, 0, NULL, &real_name)) != 0)
		goto err;

	/*
	 * Our first order of business is to verify page 0, which is
	 * the metadata page for the master database of subdatabases
	 * or of the only database in the file.  We want to do this by hand
	 * rather than just calling __db_open in case it's corrupt--various
	 * things in __db_open might act funny.
	 *
	 * Once we know the metadata page is healthy, I believe that it's
	 * safe to open the database normally and then use the page swapping
	 * code, which makes life easier.
	 */
	if ((ret = __os_open(dbenv, real_name, DB_OSO_RDONLY, 0444, fhp)) != 0)
		goto err;

	/* Verify the metadata page 0; set pagesize and type. */
	if ((ret = __db_vrfy_pagezero(dbp, vdp, fhp, flags)) != 0) {
		if (ret == DB_VERIFY_BAD)
			isbad = 1;
		else
			goto err;
	}

	/*
	 * We can assume at this point that dbp->pagesize and dbp->type are
	 * set correctly, or at least as well as they can be, and that
	 * locking, logging, and txns are not in use.  Thus we can trust
	 * the memp code not to look at the page, and thus to be safe
	 * enough to use.
	 *
	 * The dbp is not open, but the file is open in the fhp, and we
	 * cannot assume that __db_open is safe.  Call __db_dbenv_setup,
	 * the [safe] part of __db_open that initializes the environment--
	 * and the mpool--manually.
	 */
	if ((ret = __db_dbenv_setup(dbp,
	    name, DB_ODDFILESIZE | DB_RDONLY)) != 0)
		return (ret);

	/* Mark the dbp as opened, so that we correctly handle its close. */
	F_SET(dbp, DB_OPEN_CALLED);

	/*
	 * Find out the page number of the last page in the database.
	 *
	 * XXX: This currently fails if the last page is of bad type,
	 * because it calls __db_pgin and that pukes.  This is bad.
	 */
	if ((ret = memp_fget(dbp->mpf, &last, DB_MPOOL_LAST, &h)) != 0)
		goto err;
	if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
		goto err;

	vdp->last_pgno = last;

	/*
	 * DB_ORDERCHKONLY is a special case;  our file consists of
	 * several subdatabases, which use different hash, bt_compare,
	 * and/or dup_compare functions.  Consequently, we couldn't verify
	 * sorting and hashing simply by calling DB->verify() on the file.
	 * DB_ORDERCHKONLY allows us to come back and check those things;  it
	 * requires a subdatabase, and assumes that everything but that
	 * database's sorting/hashing is correct.
	 */
	if (LF_ISSET(DB_ORDERCHKONLY)) {
		ret = __db_vrfy_orderchkonly(dbp, vdp, name, subdb, flags);
		goto done;
	}

	/*
	 * When salvaging, we use a db to keep track of whether we've seen a
	 * given overflow or dup page in the course of traversing normal data.
	 * If in the end we have not, we assume its key got lost and print it
	 * with key "UNKNOWN".
	 */
	if (LF_ISSET(DB_SALVAGE)) {
		if ((ret = __db_salvage_init(vdp)) != 0)
			return (ret);

		/*
		 * If we're not being aggressive, attempt to crack subdbs.
		 * "has" will indicate whether the attempt has succeeded
		 * (even in part), meaning that we have some semblance of
		 * subdbs;  on the walkpages pass, we print out
		 * whichever data pages we have not seen.
		 */
		has = 0;
		if (!LF_ISSET(DB_AGGRESSIVE) && (__db_salvage_subdbs(dbp,
		    vdp, handle, callback, flags, &has)) != 0)
			isbad = 1;

		/*
		 * If we have subdatabases, we need to signal that if
		 * any keys are found that don't belong to a subdatabase,
		 * they'll need to have an "__OTHER__" subdatabase header
		 * printed first.  Flag this.  Else, print a header for
		 * the normal, non-subdb database.
		 */
		if (has == 1)
			F_SET(vdp, SALVAGE_PRINTHEADER);
		else if ((ret = __db_prheader(dbp,
		    NULL, 0, 0, handle, callback, vdp, PGNO_BASE_MD)) != 0)
			goto err;
	}

	if ((ret =
	    __db_vrfy_walkpages(dbp, vdp, handle, callback, flags)) != 0) {
		if (ret == DB_VERIFY_BAD)
			isbad = 1;
		else if (ret != 0)
			goto err;
	}

	/* If we're verifying, verify inter-page structure. */
	if (!LF_ISSET(DB_SALVAGE) && isbad == 0)
		if ((ret =
		    __db_vrfy_structure(dbp, vdp, name, 0, flags)) != 0) {
			if (ret == DB_VERIFY_BAD)
				isbad = 1;
			else if (ret != 0)
				goto err;
		}

	/*
	 * If we're salvaging, output with key UNKNOWN any overflow or dup pages
	 * we haven't been able to put in context.  Then destroy the salvager's
	 * state-saving database.
	 */
	if (LF_ISSET(DB_SALVAGE)) {
		if ((ret = __db_salvage_unknowns(dbp,
		    vdp, handle, callback, flags)) != 0)
			isbad = 1;
		/* No return value, since there's little we can do. */
		__db_salvage_destroy(vdp);
	}

	if (0) {
err:		(void)__db_err(dbenv, "%s: %s", name, db_strerror(ret));
	}

	if (LF_ISSET(DB_SALVAGE) &&
	    (has == 0 || F_ISSET(vdp, SALVAGE_PRINTFOOTER)))
		(void)__db_prfooter(handle, callback);

	/* Send feedback that we're done. */
done:	if (!LF_ISSET(DB_SALVAGE) && dbp->db_feedback != NULL)
		dbp->db_feedback(dbp, DB_VERIFY, 100);

	if (F_ISSET(fhp, DB_FH_VALID))
		(void)__os_closehandle(fhp);
	if (dbp)
		(void)dbp->close(dbp, 0);
	if (vdp)
		(void)__db_vrfy_dbinfo_destroy(vdp);
	if (real_name)
		__os_freestr(real_name);

	if ((ret == 0 && isbad == 1) || ret == DB_VERIFY_FATAL)
		ret = DB_VERIFY_BAD;

	return (ret);
}

/*
 * __db_vrfy_pagezero --
 *	Verify the master metadata page.  Use seek, read, and a local buffer
 *	rather than the DB paging code, for safety.
 *
 *	Must correctly (or best-guess) set dbp->type and dbp->pagesize.
 */
static int
__db_vrfy_pagezero(dbp, vdp, fhp, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	DB_FH *fhp;
	u_int32_t flags;
{
	DBMETA *meta;
	DB_ENV *dbenv;
	VRFY_PAGEINFO *pip;
	db_pgno_t freelist;
	int t_ret, ret, nr, swapped;
	u_int8_t mbuf[DBMETASIZE];

	swapped = ret = t_ret = 0;
	freelist = 0;
	dbenv = dbp->dbenv;
	meta = (DBMETA *)mbuf;
	dbp->type = DB_UNKNOWN;

	/*
	 * Seek to the metadata page.
	 * Note that if we're just starting a verification, dbp->pgsize
	 * may be zero;  this is okay, as we want page zero anyway and
	 * 0*0 == 0.
	 */
	if ((ret = __os_seek(dbenv, fhp, 0, 0, 0, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	if ((ret = __os_read(dbenv, fhp, mbuf, DBMETASIZE, (size_t *)&nr)) != 0)
		goto err;

	if (nr != DBMETASIZE) {
		EPRINT((dbp->dbenv,
		    "Incomplete metadata page %lu", (u_long)PGNO_BASE_MD));
		t_ret = DB_VERIFY_FATAL;
		goto err;
	}

	/*
	 * Check all of the fields that we can.
	 */

	/* 08-11: Current page number.  Must == pgno. */
	/* Note that endianness doesn't matter--it's zero. */
	if (meta->pgno != PGNO_BASE_MD) {
		EPRINT((dbp->dbenv, "Bad pgno: was %lu, should be %lu",
		    (u_long)meta->pgno, (u_long)PGNO_BASE_MD));
		ret = DB_VERIFY_BAD;
	}

	/* 12-15: Magic number.  Must be one of valid set. */
	if (__db_is_valid_magicno(meta->magic, &dbp->type))
		swapped = 0;
	else {
		M_32_SWAP(meta->magic);
		if (__db_is_valid_magicno(meta->magic,
		    &dbp->type))
			swapped = 1;
		else {
			EPRINT((dbp->dbenv,
			    "Bad magic number: %lu", (u_long)meta->magic));
			ret = DB_VERIFY_BAD;
		}
	}

	/*
	 * 16-19: Version.  Must be current;  for now, we
	 * don't support verification of old versions.
	 */
	if (swapped)
		M_32_SWAP(meta->version);
	if ((dbp->type == DB_BTREE && meta->version != DB_BTREEVERSION) ||
	    (dbp->type == DB_HASH && meta->version != DB_HASHVERSION) ||
	    (dbp->type == DB_QUEUE && meta->version != DB_QAMVERSION)) {
		ret = DB_VERIFY_BAD;
		EPRINT((dbp->dbenv, "%s%s", "Old or incorrect DB ",
		    "version; extraneous errors may result"));
	}

	/*
	 * 20-23: Pagesize.  Must be power of two,
	 * greater than 512, and less than 64K.
	 */
	if (swapped)
		M_32_SWAP(meta->pagesize);
	if (IS_VALID_PAGESIZE(meta->pagesize))
		dbp->pgsize = meta->pagesize;
	else {
		EPRINT((dbp->dbenv,
		    "Bad page size: %lu", (u_long)meta->pagesize));
		ret = DB_VERIFY_BAD;

		/*
		 * Now try to settle on a pagesize to use.
		 * If the user-supplied one is reasonable,
		 * use it;  else, guess.
		 */
		if (!IS_VALID_PAGESIZE(dbp->pgsize))
			dbp->pgsize = __db_guesspgsize(dbenv, fhp);
	}

	/*
	 * 25: Page type.  Must be correct for dbp->type,
	 * which is by now set as well as it can be.
	 */
	/* Needs no swapping--only one byte! */
	if ((dbp->type == DB_BTREE && meta->type != P_BTREEMETA) ||
	    (dbp->type == DB_HASH && meta->type != P_HASHMETA) ||
	    (dbp->type == DB_QUEUE && meta->type != P_QAMMETA)) {
		ret = DB_VERIFY_BAD;
		EPRINT((dbp->dbenv, "Bad page type: %lu", (u_long)meta->type));
	}

	/*
	 * 28-31: Free list page number.
	 * We'll verify its sensibility when we do inter-page
	 * verification later;  for now, just store it.
	 */
	if (swapped)
	    M_32_SWAP(meta->free);
	freelist = meta->free;

	/*
	 * Initialize vdp->pages to fit a single pageinfo structure for
	 * this one page.  We'll realloc later when we know how many
	 * pages there are.
	 */
	if ((ret = __db_vrfy_getpageinfo(vdp, PGNO_BASE_MD, &pip)) != 0)
		return (ret);
	pip->pgno = PGNO_BASE_MD;
	pip->type = meta->type;

	/*
	 * Signal that we still have to check the info specific to
	 * a given type of meta page.
	 */
	F_SET(pip, VRFY_INCOMPLETE);

	pip->free = freelist;

	if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
		return (ret);

	/* Set up the dbp's fileid.  We don't use the regular open path. */
	memcpy(dbp->fileid, meta->uid, DB_FILE_ID_LEN);

	if (0) {
err:		__db_err(dbenv, "%s", db_strerror(ret));
	}

	if (swapped == 1)
		F_SET(dbp, DB_AM_SWAP);
	if (t_ret != 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_vrfy_walkpages --
 *	Main loop of the verifier/salvager.  Walks through,
 *	page by page, and verifies all pages and/or prints all data pages.
 */
static int
__db_vrfy_walkpages(dbp, vdp, handle, callback, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	DB_ENV *dbenv;
	PAGE *h;
	db_pgno_t i;
	int ret, t_ret, isbad;

	ret = isbad = t_ret = 0;
	dbenv = dbp->dbenv;

	if ((ret = __db_fchk(dbenv,
	    "__db_vrfy_walkpages", flags, OKFLAGS)) != 0)
		return (ret);

	for (i = 0; i <= vdp->last_pgno; i++) {
		/*
		 * If DB_SALVAGE is set, we inspect our database of
		 * completed pages, and skip any we've already printed in
		 * the subdb pass.
		 */
		if (LF_ISSET(DB_SALVAGE) && (__db_salvage_isdone(vdp, i) != 0))
			continue;

		/* If an individual page get fails, keep going. */
		if ((t_ret = memp_fget(dbp->mpf, &i, 0, &h)) != 0) {
			if (ret == 0)
				ret = t_ret;
			continue;
		}

		if (LF_ISSET(DB_SALVAGE)) {
			/*
			 * We pretty much don't want to quit unless a
			 * bomb hits.  May as well return that something
			 * was screwy, however.
			 */
			if ((t_ret = __db_salvage(dbp,
			    vdp, i, h, handle, callback, flags)) != 0) {
				if (ret == 0)
					ret = t_ret;
				isbad = 1;
			}
		} else {
			/*
			 * Verify info common to all page
			 * types.
			 */
			if (i != PGNO_BASE_MD)
				if ((t_ret = __db_vrfy_common(dbp,
				    vdp, h, i, flags)) == DB_VERIFY_BAD)
					isbad = 1;

			switch (TYPE(h)) {
			case P_INVALID:
				t_ret = __db_vrfy_invalid(dbp,
				    vdp, h, i, flags);
				break;
			case __P_DUPLICATE:
				isbad = 1;
				EPRINT((dbp->dbenv,
				    "Old-style duplicate page: %lu",
				    (u_long)i));
				break;
			case P_HASH:
				t_ret = __ham_vrfy(dbp,
				    vdp, h, i, flags);
				break;
			case P_IBTREE:
			case P_IRECNO:
			case P_LBTREE:
			case P_LDUP:
				t_ret = __bam_vrfy(dbp,
				    vdp, h, i, flags);
				break;
			case P_LRECNO:
				t_ret = __ram_vrfy_leaf(dbp,
				    vdp, h, i, flags);
				break;
			case P_OVERFLOW:
				t_ret = __db_vrfy_overflow(dbp,
				    vdp, h, i, flags);
				break;
			case P_HASHMETA:
				t_ret = __ham_vrfy_meta(dbp,
				    vdp, (HMETA *)h, i, flags);
				break;
			case P_BTREEMETA:
				t_ret = __bam_vrfy_meta(dbp,
				    vdp, (BTMETA *)h, i, flags);
				break;
			case P_QAMMETA:
				t_ret = __qam_vrfy_meta(dbp,
				    vdp, (QMETA *)h, i, flags);
				break;
			case P_QAMDATA:
				t_ret = __qam_vrfy_data(dbp,
				    vdp, (QPAGE *)h, i, flags);
				break;
			default:
				EPRINT((dbp->dbenv,
				    "Unknown page type: %lu", (u_long)TYPE(h)));
				isbad = 1;
				break;
			}

			/*
			 * Set up error return.
			 */
			if (t_ret == DB_VERIFY_BAD)
				isbad = 1;
			else if (t_ret == DB_VERIFY_FATAL)
				goto err;
			else
				ret = t_ret;

			/*
			 * Provide feedback to the application about our
			 * progress.  The range 0-50% comes from the fact
			 * that this is the first of two passes through the
			 * database (front-to-back, then top-to-bottom).
			 */
			if (dbp->db_feedback != NULL)
				dbp->db_feedback(dbp, DB_VERIFY,
				    (i + 1) * 50 / (vdp->last_pgno + 1));
		}

		if ((t_ret = memp_fput(dbp->mpf, h, 0)) != 0 && ret == 0)
			ret = t_ret;
	}

	if (0) {
err:		if ((t_ret = memp_fput(dbp->mpf, h, 0)) != 0)
			return (ret == 0 ? t_ret : ret);
		return (DB_VERIFY_BAD);
	}

	return ((isbad == 1 && ret == 0) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_vrfy_structure--
 *	After a beginning-to-end walk through the database has been
 *	completed, put together the information that has been collected
 *	to verify the overall database structure.
 *
 *	Should only be called if we want to do a database verification,
 *	i.e. if DB_SALVAGE is not set.
 */
static int
__db_vrfy_structure(dbp, vdp, dbname, meta_pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	const char *dbname;
	db_pgno_t meta_pgno;
	u_int32_t flags;
{
	DB *pgset;
	DB_ENV *dbenv;
	VRFY_PAGEINFO *pip;
	db_pgno_t i;
	int ret, isbad, hassubs, p;

	isbad = 0;
	pip = NULL;
	dbenv = dbp->dbenv;
	pgset = vdp->pgset;

	if ((ret = __db_fchk(dbenv, "DB->verify", flags, OKFLAGS)) != 0)
		return (ret);
	if (LF_ISSET(DB_SALVAGE)) {
		__db_err(dbenv, "__db_vrfy_structure called with DB_SALVAGE");
		return (EINVAL);
	}

	/*
	 * Providing feedback here is tricky;  in most situations,
	 * we fetch each page one more time, but we do so in a top-down
	 * order that depends on the access method.  Worse, we do this
	 * recursively in btree, such that on any call where we're traversing
	 * a subtree we don't know where that subtree is in the whole database;
	 * worse still, any given database may be one of several subdbs.
	 *
	 * The solution is to decrement a counter vdp->pgs_remaining each time
	 * we verify (and call feedback on) a page.  We may over- or
	 * under-count, but the structure feedback function will ensure that we
	 * never give a percentage under 50 or over 100.  (The first pass
	 * covered the range 0-50%.)
	 */
	if (dbp->db_feedback != NULL)
		vdp->pgs_remaining = vdp->last_pgno + 1;

	/*
	 * Call the appropriate function to downwards-traverse the db type.
	 */
	switch(dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		if ((ret = __bam_vrfy_structure(dbp, vdp, 0, flags)) != 0) {
			if (ret == DB_VERIFY_BAD)
				isbad = 1;
			else
				goto err;
		}

		/*
		 * If we have subdatabases and we know that the database is,
		 * thus far, sound, it's safe to walk the tree of subdatabases.
		 * Do so, and verify the structure of the databases within.
		 */
		if ((ret = __db_vrfy_getpageinfo(vdp, 0, &pip)) != 0)
			goto err;
		hassubs = F_ISSET(pip, VRFY_HAS_SUBDBS);
		if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
			goto err;

		if (isbad == 0 && hassubs)
			if ((ret =
			    __db_vrfy_subdbs(dbp, vdp, dbname, flags)) != 0) {
				if (ret == DB_VERIFY_BAD)
					isbad = 1;
				else
					goto err;
			}
		break;
	case DB_HASH:
		if ((ret = __ham_vrfy_structure(dbp, vdp, 0, flags)) != 0) {
			if (ret == DB_VERIFY_BAD)
				isbad = 1;
			else
				goto err;
		}
		break;
	case DB_QUEUE:
		if ((ret = __qam_vrfy_structure(dbp, vdp, flags)) != 0) {
			if (ret == DB_VERIFY_BAD)
				isbad = 1;
		}

		/*
		 * Queue pages may be unreferenced and totally zeroed, if
		 * they're empty;  queue doesn't have much structure, so
		 * this is unlikely to be wrong in any troublesome sense.
		 * Skip to "err".
		 */
		goto err;
		/* NOTREACHED */
	default:
		/* This should only happen if the verifier is somehow broken. */
		DB_ASSERT(0);
		ret = EINVAL;
		goto err;
		/* NOTREACHED */
	}

	/* Walk free list. */
	if ((ret =
	    __db_vrfy_freelist(dbp, vdp, meta_pgno, flags)) == DB_VERIFY_BAD)
		isbad = 1;

	/*
	 * If structure checks up until now have failed, it's likely that
	 * checking what pages have been missed will result in oodles of
	 * extraneous error messages being EPRINTed.  Skip to the end
	 * if this is the case;  we're going to be printing at least one
	 * error anyway, and probably all the more salient ones.
	 */
	if (ret != 0 || isbad == 1)
		goto err;

	/*
	 * Make sure no page has been missed and that no page is still marked
	 * "all zeroes" (only certain hash pages can be, and they're unmarked
	 * in __ham_vrfy_structure).
	 */
	for (i = 0; i < vdp->last_pgno + 1; i++) {
		if ((ret = __db_vrfy_getpageinfo(vdp, i, &pip)) != 0)
			goto err;
		if ((ret = __db_vrfy_pgset_get(pgset, i, &p)) != 0)
			goto err;
		if (p == 0) {
			EPRINT((dbp->dbenv,
			    "Unreferenced page %lu", (u_long)i));
			isbad = 1;
		}

		if (F_ISSET(pip, VRFY_IS_ALLZEROES)) {
			EPRINT((dbp->dbenv,
			    "Totally zeroed page %lu", (u_long)i));
			isbad = 1;
		}
		if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
			goto err;
		pip = NULL;
	}

err:	if (pip != NULL)
		(void)__db_vrfy_putpageinfo(vdp, pip);

	return ((isbad == 1 && ret == 0) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_is_valid_pagetype
 */
static int
__db_is_valid_pagetype(type)
	u_int32_t type;
{
	switch (type) {
	case P_INVALID:			/* Order matches ordinal value. */
	case P_HASH:
	case P_IBTREE:
	case P_IRECNO:
	case P_LBTREE:
	case P_LRECNO:
	case P_OVERFLOW:
	case P_HASHMETA:
	case P_BTREEMETA:
	case P_QAMMETA:
	case P_QAMDATA:
	case P_LDUP:
		return (1);
	}
	return (0);
}

/*
 * __db_is_valid_magicno
 */
static int
__db_is_valid_magicno(magic, typep)
	u_int32_t magic;
	DBTYPE *typep;
{
	switch (magic) {
	case DB_BTREEMAGIC:
		*typep = DB_BTREE;
		return (1);
	case DB_HASHMAGIC:
		*typep = DB_HASH;
		return (1);
	case DB_QAMMAGIC:
		*typep = DB_QUEUE;
		return (1);
	}
	*typep = DB_UNKNOWN;
	return (0);
}

/*
 * __db_vrfy_common --
 *	Verify info common to all page types.
 */
static int
__db_vrfy_common(dbp, vdp, h, pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t flags;
{
	VRFY_PAGEINFO *pip;
	int ret, t_ret;
	u_int8_t *p;

	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);

	pip->pgno = pgno;
	F_CLR(pip, VRFY_IS_ALLZEROES);

	/*
	 * Hash expands the table by leaving some pages between the
	 * old last and the new last totally zeroed.  Its pgin function
	 * should fix things, but we might not be using that (e.g. if
	 * we're a subdatabase).
	 *
	 * Queue will create sparse files if sparse record numbers are used.
	 */
	if (pgno != 0 && PGNO(h) == 0) {
		for (p = (u_int8_t *)h; p < (u_int8_t *)h + dbp->pgsize; p++)
			if (*p != 0) {
				EPRINT((dbp->dbenv,
				    "Page %lu should be zeroed and is not",
				    (u_long)pgno));
				ret = DB_VERIFY_BAD;
				goto err;
			}
		/*
		 * It's totally zeroed;  mark it as a hash, and we'll
		 * check that that makes sense structurally later.
		 * (The queue verification doesn't care, since queues
		 * don't really have much in the way of structure.)
		 */
		pip->type = P_HASH;
		F_SET(pip, VRFY_IS_ALLZEROES);
		ret = 0;
		goto err;	/* well, not really an err. */
	}

	if (PGNO(h) != pgno) {
		EPRINT((dbp->dbenv,
		    "Bad page number: %lu should be %lu",
		    (u_long)h->pgno, (u_long)pgno));
		ret = DB_VERIFY_BAD;
	}

	if (!__db_is_valid_pagetype(h->type)) {
		EPRINT((dbp->dbenv, "Bad page type: %lu", (u_long)h->type));
		ret = DB_VERIFY_BAD;
	}
	pip->type = h->type;

err:	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_vrfy_invalid --
 *	Verify P_INVALID page.
 *	(Yes, there's not much to do here.)
 */
static int
__db_vrfy_invalid(dbp, vdp, h, pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t flags;
{
	VRFY_PAGEINFO *pip;
	int ret, t_ret;

	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);
	pip->next_pgno = pip->prev_pgno = 0;

	if (!IS_VALID_PGNO(NEXT_PGNO(h))) {
		EPRINT((dbp->dbenv,
		    "Invalid next_pgno %lu on page %lu",
		    (u_long)NEXT_PGNO(h), (u_long)pgno));
		ret = DB_VERIFY_BAD;
	} else
		pip->next_pgno = NEXT_PGNO(h);

	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_vrfy_datapage --
 *	Verify elements common to data pages (P_HASH, P_LBTREE,
 *	P_IBTREE, P_IRECNO, P_LRECNO, P_OVERFLOW, P_DUPLICATE)--i.e.,
 *	those defined in the PAGE structure.
 *
 *	Called from each of the per-page routines, after the
 *	all-page-type-common elements of pip have been verified and filled
 *	in.
 *
 * PUBLIC: int __db_vrfy_datapage
 * PUBLIC:     __P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t, u_int32_t));
 */
int
__db_vrfy_datapage(dbp, vdp, h, pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t flags;
{
	VRFY_PAGEINFO *pip;
	int isbad, ret, t_ret;

	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);
	isbad = 0;

	/*
	 * prev_pgno and next_pgno:  store for inter-page checks,
	 * verify that they point to actual pages and not to self.
	 *
	 * !!!
	 * Internal btree pages do not maintain these fields (indeed,
	 * they overload them).  Skip.
	 */
	if (TYPE(h) != P_IBTREE && TYPE(h) != P_IRECNO) {
		if (!IS_VALID_PGNO(PREV_PGNO(h)) || PREV_PGNO(h) == pip->pgno) {
			isbad = 1;
			EPRINT((dbp->dbenv, "Page %lu: Invalid prev_pgno %lu",
			    (u_long)pip->pgno, (u_long)PREV_PGNO(h)));
		}
		if (!IS_VALID_PGNO(NEXT_PGNO(h)) || NEXT_PGNO(h) == pip->pgno) {
			isbad = 1;
			EPRINT((dbp->dbenv, "Page %lu: Invalid next_pgno %lu",
			    (u_long)pip->pgno, (u_long)NEXT_PGNO(h)));
		}
		pip->prev_pgno = PREV_PGNO(h);
		pip->next_pgno = NEXT_PGNO(h);
	}

	/*
	 * Verify the number of entries on the page.
	 * There is no good way to determine if this is accurate;  the
	 * best we can do is verify that it's not more than can, in theory,
	 * fit on the page.  Then, we make sure there are at least
	 * this many valid elements in inp[], and hope that this catches
	 * most cases.
	 */
	if (TYPE(h) != P_OVERFLOW) {
		if (BKEYDATA_PSIZE(0) * NUM_ENT(h) > dbp->pgsize) {
			isbad = 1;
			EPRINT((dbp->dbenv,
			    "Page %lu: Too many entries: %lu",
			    (u_long)pgno, (u_long)NUM_ENT(h)));
		}
		pip->entries = NUM_ENT(h);
	}

	/*
	 * btree level.  Should be zero unless we're a btree;
	 * if we are a btree, should be between LEAFLEVEL and MAXBTREELEVEL,
	 * and we need to save it off.
	 */
	switch (TYPE(h)) {
	case P_IBTREE:
	case P_IRECNO:
		if (LEVEL(h) < LEAFLEVEL + 1 || LEVEL(h) > MAXBTREELEVEL) {
			isbad = 1;
			EPRINT((dbp->dbenv, "Bad btree level %lu on page %lu",
			    (u_long)LEVEL(h), (u_long)pgno));
		}
		pip->bt_level = LEVEL(h);
		break;
	case P_LBTREE:
	case P_LDUP:
	case P_LRECNO:
		if (LEVEL(h) != LEAFLEVEL) {
			isbad = 1;
			EPRINT((dbp->dbenv,
			    "Btree leaf page %lu has incorrect level %lu",
			    (u_long)pgno, (u_long)LEVEL(h)));
		}
		break;
	default:
		if (LEVEL(h) != 0) {
			isbad = 1;
			EPRINT((dbp->dbenv,
			    "Nonzero level %lu in non-btree database page %lu",
			    (u_long)LEVEL(h), (u_long)pgno));
		}
		break;
	}

	/*
	 * Even though inp[] occurs in all PAGEs, we look at it in the
	 * access-method-specific code, since btree and hash treat
	 * item lengths very differently, and one of the most important
	 * things we want to verify is that the data--as specified
	 * by offset and length--cover the right part of the page
	 * without overlaps, gaps, or violations of the page boundary.
	 */
	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0 && ret == 0)
		ret = t_ret;

	return ((ret == 0 && isbad == 1) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_vrfy_meta--
 *	Verify the access-method common parts of a meta page, using
 *	normal mpool routines.
 *
 * PUBLIC: int __db_vrfy_meta
 * PUBLIC:     __P((DB *, VRFY_DBINFO *, DBMETA *, db_pgno_t, u_int32_t));
 */
int
__db_vrfy_meta(dbp, vdp, meta, pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	DBMETA *meta;
	db_pgno_t pgno;
	u_int32_t flags;
{
	DBTYPE dbtype, magtype;
	VRFY_PAGEINFO *pip;
	int isbad, ret, t_ret;

	isbad = 0;
	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);

	/* type plausible for a meta page */
	switch (meta->type) {
	case P_BTREEMETA:
		dbtype = DB_BTREE;
		break;
	case P_HASHMETA:
		dbtype = DB_HASH;
		break;
	case P_QAMMETA:
		dbtype = DB_QUEUE;
		break;
	default:
		/* The verifier should never let us get here. */
		DB_ASSERT(0);
		ret = EINVAL;
		goto err;
	}

	/* magic number valid */
	if (!__db_is_valid_magicno(meta->magic, &magtype)) {
		isbad = 1;
		EPRINT((dbp->dbenv,
		    "Magic number invalid on page %lu", (u_long)pgno));
	}
	if (magtype != dbtype) {
		isbad = 1;
		EPRINT((dbp->dbenv,
		    "Magic number does not match type of page %lu",
		    (u_long)pgno));
	}

	/* version */
	if ((dbtype == DB_BTREE && meta->version != DB_BTREEVERSION) ||
	    (dbtype == DB_HASH && meta->version != DB_HASHVERSION) ||
	    (dbtype == DB_QUEUE && meta->version != DB_QAMVERSION)) {
		isbad = 1;
		EPRINT((dbp->dbenv, "%s%s", "Old of incorrect DB ",
		    "version; extraneous errors may result"));
	}

	/* pagesize */
	if (meta->pagesize != dbp->pgsize) {
		isbad = 1;
		EPRINT((dbp->dbenv,
		    "Invalid pagesize %lu on page %lu",
		    (u_long)meta->pagesize, (u_long)pgno));
	}

	/* free list */
	/*
	 * If this is not the main, master-database meta page, it
	 * should not have a free list.
	 */
	if (pgno != PGNO_BASE_MD && meta->free != PGNO_INVALID) {
		isbad = 1;
		EPRINT((dbp->dbenv,
		    "Nonempty free list on subdatabase metadata page %lu",
		    pgno));
	}

	/* Can correctly be PGNO_INVALID--that's just the end of the list. */
	if (meta->free != PGNO_INVALID && IS_VALID_PGNO(meta->free))
		pip->free = meta->free;
	else if (!IS_VALID_PGNO(meta->free)) {
		isbad = 1;
		EPRINT((dbp->dbenv,
		    "Nonsensical free list pgno %lu on page %lu",
		    (u_long)meta->free, (u_long)pgno));
	}

	/*
	 * We have now verified the common fields of the metadata page.
	 * Clear the flag that told us they had been incompletely checked.
	 */
	F_CLR(pip, VRFY_INCOMPLETE);

err:	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0 && ret == 0)
		ret = t_ret;

	return ((ret == 0 && isbad == 1) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_vrfy_freelist --
 *	Walk free list, checking off pages and verifying absence of
 *	loops.
 */
static int
__db_vrfy_freelist(dbp, vdp, meta, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t meta;
	u_int32_t flags;
{
	DB *pgset;
	VRFY_PAGEINFO *pip;
	db_pgno_t pgno;
	int p, ret, t_ret;

	pgset = vdp->pgset;
	DB_ASSERT(pgset != NULL);

	if ((ret = __db_vrfy_getpageinfo(vdp, meta, &pip)) != 0)
		return (ret);
	for (pgno = pip->free; pgno != PGNO_INVALID; pgno = pip->next_pgno) {
		if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
			return (ret);

		/* This shouldn't happen, but just in case. */
		if (!IS_VALID_PGNO(pgno)) {
			EPRINT((dbp->dbenv,
			    "Invalid next_pgno on free list page %lu",
			    (u_long)pgno));
			return (DB_VERIFY_BAD);
		}

		/* Detect cycles. */
		if ((ret = __db_vrfy_pgset_get(pgset, pgno, &p)) != 0)
			return (ret);
		if (p != 0) {
			EPRINT((dbp->dbenv,
			    "Page %lu encountered a second time on free list",
			    (u_long)pgno));
			return (DB_VERIFY_BAD);
		}
		if ((ret = __db_vrfy_pgset_inc(pgset, pgno)) != 0)
			return (ret);

		if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
			return (ret);

		if (pip->type != P_INVALID) {
			EPRINT((dbp->dbenv,
			    "Non-invalid page %lu on free list", (u_long)pgno));
			ret = DB_VERIFY_BAD;	  /* unsafe to continue */
			break;
		}
	}

	if ((t_ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_vrfy_subdbs --
 *	Walk the known-safe master database of subdbs with a cursor,
 *	verifying the structure of each subdatabase we encounter.
 */
static int
__db_vrfy_subdbs(dbp, vdp, dbname, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	const char *dbname;
	u_int32_t flags;
{
	DB *mdbp;
	DBC *dbc;
	DBT key, data;
	VRFY_PAGEINFO *pip;
	db_pgno_t meta_pgno;
	int ret, t_ret, isbad;
	u_int8_t type;

	isbad = 0;
	dbc = NULL;

	if ((ret = __db_master_open(dbp, dbname, DB_RDONLY, 0, &mdbp)) != 0)
		return (ret);

	if ((ret =
	    __db_icursor(mdbp, NULL, DB_BTREE, PGNO_INVALID, 0, &dbc)) != 0)
		goto err;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	while ((ret = dbc->c_get(dbc, &key, &data, DB_NEXT)) == 0) {
		if (data.size != sizeof(db_pgno_t)) {
			EPRINT((dbp->dbenv, "Database entry of invalid size"));
			isbad = 1;
			goto err;
		}
		memcpy(&meta_pgno, data.data, data.size);
		/*
		 * Subdatabase meta pgnos are stored in network byte
		 * order for cross-endian compatibility.  Swap if appropriate.
		 */
		DB_NTOHL(&meta_pgno);
		if (meta_pgno == PGNO_INVALID || meta_pgno > vdp->last_pgno) {
			EPRINT((dbp->dbenv,
			    "Database entry references invalid page %lu",
			    (u_long)meta_pgno));
			isbad = 1;
			goto err;
		}
		if ((ret = __db_vrfy_getpageinfo(vdp, meta_pgno, &pip)) != 0)
			goto err;
		type = pip->type;
		if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
			goto err;
		switch (type) {
		case P_BTREEMETA:
			if ((ret = __bam_vrfy_structure(
			    dbp, vdp, meta_pgno, flags)) != 0) {
				if (ret == DB_VERIFY_BAD)
					isbad = 1;
				else
					goto err;
			}
			break;
		case P_HASHMETA:
			if ((ret = __ham_vrfy_structure(
			    dbp, vdp, meta_pgno, flags)) != 0) {
				if (ret == DB_VERIFY_BAD)
					isbad = 1;
				else
					goto err;
			}
			break;
		case P_QAMMETA:
		default:
			EPRINT((dbp->dbenv,
	    "Database entry references page %lu of invalid type %lu",
			    (u_long)meta_pgno, (u_long)type));
			ret = DB_VERIFY_BAD;
			goto err;
			/* NOTREACHED */
		}
	}

	if (ret == DB_NOTFOUND)
		ret = 0;

err:	if (dbc != NULL && (t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = mdbp->close(mdbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	return ((ret == 0 && isbad == 1) ? DB_VERIFY_BAD : ret);
}

/*
 * __db_vrfy_struct_feedback --
 *	Provide feedback during top-down database structure traversal.
 *	(See comment at the beginning of __db_vrfy_structure.)
 *
 * PUBLIC: int __db_vrfy_struct_feedback __P((DB *, VRFY_DBINFO *));
 */
int
__db_vrfy_struct_feedback(dbp, vdp)
	DB *dbp;
	VRFY_DBINFO *vdp;
{
	int progress;

	if (dbp->db_feedback == NULL)
		return (0);

	if (vdp->pgs_remaining > 0)
		vdp->pgs_remaining--;

	/* Don't allow a feedback call of 100 until we're really done. */
	progress = 100 - (vdp->pgs_remaining * 50 / (vdp->last_pgno + 1));
	dbp->db_feedback(dbp, DB_VERIFY, progress == 100 ? 99 : progress);

	return (0);
}

/*
 * __db_vrfy_orderchkonly --
 *	Do an sort-order/hashing check on a known-otherwise-good subdb.
 */
static int
__db_vrfy_orderchkonly(dbp, vdp, name, subdb, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	const char *name, *subdb;
	u_int32_t flags;
{
	BTMETA *btmeta;
	DB *mdbp, *pgset;
	DBC *pgsc;
	DBT key, data;
	HASH *h_internal;
	HMETA *hmeta;
	PAGE *h, *currpg;
	db_pgno_t meta_pgno, p, pgno;
	u_int32_t bucket;
	int t_ret, ret;

	currpg = h = NULL;
	pgsc = NULL;
	pgset = NULL;

	LF_CLR(DB_NOORDERCHK);

	/* Open the master database and get the meta_pgno for the subdb. */
	if ((ret = db_create(&mdbp, NULL, 0)) != 0)
		return (ret);
	if ((ret = __db_master_open(dbp, name, DB_RDONLY, 0, &mdbp)) != 0)
		goto err;

	memset(&key, 0, sizeof(key));
	key.data = (void *)subdb;
	memset(&data, 0, sizeof(data));
	if ((ret = dbp->get(dbp, NULL, &key, &data, 0)) != 0)
		goto err;

	if (data.size != sizeof(db_pgno_t)) {
		EPRINT((dbp->dbenv, "Database entry of invalid size"));
		ret = DB_VERIFY_BAD;
		goto err;
	}

	memcpy(&meta_pgno, data.data, data.size);

	if ((ret = memp_fget(dbp->mpf, &meta_pgno, 0, &h)) != 0)
		goto err;

	if ((ret = __db_vrfy_pgset(dbp->dbenv, dbp->pgsize, &pgset)) != 0)
		goto err;

	switch (TYPE(h)) {
	case P_BTREEMETA:
		btmeta = (BTMETA *)h;
		if (F_ISSET(&btmeta->dbmeta, BTM_RECNO)) {
			/* Recnos have no order to check. */
			ret = 0;
			goto err;
		}
		if ((ret =
		    __db_meta2pgset(dbp, vdp, meta_pgno, flags, pgset)) != 0)
			goto err;
		if ((ret = pgset->cursor(pgset, NULL, &pgsc, 0)) != 0)
			goto err;
		while ((ret = __db_vrfy_pgset_next(pgsc, &p)) == 0) {
			if ((ret = memp_fget(dbp->mpf, &p, 0, &currpg)) != 0)
				goto err;
			if ((ret = __bam_vrfy_itemorder(dbp,
			    NULL, currpg, p, NUM_ENT(currpg), 1,
			    F_ISSET(&btmeta->dbmeta, BTM_DUP), flags)) != 0)
				goto err;
			if ((ret = memp_fput(dbp->mpf, currpg, 0)) != 0)
				goto err;
			currpg = NULL;
		}
		if ((ret = pgsc->c_close(pgsc)) != 0)
			goto err;
		break;
	case P_HASHMETA:
		hmeta = (HMETA *)h;
		h_internal = (HASH *)dbp->h_internal;
		/*
		 * Make sure h_charkey is right.
		 */
		if (h_internal == NULL || h_internal->h_hash == NULL) {
			EPRINT((dbp->dbenv,
		    "DB_ORDERCHKONLY requires that a hash function be set"));
			ret = DB_VERIFY_BAD;
			goto err;
		}
		if (hmeta->h_charkey !=
		    h_internal->h_hash(dbp, CHARKEY, sizeof(CHARKEY))) {
			EPRINT((dbp->dbenv,
			    "Incorrect hash function for database"));
			ret = DB_VERIFY_BAD;
			goto err;
		}

		/*
		 * Foreach bucket, verify hashing on each page in the
		 * corresponding chain of pages.
		 */
		for (bucket = 0; bucket <= hmeta->max_bucket; bucket++) {
			pgno = BS_TO_PAGE(bucket, hmeta->spares);
			while (pgno != PGNO_INVALID) {
				if ((ret = memp_fget(dbp->mpf,
				    &pgno, 0, &currpg)) != 0)
					goto err;
				if ((ret = __ham_vrfy_hashing(dbp,
				    NUM_ENT(currpg),hmeta, bucket, pgno,
				    flags, h_internal->h_hash)) != 0)
					goto err;
				pgno = NEXT_PGNO(currpg);
				if ((ret = memp_fput(dbp->mpf, currpg, 0)) != 0)
					goto err;
				currpg = NULL;
			}
		}
		break;
	default:
		EPRINT((dbp->dbenv, "Database meta page %lu of bad type %lu",
		    (u_long)meta_pgno, (u_long)TYPE(h)));
		ret = DB_VERIFY_BAD;
		break;
	}

err:	if (pgsc != NULL)
		(void)pgsc->c_close(pgsc);
	if (pgset != NULL)
		(void)pgset->close(pgset, 0);
	if (h != NULL && (t_ret = memp_fput(dbp->mpf, h, 0)) != 0)
		ret = t_ret;
	if (currpg != NULL && (t_ret = memp_fput(dbp->mpf, currpg, 0)) != 0)
		ret = t_ret;
	if ((t_ret = mdbp->close(mdbp, 0)) != 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_salvage --
 *	Walk through a page, salvaging all likely or plausible (w/
 *	DB_AGGRESSIVE) key/data pairs.
 *
 * PUBLIC: int __db_salvage __P((DB *, VRFY_DBINFO *, db_pgno_t, PAGE *,
 * PUBLIC:     void *, int (*)(void *, const void *), u_int32_t));
 */
int
__db_salvage(dbp, vdp, pgno, h, handle, callback, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	PAGE *h;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	DB_ASSERT(LF_ISSET(DB_SALVAGE));

	/* If we got this page in the subdb pass, we can safely skip it. */
	if (__db_salvage_isdone(vdp, pgno))
		return (0);

	switch (TYPE(h)) {
	case P_HASH:
		return (__ham_salvage(dbp,
		    vdp, pgno, h, handle, callback, flags));
		/* NOTREACHED */
	case P_LBTREE:
		return (__bam_salvage(dbp,
		    vdp, pgno, P_LBTREE, h, handle, callback, NULL, flags));
		/* NOTREACHED */
	case P_LDUP:
		return (__db_salvage_markneeded(vdp, pgno, SALVAGE_LDUP));
		/* NOTREACHED */
	case P_OVERFLOW:
		return (__db_salvage_markneeded(vdp, pgno, SALVAGE_OVERFLOW));
		/* NOTREACHED */
	case P_LRECNO:
		/*
		 * Recnos are tricky -- they may represent dup pages, or
		 * they may be subdatabase/regular database pages in their
		 * own right.  If the former, they need to be printed with a
		 * key, preferably when we hit the corresponding datum in
		 * a btree/hash page.  If the latter, there is no key.
		 *
		 * If a database is sufficiently frotzed, we're not going
		 * to be able to get this right, so we best-guess:  just
		 * mark it needed now, and if we're really a normal recno
		 * database page, the "unknowns" pass will pick us up.
		 */
		return (__db_salvage_markneeded(vdp, pgno, SALVAGE_LRECNO));
		/* NOTREACHED */
	case P_IBTREE:
	case P_INVALID:
	case P_IRECNO:
	case __P_DUPLICATE:
	default:
		/* XXX: Should we be more aggressive here? */
		break;
	}
	return (0);
}

/*
 * __db_salvage_unknowns --
 *	Walk through the salvager database, printing with key "UNKNOWN"
 *	any pages we haven't dealt with.
 */
static int
__db_salvage_unknowns(dbp, vdp, handle, callback, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	DBT unkdbt, key, *dbt;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t pgtype;
	int ret, err_ret;
	void *ovflbuf;

	memset(&unkdbt, 0, sizeof(DBT));
	unkdbt.size = strlen("UNKNOWN") + 1;
	unkdbt.data = "UNKNOWN";

	if ((ret = __os_malloc(dbp->dbenv, dbp->pgsize, 0, &ovflbuf)) != 0)
		return (ret);

	err_ret = 0;
	while ((ret = __db_salvage_getnext(vdp, &pgno, &pgtype)) == 0) {
		dbt = NULL;

		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0) {
			err_ret = ret;
			continue;
		}

		switch (pgtype) {
		case SALVAGE_LDUP:
		case SALVAGE_LRECNODUP:
			dbt = &unkdbt;
			/* FALLTHROUGH */
		case SALVAGE_LBTREE:
		case SALVAGE_LRECNO:
			if ((ret = __bam_salvage(dbp, vdp, pgno, pgtype,
			    h, handle, callback, dbt, flags)) != 0)
				err_ret = ret;
			break;
		case SALVAGE_OVERFLOW:
			/*
			 * XXX:
			 * This may generate multiple "UNKNOWN" keys in
			 * a database with no dups.  What to do?
			 */
			if ((ret = __db_safe_goff(dbp,
			    vdp, pgno, &key, &ovflbuf, flags)) != 0) {
				err_ret = ret;
				continue;
			}
			if ((ret = __db_prdbt(&key,
			    0, " ", handle, callback, 0, NULL)) != 0) {
				err_ret = ret;
				continue;
			}
			if ((ret = __db_prdbt(&unkdbt,
				0, " ", handle, callback, 0, NULL)) != 0)
				err_ret = ret;
			break;
		case SALVAGE_HASH:
			if ((ret = __ham_salvage(
			    dbp, vdp, pgno, h, handle, callback, flags)) != 0)
				err_ret = ret;
			break;
		case SALVAGE_INVALID:
		case SALVAGE_IGNORE:
		default:
			/*
			 * Shouldn't happen, but if it does, just do what the
			 * nice man says.
			 */
			DB_ASSERT(0);
			break;
		}
		if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
			err_ret = ret;
	}

	__os_free(ovflbuf, 0);

	if (err_ret != 0 && ret == 0)
		ret = err_ret;

	return (ret == DB_NOTFOUND ? 0 : ret);
}

/*
 * Offset of the ith inp array entry, which we can compare to the offset
 * the entry stores.
 */
#define	INP_OFFSET(h, i)	\
    ((db_indx_t)((u_int8_t *)(h)->inp + (i) - (u_int8_t *)(h)))

/*
 * __db_vrfy_inpitem --
 *	Verify that a single entry in the inp array is sane, and update
 *	the high water mark and current item offset.  (The former of these is
 *	used for state information between calls, and is required;  it must
 *	be initialized to the pagesize before the first call.)
 *
 *	Returns DB_VERIFY_FATAL if inp has collided with the data,
 *	since verification can't continue from there;  returns DB_VERIFY_BAD
 *	if anything else is wrong.
 *
 * PUBLIC: int __db_vrfy_inpitem __P((DB *, PAGE *,
 * PUBLIC:     db_pgno_t, u_int32_t, int, u_int32_t, u_int32_t *, u_int32_t *));
 */
int
__db_vrfy_inpitem(dbp, h, pgno, i, is_btree, flags, himarkp, offsetp)
	DB *dbp;
	PAGE *h;
	db_pgno_t pgno;
	u_int32_t i;
	int is_btree;
	u_int32_t flags, *himarkp, *offsetp;
{
	BKEYDATA *bk;
	db_indx_t offset, len;

	DB_ASSERT(himarkp != NULL);

	/*
	 * Check that the inp array, which grows from the beginning of the
	 * page forward, has not collided with the data, which grow from the
	 * end of the page backward.
	 */
	if (h->inp + i >= (db_indx_t *)((u_int8_t *)h + *himarkp)) {
		/* We've collided with the data.  We need to bail. */
		EPRINT((dbp->dbenv,
		    "Page %lu entries listing %lu overlaps data",
		    (u_long)pgno, (u_long)i));
		return (DB_VERIFY_FATAL);
	}

	offset = h->inp[i];

	/*
	 * Check that the item offset is reasonable:  it points somewhere
	 * after the inp array and before the end of the page.
	 */
	if (offset <= INP_OFFSET(h, i) || offset > dbp->pgsize) {
		EPRINT((dbp->dbenv,
		    "Bad offset %lu at page %lu index %lu",
		    (u_long)offset, (u_long)pgno, (u_long)i));
		return (DB_VERIFY_BAD);
	}

	/* Update the high-water mark (what HOFFSET should be) */
	if (offset < *himarkp)
		*himarkp = offset;

	if (is_btree) {
		/*
		 * Check that the item length remains on-page.
		 */
		bk = GET_BKEYDATA(h, i);

		/*
		 * We need to verify the type of the item here;
		 * we can't simply assume that it will be one of the
		 * expected three.  If it's not a recognizable type,
		 * it can't be considered to have a verifiable
		 * length, so it's not possible to certify it as safe.
		 */
		switch (B_TYPE(bk->type)) {
		case B_KEYDATA:
			len = bk->len;
			break;
		case B_DUPLICATE:
		case B_OVERFLOW:
			len = BOVERFLOW_SIZE;
			break;
		default:
			EPRINT((dbp->dbenv,
			    "Item %lu on page %lu of unrecognizable type",
			    i, pgno));
			return (DB_VERIFY_BAD);
		}

		if ((size_t)(offset + len) > dbp->pgsize) {
			EPRINT((dbp->dbenv,
			    "Item %lu on page %lu extends past page boundary",
			    (u_long)i, (u_long)pgno));
			return (DB_VERIFY_BAD);
		}
	}

	if (offsetp != NULL)
		*offsetp = offset;
	return (0);
}

/*
 * __db_vrfy_duptype--
 *	Given a page number and a set of flags to __bam_vrfy_subtree,
 *	verify that the dup tree type is correct--i.e., it's a recno
 *	if DUPSORT is not set and a btree if it is.
 *
 * PUBLIC: int __db_vrfy_duptype
 * PUBLIC:     __P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t));
 */
int
__db_vrfy_duptype(dbp, vdp, pgno, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	u_int32_t flags;
{
	VRFY_PAGEINFO *pip;
	int ret, isbad;

	isbad = 0;

	if ((ret = __db_vrfy_getpageinfo(vdp, pgno, &pip)) != 0)
		return (ret);

	switch (pip->type) {
	case P_IBTREE:
	case P_LDUP:
		if (!LF_ISSET(ST_DUPSORT)) {
			EPRINT((dbp->dbenv,
	    "Sorted duplicate set at page %lu in unsorted-dup database",
			    (u_long)pgno));
			isbad = 1;
		}
		break;
	case P_IRECNO:
	case P_LRECNO:
		if (LF_ISSET(ST_DUPSORT)) {
			EPRINT((dbp->dbenv,
	    "Unsorted duplicate set at page %lu in sorted-dup database",
			    (u_long)pgno));
			isbad = 1;
		}
		break;
	default:
		EPRINT((dbp->dbenv,
		    "Duplicate page %lu of inappropriate type %lu",
		    (u_long)pgno, (u_long)pip->type));
		isbad = 1;
		break;
	}

	if ((ret = __db_vrfy_putpageinfo(vdp, pip)) != 0)
		return (ret);
	return (isbad == 1 ? DB_VERIFY_BAD : 0);
}

/*
 * __db_salvage_duptree --
 *	Attempt to salvage a given duplicate tree, given its alleged root.
 *
 *	The key that corresponds to this dup set has been passed to us
 *	in DBT *key.  Because data items follow keys, though, it has been
 *	printed once already.
 *
 *	The basic idea here is that pgno ought to be a P_LDUP, a P_LRECNO, a
 *	P_IBTREE, or a P_IRECNO.  If it's an internal page, use the verifier
 *	functions to make sure it's safe;  if it's not, we simply bail and the
 *	data will have to be printed with no key later on.  if it is safe,
 *	recurse on each of its children.
 *
 *	Whether or not it's safe, if it's a leaf page, __bam_salvage it.
 *
 *	At all times, use the DB hanging off vdp to mark and check what we've
 *	done, so each page gets printed exactly once and we don't get caught
 *	in any cycles.
 *
 * PUBLIC: int __db_salvage_duptree __P((DB *, VRFY_DBINFO *, db_pgno_t,
 * PUBLIC:     DBT *, void *, int (*)(void *, const void *), u_int32_t));
 */
int
__db_salvage_duptree(dbp, vdp, pgno, key, handle, callback, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	DBT *key;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	PAGE *h;
	int ret, t_ret;

	if (pgno == PGNO_INVALID || !IS_VALID_PGNO(pgno))
		return (DB_VERIFY_BAD);

	/* We have a plausible page.  Try it. */
	if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0)
		return (ret);

	switch (TYPE(h)) {
	case P_IBTREE:
	case P_IRECNO:
		if ((ret = __db_vrfy_common(dbp, vdp, h, pgno, flags)) != 0)
			goto err;
		if ((ret = __bam_vrfy(dbp,
		    vdp, h, pgno, flags | DB_NOORDERCHK)) != 0 ||
		    (ret = __db_salvage_markdone(vdp, pgno)) != 0)
			goto err;
		/*
		 * We have a known-healthy internal page.  Walk it.
		 */
		if ((ret = __bam_salvage_walkdupint(dbp, vdp, h, key,
		    handle, callback, flags)) != 0)
			goto err;
		break;
	case P_LRECNO:
	case P_LDUP:
		if ((ret = __bam_salvage(dbp,
		    vdp, pgno, TYPE(h), h, handle, callback, key, flags)) != 0)
			goto err;
		break;
	default:
		ret = DB_VERIFY_BAD;
		goto err;
		/* NOTREACHED */
	}

err:	if ((t_ret = memp_fput(dbp->mpf, h, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_salvage_subdbs --
 *	Check and see if this database has subdbs;  if so, try to salvage
 *	them independently.
 */
static int
__db_salvage_subdbs(dbp, vdp, handle, callback, flags, hassubsp)
	DB *dbp;
	VRFY_DBINFO *vdp;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
	int *hassubsp;
{
	BTMETA *btmeta;
	DB *pgset;
	DBC *pgsc;
	PAGE *h;
	db_pgno_t p, meta_pgno;
	int ret, err_ret;

	err_ret = 0;
	pgsc = NULL;
	pgset = NULL;

	meta_pgno = PGNO_BASE_MD;
	if ((ret = memp_fget(dbp->mpf, &meta_pgno, 0, &h)) != 0)
		return (ret);

	if (TYPE(h) == P_BTREEMETA)
		btmeta = (BTMETA *)h;
	else {
		/* Not a btree metadata, ergo no subdbs, so just return. */
		ret = 0;
		goto err;
	}

	/* If it's not a safe page, bail on the attempt. */
	if ((ret = __db_vrfy_common(dbp, vdp, h, PGNO_BASE_MD, flags)) != 0 ||
	   (ret = __bam_vrfy_meta(dbp, vdp, btmeta, PGNO_BASE_MD, flags)) != 0)
		goto err;

	if (!F_ISSET(&btmeta->dbmeta, BTM_SUBDB)) {
		/* No subdbs, just return. */
		ret = 0;
		goto err;
	}

	/* We think we've got subdbs.  Mark it so. */
	*hassubsp = 1;

	if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
		return (ret);

	/*
	 * We have subdbs.  Try to crack them.
	 *
	 * To do so, get a set of leaf pages in the master
	 * database, and then walk each of the valid ones, salvaging
	 * subdbs as we go.  If any prove invalid, just drop them;  we'll
	 * pick them up on a later pass.
	 */
	if ((ret = __db_vrfy_pgset(dbp->dbenv, dbp->pgsize, &pgset)) != 0)
		return (ret);
	if ((ret =
	    __db_meta2pgset(dbp, vdp, PGNO_BASE_MD, flags, pgset)) != 0)
		goto err;

	if ((ret = pgset->cursor(pgset, NULL, &pgsc, 0)) != 0)
		goto err;
	while ((ret = __db_vrfy_pgset_next(pgsc, &p)) == 0) {
		if ((ret = memp_fget(dbp->mpf, &p, 0, &h)) != 0) {
			err_ret = ret;
			continue;
		}
		if ((ret = __db_vrfy_common(dbp, vdp, h, p, flags)) != 0 ||
		    (ret = __bam_vrfy(dbp,
		    vdp, h, p, flags | DB_NOORDERCHK)) != 0)
			goto nextpg;
		if (TYPE(h) != P_LBTREE)
			goto nextpg;
		else if ((ret = __db_salvage_subdbpg(
		    dbp, vdp, h, handle, callback, flags)) != 0)
			err_ret = ret;
nextpg:		if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
			err_ret = ret;
	}

	if (ret != DB_NOTFOUND)
		goto err;
	if ((ret = pgsc->c_close(pgsc)) != 0)
		goto err;

	ret = pgset->close(pgset, 0);
	return ((ret == 0 && err_ret != 0) ? err_ret : ret);

	/* NOTREACHED */

err:	if (pgsc != NULL)
		(void)pgsc->c_close(pgsc);
	if (pgset != NULL)
		(void)pgset->close(pgset, 0);
	(void)memp_fput(dbp->mpf, h, 0);
	return (ret);
}

/*
 * __db_salvage_subdbpg --
 *	Given a known-good leaf page in the master database, salvage all
 *	leaf pages corresponding to each subdb.
 *
 * PUBLIC: int __db_salvage_subdbpg
 * PUBLIC:     __P((DB *, VRFY_DBINFO *, PAGE *, void *,
 * PUBLIC:     int (*)(void *, const void *), u_int32_t));
 */
int
__db_salvage_subdbpg(dbp, vdp, master, handle, callback, flags)
	DB *dbp;
	VRFY_DBINFO *vdp;
	PAGE *master;
	void *handle;
	int (*callback) __P((void *, const void *));
	u_int32_t flags;
{
	BKEYDATA *bkkey, *bkdata;
	BOVERFLOW *bo;
	DB *pgset;
	DBC *pgsc;
	DBT key;
	PAGE *subpg;
	db_indx_t i;
	db_pgno_t meta_pgno, p;
	int ret, err_ret, t_ret;
	char *subdbname;

	ret = err_ret = 0;
	subdbname = NULL;

	if ((ret = __db_vrfy_pgset(dbp->dbenv, dbp->pgsize, &pgset)) != 0)
		return (ret);

	/*
	 * For each entry, get and salvage the set of pages
	 * corresponding to that entry.
	 */
	for (i = 0; i < NUM_ENT(master); i += P_INDX) {
		bkkey = GET_BKEYDATA(master, i);
		bkdata = GET_BKEYDATA(master, i + O_INDX);

		/* Get the subdatabase name. */
		if (B_TYPE(bkkey->type) == B_OVERFLOW) {
			/*
			 * We can, in principle anyway, have a subdb
			 * name so long it overflows.  Ick.
			 */
			bo = (BOVERFLOW *)bkkey;
			if ((ret = __db_safe_goff(dbp, vdp, bo->pgno, &key,
			    (void **)&subdbname, flags)) != 0) {
				err_ret = DB_VERIFY_BAD;
				continue;
			}

			/* Nul-terminate it. */
			if ((ret = __os_realloc(dbp->dbenv,
			    key.size + 1, NULL, &subdbname)) != 0)
				goto err;
			subdbname[key.size] = '\0';
		} else if (B_TYPE(bkkey->type == B_KEYDATA)) {
			if ((ret = __os_realloc(dbp->dbenv,
			    bkkey->len + 1, NULL, &subdbname)) != 0)
				goto err;
			memcpy(subdbname, bkkey->data, bkkey->len);
			subdbname[bkkey->len] = '\0';
		}

		/* Get the corresponding pgno. */
		if (bkdata->len != sizeof(db_pgno_t)) {
			err_ret = DB_VERIFY_BAD;
			continue;
		}
		memcpy(&meta_pgno, bkdata->data, sizeof(db_pgno_t));

		/* If we can't get the subdb meta page, just skip the subdb. */
		if (!IS_VALID_PGNO(meta_pgno) ||
		    (ret = memp_fget(dbp->mpf, &meta_pgno, 0, &subpg)) != 0) {
			err_ret = ret;
			continue;
		}

		/*
		 * Verify the subdatabase meta page.  This has two functions.
		 * First, if it's bad, we have no choice but to skip the subdb
		 * and let the pages just get printed on a later pass.  Second,
		 * the access-method-specific meta verification routines record
		 * the various state info (such as the presence of dups)
		 * that we need for __db_prheader().
		 */
		if ((ret =
		    __db_vrfy_common(dbp, vdp, subpg, meta_pgno, flags)) != 0) {
			err_ret = ret;
			(void)memp_fput(dbp->mpf, subpg, 0);
			continue;
		}
		switch (TYPE(subpg)) {
		case P_BTREEMETA:
			if ((ret = __bam_vrfy_meta(dbp,
			    vdp, (BTMETA *)subpg, meta_pgno, flags)) != 0) {
				err_ret = ret;
				(void)memp_fput(dbp->mpf, subpg, 0);
				continue;
			}
			break;
		case P_HASHMETA:
			if ((ret = __ham_vrfy_meta(dbp,
			    vdp, (HMETA *)subpg, meta_pgno, flags)) != 0) {
				err_ret = ret;
				(void)memp_fput(dbp->mpf, subpg, 0);
				continue;
			}
			break;
		default:
			/* This isn't an appropriate page;  skip this subdb. */
			err_ret = DB_VERIFY_BAD;
			continue;
			/* NOTREACHED */
		}

		if ((ret = memp_fput(dbp->mpf, subpg, 0)) != 0) {
			err_ret = ret;
			continue;
		}

		/* Print a subdatabase header. */
		if ((ret = __db_prheader(dbp,
		    subdbname, 0, 0, handle, callback, vdp, meta_pgno)) != 0)
			goto err;

		if ((ret = __db_meta2pgset(dbp, vdp, meta_pgno,
		    flags, pgset)) != 0) {
			err_ret = ret;
			continue;
		}

		if ((ret = pgset->cursor(pgset, NULL, &pgsc, 0)) != 0)
			goto err;
		while ((ret = __db_vrfy_pgset_next(pgsc, &p)) == 0) {
			if ((ret = memp_fget(dbp->mpf, &p, 0, &subpg)) != 0) {
				err_ret = ret;
				continue;
			}
			if ((ret = __db_salvage(dbp, vdp, p, subpg,
			    handle, callback, flags)) != 0)
				err_ret = ret;
			if ((ret = memp_fput(dbp->mpf, subpg, 0)) != 0)
				err_ret = ret;
		}

		if (ret != DB_NOTFOUND)
			goto err;

		if ((ret = pgsc->c_close(pgsc)) != 0)
			goto err;
		if ((ret = __db_prfooter(handle, callback)) != 0)
			goto err;
	}
err:	if (subdbname)
		__os_free(subdbname, 0);

	if ((t_ret = pgset->close(pgset, 0)) != 0)
		ret = t_ret;

	if ((t_ret = __db_salvage_markdone(vdp, PGNO(master))) != 0)
		return (t_ret);

	return ((err_ret != 0) ? err_ret : ret);
}

/*
 * __db_meta2pgset --
 *	Given a known-safe meta page number, return the set of pages
 *	corresponding to the database it represents.  Return DB_VERIFY_BAD if
 *	it's not a suitable meta page or is invalid.
 */
static int
__db_meta2pgset(dbp, vdp, pgno, flags, pgset)
	DB *dbp;
	VRFY_DBINFO *vdp;
	db_pgno_t pgno;
	u_int32_t flags;
	DB *pgset;
{
	PAGE *h;
	int ret, t_ret;

	if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0)
		return (ret);

	switch (TYPE(h)) {
	case P_BTREEMETA:
		ret = __bam_meta2pgset(dbp, vdp, (BTMETA *)h, flags, pgset);
		break;
	case P_HASHMETA:
		ret = __ham_meta2pgset(dbp, vdp, (HMETA *)h, flags, pgset);
		break;
	default:
		ret = DB_VERIFY_BAD;
		break;
	}

	if ((t_ret = memp_fput(dbp->mpf, h, 0)) != 0)
		return (t_ret);
	return (ret);
}

/*
 * __db_guesspgsize --
 *	Try to guess what the pagesize is if the one on the meta page
 *	and the one in the db are invalid.
 */
static int
__db_guesspgsize(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	db_pgno_t i;
	size_t nr;
	u_int32_t guess;
	u_int8_t type;
	int ret;

	for (guess = DB_MAX_PGSIZE; guess >= DB_MIN_PGSIZE; guess >>= 1) {
		/*
		 * We try to read three pages ahead after the first one
		 * and make sure we have plausible types for all of them.
		 * If the seeks fail, continue with a smaller size;
		 * we're probably just looking past the end of the database.
		 * If they succeed and the types are reasonable, also continue
		 * with a size smaller;  we may be looking at pages N,
		 * 2N, and 3N for some N > 1.
		 *
		 * As soon as we hit an invalid type, we stop and return
		 * our previous guess; that last one was probably the page size.
		 */
		for (i = 1; i <= 3; i++) {
			if ((ret = __os_seek(dbenv, fhp, guess,
			    i, SSZ(DBMETA, type), 0, DB_OS_SEEK_SET)) != 0)
				break;
			if ((ret = __os_read(dbenv,
			    fhp, &type, 1, &nr)) != 0 || nr == 0)
				break;
			if (type == P_INVALID || type >= P_PAGETYPE_MAX)
				return (guess << 1);
		}
	}

	/*
	 * If we're just totally confused--the corruption takes up most of the
	 * beginning pages of the database--go with the default size.
	 */
	return (DB_DEF_IOSIZE);
}
