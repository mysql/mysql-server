/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_Db.c,v 11.34 2000/11/30 00:58:38 ubell Exp $";
#endif /* not lint */

#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "db_int.h"
#include "db_page.h"
#include "db_ext.h"
#include "java_util.h"
#include "com_sleepycat_db_Db.h"

/* This struct is used in Db.verify and its callback */
struct verify_callback_struct {
	JNIEnv *env;
	jobject streamobj;
	jbyteArray bytes;
	int nbytes;
	jmethodID writemid;
};

JAVADB_WO_ACCESS_METHOD(Db, jint, flags, DB, flags)
JAVADB_WO_ACCESS_METHOD(Db, jint, h_1ffactor, DB, h_ffactor)
JAVADB_WO_ACCESS_METHOD(Db, jint, h_1nelem, DB, h_nelem)
JAVADB_WO_ACCESS_METHOD(Db, jint, lorder, DB, lorder)
JAVADB_WO_ACCESS_METHOD(Db, jint, re_1delim, DB, re_delim)
JAVADB_WO_ACCESS_METHOD(Db, jint, re_1len, DB, re_len)
JAVADB_WO_ACCESS_METHOD(Db, jint, re_1pad, DB, re_pad)
JAVADB_WO_ACCESS_METHOD(Db, jint, q_1extentsize, DB, q_extentsize)
JAVADB_WO_ACCESS_METHOD(Db, jint, bt_1maxkey, DB, bt_maxkey)
JAVADB_WO_ACCESS_METHOD(Db, jint, bt_1minkey, DB, bt_minkey)

/* This only gets called once ever, at the beginning of execution
 * and can be used to initialize unchanging methodIds, fieldIds, etc.
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_one_1time_1init
  (JNIEnv *jnienv,  /*Db.class*/ jclass jthisclass)
{
	COMPQUIET(jnienv, NULL);
	COMPQUIET(jthisclass, NULL);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1init
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbEnv*/ jobject jdbenv, jint flags)
{
	int err;
	DB *db;
	DB_JAVAINFO *dbinfo;
	DB_ENV *dbenv;

	dbenv = get_DB_ENV(jnienv, jdbenv);
	dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	DB_ASSERT(dbinfo == NULL);

	err = db_create(&db, dbenv, flags);
	if (verify_return(jnienv, err, 0)) {
		set_private_dbobj(jnienv, name_DB, jthis, db);
		dbinfo = dbji_construct(jnienv, flags);
		set_private_info(jnienv, name_DB, jthis, dbinfo);
		db->cj_internal = dbinfo;
	}
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db__1close
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jint flags)
{
	int err;
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	JAVADB_API_BEGIN(db, jthis);

	/* Null out the private data to indicate the DB is invalid.
	 * We do this in advance to help guard against multithreading
	 * issues.
	 */
	set_private_dbobj(jnienv, name_DB, jthis, 0);

	err = db->close(db, flags);
	if (err != DB_INCOMPLETE)
		verify_return(jnienv, err, 0);
	dbji_dealloc(dbinfo, jnienv);

	/* don't call JAVADB_API_END - db cannot be used */
	return (err);
}

