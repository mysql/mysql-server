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
#include "hash.h"
#include "txn.h"

int
__ham_insdel_log(dbenv, txnid, ret_lsnp, flags,
	opcode, fileid, pgno, ndx, pagelsn, key,
	data)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	int32_t fileid;
	db_pgno_t pgno;
	u_int32_t ndx;
	DB_LSN * pagelsn;
	const DBT *key;
	const DBT *data;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_insdel;
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
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(ndx)
	    + sizeof(*pagelsn)
	    + sizeof(u_int32_t) + (key == NULL ? 0 : key->size)
	    + sizeof(u_int32_t) + (data == NULL ? 0 : data->size);
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
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	memcpy(bp, &ndx, sizeof(ndx));
	bp += sizeof(ndx);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	if (key == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &key->size, sizeof(key->size));
		bp += sizeof(key->size);
		memcpy(bp, key->data, key->size);
		bp += key->size;
	}
	if (data == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &data->size, sizeof(data->size));
		bp += sizeof(data->size);
		memcpy(bp, data->data, data->size);
		bp += data->size;
	}
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_insdel_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_insdel_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_insdel_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_insdel: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tndx: %lu\n", (u_long)argp->ndx);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tkey: ");
	for (i = 0; i < argp->key.size; i++) {
		ch = ((u_int8_t *)argp->key.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tdata: ");
	for (i = 0; i < argp->data.size; i++) {
		ch = ((u_int8_t *)argp->data.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_insdel_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_insdel_args **argpp;
{
	__ham_insdel_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_insdel_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->ndx, bp, sizeof(argp->ndx));
	bp += sizeof(argp->ndx);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memset(&argp->key, 0, sizeof(argp->key));
	memcpy(&argp->key.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->key.data = bp;
	bp += argp->key.size;
	memset(&argp->data, 0, sizeof(argp->data));
	memcpy(&argp->data.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->data.data = bp;
	bp += argp->data.size;
	*argpp = argp;
	return (0);
}

int
__ham_newpage_log(dbenv, txnid, ret_lsnp, flags,
	opcode, fileid, prev_pgno, prevlsn, new_pgno, pagelsn,
	next_pgno, nextlsn)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	int32_t fileid;
	db_pgno_t prev_pgno;
	DB_LSN * prevlsn;
	db_pgno_t new_pgno;
	DB_LSN * pagelsn;
	db_pgno_t next_pgno;
	DB_LSN * nextlsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_newpage;
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
	    + sizeof(fileid)
	    + sizeof(prev_pgno)
	    + sizeof(*prevlsn)
	    + sizeof(new_pgno)
	    + sizeof(*pagelsn)
	    + sizeof(next_pgno)
	    + sizeof(*nextlsn);
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
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &prev_pgno, sizeof(prev_pgno));
	bp += sizeof(prev_pgno);
	if (prevlsn != NULL)
		memcpy(bp, prevlsn, sizeof(*prevlsn));
	else
		memset(bp, 0, sizeof(*prevlsn));
	bp += sizeof(*prevlsn);
	memcpy(bp, &new_pgno, sizeof(new_pgno));
	bp += sizeof(new_pgno);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	memcpy(bp, &next_pgno, sizeof(next_pgno));
	bp += sizeof(next_pgno);
	if (nextlsn != NULL)
		memcpy(bp, nextlsn, sizeof(*nextlsn));
	else
		memset(bp, 0, sizeof(*nextlsn));
	bp += sizeof(*nextlsn);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_newpage_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_newpage_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_newpage_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_newpage: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tprev_pgno: %lu\n", (u_long)argp->prev_pgno);
	printf("\tprevlsn: [%lu][%lu]\n",
	    (u_long)argp->prevlsn.file, (u_long)argp->prevlsn.offset);
	printf("\tnew_pgno: %lu\n", (u_long)argp->new_pgno);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tnext_pgno: %lu\n", (u_long)argp->next_pgno);
	printf("\tnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nextlsn.file, (u_long)argp->nextlsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_newpage_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_newpage_args **argpp;
{
	__ham_newpage_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_newpage_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->prev_pgno, bp, sizeof(argp->prev_pgno));
	bp += sizeof(argp->prev_pgno);
	memcpy(&argp->prevlsn, bp,  sizeof(argp->prevlsn));
	bp += sizeof(argp->prevlsn);
	memcpy(&argp->new_pgno, bp, sizeof(argp->new_pgno));
	bp += sizeof(argp->new_pgno);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->next_pgno, bp, sizeof(argp->next_pgno));
	bp += sizeof(argp->next_pgno);
	memcpy(&argp->nextlsn, bp,  sizeof(argp->nextlsn));
	bp += sizeof(argp->nextlsn);
	*argpp = argp;
	return (0);
}

int
__ham_splitmeta_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_splitmeta_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_splitmeta_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_splitmeta: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tbucket: %lu\n", (u_long)argp->bucket);
	printf("\tovflpoint: %lu\n", (u_long)argp->ovflpoint);
	printf("\tspares: %lu\n", (u_long)argp->spares);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_splitmeta_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_splitmeta_args **argpp;
{
	__ham_splitmeta_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_splitmeta_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->bucket, bp, sizeof(argp->bucket));
	bp += sizeof(argp->bucket);
	memcpy(&argp->ovflpoint, bp, sizeof(argp->ovflpoint));
	bp += sizeof(argp->ovflpoint);
	memcpy(&argp->spares, bp, sizeof(argp->spares));
	bp += sizeof(argp->spares);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	*argpp = argp;
	return (0);
}

int
__ham_splitdata_log(dbenv, txnid, ret_lsnp, flags,
	fileid, opcode, pgno, pageimage, pagelsn)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	u_int32_t opcode;
	db_pgno_t pgno;
	const DBT *pageimage;
	DB_LSN * pagelsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_splitdata;
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
	    + sizeof(fileid)
	    + sizeof(opcode)
	    + sizeof(pgno)
	    + sizeof(u_int32_t) + (pageimage == NULL ? 0 : pageimage->size)
	    + sizeof(*pagelsn);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &opcode, sizeof(opcode));
	bp += sizeof(opcode);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	if (pageimage == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &pageimage->size, sizeof(pageimage->size));
		bp += sizeof(pageimage->size);
		memcpy(bp, pageimage->data, pageimage->size);
		bp += pageimage->size;
	}
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_splitdata_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_splitdata_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_splitdata_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_splitdata: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tpageimage: ");
	for (i = 0; i < argp->pageimage.size; i++) {
		ch = ((u_int8_t *)argp->pageimage.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_splitdata_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_splitdata_args **argpp;
{
	__ham_splitdata_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_splitdata_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memset(&argp->pageimage, 0, sizeof(argp->pageimage));
	memcpy(&argp->pageimage.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->pageimage.data = bp;
	bp += argp->pageimage.size;
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	*argpp = argp;
	return (0);
}

int
__ham_replace_log(dbenv, txnid, ret_lsnp, flags,
	fileid, pgno, ndx, pagelsn, off, olditem,
	newitem, makedup)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	db_pgno_t pgno;
	u_int32_t ndx;
	DB_LSN * pagelsn;
	int32_t off;
	const DBT *olditem;
	const DBT *newitem;
	u_int32_t makedup;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_replace;
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
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(ndx)
	    + sizeof(*pagelsn)
	    + sizeof(off)
	    + sizeof(u_int32_t) + (olditem == NULL ? 0 : olditem->size)
	    + sizeof(u_int32_t) + (newitem == NULL ? 0 : newitem->size)
	    + sizeof(makedup);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	memcpy(bp, &ndx, sizeof(ndx));
	bp += sizeof(ndx);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	memcpy(bp, &off, sizeof(off));
	bp += sizeof(off);
	if (olditem == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &olditem->size, sizeof(olditem->size));
		bp += sizeof(olditem->size);
		memcpy(bp, olditem->data, olditem->size);
		bp += olditem->size;
	}
	if (newitem == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &newitem->size, sizeof(newitem->size));
		bp += sizeof(newitem->size);
		memcpy(bp, newitem->data, newitem->size);
		bp += newitem->size;
	}
	memcpy(bp, &makedup, sizeof(makedup));
	bp += sizeof(makedup);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_replace_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_replace_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_replace_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_replace: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tndx: %lu\n", (u_long)argp->ndx);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\toff: %ld\n", (long)argp->off);
	printf("\tolditem: ");
	for (i = 0; i < argp->olditem.size; i++) {
		ch = ((u_int8_t *)argp->olditem.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tnewitem: ");
	for (i = 0; i < argp->newitem.size; i++) {
		ch = ((u_int8_t *)argp->newitem.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tmakedup: %lu\n", (u_long)argp->makedup);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_replace_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_replace_args **argpp;
{
	__ham_replace_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_replace_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->ndx, bp, sizeof(argp->ndx));
	bp += sizeof(argp->ndx);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->off, bp, sizeof(argp->off));
	bp += sizeof(argp->off);
	memset(&argp->olditem, 0, sizeof(argp->olditem));
	memcpy(&argp->olditem.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->olditem.data = bp;
	bp += argp->olditem.size;
	memset(&argp->newitem, 0, sizeof(argp->newitem));
	memcpy(&argp->newitem.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->newitem.data = bp;
	bp += argp->newitem.size;
	memcpy(&argp->makedup, bp, sizeof(argp->makedup));
	bp += sizeof(argp->makedup);
	*argpp = argp;
	return (0);
}

int
__ham_newpgno_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_newpgno_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_newpgno_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_newpgno: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tfree_pgno: %lu\n", (u_long)argp->free_pgno);
	printf("\told_type: %lu\n", (u_long)argp->old_type);
	printf("\told_pgno: %lu\n", (u_long)argp->old_pgno);
	printf("\tnew_type: %lu\n", (u_long)argp->new_type);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_newpgno_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_newpgno_args **argpp;
{
	__ham_newpgno_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_newpgno_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->free_pgno, bp, sizeof(argp->free_pgno));
	bp += sizeof(argp->free_pgno);
	memcpy(&argp->old_type, bp, sizeof(argp->old_type));
	bp += sizeof(argp->old_type);
	memcpy(&argp->old_pgno, bp, sizeof(argp->old_pgno));
	bp += sizeof(argp->old_pgno);
	memcpy(&argp->new_type, bp, sizeof(argp->new_type));
	bp += sizeof(argp->new_type);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	*argpp = argp;
	return (0);
}

int
__ham_ovfl_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_ovfl_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_ovfl_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_ovfl: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tstart_pgno: %lu\n", (u_long)argp->start_pgno);
	printf("\tnpages: %lu\n", (u_long)argp->npages);
	printf("\tfree_pgno: %lu\n", (u_long)argp->free_pgno);
	printf("\tovflpoint: %lu\n", (u_long)argp->ovflpoint);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_ovfl_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_ovfl_args **argpp;
{
	__ham_ovfl_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_ovfl_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->start_pgno, bp, sizeof(argp->start_pgno));
	bp += sizeof(argp->start_pgno);
	memcpy(&argp->npages, bp, sizeof(argp->npages));
	bp += sizeof(argp->npages);
	memcpy(&argp->free_pgno, bp, sizeof(argp->free_pgno));
	bp += sizeof(argp->free_pgno);
	memcpy(&argp->ovflpoint, bp, sizeof(argp->ovflpoint));
	bp += sizeof(argp->ovflpoint);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	*argpp = argp;
	return (0);
}

int
__ham_copypage_log(dbenv, txnid, ret_lsnp, flags,
	fileid, pgno, pagelsn, next_pgno, nextlsn, nnext_pgno,
	nnextlsn, page)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	db_pgno_t pgno;
	DB_LSN * pagelsn;
	db_pgno_t next_pgno;
	DB_LSN * nextlsn;
	db_pgno_t nnext_pgno;
	DB_LSN * nnextlsn;
	const DBT *page;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_copypage;
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
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(*pagelsn)
	    + sizeof(next_pgno)
	    + sizeof(*nextlsn)
	    + sizeof(nnext_pgno)
	    + sizeof(*nnextlsn)
	    + sizeof(u_int32_t) + (page == NULL ? 0 : page->size);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	memcpy(bp, &next_pgno, sizeof(next_pgno));
	bp += sizeof(next_pgno);
	if (nextlsn != NULL)
		memcpy(bp, nextlsn, sizeof(*nextlsn));
	else
		memset(bp, 0, sizeof(*nextlsn));
	bp += sizeof(*nextlsn);
	memcpy(bp, &nnext_pgno, sizeof(nnext_pgno));
	bp += sizeof(nnext_pgno);
	if (nnextlsn != NULL)
		memcpy(bp, nnextlsn, sizeof(*nnextlsn));
	else
		memset(bp, 0, sizeof(*nnextlsn));
	bp += sizeof(*nnextlsn);
	if (page == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &page->size, sizeof(page->size));
		bp += sizeof(page->size);
		memcpy(bp, page->data, page->size);
		bp += page->size;
	}
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_copypage_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_copypage_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_copypage_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_copypage: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tnext_pgno: %lu\n", (u_long)argp->next_pgno);
	printf("\tnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nextlsn.file, (u_long)argp->nextlsn.offset);
	printf("\tnnext_pgno: %lu\n", (u_long)argp->nnext_pgno);
	printf("\tnnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nnextlsn.file, (u_long)argp->nnextlsn.offset);
	printf("\tpage: ");
	for (i = 0; i < argp->page.size; i++) {
		ch = ((u_int8_t *)argp->page.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_copypage_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_copypage_args **argpp;
{
	__ham_copypage_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_copypage_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->next_pgno, bp, sizeof(argp->next_pgno));
	bp += sizeof(argp->next_pgno);
	memcpy(&argp->nextlsn, bp,  sizeof(argp->nextlsn));
	bp += sizeof(argp->nextlsn);
	memcpy(&argp->nnext_pgno, bp, sizeof(argp->nnext_pgno));
	bp += sizeof(argp->nnext_pgno);
	memcpy(&argp->nnextlsn, bp,  sizeof(argp->nnextlsn));
	bp += sizeof(argp->nnextlsn);
	memset(&argp->page, 0, sizeof(argp->page));
	memcpy(&argp->page.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->page.data = bp;
	bp += argp->page.size;
	*argpp = argp;
	return (0);
}

int
__ham_metagroup_log(dbenv, txnid, ret_lsnp, flags,
	fileid, bucket, pgno, metalsn, pagelsn)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	u_int32_t bucket;
	db_pgno_t pgno;
	DB_LSN * metalsn;
	DB_LSN * pagelsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_metagroup;
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
	    + sizeof(fileid)
	    + sizeof(bucket)
	    + sizeof(pgno)
	    + sizeof(*metalsn)
	    + sizeof(*pagelsn);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &bucket, sizeof(bucket));
	bp += sizeof(bucket);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	if (metalsn != NULL)
		memcpy(bp, metalsn, sizeof(*metalsn));
	else
		memset(bp, 0, sizeof(*metalsn));
	bp += sizeof(*metalsn);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_metagroup_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_metagroup_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_metagroup_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_metagroup: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tbucket: %lu\n", (u_long)argp->bucket);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_metagroup_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_metagroup_args **argpp;
{
	__ham_metagroup_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_metagroup_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->bucket, bp, sizeof(argp->bucket));
	bp += sizeof(argp->bucket);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	*argpp = argp;
	return (0);
}

int
__ham_groupalloc1_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_groupalloc1_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_groupalloc1_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_groupalloc1: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\tmmetalsn: [%lu][%lu]\n",
	    (u_long)argp->mmetalsn.file, (u_long)argp->mmetalsn.offset);
	printf("\tstart_pgno: %lu\n", (u_long)argp->start_pgno);
	printf("\tnum: %lu\n", (u_long)argp->num);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_groupalloc1_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_groupalloc1_args **argpp;
{
	__ham_groupalloc1_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_groupalloc1_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	memcpy(&argp->mmetalsn, bp,  sizeof(argp->mmetalsn));
	bp += sizeof(argp->mmetalsn);
	memcpy(&argp->start_pgno, bp, sizeof(argp->start_pgno));
	bp += sizeof(argp->start_pgno);
	memcpy(&argp->num, bp, sizeof(argp->num));
	bp += sizeof(argp->num);
	*argpp = argp;
	return (0);
}

int
__ham_groupalloc2_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_groupalloc2_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_groupalloc2_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_groupalloc2: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	printf("\talloc_lsn: [%lu][%lu]\n",
	    (u_long)argp->alloc_lsn.file, (u_long)argp->alloc_lsn.offset);
	printf("\tstart_pgno: %lu\n", (u_long)argp->start_pgno);
	printf("\tnum: %lu\n", (u_long)argp->num);
	printf("\tfree: %lu\n", (u_long)argp->free);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_groupalloc2_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_groupalloc2_args **argpp;
{
	__ham_groupalloc2_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_groupalloc2_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);
	memcpy(&argp->alloc_lsn, bp,  sizeof(argp->alloc_lsn));
	bp += sizeof(argp->alloc_lsn);
	memcpy(&argp->start_pgno, bp, sizeof(argp->start_pgno));
	bp += sizeof(argp->start_pgno);
	memcpy(&argp->num, bp, sizeof(argp->num));
	bp += sizeof(argp->num);
	memcpy(&argp->free, bp, sizeof(argp->free));
	bp += sizeof(argp->free);
	*argpp = argp;
	return (0);
}

int
__ham_groupalloc_log(dbenv, txnid, ret_lsnp, flags,
	fileid, meta_lsn, start_pgno, num, free)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	DB_LSN * meta_lsn;
	db_pgno_t start_pgno;
	u_int32_t num;
	db_pgno_t free;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_groupalloc;
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
	    + sizeof(fileid)
	    + sizeof(*meta_lsn)
	    + sizeof(start_pgno)
	    + sizeof(num)
	    + sizeof(free);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	if (meta_lsn != NULL)
		memcpy(bp, meta_lsn, sizeof(*meta_lsn));
	else
		memset(bp, 0, sizeof(*meta_lsn));
	bp += sizeof(*meta_lsn);
	memcpy(bp, &start_pgno, sizeof(start_pgno));
	bp += sizeof(start_pgno);
	memcpy(bp, &num, sizeof(num));
	bp += sizeof(num);
	memcpy(bp, &free, sizeof(free));
	bp += sizeof(free);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_groupalloc_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_groupalloc_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_groupalloc_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_groupalloc: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tmeta_lsn: [%lu][%lu]\n",
	    (u_long)argp->meta_lsn.file, (u_long)argp->meta_lsn.offset);
	printf("\tstart_pgno: %lu\n", (u_long)argp->start_pgno);
	printf("\tnum: %lu\n", (u_long)argp->num);
	printf("\tfree: %lu\n", (u_long)argp->free);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_groupalloc_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_groupalloc_args **argpp;
{
	__ham_groupalloc_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_groupalloc_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->meta_lsn, bp,  sizeof(argp->meta_lsn));
	bp += sizeof(argp->meta_lsn);
	memcpy(&argp->start_pgno, bp, sizeof(argp->start_pgno));
	bp += sizeof(argp->start_pgno);
	memcpy(&argp->num, bp, sizeof(argp->num));
	bp += sizeof(argp->num);
	memcpy(&argp->free, bp, sizeof(argp->free));
	bp += sizeof(argp->free);
	*argpp = argp;
	return (0);
}

