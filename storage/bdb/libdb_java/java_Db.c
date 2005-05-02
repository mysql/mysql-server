/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_Db.c,v 11.80 2002/08/29 14:22:23 margo Exp $";
#endif /* not lint */

#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc_auto/db_ext.h"
#include "java_util.h"
#include "java_stat_auto.h"
#include "com_sleepycat_db_Db.h"

/* This struct is used in Db.verify and its callback */
struct verify_callback_struct {
	JNIEnv *env;
	jobject streamobj;
	jbyteArray bytes;
	int nbytes;
	jmethodID writemid;
};

JAVADB_GET_FLD(Db, jint, flags_1raw, DB, flags)

JAVADB_SET_METH(Db, jint, flags, DB, flags)
JAVADB_SET_METH(Db, jint, h_1ffactor, DB, h_ffactor)
JAVADB_SET_METH(Db, jint, h_1nelem, DB, h_nelem)
JAVADB_SET_METH(Db, jint, lorder, DB, lorder)
JAVADB_SET_METH(Db, jint, re_1delim, DB, re_delim)
JAVADB_SET_METH(Db, jint, re_1len, DB, re_len)
JAVADB_SET_METH(Db, jint, re_1pad, DB, re_pad)
JAVADB_SET_METH(Db, jint, q_1extentsize, DB, q_extentsize)
JAVADB_SET_METH(Db, jint, bt_1maxkey, DB, bt_maxkey)
JAVADB_SET_METH(Db, jint, bt_1minkey, DB, bt_minkey)

/*
 * This only gets called once ever, at the beginning of execution
 * and can be used to initialize unchanging methodIds, fieldIds, etc.
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_one_1time_1init
  (JNIEnv *jnienv,  /*Db.class*/ jclass jthisclass)
{
	COMPQUIET(jthisclass, NULL);

	one_time_init(jnienv);
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
		dbinfo = dbji_construct(jnienv, jthis, flags);
		set_private_info(jnienv, name_DB, jthis, dbinfo);
		db->api_internal = dbinfo;
	}
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1associate
    (JNIEnv *jnienv, /*Db*/ jobject jthis, /* DbTxn */ jobject jtxn,
     /*Db*/ jobject jsecondary, /*DbSecondaryKeyCreate*/ jobject jcallback,
     jint flags)
{
	DB *db, *secondary;
	DB_JAVAINFO *second_info;
	DB_TXN *txn;

	db = get_DB(jnienv, jthis);
	txn = get_DB_TXN(jnienv, jtxn);
	secondary = get_DB(jnienv, jsecondary);

	second_info = (DB_JAVAINFO*)secondary->api_internal;
	dbji_set_assoc_object(second_info, jnienv, db, txn, secondary,
			      jcallback, flags);

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

	/*
	 * Null out the private data to indicate the DB is invalid.
	 * We do this in advance to help guard against multithreading
	 * issues.
	 */
	set_private_dbobj(jnienv, name_DB, jthis, 0);

	err = db->close(db, flags);
	verify_return(jnienv, err, 0);
	dbji_dealloc(dbinfo, jnienv);

	return (err);
}