/* We are being notified that the parent DbEnv has closed.
 * Zero out the pointer to the DB, since it is no longer
 * valid, to prevent mistakes.  The user will get a null
 * pointer exception if they try to use this Db again.
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1notify_1internal
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	set_private_dbobj(jnienv, name_DB, jthis, 0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_append_1recno_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbAppendRecno*/ jobject jcallback)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	JAVADB_API_BEGIN(db, jthis);
	dbinfo = (DB_JAVAINFO*)db->cj_internal;
	dbji_set_append_recno_object(dbinfo, jnienv, db, jcallback);
	JAVADB_API_END(db);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_bt_1compare_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbBtreeCompare*/ jobject jbtcompare)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	JAVADB_API_BEGIN(db, jthis);
	dbinfo = (DB_JAVAINFO*)db->cj_internal;
	dbji_set_bt_compare_object(dbinfo, jnienv, db, jbtcompare);
	JAVADB_API_END(db);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_bt_1prefix_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbBtreePrefix*/ jobject jbtprefix)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	JAVADB_API_BEGIN(db, jthis);
	dbinfo = (DB_JAVAINFO*)db->cj_internal;
	dbji_set_bt_prefix_object(dbinfo, jnienv, db, jbtprefix);
	JAVADB_API_END(db);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_Db_cursor
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid, jint flags)
{
	int err;
	DBC *dbc;
	DB *db = get_DB(jnienv, jthis);
	DB_TXN *dbtxnid = get_DB_TXN(jnienv, txnid);

	if (!verify_non_null(jnienv, db))
		return (NULL);
	err = db->cursor(db, dbtxnid, &dbc, flags);
	verify_return(jnienv, err, 0);
	return (get_Dbc(jnienv, dbc));
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_del
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject key, jint dbflags)
{
	int err;
	DB_TXN *dbtxnid;
	DB *db;
	JDBT dbkey;

	err = 0;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	JAVADB_API_BEGIN(db, jthis);
	dbtxnid = get_DB_TXN(jnienv, txnid);
	if (jdbt_lock(&dbkey, jnienv, key, inOp) != 0)
		goto out;

	err = db->del(db, dbtxnid, &dbkey.dbt->dbt, dbflags);
	if (err != DB_NOTFOUND) {
		verify_return(jnienv, err, 0);
	}

 out:
	jdbt_unlock(&dbkey, jnienv);
	JAVADB_API_END(db);
	return (err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_dup_1compare_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbDupCompare*/ jobject jdupcompare)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	JAVADB_API_BEGIN(db, jthis);
	dbinfo = (DB_JAVAINFO*)db->cj_internal;
	dbji_set_dup_compare_object(dbinfo, jnienv, db, jdupcompare);
	JAVADB_API_END(db);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_err
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jint ecode, jstring msg)
{
	DB *db;
	JSTR msg_string;

	if (jstr_lock(&msg_string, jnienv, msg) != 0)
		goto out;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		goto out;

	JAVADB_API_BEGIN(db, jthis);
	db->err(db, ecode, msg_string.string);
	JAVADB_API_END(db);

 out:
	jstr_unlock(&msg_string, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_errx
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring msg)
{
	JSTR msg_string;
	DB *db = get_DB(jnienv, jthis);

	if (jstr_lock(&msg_string, jnienv, msg) != 0)
		goto out;
	if (!verify_non_null(jnienv, db))
		goto out;

	JAVADB_API_BEGIN(db, jthis);
	db->errx(db, msg_string.string);
	JAVADB_API_END(db);

 out:
	jstr_unlock(&msg_string, jnienv);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_fd
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	int err;
	int return_value = 0;
	DB *db = get_DB(jnienv, jthis);

	if (!verify_non_null(jnienv, db))
		return (0);

	JAVADB_API_BEGIN(db, jthis);
	err = db->fd(db, &return_value);
	verify_return(jnienv, err, 0);
	JAVADB_API_END(db);

	return (return_value);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_feedback_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbFeedback*/ jobject jfeedback)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	JAVADB_API_BEGIN(db, jthis);
	dbinfo = (DB_JAVAINFO*)db->cj_internal;
	dbji_set_feedback_object(dbinfo, jnienv, db, jfeedback);
	JAVADB_API_END(db);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_get
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject key, /*Dbt*/ jobject data, jint flags)
{
	int err, op_flags, retry;
	DB *db;
	OpKind keyop, dataop;
	DB_TXN *dbtxnid;
	JDBT dbkey, dbdata;

	err = 0;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		goto out3;

	JAVADB_API_BEGIN(db, jthis);

	/* Depending on flags, the key may be input/output. */
	keyop = inOp;
	dataop = outOp;
	op_flags = flags & DB_OPFLAGS_MASK;
	if (op_flags == DB_SET_RECNO) {
		keyop = inOutOp;
	}
	else if (op_flags == DB_GET_BOTH) {
		keyop = inOutOp;
		dataop = inOutOp;
	}

	dbtxnid = get_DB_TXN(jnienv, txnid);

	if (jdbt_lock(&dbkey, jnienv, key, keyop) != 0)
		goto out2;
	if (jdbt_lock(&dbdata, jnienv, data, dataop) != 0)
		goto out1;
	for (retry = 0; retry < 3; retry++) {
		err = db->get(db, dbtxnid, &dbkey.dbt->dbt, &dbdata.dbt->dbt, flags);

		/* If we failed due to lack of memory in our DBT arrays,
		 * retry.
		 */
		if (err != ENOMEM)
			break;
		if (!jdbt_realloc(&dbdata, jnienv))
			break;
	}
	if (err != DB_NOTFOUND) {
		verify_return(jnienv, err, 0);
	}
 out1:
	jdbt_unlock(&dbdata, jnienv);
 out2:
	jdbt_unlock(&dbkey, jnienv);
 out3:
	JAVADB_API_END(db);
	return (err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_hash_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbHash*/ jobject jhash)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	JAVADB_API_BEGIN(db, jthis);
	dbinfo = (DB_JAVAINFO*)db->cj_internal;
	dbji_set_h_hash_object(dbinfo, jnienv, db, jhash);
	JAVADB_API_END(db);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_Db_join
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*Dbc[]*/ jobjectArray curslist,
   jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);
	int count = (*jnienv)->GetArrayLength(jnienv, curslist);
	DBC **newlist = (DBC **)malloc(sizeof(DBC *) * (count+1));
	DBC *dbc;
	int i;

	/* Convert the java array of Dbc's to a C array of DBC's. */
	for (i=0; i<count; i++) {
		jobject jobj = (*jnienv)->GetObjectArrayElement(jnienv, curslist, i);
		if (jobj == 0) {
			/*
			 * An embedded null in the array is treated
			 * as an endpoint.
			 */
			newlist[i] = 0;
			break;
		}
		else {
			newlist[i] = get_DBC(jnienv, jobj);
		}
	}
	newlist[count] = 0;

	if (!verify_non_null(jnienv, db))
		return (NULL);
	JAVADB_API_BEGIN(db, jthis);

	err = db->join(db, newlist, &dbc, flags);
	free(newlist);
	verify_return(jnienv, err, 0);

	JAVADB_API_END(db);
	return (get_Dbc(jnienv, dbc));
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_key_1range
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject jtxn,
   /*Dbt*/ jobject jkey, jobject /*DbKeyRange*/ range, jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);
	DB_TXN *txn = get_DB_TXN(jnienv, jtxn);
	JDBT dbkey;
	DB_KEY_RANGE result;
	jfieldID fid;
	jclass krclass;

	if (!verify_non_null(jnienv, db))
		return;
	JAVADB_API_BEGIN(db, jthis);
	if (!verify_non_null(jnienv, range))
		return;
	if (jdbt_lock(&dbkey, jnienv, jkey, inOp) != 0)
		goto out;
	err = db->key_range(db, txn, &dbkey.dbt->dbt, &result, flags);
	if (verify_return(jnienv, err, 0)) {
		/* fill in the values of the DbKeyRange structure */
		krclass = get_class(jnienv, "DbKeyRange");
		fid = (*jnienv)->GetFieldID(jnienv, krclass, "less", "D");
		(*jnienv)->SetDoubleField(jnienv, range, fid, result.less);
		fid = (*jnienv)->GetFieldID(jnienv, krclass, "equal", "D");
		(*jnienv)->SetDoubleField(jnienv, range, fid, result.equal);
		fid = (*jnienv)->GetFieldID(jnienv, krclass, "greater", "D");
		(*jnienv)->SetDoubleField(jnienv, range, fid, result.greater);
	}
 out:
	jdbt_unlock(&dbkey, jnienv);
	JAVADB_API_END(db);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_put
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject key, /*Dbt*/ jobject data, jint flags)
{
	int err;
	DB *db;
	DB_TXN *dbtxnid;
	JDBT dbkey, dbdata;

	err = 0;
	db = get_DB(jnienv, jthis);
	dbtxnid = get_DB_TXN(jnienv, txnid);
	if (!verify_non_null(jnienv, db))
		return (0);    /* error will be thrown, retval doesn't matter */
	JAVADB_API_BEGIN(db, jthis);

	if (jdbt_lock(&dbkey, jnienv, key, inOp) != 0)
		goto out2;
	if (jdbt_lock(&dbdata, jnienv, data, inOp) != 0)
		goto out1;

	if (!verify_non_null(jnienv, db))
		goto out1;
	err = db->put(db, dbtxnid, &dbkey.dbt->dbt, &dbdata.dbt->dbt, flags);
	if (err != DB_KEYEXIST) {
		verify_return(jnienv, err, 0);
	}
 out1:
	jdbt_unlock(&dbdata, jnienv);
 out2:
	jdbt_unlock(&dbkey, jnienv);
	JAVADB_API_END(db);
	return (err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_rename
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring file,
   jstring database, jstring newname, jint flags)
{
	int err;
	DB *db;
	DB_JAVAINFO *dbinfo;
	JSTR j_file;
	JSTR j_database;
	JSTR j_newname;

	db = get_DB(jnienv, jthis);
	dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;
	JAVADB_API_BEGIN(db, jthis);
	if (jstr_lock(&j_file, jnienv, file) != 0)
		goto out3;
	if (jstr_lock(&j_database, jnienv, database) != 0)
		goto out2;
	if (jstr_lock(&j_newname, jnienv, newname) != 0)
		goto out1;

	err = db->rename(db, j_file.string, j_database.string,
			 j_newname.string, flags);

	verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);
	dbji_dealloc(dbinfo, jnienv);
	set_private_dbobj(jnienv, name_DB, jthis, 0);

 out1:
	jstr_unlock(&j_newname, jnienv);
 out2:
	jstr_unlock(&j_database, jnienv);
 out3:
	jstr_unlock(&j_file, jnienv);
	/* don't call JAVADB_API_END - db cannot be used */
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_remove
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring file,
   jstring database, jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);
	DB_JAVAINFO *dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	JSTR j_file;
	JSTR j_database;

	dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;
	JAVADB_API_BEGIN(db, jthis);
	if (jstr_lock(&j_file, jnienv, file) != 0)
		goto out2;
	if (jstr_lock(&j_database, jnienv, database) != 0)
		goto out1;
	err = db->remove(db, j_file.string, j_database.string, flags);

	set_private_dbobj(jnienv, name_DB, jthis, 0);
	verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);
	dbji_dealloc(dbinfo, jnienv);

 out1:
	jstr_unlock(&j_database, jnienv);
 out2:
	jstr_unlock(&j_file, jnienv);
	/* don't call JAVADB_API_END - db cannot be used */
}

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_Db_set_1pagesize
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jlong value)
{
	int err;
	DB *db;

	db = get_DB(jnienv, jthis);
	if (verify_non_null(jnienv, db)) {
		JAVADB_API_BEGIN(db, jthis);
		err = db->set_pagesize(db, (u_int32_t)value);
		verify_return(jnienv, err, 0);
		JAVADB_API_END(db);
	}
}

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_Db_set_1cachesize
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jint gbytes, jint bytes,
   jint ncaches)
{
	int err;
	DB *db;

	db = get_DB(jnienv, jthis);
	if (verify_non_null(jnienv, db)) {
		JAVADB_API_BEGIN(db, jthis);
		err = db->set_cachesize(db, gbytes, bytes, ncaches);
		verify_return(jnienv, err, 0);
		JAVADB_API_END(db);
	}
}

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_Db_set_1re_1source
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring re_source)
{
	int err;
	DB *db;

	db = get_DB(jnienv, jthis);
	if (verify_non_null(jnienv, db)) {
		JAVADB_API_BEGIN(db, jthis);

		/* XXX does the string from get_c_string ever get freed? */
		if (re_source != NULL)
			err = db->set_re_source(db, get_c_string(jnienv, re_source));
		else
			err = db->set_re_source(db, 0);

		verify_return(jnienv, err, 0);
		JAVADB_API_END(db);
	}
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_Db_stat
  (JNIEnv *jnienv, jobject jthis, jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);
	jobject retval = NULL;
	jclass dbclass;
	void *statp = 0;
	DB_BTREE_STAT *bstp;
	DB_HASH_STAT *hstp;
	DB_QUEUE_STAT *qstp;

	if (!verify_non_null(jnienv, db))
		return (NULL);

	JAVADB_API_BEGIN(db, jthis);

	err = db->stat(db, &statp, NULL, flags);
	if (verify_return(jnienv, err, 0)) {
		DBTYPE dbtype = db->get_type(db);
		switch (dbtype) {

			/* Btree and recno share the same stat structure */
		case DB_BTREE:
		case DB_RECNO:
			bstp = (DB_BTREE_STAT *)statp;
			retval = create_default_object(jnienv,
						       name_DB_BTREE_STAT);
			dbclass = get_class(jnienv, name_DB_BTREE_STAT);

			/* Set the individual fields */
			set_int_field(jnienv, dbclass, retval,
				      "bt_magic", bstp->bt_magic);
			set_int_field(jnienv, dbclass, retval,
				      "bt_version", bstp->bt_version);
			set_int_field(jnienv, dbclass, retval,
				      "bt_metaflags", bstp->bt_metaflags);
			set_int_field(jnienv, dbclass, retval,
				      "bt_nkeys", bstp->bt_nkeys);
			set_int_field(jnienv, dbclass, retval,
				      "bt_ndata", bstp->bt_ndata);
			set_int_field(jnienv, dbclass, retval,
				      "bt_pagesize", bstp->bt_pagesize);
			set_int_field(jnienv, dbclass, retval,
				      "bt_maxkey", bstp->bt_maxkey);
			set_int_field(jnienv, dbclass, retval,
				      "bt_minkey", bstp->bt_minkey);
			set_int_field(jnienv, dbclass, retval,
				      "bt_re_len", bstp->bt_re_len);
			set_int_field(jnienv, dbclass, retval,
				      "bt_re_pad", bstp->bt_re_pad);
			set_int_field(jnienv, dbclass, retval,
				      "bt_levels", bstp->bt_levels);
			set_int_field(jnienv, dbclass, retval,
				      "bt_int_pg", bstp->bt_int_pg);
			set_int_field(jnienv, dbclass, retval,
				      "bt_leaf_pg", bstp->bt_leaf_pg);
			set_int_field(jnienv, dbclass, retval,
				      "bt_dup_pg", bstp->bt_dup_pg);
			set_int_field(jnienv, dbclass, retval,
				      "bt_over_pg", bstp->bt_over_pg);
			set_int_field(jnienv, dbclass, retval,
				      "bt_free", bstp->bt_free);
			set_int_field(jnienv, dbclass, retval,
				      "bt_int_pgfree", bstp->bt_int_pgfree);
			set_int_field(jnienv, dbclass, retval,
				      "bt_leaf_pgfree", bstp->bt_leaf_pgfree);
			set_int_field(jnienv, dbclass, retval,
				      "bt_dup_pgfree", bstp->bt_dup_pgfree);
			set_int_field(jnienv, dbclass, retval,
				      "bt_over_pgfree", bstp->bt_over_pgfree);

			break;

			/* Hash stat structure */
		case DB_HASH:
			hstp = (DB_HASH_STAT *)statp;
			retval = create_default_object(jnienv,
						       name_DB_HASH_STAT);
			dbclass = get_class(jnienv, name_DB_HASH_STAT);

			/* Set the individual fields */
			set_int_field(jnienv, dbclass, retval,
				      "hash_magic", hstp->hash_magic);
			set_int_field(jnienv, dbclass, retval,
				      "hash_version", hstp->hash_version);
			set_int_field(jnienv, dbclass, retval,
				      "hash_metaflags", hstp->hash_metaflags);
			set_int_field(jnienv, dbclass, retval,
				      "hash_nkeys", hstp->hash_nkeys);
			set_int_field(jnienv, dbclass, retval,
				      "hash_ndata", hstp->hash_ndata);
			set_int_field(jnienv, dbclass, retval,
				      "hash_pagesize", hstp->hash_pagesize);
			set_int_field(jnienv, dbclass, retval,
				      "hash_nelem", hstp->hash_nelem);
			set_int_field(jnienv, dbclass, retval,
				      "hash_ffactor", hstp->hash_ffactor);
			set_int_field(jnienv, dbclass, retval,
				      "hash_buckets", hstp->hash_buckets);
			set_int_field(jnienv, dbclass, retval,
				      "hash_free", hstp->hash_free);
			set_int_field(jnienv, dbclass, retval,
				      "hash_bfree", hstp->hash_bfree);
			set_int_field(jnienv, dbclass, retval,
				      "hash_bigpages", hstp->hash_bigpages);
			set_int_field(jnienv, dbclass, retval,
				      "hash_big_bfree", hstp->hash_big_bfree);
			set_int_field(jnienv, dbclass, retval,
				      "hash_overflows", hstp->hash_overflows);
			set_int_field(jnienv, dbclass, retval,
				      "hash_ovfl_free", hstp->hash_ovfl_free);
			set_int_field(jnienv, dbclass, retval,
				      "hash_dup", hstp->hash_dup);
			set_int_field(jnienv, dbclass, retval,
				      "hash_dup_free", hstp->hash_dup_free);

			break;

		case DB_QUEUE:
			qstp = (DB_QUEUE_STAT *)statp;
			retval = create_default_object(jnienv,
						       name_DB_QUEUE_STAT);
			dbclass = get_class(jnienv, name_DB_QUEUE_STAT);

			/* Set the individual fields */
			set_int_field(jnienv, dbclass, retval,
				      "qs_magic", qstp->qs_magic);
			set_int_field(jnienv, dbclass, retval,
				      "qs_version", qstp->qs_version);
			set_int_field(jnienv, dbclass, retval,
				      "qs_metaflags", qstp->qs_metaflags);
			set_int_field(jnienv, dbclass, retval,
				      "qs_nkeys", qstp->qs_nkeys);
			set_int_field(jnienv, dbclass, retval,
				      "qs_ndata", qstp->qs_ndata);
			set_int_field(jnienv, dbclass, retval,
				      "qs_pagesize", qstp->qs_pagesize);
			set_int_field(jnienv, dbclass, retval,
				      "qs_pages", qstp->qs_pages);
			set_int_field(jnienv, dbclass, retval,
				      "qs_re_len", qstp->qs_re_len);
			set_int_field(jnienv, dbclass, retval,
				      "qs_re_pad", qstp->qs_re_pad);
			set_int_field(jnienv, dbclass, retval,
				      "qs_pgfree", qstp->qs_pgfree);
			set_int_field(jnienv, dbclass, retval,
				      "qs_first_recno", qstp->qs_first_recno);
			set_int_field(jnienv, dbclass, retval,
				      "qs_cur_recno", qstp->qs_cur_recno);
			break;

			/* That's all the database types we're aware of! */
		default:
			report_exception(jnienv,
					 "Db.stat not implemented for types"
					 "other than HASH, BTREE and RECNO",
					 EINVAL, 0);
			break;
		}
		free(statp);
	}
	JAVADB_API_END(db);
	return (retval);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_sync
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);

	if (!verify_non_null(jnienv, db))
		return (0);
	JAVADB_API_BEGIN(db, jthis);
	err = db->sync(db, flags);
	if (err != DB_INCOMPLETE)
		verify_return(jnienv, err, 0);
	JAVADB_API_END(db);
	return (err);
}

