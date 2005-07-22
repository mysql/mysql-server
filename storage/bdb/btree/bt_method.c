/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: bt_method.c,v 11.38 2004/09/22 03:31:26 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/qam.h"

static int __bam_set_bt_maxkey __P((DB *, u_int32_t));
static int __bam_set_bt_minkey __P((DB *, u_int32_t));
static int __bam_set_bt_prefix
	       __P((DB *, size_t(*)(DB *, const DBT *, const DBT *)));
static int __ram_get_re_delim __P((DB *, int *));
static int __ram_set_re_delim __P((DB *, int));
static int __ram_set_re_len __P((DB *, u_int32_t));
static int __ram_set_re_pad __P((DB *, int));
static int __ram_get_re_source __P((DB *, const char **));
static int __ram_set_re_source __P((DB *, const char *));

/*
 * __bam_db_create --
 *	Btree specific initialization of the DB structure.
 *
 * PUBLIC: int __bam_db_create __P((DB *));
 */
int
__bam_db_create(dbp)
	DB *dbp;
{
	BTREE *t;
	int ret;

	/* Allocate and initialize the private btree structure. */
	if ((ret = __os_calloc(dbp->dbenv, 1, sizeof(BTREE), &t)) != 0)
		return (ret);
	dbp->bt_internal = t;

	t->bt_minkey = DEFMINKEYPAGE;		/* Btree */
	t->bt_compare = __bam_defcmp;
	t->bt_prefix = __bam_defpfx;

	dbp->set_bt_compare = __bam_set_bt_compare;
	dbp->set_bt_maxkey = __bam_set_bt_maxkey;
	dbp->get_bt_minkey = __bam_get_bt_minkey;
	dbp->set_bt_minkey = __bam_set_bt_minkey;
	dbp->set_bt_prefix = __bam_set_bt_prefix;

	t->re_pad = ' ';			/* Recno */
	t->re_delim = '\n';
	t->re_eof = 1;

	dbp->get_re_delim = __ram_get_re_delim;
	dbp->set_re_delim = __ram_set_re_delim;
	dbp->get_re_len = __ram_get_re_len;
	dbp->set_re_len = __ram_set_re_len;
	dbp->get_re_pad = __ram_get_re_pad;
	dbp->set_re_pad = __ram_set_re_pad;
	dbp->get_re_source = __ram_get_re_source;
	dbp->set_re_source = __ram_set_re_source;

	return (0);
}

/*
 * __bam_db_close --
 *	Btree specific discard of the DB structure.
 *
 * PUBLIC: int __bam_db_close __P((DB *));
 */
int
__bam_db_close(dbp)
	DB *dbp;
{
	BTREE *t;

	if ((t = dbp->bt_internal) == NULL)
		return (0);
						/* Recno */
	/* Close any backing source file descriptor. */
	if (t->re_fp != NULL)
		(void)fclose(t->re_fp);

	/* Free any backing source file name. */
	if (t->re_source != NULL)
		__os_free(dbp->dbenv, t->re_source);

	__os_free(dbp->dbenv, t);
	dbp->bt_internal = NULL;

	return (0);
}

/*
 * __bam_map_flags --
 *	Map Btree specific flags from public to the internal values.
 *
 * PUBLIC: void __bam_map_flags __P((DB *, u_int32_t *, u_int32_t *));
 */
void
__bam_map_flags(dbp, inflagsp, outflagsp)
	DB *dbp;
	u_int32_t *inflagsp, *outflagsp;
{
	COMPQUIET(dbp, NULL);

	if (FLD_ISSET(*inflagsp, DB_DUP)) {
		FLD_SET(*outflagsp, DB_AM_DUP);
		FLD_CLR(*inflagsp, DB_DUP);
	}
	if (FLD_ISSET(*inflagsp, DB_DUPSORT)) {
		FLD_SET(*outflagsp, DB_AM_DUP | DB_AM_DUPSORT);
		FLD_CLR(*inflagsp, DB_DUPSORT);
	}
	if (FLD_ISSET(*inflagsp, DB_RECNUM)) {
		FLD_SET(*outflagsp, DB_AM_RECNUM);
		FLD_CLR(*inflagsp, DB_RECNUM);
	}
	if (FLD_ISSET(*inflagsp, DB_REVSPLITOFF)) {
		FLD_SET(*outflagsp, DB_AM_REVSPLITOFF);
		FLD_CLR(*inflagsp, DB_REVSPLITOFF);
	}
}

/*
 * __bam_set_flags --
 *	Set Btree specific flags.
 *
 * PUBLIC: int __bam_set_flags __P((DB *, u_int32_t *flagsp));
 */