int
__ham_curadj_log(dbenv, txnid, ret_lsnp, flags,
	fileid, pgno, indx, len, dup_off, add,
	is_dup, order)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	db_pgno_t pgno;
	u_int32_t indx;
	u_int32_t len;
	u_int32_t dup_off;
	int add;
	int is_dup;
	u_int32_t order;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_curadj;
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
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(indx)
	    + sizeof(len)
	    + sizeof(dup_off)
	    + sizeof(add)
	    + sizeof(is_dup)
	    + sizeof(order);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	memcpy(bp, &indx, sizeof(indx));
	bp += sizeof(indx);
	memcpy(bp, &len, sizeof(len));
	bp += sizeof(len);
	memcpy(bp, &dup_off, sizeof(dup_off));
	bp += sizeof(dup_off);
	memcpy(bp, &add, sizeof(add));
	bp += sizeof(add);
	memcpy(bp, &is_dup, sizeof(is_dup));
	bp += sizeof(is_dup);
	memcpy(bp, &order, sizeof(order));
	bp += sizeof(order);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_curadj_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_curadj_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_curadj_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_curadj: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tindx: %lu\n", (u_long)argp->indx);
	printf("\tlen: %lu\n", (u_long)argp->len);
	printf("\tdup_off: %lu\n", (u_long)argp->dup_off);
	printf("\tadd: %ld\n", (long)argp->add);
	printf("\tis_dup: %ld\n", (long)argp->is_dup);
	printf("\torder: %lu\n", (u_long)argp->order);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_curadj_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_curadj_args **argpp;
{
	__ham_curadj_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_curadj_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->indx, bp, sizeof(argp->indx));
	bp += sizeof(argp->indx);
	memcpy(&argp->len, bp, sizeof(argp->len));
	bp += sizeof(argp->len);
	memcpy(&argp->dup_off, bp, sizeof(argp->dup_off));
	bp += sizeof(argp->dup_off);
	memcpy(&argp->add, bp, sizeof(argp->add));
	bp += sizeof(argp->add);
	memcpy(&argp->is_dup, bp, sizeof(argp->is_dup));
	bp += sizeof(argp->is_dup);
	memcpy(&argp->order, bp, sizeof(argp->order));
	bp += sizeof(argp->order);
	*argpp = argp;
	return (0);
}