/*
 * We are being notified that the parent DbEnv has closed.
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

	dbinfo = (DB_JAVAINFO*)db->api_internal;
	dbji_set_append_recno_object(dbinfo, jnienv, db, jcallback);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_bt_1compare_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbBtreeCompare*/ jobject jbtcompare)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	dbinfo = (DB_JAVAINFO*)db->api_internal;
	dbji_set_bt_compare_object(dbinfo, jnienv, db, jbtcompare);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_bt_1prefix_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbBtreePrefix*/ jobject jbtprefix)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	dbinfo = (DB_JAVAINFO*)db->api_internal;
	dbji_set_bt_prefix_object(dbinfo, jnienv, db, jbtprefix);
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
	LOCKED_DBT lkey;

	err = 0;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	dbtxnid = get_DB_TXN(jnienv, txnid);
	if (locked_dbt_get(&lkey, jnienv, db->dbenv, key, inOp) != 0)
		goto out;

	err = db->del(db, dbtxnid, &lkey.javainfo->dbt, dbflags);
	if (!DB_RETOK_DBDEL(err))
		verify_return(jnienv, err, 0);

 out:
	locked_dbt_put(&lkey, jnienv, db->dbenv);
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

	dbinfo = (DB_JAVAINFO*)db->api_internal;
	dbji_set_dup_compare_object(dbinfo, jnienv, db, jdupcompare);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_err
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jint ecode, jstring msg)
{
	DB *db;
	LOCKED_STRING ls_msg;

	if (locked_string_get(&ls_msg, jnienv, msg) != 0)
		goto out;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		goto out;

	db->err(db, ecode, "%s", ls_msg.string);

 out:
	locked_string_put(&ls_msg, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_errx
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring msg)
{
	LOCKED_STRING ls_msg;
	DB *db = get_DB(jnienv, jthis);

	if (locked_string_get(&ls_msg, jnienv, msg) != 0)
		goto out;
	if (!verify_non_null(jnienv, db))
		goto out;

	db->errx(db, "%s", ls_msg.string);

 out:
	locked_string_put(&ls_msg, jnienv);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_fd
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	int err;
	int return_value = 0;
	DB *db = get_DB(jnienv, jthis);

	if (!verify_non_null(jnienv, db))
		return (0);

	err = db->fd(db, &return_value);
	verify_return(jnienv, err, 0);

	return (return_value);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_set_1encrypt
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring jpasswd, jint flags)
{
	int err;
	DB *db;
	LOCKED_STRING ls_passwd;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;
	if (locked_string_get(&ls_passwd, jnienv, jpasswd) != 0)
		goto out;

	err = db->set_encrypt(db, ls_passwd.string, flags);
	verify_return(jnienv, err, 0);

out:	locked_string_put(&ls_passwd, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_feedback_1changed
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbFeedback*/ jobject jfeedback)
{
	DB *db;
	DB_JAVAINFO *dbinfo;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;

	dbinfo = (DB_JAVAINFO*)db->api_internal;
	dbji_set_feedback_object(dbinfo, jnienv, db, jfeedback);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_get
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject key, /*Dbt*/ jobject data, jint flags)
{
	int err, op_flags, retry;
	DB *db;
	DB_ENV *dbenv;
	OpKind keyop, dataop;
	DB_TXN *dbtxnid;
	LOCKED_DBT lkey, ldata;

	err = 0;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		goto out3;
	dbenv = db->dbenv;

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

	if (locked_dbt_get(&lkey, jnienv, dbenv, key, keyop) != 0)
		goto out2;
	if (locked_dbt_get(&ldata, jnienv, dbenv, data, dataop) != 0)
		goto out1;
	for (retry = 0; retry < 3; retry++) {
		err = db->get(db,
		    dbtxnid, &lkey.javainfo->dbt, &ldata.javainfo->dbt, flags);

		/*
		 * If we failed due to lack of memory in our DBT arrays,
		 * retry.
		 */
		if (err != ENOMEM)
			break;
		if (!locked_dbt_realloc(&lkey, jnienv, dbenv) &&
		    !locked_dbt_realloc(&ldata, jnienv, dbenv))
			break;
	}
 out1:
	locked_dbt_put(&ldata, jnienv, dbenv);
 out2:
	locked_dbt_put(&lkey, jnienv, dbenv);
 out3:
	if (!DB_RETOK_DBGET(err)) {
		if (verify_dbt(jnienv, err, &lkey) &&
		    verify_dbt(jnienv, err, &ldata))
			verify_return(jnienv, err, 0);
	}
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

	dbinfo = (DB_JAVAINFO*)db->api_internal;
	dbji_set_h_hash_object(dbinfo, jnienv, db, jhash);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_Db_join
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*Dbc[]*/ jobjectArray curslist,
   jint flags)
{
	int err;
	DB *db;
	int count;
	DBC **newlist;
	DBC *dbc;
	int i;
	int size;

	db = get_DB(jnienv, jthis);
	count = (*jnienv)->GetArrayLength(jnienv, curslist);
	size = sizeof(DBC *) * (count+1);
	if ((err = __os_malloc(db->dbenv, size, &newlist)) != 0) {
		if (!verify_return(jnienv, err, 0))
			return (NULL);
	}

	/* Convert the java array of Dbc's to a C array of DBC's. */
	for (i = 0; i < count; i++) {
		jobject jobj =
		    (*jnienv)->GetObjectArrayElement(jnienv, curslist, i);
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

	err = db->join(db, newlist, &dbc, flags);
	verify_return(jnienv, err, 0);
	__os_free(db->dbenv, newlist);

	return (get_Dbc(jnienv, dbc));
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_key_1range
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject jkey, jobject /*DbKeyRange*/ range, jint flags)
{
	int err;
	DB *db;
	DB_TXN *dbtxnid;
	LOCKED_DBT lkey;
	DB_KEY_RANGE result;
	jfieldID fid;
	jclass krclass;

	db = get_DB(jnienv, jthis);
	dbtxnid = get_DB_TXN(jnienv, txnid);
	if (!verify_non_null(jnienv, db))
		return;
	if (!verify_non_null(jnienv, range))
		return;
	if (locked_dbt_get(&lkey, jnienv, db->dbenv, jkey, inOp) != 0)
		goto out;
	err = db->key_range(db, dbtxnid, &lkey.javainfo->dbt, &result, flags);
	if (verify_return(jnienv, err, 0)) {
		/* fill in the values of the DbKeyRange structure */
		if ((krclass = get_class(jnienv, "DbKeyRange")) == NULL)
			return;	/* An exception has been posted. */
		fid = (*jnienv)->GetFieldID(jnienv, krclass, "less", "D");
		(*jnienv)->SetDoubleField(jnienv, range, fid, result.less);
		fid = (*jnienv)->GetFieldID(jnienv, krclass, "equal", "D");
		(*jnienv)->SetDoubleField(jnienv, range, fid, result.equal);
		fid = (*jnienv)->GetFieldID(jnienv, krclass, "greater", "D");
		(*jnienv)->SetDoubleField(jnienv, range, fid, result.greater);
	}
 out:
	locked_dbt_put(&lkey, jnienv, db->dbenv);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_pget
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject key, /*Dbt*/ jobject rkey, /*Dbt*/ jobject data, jint flags)
{
	int err, op_flags, retry;
	DB *db;
	DB_ENV *dbenv;
	OpKind keyop, rkeyop, dataop;
	DB_TXN *dbtxnid;
	LOCKED_DBT lkey, lrkey, ldata;

	err = 0;
	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		goto out4;
	dbenv = db->dbenv;

	/* Depending on flags, the key may be input/output. */
	keyop = inOp;
	rkeyop = outOp;
	dataop = outOp;
	op_flags = flags & DB_OPFLAGS_MASK;
	if (op_flags == DB_SET_RECNO) {
		keyop = inOutOp;
	}
	else if (op_flags == DB_GET_BOTH) {
		keyop = inOutOp;
		rkeyop = inOutOp;
		dataop = inOutOp;
	}

	dbtxnid = get_DB_TXN(jnienv, txnid);

	if (locked_dbt_get(&lkey, jnienv, dbenv, key, keyop) != 0)
		goto out3;
	if (locked_dbt_get(&lrkey, jnienv, dbenv, rkey, rkeyop) != 0)
		goto out2;
	if (locked_dbt_get(&ldata, jnienv, dbenv, data, dataop) != 0)
		goto out1;
	for (retry = 0; retry < 3; retry++) {
		err = db->pget(db, dbtxnid, &lkey.javainfo->dbt,
		    &lrkey.javainfo->dbt, &ldata.javainfo->dbt, flags);

		/*
		 * If we failed due to lack of memory in our DBT arrays,
		 * retry.
		 */
		if (err != ENOMEM)
			break;
		if (!locked_dbt_realloc(&lkey, jnienv, dbenv) &&
		    !locked_dbt_realloc(&lrkey, jnienv, dbenv) &&
		    !locked_dbt_realloc(&ldata, jnienv, dbenv))
			break;
	}
 out1:
	locked_dbt_put(&ldata, jnienv, dbenv);
 out2:
	locked_dbt_put(&lrkey, jnienv, dbenv);
 out3:
	locked_dbt_put(&lkey, jnienv, dbenv);
 out4:
	if (!DB_RETOK_DBGET(err)) {
		if (verify_dbt(jnienv, err, &lkey) &&
		    verify_dbt(jnienv, err, &lrkey) &&
		    verify_dbt(jnienv, err, &ldata))
			verify_return(jnienv, err, 0);
	}
	return (err);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_put
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   /*Dbt*/ jobject key, /*Dbt*/ jobject data, jint flags)
{
	int err;
	DB *db;
	DB_ENV *dbenv;
	DB_TXN *dbtxnid;
	LOCKED_DBT lkey, ldata;
	OpKind keyop;

	err = 0;
	db = get_DB(jnienv, jthis);
	dbtxnid = get_DB_TXN(jnienv, txnid);
	if (!verify_non_null(jnienv, db))
		return (0);   /* error will be thrown, retval doesn't matter */
	dbenv = db->dbenv;

	/*
	 * For DB_APPEND, the key may be output-only;  for all other flags,
	 * it's input-only.
	 */
	if ((flags & DB_OPFLAGS_MASK) == DB_APPEND)
		keyop = outOp;
	else
		keyop = inOp;

	if (locked_dbt_get(&lkey, jnienv, dbenv, key, keyop) != 0)
		goto out2;
	if (locked_dbt_get(&ldata, jnienv, dbenv, data, inOp) != 0)
		goto out1;

	if (!verify_non_null(jnienv, db))
		goto out1;

	err = db->put(db,
	    dbtxnid, &lkey.javainfo->dbt, &ldata.javainfo->dbt, flags);
	if (!DB_RETOK_DBPUT(err))
		verify_return(jnienv, err, 0);

 out1:
	locked_dbt_put(&ldata, jnienv, dbenv);
 out2:
	locked_dbt_put(&lkey, jnienv, dbenv);
	return (err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1remove
  (JNIEnv *jnienv, /*Db*/ jobject jthis,
   jstring file, jstring database, jint flags)
{
	int err;
	DB *db;
	DB_JAVAINFO *dbinfo;
	LOCKED_STRING ls_file;
	LOCKED_STRING ls_database;

	db = get_DB(jnienv, jthis);
	dbinfo = get_DB_JAVAINFO(jnienv, jthis);

	if (!verify_non_null(jnienv, db))
		return;
	if (locked_string_get(&ls_file, jnienv, file) != 0)
		goto out2;
	if (locked_string_get(&ls_database, jnienv, database) != 0)
		goto out1;
	err = db->remove(db, ls_file.string, ls_database.string, flags);

	set_private_dbobj(jnienv, name_DB, jthis, 0);
	verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);

 out1:
	locked_string_put(&ls_database, jnienv);
 out2:
	locked_string_put(&ls_file, jnienv);

	dbji_dealloc(dbinfo, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1rename
  (JNIEnv *jnienv, /*Db*/ jobject jthis,
   jstring file, jstring database, jstring newname, jint flags)
{
	int err;
	DB *db;
	DB_JAVAINFO *dbinfo;
	LOCKED_STRING ls_file;
	LOCKED_STRING ls_database;
	LOCKED_STRING ls_newname;

	db = get_DB(jnienv, jthis);
	dbinfo = get_DB_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;
	if (locked_string_get(&ls_file, jnienv, file) != 0)
		goto out3;
	if (locked_string_get(&ls_database, jnienv, database) != 0)
		goto out2;
	if (locked_string_get(&ls_newname, jnienv, newname) != 0)
		goto out1;

	err = db->rename(db, ls_file.string, ls_database.string,
			 ls_newname.string, flags);

	verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);
	set_private_dbobj(jnienv, name_DB, jthis, 0);

 out1:
	locked_string_put(&ls_newname, jnienv);
 out2:
	locked_string_put(&ls_database, jnienv);
 out3:
	locked_string_put(&ls_file, jnienv);

	dbji_dealloc(dbinfo, jnienv);
}

JAVADB_METHOD(Db_set_1pagesize, (JAVADB_ARGS, jlong pagesize), DB,
    set_pagesize, (c_this, (u_int32_t)pagesize))
JAVADB_METHOD(Db_set_1cachesize,
    (JAVADB_ARGS, jint gbytes, jint bytes, jint ncaches), DB,
    set_cachesize, (c_this, gbytes, bytes, ncaches))
JAVADB_METHOD(Db_set_1cache_1priority, (JAVADB_ARGS, jint priority), DB,
    set_cache_priority, (c_this, (DB_CACHE_PRIORITY)priority))

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_Db_set_1re_1source
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring re_source)
{
	int err;
	DB *db;

	db = get_DB(jnienv, jthis);
	if (verify_non_null(jnienv, db)) {

		/* XXX does the string from get_c_string ever get freed? */
		if (re_source != NULL)
			err = db->set_re_source(db,
			    get_c_string(jnienv, re_source));
		else
			err = db->set_re_source(db, 0);

		verify_return(jnienv, err, 0);
	}
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_Db_stat
  (JNIEnv *jnienv, jobject jthis, jint flags)
{
	DB *db;
	DB_BTREE_STAT *bstp;
	DB_HASH_STAT *hstp;
	DB_QUEUE_STAT *qstp;
	DBTYPE dbtype;
	jobject retval;
	jclass dbclass;
	size_t bytesize;
	void *statp;

	bytesize = 0;
	retval = NULL;
	statp = NULL;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (NULL);

	if (verify_return(jnienv, db->stat(db, &statp, flags), 0) &&
	    verify_return(jnienv, db->get_type(db, &dbtype), 0)) {
		switch (dbtype) {
			/* Btree and recno share the same stat structure */
		case DB_BTREE:
		case DB_RECNO:
			bstp = (DB_BTREE_STAT *)statp;
			bytesize = sizeof(DB_BTREE_STAT);
			retval = create_default_object(jnienv,
						       name_DB_BTREE_STAT);
			if ((dbclass =
			    get_class(jnienv, name_DB_BTREE_STAT)) == NULL)
				break;	/* An exception has been posted. */

			__jv_fill_bt_stat(jnienv, dbclass, retval, bstp);
			break;

			/* Hash stat structure */
		case DB_HASH:
			hstp = (DB_HASH_STAT *)statp;
			bytesize = sizeof(DB_HASH_STAT);
			retval = create_default_object(jnienv,
						       name_DB_HASH_STAT);
			if ((dbclass =
			    get_class(jnienv, name_DB_HASH_STAT)) == NULL)
				break;	/* An exception has been posted. */

			__jv_fill_h_stat(jnienv, dbclass, retval, hstp);
			break;

		case DB_QUEUE:
			qstp = (DB_QUEUE_STAT *)statp;
			bytesize = sizeof(DB_QUEUE_STAT);
			retval = create_default_object(jnienv,
						       name_DB_QUEUE_STAT);
			if ((dbclass =
			    get_class(jnienv, name_DB_QUEUE_STAT)) == NULL)
				break;	/* An exception has been posted. */

			__jv_fill_qam_stat(jnienv, dbclass, retval, qstp);
			break;

			/* That's all the database types we're aware of! */
		default:
			report_exception(jnienv,
					 "Db.stat not implemented for types"
					 " other than BTREE, HASH, QUEUE,"
					 " and RECNO",
					 EINVAL, 0);
			break;
		}
		if (bytesize != 0)
			__os_ufree(db->dbenv, statp);
	}
	return (retval);
}

JAVADB_METHOD(Db_sync, (JAVADB_ARGS, jint flags), DB,
    sync, (c_this, flags))

JNIEXPORT jboolean JNICALL Java_com_sleepycat_db_Db_get_1byteswapped
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	DB *db;
	int err, isbyteswapped;

	/* This value should never be seen, because of the exception. */
	isbyteswapped = 0;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	err = db->get_byteswapped(db, &isbyteswapped);
	(void)verify_return(jnienv, err, 0);

	return ((jboolean)isbyteswapped);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_get_1type
  (JNIEnv *jnienv, /*Db*/ jobject jthis)
{
	DB *db;
	int err;
	DBTYPE dbtype;

	/* This value should never be seen, because of the exception. */
	dbtype = DB_UNKNOWN;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return (0);

	err = db->get_type(db, &dbtype);
	(void)verify_return(jnienv, err, 0);

	return ((jint)dbtype);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db__1open
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject txnid,
   jstring file, jstring database, jint type, jint flags, jint mode)
{
	int err;
	DB *db;
	DB_TXN *dbtxnid;
	LOCKED_STRING ls_file;
	LOCKED_STRING ls_database;

	/* Java is assumed to be threaded */
	flags |= DB_THREAD;

	db = get_DB(jnienv, jthis);

	dbtxnid = get_DB_TXN(jnienv, txnid);
	if (locked_string_get(&ls_file, jnienv, file) != 0)
		goto out2;
	if (locked_string_get(&ls_database, jnienv, database) != 0)
		goto out1;
	if (verify_non_null(jnienv, db)) {
		err = db->open(db, dbtxnid, ls_file.string, ls_database.string,
			       (DBTYPE)type, flags, mode);
		verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);
	}
 out1:
	locked_string_put(&ls_database, jnienv);
 out2:
	locked_string_put(&ls_file, jnienv);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Db_truncate
  (JNIEnv *jnienv, /*Db*/ jobject jthis, /*DbTxn*/ jobject jtxnid, jint flags)
{
	int err;
	DB *db;
	u_int32_t count;
	DB_TXN *dbtxnid;

	db = get_DB(jnienv, jthis);
	dbtxnid = get_DB_TXN(jnienv, jtxnid);
	count = 0;
	if (verify_non_null(jnienv, db)) {
		err = db->truncate(db, dbtxnid, &count, flags);
		verify_return(jnienv, err, 0);
	}
	return (jint)count;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_upgrade
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring name,
   jint flags)
{
	int err;
	DB *db = get_DB(jnienv, jthis);
	LOCKED_STRING ls_name;

	if (verify_non_null(jnienv, db)) {
		if (locked_string_get(&ls_name, jnienv, name) != 0)
			goto out;
		err = db->upgrade(db, ls_name.string, flags);
		verify_return(jnienv, err, 0);
	}
 out:
	locked_string_put(&ls_name, jnienv);
}

static int java_verify_callback(void *handle, const void *str_arg)
{
	char *str;
	struct verify_callback_struct *vc;
	int len;
	JNIEnv *jnienv;

	str = (char *)str_arg;
	vc = (struct verify_callback_struct *)handle;
	jnienv = vc->env;
	len = strlen(str)+1;
	if (len > vc->nbytes) {
		vc->nbytes = len;
		vc->bytes = (*jnienv)->NewByteArray(jnienv, len);
	}

	if (vc->bytes != NULL) {
		(*jnienv)->SetByteArrayRegion(jnienv, vc->bytes, 0, len,
		    (jbyte*)str);
		(*jnienv)->CallVoidMethod(jnienv, vc->streamobj,
		    vc->writemid, vc->bytes, 0, len-1);
	}

	if ((*jnienv)->ExceptionOccurred(jnienv) != NULL)
		return (EIO);

	return (0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Db_verify
  (JNIEnv *jnienv, /*Db*/ jobject jthis, jstring name,
   jstring subdb, jobject stream, jint flags)
{
	int err;
	DB *db;
	LOCKED_STRING ls_name;
	LOCKED_STRING ls_subdb;
	struct verify_callback_struct vcs;
	jclass streamclass;

	db = get_DB(jnienv, jthis);
	if (!verify_non_null(jnienv, db))
		return;
	if (locked_string_get(&ls_name, jnienv, name) != 0)
		goto out2;
	if (locked_string_get(&ls_subdb, jnienv, subdb) != 0)
		goto out1;

	/* set up everything we need for the callbacks */
	vcs.env = jnienv;
	vcs.streamobj = stream;
	vcs.nbytes = 100;
	if ((vcs.bytes = (*jnienv)->NewByteArray(jnienv, vcs.nbytes)) == NULL)
		goto out1;

	/* get the method ID for OutputStream.write(byte[], int, int); */
	streamclass = (*jnienv)->FindClass(jnienv, "java/io/OutputStream");
	vcs.writemid = (*jnienv)->GetMethodID(jnienv, streamclass,
					      "write", "([BII)V");

	/* invoke verify - this will invoke the callback repeatedly. */
	err = __db_verify_internal(db, ls_name.string, ls_subdb.string,
				   &vcs, java_verify_callback, flags);
	verify_return(jnienv, err, 0);

out1:
	locked_string_put(&ls_subdb, jnienv);
out2:
	locked_string_put(&ls_name, jnienv);
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

	/*
	 * Note: We can never be sure if the underlying DB is attached to
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
