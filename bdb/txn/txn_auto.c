/* Do not edit: automatically built by gen_rec.awk. */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_dispatch.h"
#include "db_am.h"
#include "txn.h"

int
__txn_old_regop_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_old_regop_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_old_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_old_regop: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_old_regop_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_old_regop_args **argpp;
{
	__txn_old_regop_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_old_regop_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	*argpp = argp;
	return (0);
}

int
__txn_regop_log(dbenv, txnid, ret_lsnp, flags,
	opcode, timestamp)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	int32_t timestamp;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_txn_regop;
	if (txnid != NULL &&
	    TAILQ_FIRST(&txnid->kids) != NULL &&
	    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
		return (ret);
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		ZERO_LSN(null_lsn);
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(timestamp);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &opcode, sizeof(opcode));
	bp += sizeof(opcode);
	memcpy(bp, &timestamp, sizeof(timestamp));
	bp += sizeof(timestamp);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__txn_regop_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_regop_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_regop: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\ttimestamp: %ld\n", (long)argp->timestamp);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_regop_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_regop_args **argpp;
{
	__txn_regop_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_regop_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memcpy(&argp->timestamp, bp, sizeof(argp->timestamp));
	bp += sizeof(argp->timestamp);
	*argpp = argp;
	return (0);
}

int
__txn_old_ckp_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_old_ckp_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_old_ckp_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_old_ckp: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tckp_lsn: [%lu][%lu]\n",
	    (u_long)argp->ckp_lsn.file, (u_long)argp->ckp_lsn.offset);
	printf("\tlast_ckp: [%lu][%lu]\n",
	    (u_long)argp->last_ckp.file, (u_long)argp->last_ckp.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_old_ckp_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_old_ckp_args **argpp;
{
	__txn_old_ckp_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_old_ckp_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->ckp_lsn, bp,  sizeof(argp->ckp_lsn));
	bp += sizeof(argp->ckp_lsn);
	memcpy(&argp->last_ckp, bp,  sizeof(argp->last_ckp));
	bp += sizeof(argp->last_ckp);
	*argpp = argp;
	return (0);
}

int
__txn_ckp_log(dbenv, txnid, ret_lsnp, flags,
	ckp_lsn, last_ckp, timestamp)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	DB_LSN * ckp_lsn;
	DB_LSN * last_ckp;
	int32_t timestamp;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_txn_ckp;
	if (txnid != NULL &&
	    TAILQ_FIRST(&txnid->kids) != NULL &&
	    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
		return (ret);
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		ZERO_LSN(null_lsn);
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(*ckp_lsn)
	    + sizeof(*last_ckp)
	    + sizeof(timestamp);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	if (ckp_lsn != NULL)
		memcpy(bp, ckp_lsn, sizeof(*ckp_lsn));
	else
		memset(bp, 0, sizeof(*ckp_lsn));
	bp += sizeof(*ckp_lsn);
	if (last_ckp != NULL)
		memcpy(bp, last_ckp, sizeof(*last_ckp));
	else
		memset(bp, 0, sizeof(*last_ckp));
	bp += sizeof(*last_ckp);
	memcpy(bp, &timestamp, sizeof(timestamp));
	bp += sizeof(timestamp);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__txn_ckp_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_ckp_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_ckp_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_ckp: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tckp_lsn: [%lu][%lu]\n",
	    (u_long)argp->ckp_lsn.file, (u_long)argp->ckp_lsn.offset);
	printf("\tlast_ckp: [%lu][%lu]\n",
	    (u_long)argp->last_ckp.file, (u_long)argp->last_ckp.offset);
	printf("\ttimestamp: %ld\n", (long)argp->timestamp);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_ckp_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_ckp_args **argpp;
{
	__txn_ckp_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_ckp_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->ckp_lsn, bp,  sizeof(argp->ckp_lsn));
	bp += sizeof(argp->ckp_lsn);
	memcpy(&argp->last_ckp, bp,  sizeof(argp->last_ckp));
	bp += sizeof(argp->last_ckp);
	memcpy(&argp->timestamp, bp, sizeof(argp->timestamp));
	bp += sizeof(argp->timestamp);
	*argpp = argp;
	return (0);
}