int
__bam_set_flags(dbp, flagsp)
	DB *dbp;
	u_int32_t *flagsp;
{
	u_int32_t flags;

	flags = *flagsp;
	if (LF_ISSET(DB_DUP | DB_DUPSORT | DB_RECNUM | DB_REVSPLITOFF))
		DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_flags");

	/*
	 * The DB_DUP and DB_DUPSORT flags are shared by the Hash
	 * and Btree access methods.
	 */
	if (LF_ISSET(DB_DUP | DB_DUPSORT))
		DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE | DB_OK_HASH);

	if (LF_ISSET(DB_RECNUM | DB_REVSPLITOFF))
		DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	/* DB_DUP/DB_DUPSORT is incompatible with DB_RECNUM. */
	if (LF_ISSET(DB_DUP | DB_DUPSORT) && F_ISSET(dbp, DB_AM_RECNUM))
		goto incompat;

	/* DB_RECNUM is incompatible with DB_DUP/DB_DUPSORT. */
	if (LF_ISSET(DB_RECNUM) && F_ISSET(dbp, DB_AM_DUP))
		goto incompat;

	if (LF_ISSET(DB_DUPSORT) && dbp->dup_compare == NULL)
		dbp->dup_compare = __bam_defcmp;

	__bam_map_flags(dbp, flagsp, &dbp->flags);
	return (0);

incompat:
	return (__db_ferr(dbp->dbenv, "DB->set_flags", 1));
}

/*
 * __bam_set_bt_compare --
 *	Set the comparison function.
 *
 * PUBLIC: int __bam_set_bt_compare
 * PUBLIC:         __P((DB *, int (*)(DB *, const DBT *, const DBT *)));
 */
int
__bam_set_bt_compare(dbp, func)
	DB *dbp;
	int (*func) __P((DB *, const DBT *, const DBT *));
{
	BTREE *t;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_bt_compare");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	t = dbp->bt_internal;

	/*
	 * Can't default the prefix routine if the user supplies a comparison
	 * routine; shortening the keys can break their comparison algorithm.
	 */
	t->bt_compare = func;
	if (t->bt_prefix == __bam_defpfx)
		t->bt_prefix = NULL;

	return (0);
}

/*
 * __bam_set_bt_maxkey --
 *	Set the maximum keys per page.
 */
static int
__bam_set_bt_maxkey(dbp, bt_maxkey)
	DB *dbp;
	u_int32_t bt_maxkey;
{
	BTREE *t;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_bt_maxkey");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	t = dbp->bt_internal;

	if (bt_maxkey < 1) {
		__db_err(dbp->dbenv, "minimum bt_maxkey value is 1");
		return (EINVAL);
	}

	t->bt_maxkey = bt_maxkey;
	return (0);
}

/*
 * __db_get_bt_minkey --
 *	Get the minimum keys per page.
 *
 * PUBLIC: int __bam_get_bt_minkey __P((DB *, u_int32_t *));
 */
int
__bam_get_bt_minkey(dbp, bt_minkeyp)
	DB *dbp;
	u_int32_t *bt_minkeyp;
{
	BTREE *t;

	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	t = dbp->bt_internal;
	*bt_minkeyp = t->bt_minkey;
	return (0);
}

/*
 * __bam_set_bt_minkey --
 *	Set the minimum keys per page.
 */
static int
__bam_set_bt_minkey(dbp, bt_minkey)
	DB *dbp;
	u_int32_t bt_minkey;
{
	BTREE *t;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_bt_minkey");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	t = dbp->bt_internal;

	if (bt_minkey < 2) {
		__db_err(dbp->dbenv, "minimum bt_minkey value is 2");
		return (EINVAL);
	}

	t->bt_minkey = bt_minkey;
	return (0);
}

/*
 * __bam_set_bt_prefix --
 *	Set the prefix function.
 */
static int
__bam_set_bt_prefix(dbp, func)
	DB *dbp;
	size_t (*func) __P((DB *, const DBT *, const DBT *));
{
	BTREE *t;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_bt_prefix");
	DB_ILLEGAL_METHOD(dbp, DB_OK_BTREE);

	t = dbp->bt_internal;

	t->bt_prefix = func;
	return (0);
}

/*
 * __ram_map_flags --
 *	Map Recno specific flags from public to the internal values.
 *
 * PUBLIC: void __ram_map_flags __P((DB *, u_int32_t *, u_int32_t *));
 */
void
__ram_map_flags(dbp, inflagsp, outflagsp)
	DB *dbp;
	u_int32_t *inflagsp, *outflagsp;
{
	COMPQUIET(dbp, NULL);

	if (FLD_ISSET(*inflagsp, DB_RENUMBER)) {
		FLD_SET(*outflagsp, DB_AM_RENUMBER);
		FLD_CLR(*inflagsp, DB_RENUMBER);
	}
	if (FLD_ISSET(*inflagsp, DB_SNAPSHOT)) {
		FLD_SET(*outflagsp, DB_AM_SNAPSHOT);
		FLD_CLR(*inflagsp, DB_SNAPSHOT);
	}
}

/*
 * __ram_set_flags --
 *	Set Recno specific flags.
 *
 * PUBLIC: int __ram_set_flags __P((DB *, u_int32_t *flagsp));
 */
