/* Do not edit: automatically built by gen_rec.awk. */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>

#include "ex_apprec.h"
/*
 * PUBLIC: int ex_apprec_mkdir_log __P((DB_ENV *, DB_TXN *, DB_LSN *,
 * PUBLIC:     u_int32_t, const DBT *));
 */
int
ex_apprec_mkdir_log(dbenv, txnid, ret_lsnp, flags,
    dirname)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	const DBT *dirname;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t npad, rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ex_apprec_mkdir;
	npad = 0;

	if (txnid == NULL) {
		txn_num = 0;
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else {
		txn_num = txnid->txnid;
		lsnp = &txnid->last_lsn;
	}

	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(u_int32_t) + (dirname == NULL ? 0 : dirname->size);
	if ((logrec.data = malloc(logrec.size)) == NULL)
		return (ENOMEM);

	if (npad > 0)
		memset((u_int8_t *)logrec.data + logrec.size - npad, 0, npad);

	bp = logrec.data;

	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);

	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);

	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	if (dirname == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &dirname->size, sizeof(dirname->size));
		bp += sizeof(dirname->size);
		memcpy(bp, dirname->data, dirname->size);
		bp += dirname->size;
	}

	ret = dbenv->log_put(dbenv,
	   ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL && ret == 0)
		txnid->last_lsn = *ret_lsnp;
#ifdef LOG_DIAGNOSTIC
	if (ret != 0)
		(void)ex_apprec_mkdir_print(dbenv,
		    (DBT *)&logrec, ret_lsnp, NULL, NULL);
#endif
	free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int ex_apprec_mkdir_print __P((DB_ENV *, DBT *, DB_LSN *,
 * PUBLIC:     db_recops, void *));
 */
int
ex_apprec_mkdir_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	ex_apprec_mkdir_args *argp;
	u_int32_t i;
	int ch;
	int ret;

	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = ex_apprec_mkdir_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	(void)printf(
	    "[%lu][%lu]ex_apprec_mkdir: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	(void)printf("\tdirname: ");
	for (i = 0; i < argp->dirname.size; i++) {
		ch = ((u_int8_t *)argp->dirname.data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	(void)printf("\n");
	(void)printf("\n");
	free(argp);
	return (0);
}

/*
 * PUBLIC: int ex_apprec_mkdir_read __P((DB_ENV *, void *,
 * PUBLIC:     ex_apprec_mkdir_args **));
 */
int
ex_apprec_mkdir_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	ex_apprec_mkdir_args **argpp;
{
	ex_apprec_mkdir_args *argp;
	u_int8_t *bp;
	/* Keep the compiler quiet. */

	dbenv = NULL;
	if ((argp = malloc(sizeof(ex_apprec_mkdir_args) + sizeof(DB_TXN))) == NULL)
		return (ENOMEM);

	argp->txnid = (DB_TXN *)&argp[1];

	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);

	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);

	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);

	memset(&argp->dirname, 0, sizeof(argp->dirname));
	memcpy(&argp->dirname.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->dirname.data = bp;
	bp += argp->dirname.size;

	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int ex_apprec_init_print __P((DB_ENV *, int (***)(DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *), size_t *));
 */
int
ex_apprec_init_print(dbenv, dtabp, dtabsizep)
	DB_ENV *dbenv;
	int (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t *dtabsizep;
{
	int __db_add_recovery __P((DB_ENV *,
	    int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *),
	    size_t *,
	    int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), u_int32_t));
	int ret;

	if ((ret = __db_add_recovery(dbenv, dtabp, dtabsizep,
	    ex_apprec_mkdir_print, DB_ex_apprec_mkdir)) != 0)
		return (ret);
	return (0);
}