int
__ham_chgpg_log(dbenv, txnid, ret_lsnp, flags,
	fileid, mode, old_pgno, new_pgno, old_indx, new_indx)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	int32_t fileid;
	db_ham_mode mode;
	db_pgno_t old_pgno;
	db_pgno_t new_pgno;
	u_int32_t old_indx;
	u_int32_t new_indx;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_chgpg;
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
	    + sizeof(fileid)
	    + sizeof(mode)
	    + sizeof(old_pgno)
	    + sizeof(new_pgno)
	    + sizeof(old_indx)
	    + sizeof(new_indx);
	if ((ret = __os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &mode, sizeof(mode));
	bp += sizeof(mode);
	memcpy(bp, &old_pgno, sizeof(old_pgno));
	bp += sizeof(old_pgno);
	memcpy(bp, &new_pgno, sizeof(new_pgno));
	bp += sizeof(new_pgno);
	memcpy(bp, &old_indx, sizeof(old_indx));
	bp += sizeof(old_indx);
	memcpy(bp, &new_indx, sizeof(new_indx));
	bp += sizeof(new_indx);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__os_free(logrec.data, logrec.size);
	return (ret);
}

int
__ham_chgpg_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__ham_chgpg_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = DB_TXN_ABORT;
	notused3 = NULL;

	if ((ret = __ham_chgpg_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_chgpg: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tmode: %ld\n", (long)argp->mode);
	printf("\told_pgno: %lu\n", (u_long)argp->old_pgno);
	printf("\tnew_pgno: %lu\n", (u_long)argp->new_pgno);
	printf("\told_indx: %lu\n", (u_long)argp->old_indx);
	printf("\tnew_indx: %lu\n", (u_long)argp->new_indx);
	printf("\n");
	__os_free(argp, 0);
	return (0);
}

int
__ham_chgpg_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__ham_chgpg_args **argpp;
{
	__ham_chgpg_args *argp;
	u_int8_t *bp;
	int ret;

	ret = __os_malloc(dbenv, sizeof(__ham_chgpg_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->mode, bp, sizeof(argp->mode));
	bp += sizeof(argp->mode);
	memcpy(&argp->old_pgno, bp, sizeof(argp->old_pgno));
	bp += sizeof(argp->old_pgno);
	memcpy(&argp->new_pgno, bp, sizeof(argp->new_pgno));
	bp += sizeof(argp->new_pgno);
	memcpy(&argp->old_indx, bp, sizeof(argp->old_indx));
	bp += sizeof(argp->old_indx);
	memcpy(&argp->new_indx, bp, sizeof(argp->new_indx));
	bp += sizeof(argp->new_indx);
	*argpp = argp;
	return (0);
}

int
__ham_init_print(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __ham_insdel_print, DB_ham_insdel)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpage_print, DB_ham_newpage)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitmeta_print, DB_ham_splitmeta)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitdata_print, DB_ham_splitdata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_replace_print, DB_ham_replace)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpgno_print, DB_ham_newpgno)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_ovfl_print, DB_ham_ovfl)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_copypage_print, DB_ham_copypage)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_metagroup_print, DB_ham_metagroup)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_groupalloc1_print, DB_ham_groupalloc1)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_groupalloc2_print, DB_ham_groupalloc2)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_groupalloc_print, DB_ham_groupalloc)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_curadj_print, DB_ham_curadj)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_chgpg_print, DB_ham_chgpg)) != 0)
		return (ret);
	return (0);
}

int
__ham_init_recover(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __ham_insdel_recover, DB_ham_insdel)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpage_recover, DB_ham_newpage)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_ham_splitmeta)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitdata_recover, DB_ham_splitdata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_replace_recover, DB_ham_replace)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_ham_newpgno)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_ham_ovfl)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_copypage_recover, DB_ham_copypage)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_metagroup_recover, DB_ham_metagroup)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_ham_groupalloc1)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __deprecated_recover, DB_ham_groupalloc2)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_groupalloc_recover, DB_ham_groupalloc)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_curadj_recover, DB_ham_curadj)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_chgpg_recover, DB_ham_chgpg)) != 0)
		return (ret);
	return (0);
}