JNIEXPORT jboolean JNICALL Java_com_sleepycat_db_Db_get_1byteswapped
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	DB *db;
	jboolean retval;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	JAVADB_API_BEGIN(db, jthis);
	retval = db->get_byteswapped(db) ? 1 : 0;
	JAVADB_API_END(db);
	return (retval);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_get_1type
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	DB *db;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	return ((jint)db->type);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1open
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring file, jstring database,
   jint type, jint flags, jint mode)
{
	int err;
	DB *db;
	JSTR dbfile;
	JSTR dbdatabase;

	/* Java is assumed to be threaded. */
	flags |= DB_THREAD;

	db = get_DB(jnienv, jthis);
	if (jstr_lock(&dbfile, jnienv, file) != 0)
		goto out2;
	if (jstr_lock(&dbdatabase, jnienv, database) != 0)
		goto out1;
	if (verify_non_null(jnienv, db)) {
		JAVADB_API_BEGIN(db, jthis);
		err = db->open(db, dbfile.string, dbdatabase.string,
			       (DBTYPE)type, flags, mode);
		verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);
		JAVADB_API_END(db);
	}
 out1:
	jstr_unlock(&dbdatabase, jnienv);
 out2:
	jstr_unlock(&dbfile, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_upgrade
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring name,
   jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);
	JSTR j_name;

	if (verify_non_null(jnienv, db)) {
		JAVADB_API_BEGIN(db, jthis);
		if (jstr_lock(&j_name, jnienv, name) != 0)
			goto out;
		err = db->upgrade(db, j_name.string, flags);
		verify_return(jnienv, err, 0);
		JAVADB_API_END(db);
	}
 out:
	jstr_unlock(&j_name, jnienv);
}