int
__ram_set_flags(dbp, flagsp)
	DB *dbp;
	u_int32_t *flagsp;
{
	u_int32_t flags;

	flags = *flagsp;
	if (LF_ISSET(DB_RENUMBER | DB_SNAPSHOT)) {
		DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_flags");
		DB_ILLEGAL_METHOD(dbp, DB_OK_RECNO);
	}

	__ram_map_flags(dbp, flagsp, &dbp->flags);
	return (0);
}

/*
 * __db_get_re_delim --
 *	Get the variable-length input record delimiter.
 */
static int
__ram_get_re_delim(dbp, re_delimp)
	DB *dbp;
	int *re_delimp;
{
	BTREE *t;

	DB_ILLEGAL_METHOD(dbp, DB_OK_RECNO);
	t = dbp->bt_internal;
	*re_delimp = t->re_delim;
	return (0);
}

/*
 * __ram_set_re_delim --
 *	Set the variable-length input record delimiter.
 */
static int
__ram_set_re_delim(dbp, re_delim)
	DB *dbp;
	int re_delim;
{
	BTREE *t;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_re_delim");
	DB_ILLEGAL_METHOD(dbp, DB_OK_RECNO);

	t = dbp->bt_internal;

	t->re_delim = re_delim;
	F_SET(dbp, DB_AM_DELIMITER);

	return (0);
}

/*
 * __db_get_re_len --
 *	Get the variable-length input record length.
 *
 * PUBLIC: int __ram_get_re_len __P((DB *, u_int32_t *));
 */
int
__ram_get_re_len(dbp, re_lenp)
	DB *dbp;
	u_int32_t *re_lenp;
{
	BTREE *t;
	QUEUE *q;

	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE | DB_OK_RECNO);

	/*
	 * This has to work for all access methods, before or after opening the
	 * database.  When the record length is set with __ram_set_re_len, the
	 * value in both the BTREE and QUEUE structs will be correct.
	 * Otherwise, this only makes sense after the database in opened, in
	 * which case we know the type.
	 */
	if (dbp->type == DB_QUEUE) {
		q = dbp->q_internal;
		*re_lenp = q->re_len;
	} else {
		t = dbp->bt_internal;
		*re_lenp = t->re_len;
	}

	return (0);
}

/*
 * __ram_set_re_len --
 *	Set the variable-length input record length.
 */
static int
__ram_set_re_len(dbp, re_len)
	DB *dbp;
	u_int32_t re_len;
{
	BTREE *t;
	QUEUE *q;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_re_len");
	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE | DB_OK_RECNO);

	t = dbp->bt_internal;
	t->re_len = re_len;

	q = dbp->q_internal;
	q->re_len = re_len;

	F_SET(dbp, DB_AM_FIXEDLEN);

	return (0);
}

/*
 * __db_get_re_pad --
 *	Get the fixed-length record pad character.
 *
 * PUBLIC: int __ram_get_re_pad __P((DB *, int *));
 */
int
__ram_get_re_pad(dbp, re_padp)
	DB *dbp;
	int *re_padp;
{
	BTREE *t;
	QUEUE *q;

	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE | DB_OK_RECNO);

	/*
	 * This has to work for all access methods, before or after opening the
	 * database.  When the record length is set with __ram_set_re_pad, the
	 * value in both the BTREE and QUEUE structs will be correct.
	 * Otherwise, this only makes sense after the database in opened, in
	 * which case we know the type.
	 */
	if (dbp->type == DB_QUEUE) {
		q = dbp->q_internal;
		*re_padp = q->re_pad;
	} else {
		t = dbp->bt_internal;
		*re_padp = t->re_pad;
	}

	return (0);
}

/*
 * __ram_set_re_pad --
 *	Set the fixed-length record pad character.
 */
static int
__ram_set_re_pad(dbp, re_pad)
	DB *dbp;
	int re_pad;
{
	BTREE *t;
	QUEUE *q;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_re_pad");
	DB_ILLEGAL_METHOD(dbp, DB_OK_QUEUE | DB_OK_RECNO);

	t = dbp->bt_internal;
	t->re_pad = re_pad;

	q = dbp->q_internal;
	q->re_pad = re_pad;

	F_SET(dbp, DB_AM_PAD);

	return (0);
}

/*
 * __db_get_re_source --
 *	Get the backing source file name.
 */
static int
__ram_get_re_source(dbp, re_sourcep)
	DB *dbp;
	const char **re_sourcep;
{
	BTREE *t;

	DB_ILLEGAL_METHOD(dbp, DB_OK_RECNO);

	t = dbp->bt_internal;
	*re_sourcep = t->re_source;
	return (0);
}

/*
 * __ram_set_re_source --
 *	Set the backing source file name.
 */
static int
__ram_set_re_source(dbp, re_source)
	DB *dbp;
	const char *re_source;
{
	BTREE *t;

	DB_ILLEGAL_AFTER_OPEN(dbp, "DB->set_re_source");
	DB_ILLEGAL_METHOD(dbp, DB_OK_RECNO);

	t = dbp->bt_internal;

	return (__os_strdup(dbp->dbenv, re_source, &t->re_source));
}