int
__txn_xa_regop_old_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_xa_regop_old_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_xa_regop_old_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_xa_regop_old: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\txid: ");
	for (i = 0; i < argp->xid.size; i++) {
		ch = ((u_int8_t *)argp->xid.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tformatID: %ld\n", (long)argp->formatID);
	printf("\tgtrid: %u\n", argp->gtrid);
	printf("\tbqual: %u\n", argp->bqual);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_xa_regop_old_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_xa_regop_old_args **argpp;
{
	__txn_xa_regop_old_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_xa_regop_old_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memset(&argp->xid, 0, sizeof(argp->xid));
	memcpy(&argp->xid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->xid.data = bp;
	bp += argp->xid.size;
	memcpy(&argp->formatID, bp, sizeof(argp->formatID));
	bp += sizeof(argp->formatID);
	memcpy(&argp->gtrid, bp, sizeof(argp->gtrid));
	bp += sizeof(argp->gtrid);
	memcpy(&argp->bqual, bp, sizeof(argp->bqual));
	bp += sizeof(argp->bqual);
	*argpp = argp;
	return (0);
}

int
__txn_xa_regop_log(dbenv, txnid, ret_lsnp, flags,
	opcode, xid, formatID, gtrid, bqual, begin_lsn)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	const DBT *xid;
	int32_t formatID;
	u_int32_t gtrid;
	u_int32_t bqual;
	DB_LSN * begin_lsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_txn_xa_regop;
	if (txnid != NULL &&
	    TAILQ_FIRST(&txnid->kids) != NULL &&
	    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
		return (ret);
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		ZERO_LSN(null_lsn);
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(u_int32_t) + (xid == NULL ? 0 : xid->size)
	    + sizeof(formatID)
	    + sizeof(gtrid)
	    + sizeof(bqual)
	    + sizeof(*begin_lsn);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &opcode, sizeof(opcode));
	bp += sizeof(opcode);
	if (xid == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &xid->size, sizeof(xid->size));
		bp += sizeof(xid->size);
		memcpy(bp, xid->data, xid->size);
		bp += xid->size;
	}
	memcpy(bp, &formatID, sizeof(formatID));
	bp += sizeof(formatID);
	memcpy(bp, &gtrid, sizeof(gtrid));
	bp += sizeof(gtrid);
	memcpy(bp, &bqual, sizeof(bqual));
	bp += sizeof(bqual);
	if (begin_lsn != NULL)
		memcpy(bp, begin_lsn, sizeof(*begin_lsn));
	else
		memset(bp, 0, sizeof(*begin_lsn));
	bp += sizeof(*begin_lsn);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__txn_xa_regop_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_xa_regop_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_xa_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_xa_regop: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\txid: ");
	for (i = 0; i < argp->xid.size; i++) {
		ch = ((u_int8_t *)argp->xid.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tformatID: %ld\n", (long)argp->formatID);
	printf("\tgtrid: %u\n", argp->gtrid);
	printf("\tbqual: %u\n", argp->bqual);
	printf("\tbegin_lsn: [%lu][%lu]\n",
	    (u_long)argp->begin_lsn.file, (u_long)argp->begin_lsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_xa_regop_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_xa_regop_args **argpp;
{
	__txn_xa_regop_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_xa_regop_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memset(&argp->xid, 0, sizeof(argp->xid));
	memcpy(&argp->xid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->xid.data = bp;
	bp += argp->xid.size;
	memcpy(&argp->formatID, bp, sizeof(argp->formatID));
	bp += sizeof(argp->formatID);
	memcpy(&argp->gtrid, bp, sizeof(argp->gtrid));
	bp += sizeof(argp->gtrid);
	memcpy(&argp->bqual, bp, sizeof(argp->bqual));
	bp += sizeof(argp->bqual);
	memcpy(&argp->begin_lsn, bp,  sizeof(argp->begin_lsn));
	bp += sizeof(argp->begin_lsn);
	*argpp = argp;
	return (0);
}

int
__txn_child_old_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_child_old_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_child_old_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_child_old: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tparent: 0x%lx\n", (u_long)argp->parent);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_child_old_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_child_old_args **argpp;
{
	__txn_child_old_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_child_old_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memcpy(&argp->parent, bp, sizeof(argp->parent));
	bp += sizeof(argp->parent);
	*argpp = argp;
	return (0);
}

int
__txn_child_log(dbenv, txnid, ret_lsnp, flags,
	child, c_lsn)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t child;
	DB_LSN * c_lsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_txn_child;
	if (txnid != NULL &&
	    TAILQ_FIRST(&txnid->kids) != NULL &&
	    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)
		return (ret);
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		ZERO_LSN(null_lsn);
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(child)
	    + sizeof(*c_lsn);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &child, sizeof(child));
	bp += sizeof(child);
	if (c_lsn != NULL)
		memcpy(bp, c_lsn, sizeof(*c_lsn));
	else
		memset(bp, 0, sizeof(*c_lsn));
	bp += sizeof(*c_lsn);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__txn_child_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__txn_child_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __txn_child_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]txn_child: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tchild: 0x%lx\n", (u_long)argp->child);
	printf("\tc_lsn: [%lu][%lu]\n",
	    (u_long)argp->c_lsn.file, (u_long)argp->c_lsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__txn_child_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__txn_child_args **argpp;
{
	__txn_child_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__txn_child_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->child, bp, sizeof(argp->child));
	bp += sizeof(argp->child);
	memcpy(&argp->c_lsn, bp,  sizeof(argp->c_lsn));
	bp += sizeof(argp->c_lsn);
	*argpp = argp;
	return (0);
}

int
__txn_init_print(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __txn_old_regop_print, DB_txn_old_regop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_regop_print, DB_txn_regop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_old_ckp_print, DB_txn_old_ckp)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_ckp_print, DB_txn_ckp)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_xa_regop_old_print, DB_txn_xa_regop_old)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_xa_regop_print, DB_txn_xa_regop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_child_old_print, DB_txn_child_old)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_child_print, DB_txn_child)) != 0)
		return (ret);
	return (0);
}

int
__txn_init_recover(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_txn_old_regop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_regop_recover, DB_txn_regop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_txn_old_ckp)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_ckp_recover, DB_txn_ckp)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_txn_xa_regop_old)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_xa_regop_recover, DB_txn_xa_regop)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_txn_child_old)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __txn_child_recover, DB_txn_child)) != 0)
		return (ret);
	return (0);
}