static int java_verify_callback(void *handle, const void *str_arg)
{
	char *str;
	struct verify_callback_struct *vc;
	int len;
	jthrowable except;
	JNIEnv *jnienv;

	str = (char *)str_arg;
	vc = (struct verify_callback_struct *)handle;
	jnienv = vc->env;
	len = strlen(str)+1;
	if (len > vc->nbytes) {
		vc->nbytes = len;
		vc->bytes = (*jnienv)->NewByteArray(jnienv, len);
	}
	(*jnienv)->SetByteArrayRegion(jnienv, vc->bytes, 0, len, (jbyte*)str);
	(*jnienv)->CallVoidMethod(jnienv, vc->streamobj,
				  vc->writemid, vc->bytes, 0, len-1);

	if ((except = (*jnienv)->ExceptionOccurred(jnienv)) != NULL)
		return (EIO);

	return (0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_verify
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring name,
   jstring subdb, jobject stream, jint flags)
{
	int err;
	DB *db;
	JSTR j_name;
	JSTR j_subdb;
	struct verify_callback_struct vcs;
	jclass streamclass;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;
	JAVADB_API_BEGIN(db, jthis);

	if (jstr_lock(&j_name, jnienv, name) != 0)
		goto out2;
	if (jstr_lock(&j_subdb, jnienv, subdb) != 0)
		goto out1;

	/* set up everything we need for the callbacks */
	vcs.env = jnienv;
	vcs.streamobj = stream;
	vcs.nbytes = 100;
	vcs.bytes = (*jnienv)->NewByteArray(jnienv, vcs.nbytes);

	/* get the method ID for OutputStream.write(byte[], int, int); */
	streamclass = (*jnienv)->FindClass(jnienv, "java/io/OutputStream");
	vcs.writemid = (*jnienv)->GetMethodID(jnienv, streamclass,
					      "write", "([BII)V");

	/* invoke verify - this will invoke the callback repeatedly. */
	err = __db_verify_internal(db, j_name.string, j_subdb.string,
				   &vcs, java_verify_callback, flags);
	verify_return(jnienv, err, 0);

out1:
	jstr_unlock(&j_subdb, jnienv);
out2:
	jstr_unlock(&j_name, jnienv);
	JAVADB_API_END(db);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1finalize
    (JNIEnv *jnienv, jobject jthis,
     jobject /*DbErrcall*/ errcall, jstring errpfx)
{
	DB_JAVAINFO *dbinfo;
	DB *db;

	dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	db = get_DB(jnienv, jthis);
	DB_ASSERT(dbinfo != NULL);

	/* Note: We can never be sure if the underlying DB is attached to
	 * a DB_ENV that was already closed.  Sure, that's a user error,
	 * but it shouldn't crash the VM.  Therefore, we cannot just
	 * automatically close if the handle indicates we are not yet
	 * closed.  The best we can do is detect this and report it.
	 */
	if (db != NULL) {
		/* If this error occurs, this object was never closed. */
		report_errcall(jnienv, errcall, errpfx,
			       "Db.finalize: open Db object destroyed");
	}

	/* Shouldn't see this object again, but just in case */
	set_private_dbobj(jnienv, name_DB, jthis, 0);
	set_private_info(jnienv, name_DB, jthis, 0);

	dbji_destroy(dbinfo, jnienv);
}
