/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbEnv.c,v 11.105 2002/08/29 14:22:23 margo Exp $";
#endif /* not lint */

#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "db_int.h"
#include "java_util.h"
#include "java_stat_auto.h"
#include "com_sleepycat_db_DbEnv.h"

/* We keep these lined up, and alphabetical by field name,
 * for comparison with C++'s list.
 */
JAVADB_SET_METH_STR(DbEnv,   data_1dir, DB_ENV, data_dir)
JAVADB_SET_METH(DbEnv, jint, lg_1bsize, DB_ENV, lg_bsize)
JAVADB_SET_METH_STR(DbEnv,   lg_1dir, DB_ENV, lg_dir)
JAVADB_SET_METH(DbEnv, jint, lg_1max, DB_ENV, lg_max)
JAVADB_SET_METH(DbEnv, jint, lg_1regionmax, DB_ENV, lg_regionmax)
JAVADB_SET_METH(DbEnv, jint, lk_1detect, DB_ENV, lk_detect)
JAVADB_SET_METH(DbEnv, jint, lk_1max, DB_ENV, lk_max)
JAVADB_SET_METH(DbEnv, jint, lk_1max_1locks, DB_ENV, lk_max_locks)
JAVADB_SET_METH(DbEnv, jint, lk_1max_1lockers, DB_ENV, lk_max_lockers)
JAVADB_SET_METH(DbEnv, jint, lk_1max_1objects, DB_ENV, lk_max_objects)
/* mp_mmapsize is declared below, it needs an extra cast */
JAVADB_SET_METH_STR(DbEnv,   tmp_1dir, DB_ENV, tmp_dir)
JAVADB_SET_METH(DbEnv, jint, tx_1max, DB_ENV, tx_max)

static void DbEnv_errcall_callback(const char *prefix, char *message)
{
	JNIEnv *jnienv;
	DB_ENV_JAVAINFO *envinfo = (DB_ENV_JAVAINFO *)prefix;
	jstring pre;

	/*
	 * Note: these error cases are "impossible", and would
	 * normally warrant an exception.  However, without
	 * a jnienv, we cannot throw an exception...
	 * We don't want to trap or exit, since the point of
	 * this facility is for the user to completely control
	 * error situations.
	 */
	if (envinfo == NULL) {
		/*
		 * Something is *really* wrong here, the
		 * prefix is set in every environment created.
		 */
		fprintf(stderr, "Error callback failed!\n");
		fprintf(stderr, "error: %s\n", message);
		return;
	}

	/* Should always succeed... */
	jnienv = dbjie_get_jnienv(envinfo);

	if (jnienv == NULL) {

		/* But just in case... */
		fprintf(stderr, "Cannot attach to current thread!\n");
		fprintf(stderr, "error: %s\n", message);
		return;
	}

	pre = dbjie_get_errpfx(envinfo, jnienv);
	report_errcall(jnienv, dbjie_get_errcall(envinfo), pre, message);
}

static void DbEnv_initialize(JNIEnv *jnienv, DB_ENV *dbenv,
			     /*DbEnv*/ jobject jenv,
			     /*DbErrcall*/ jobject jerrcall,
			     int is_dbopen)
{
	DB_ENV_JAVAINFO *envinfo;

	envinfo = get_DB_ENV_JAVAINFO(jnienv, jenv);
	DB_ASSERT(envinfo == NULL);
	envinfo = dbjie_construct(jnienv, jenv, jerrcall, is_dbopen);
	set_private_info(jnienv, name_DB_ENV, jenv, envinfo);
	dbenv->set_errpfx(dbenv, (const char*)envinfo);
	dbenv->set_errcall(dbenv, DbEnv_errcall_callback);
	dbenv->api2_internal = envinfo;
	set_private_dbobj(jnienv, name_DB_ENV, jenv, dbenv);
}

/*
 * This is called when this DbEnv was made on behalf of a Db
 * created directly (without a parent DbEnv), and the Db is
 * being closed.  We'll zero out the pointer to the DB_ENV,
 * since it is no longer valid, to prevent mistakes.
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1notify_1db_1close
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis)
{
	DB_ENV_JAVAINFO *dbenvinfo;

	set_private_dbobj(jnienv, name_DB_ENV, jthis, 0);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (dbenvinfo != NULL)
		dbjie_dealloc(dbenvinfo, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_feedback_1changed
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis,
   /*DbEnvFeedback*/ jobject jfeedback)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv) ||
	    !verify_non_null(jnienv, dbenvinfo))
		return;

	dbjie_set_feedback_object(dbenvinfo, jnienv, dbenv, jfeedback);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1init
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jobject /*DbErrcall*/ jerrcall,
   jint flags)
{
	int err;
	DB_ENV *dbenv;

	err = db_env_create(&dbenv, flags);
	if (verify_return(jnienv, err, 0))
		DbEnv_initialize(jnienv, dbenv, jthis, jerrcall, 0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1init_1using_1db
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jobject /*DbErrcall*/ jerrcall,
   /*Db*/ jobject jdb)
{
	DB_ENV *dbenv;
	DB *db;

	db = get_DB(jnienv, jdb);
	dbenv = db->dbenv;
	DbEnv_initialize(jnienv, dbenv, jthis, jerrcall, 0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_open
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jstring db_home,
   jint flags, jint mode)
{
	int err;
	DB_ENV *dbenv;
	LOCKED_STRING ls_home;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv) ||
	    !verify_non_null(jnienv, dbenvinfo))
		return;
	if (locked_string_get(&ls_home, jnienv, db_home) != 0)
		goto out;

	/* Java is assumed to be threaded. */
	flags |= DB_THREAD;

	err = dbenv->open(dbenv, ls_home.string, flags, mode);
	verify_return(jnienv, err, EXCEPTION_FILE_NOT_FOUND);
 out:
	locked_string_put(&ls_home, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_remove
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jstring db_home, jint flags)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;
	LOCKED_STRING ls_home;
	int err = 0;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;
	if (locked_string_get(&ls_home, jnienv, db_home) != 0)
		goto out;

	err = dbenv->remove(dbenv, ls_home.string, flags);
	set_private_dbobj(jnienv, name_DB_ENV, jthis, 0);

	verify_return(jnienv, err, 0);
 out:
	locked_string_put(&ls_home, jnienv);

	if (dbenvinfo != NULL)
		dbjie_dealloc(dbenvinfo, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1close
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;

	err = dbenv->close(dbenv, flags);
	set_private_dbobj(jnienv, name_DB_ENV, jthis, 0);

	if (dbenvinfo != NULL)
		dbjie_dealloc(dbenvinfo, jnienv);

	/* Throw an exception if the close failed. */
	verify_return(jnienv, err, 0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_dbremove
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbTxn*/ jobject jtxn,
   jstring name, jstring subdb, jint flags)
{
	LOCKED_STRING ls_name, ls_subdb;
	DB_ENV *dbenv;
	DB_TXN *txn;
	int err;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;
	txn = get_DB_TXN(jnienv, jtxn);
	if (locked_string_get(&ls_name, jnienv, name) != 0)
		return;
	if (locked_string_get(&ls_subdb, jnienv, subdb) != 0)
		goto err1;

	err = dbenv->dbremove(dbenv, txn, ls_name.string, ls_subdb.string,
	    flags);

	/* Throw an exception if the dbremove failed. */
	verify_return(jnienv, err, 0);

	locked_string_put(&ls_subdb, jnienv);
err1:	locked_string_put(&ls_name, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_dbrename
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbTxn*/ jobject jtxn,
   jstring name, jstring subdb, jstring newname, jint flags)
{
	LOCKED_STRING ls_name, ls_subdb, ls_newname;
	DB_ENV *dbenv;
	DB_TXN *txn;
	int err;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;
	txn = get_DB_TXN(jnienv, jtxn);
	if (locked_string_get(&ls_name, jnienv, name) != 0)
		return;
	if (locked_string_get(&ls_subdb, jnienv, subdb) != 0)
		goto err2;
	if (locked_string_get(&ls_newname, jnienv, newname) != 0)
		goto err1;

	err = dbenv->dbrename(dbenv, txn, ls_name.string, ls_subdb.string,
	    ls_newname.string, flags);

	/* Throw an exception if the dbrename failed. */
	verify_return(jnienv, err, 0);

	locked_string_put(&ls_newname, jnienv);
err1:	locked_string_put(&ls_subdb, jnienv);
err2:	locked_string_put(&ls_name, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_err
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint ecode, jstring msg)
{
	LOCKED_STRING ls_msg;
	DB_ENV *dbenv;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;

	if (locked_string_get(&ls_msg, jnienv, msg) != 0)
		goto out;

	dbenv->err(dbenv, ecode, "%s", ls_msg.string);
 out:
	locked_string_put(&ls_msg, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_errx
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jstring msg)
{
	LOCKED_STRING ls_msg;
	DB_ENV *dbenv;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;

	if (locked_string_get(&ls_msg, jnienv, msg) != 0)
		goto out;

	dbenv->errx(dbenv, "%s", ls_msg.string);
 out:
	locked_string_put(&ls_msg, jnienv);
}

/*static*/
JNIEXPORT jstring JNICALL Java_com_sleepycat_db_DbEnv_strerror
  (JNIEnv *jnienv, jclass jthis_class, jint ecode)
{
	const char *message;

	COMPQUIET(jthis_class, NULL);
	message = db_strerror(ecode);
	return (get_java_string(jnienv, message));
}

JAVADB_METHOD(DbEnv_set_1cachesize,
    (JAVADB_ARGS, jint gbytes, jint bytes, jint ncaches), DB_ENV,
    set_cachesize, (c_this, gbytes, bytes, ncaches))

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_set_1encrypt
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jstring jpasswd, jint flags)
{
	int err;
	DB_ENV *dbenv;
	LOCKED_STRING ls_passwd;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;
	if (locked_string_get(&ls_passwd, jnienv, jpasswd) != 0)
		goto out;

	err = dbenv->set_encrypt(dbenv, ls_passwd.string, flags);
	verify_return(jnienv, err, 0);

out:	locked_string_put(&ls_passwd, jnienv);
}

JAVADB_METHOD(DbEnv_set_1flags,
    (JAVADB_ARGS, jint flags, jboolean onoff), DB_ENV,
    set_flags, (c_this, flags, onoff ? 1 : 0))

JAVADB_METHOD(DbEnv_set_1mp_1mmapsize, (JAVADB_ARGS, jlong value), DB_ENV,
    set_mp_mmapsize, (c_this, (size_t)value))

JAVADB_METHOD(DbEnv_set_1tas_1spins, (JAVADB_ARGS, jint spins), DB_ENV,
    set_tas_spins, (c_this, (u_int32_t)spins))

JAVADB_METHOD(DbEnv_set_1timeout,
    (JAVADB_ARGS, jlong timeout, jint flags), DB_ENV,
    set_timeout, (c_this, (u_int32_t)timeout, flags))

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_set_1lk_1conflicts
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jobjectArray array)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;
	int err;
	jsize i, len;
	u_char *newarr;
	int bytesize;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv) ||
	    !verify_non_null(jnienv, dbenvinfo))
		return;

	len = (*jnienv)->GetArrayLength(jnienv, array);
	bytesize = sizeof(u_char) * len * len;

	if ((err = __os_malloc(dbenv, bytesize, &newarr)) != 0) {
		if (!verify_return(jnienv, err, 0))
			return;
	}

	for (i=0; i<len; i++) {
		jobject subArray =
			(*jnienv)->GetObjectArrayElement(jnienv, array, i);
		(*jnienv)->GetByteArrayRegion(jnienv, (jbyteArray)subArray,
					      0, len,
					      (jbyte *)&newarr[i*len]);
	}
	dbjie_set_conflict(dbenvinfo, newarr, bytesize);
	err = dbenv->set_lk_conflicts(dbenv, newarr, len);
	verify_return(jnienv, err, 0);
}

JNIEXPORT jint JNICALL
  Java_com_sleepycat_db_DbEnv_rep_1elect
  (JNIEnv *jnienv, /* DbEnv */ jobject jthis, jint nsites, jint pri,
   jint timeout)
{
	DB_ENV *dbenv;
	int err, id;

	if (!verify_non_null(jnienv, jthis))
		return (DB_EID_INVALID);

	dbenv = get_DB_ENV(jnienv, jthis);

	err = dbenv->rep_elect(dbenv, (int)nsites,
	    (int)pri, (u_int32_t)timeout, &id);
	verify_return(jnienv, err, 0);

	return ((jint)id);
}

JNIEXPORT jint JNICALL
  Java_com_sleepycat_db_DbEnv_rep_1process_1message
  (JNIEnv *jnienv, /* DbEnv */ jobject jthis, /* Dbt */ jobject control,
  /* Dbt */ jobject rec, /* RepProcessMessage */ jobject result)
{
	DB_ENV *dbenv;
	LOCKED_DBT cdbt, rdbt;
	int err, envid;

	if (!verify_non_null(jnienv, jthis) || !verify_non_null(jnienv, result))
		return (-1);

	dbenv = get_DB_ENV(jnienv, jthis);
	err = 0;

	/* The DBTs are always inputs. */
	if (locked_dbt_get(&cdbt, jnienv, dbenv, control, inOp) != 0)
		goto out2;
	if (locked_dbt_get(&rdbt, jnienv, dbenv, rec, inOp) != 0)
		goto out1;

	envid = (*jnienv)->GetIntField(jnienv,
	    result, fid_RepProcessMessage_envid);

	err = dbenv->rep_process_message(dbenv, &cdbt.javainfo->dbt,
	    &rdbt.javainfo->dbt, &envid);

	if (err == DB_REP_NEWMASTER)
		(*jnienv)->SetIntField(jnienv,
		    result, fid_RepProcessMessage_envid, envid);
	else if (!DB_RETOK_REPPMSG(err))
		verify_return(jnienv, err, 0);

out1:	locked_dbt_put(&rdbt, jnienv, dbenv);
out2:	locked_dbt_put(&cdbt, jnienv, dbenv);

	return (err);
}

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_DbEnv_rep_1start
  (JNIEnv *jnienv, /* DbEnv */ jobject jthis, /* Dbt */ jobject cookie,
   jint flags)
{
	DB_ENV *dbenv;
	DBT *dbtp;
	LOCKED_DBT ldbt;
	int err;

	if (!verify_non_null(jnienv, jthis))
		return;

	dbenv = get_DB_ENV(jnienv, jthis);

	/* The Dbt cookie may be null;  if so, pass in a NULL DBT. */
	if (cookie != NULL) {
		if (locked_dbt_get(&ldbt, jnienv, dbenv, cookie, inOp) != 0)
			goto out;
		dbtp = &ldbt.javainfo->dbt;
	} else
		dbtp = NULL;

	err = dbenv->rep_start(dbenv, dbtp, flags);
	verify_return(jnienv, err, 0);

out:	if (cookie != NULL)
		locked_dbt_put(&ldbt, jnienv, dbenv);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_rep_1stat
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	DB_REP_STAT *statp = NULL;
	jobject retval = NULL;
	jclass dbclass;

	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->rep_stat(dbenv, &statp, (u_int32_t)flags);
	if (verify_return(jnienv, err, 0)) {
		if ((dbclass = get_class(jnienv, name_DB_REP_STAT)) == NULL ||
		    (retval =
		      create_default_object(jnienv, name_DB_REP_STAT)) == NULL)
			goto err;	/* An exception has been posted. */

		__jv_fill_rep_stat(jnienv, dbclass, retval, statp);

err:		__os_ufree(dbenv, statp);
	}
	return (retval);
}

JNIEXPORT void JNICALL
Java_com_sleepycat_db_DbEnv_set_1rep_1limit
  (JNIEnv *jnienv, /* DbEnv */ jobject jthis, jint gbytes, jint bytes)
{
	DB_ENV *dbenv;
	int err;

	dbenv = get_DB_ENV(jnienv, jthis);

	if (verify_non_null(jnienv, dbenv)) {
		err = dbenv->set_rep_limit(dbenv,
		    (u_int32_t)gbytes, (u_int32_t)bytes);
		verify_return(jnienv, err, 0);
	}
}

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_DbEnv_rep_1transport_1changed
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint envid,
   /* DbRepTransport */ jobject jreptransport)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv) ||
	    !verify_non_null(jnienv, dbenvinfo) ||
	    !verify_non_null(jnienv, jreptransport))
		return;

	dbjie_set_rep_transport_object(dbenvinfo,
	    jnienv, dbenv, envid, jreptransport);
}

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_DbEnv_set_1rpc_1server
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbClient*/ jobject jclient,
   jstring jhost, jlong tsec, jlong ssec, jint flags)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	const char *host = (*jnienv)->GetStringUTFChars(jnienv, jhost, NULL);

	if (jclient != NULL) {
		report_exception(jnienv, "DbEnv.set_rpc_server client arg "
				 "must be null; reserved for future use",
				 EINVAL, 0);
		return;
	}
	if (verify_non_null(jnienv, dbenv)) {
		err = dbenv->set_rpc_server(dbenv, NULL, host,
					(long)tsec, (long)ssec, flags);

		/* Throw an exception if the call failed. */
		verify_return(jnienv, err, 0);
	}
}

JAVADB_METHOD(DbEnv_set_1shm_1key, (JAVADB_ARGS, jlong shm_key), DB_ENV,
    set_shm_key, (c_this, (long)shm_key))

JNIEXPORT void JNICALL
  Java_com_sleepycat_db_DbEnv__1set_1tx_1timestamp
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jlong seconds)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	time_t time = seconds;

	if (verify_non_null(jnienv, dbenv)) {
		err = dbenv->set_tx_timestamp(dbenv, &time);

		/* Throw an exception if the call failed. */
		verify_return(jnienv, err, 0);
	}
}

JAVADB_METHOD(DbEnv_set_1verbose,
    (JAVADB_ARGS, jint which, jboolean onoff), DB_ENV,
    set_verbose, (c_this, which, onoff ? 1 : 0))

/*static*/
JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1major
  (JNIEnv * jnienv, jclass this_class)
{
	COMPQUIET(jnienv, NULL);
	COMPQUIET(this_class, NULL);

	return (DB_VERSION_MAJOR);
}

/*static*/
JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1minor
  (JNIEnv * jnienv, jclass this_class)
{
	COMPQUIET(jnienv, NULL);
	COMPQUIET(this_class, NULL);

	return (DB_VERSION_MINOR);
}

/*static*/
JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1patch
  (JNIEnv * jnienv, jclass this_class)
{
	COMPQUIET(jnienv, NULL);
	COMPQUIET(this_class, NULL);

	return (DB_VERSION_PATCH);
}

/*static*/
JNIEXPORT jstring JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1string
  (JNIEnv *jnienv, jclass this_class)
{
	COMPQUIET(this_class, NULL);

	return ((*jnienv)->NewStringUTF(jnienv, DB_VERSION_STRING));
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_lock_1id
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis)
{
	int err;
	u_int32_t id;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

	if (!verify_non_null(jnienv, dbenv))
		return (-1);
	err = dbenv->lock_id(dbenv, &id);
	verify_return(jnienv, err, 0);
	return (id);
}

JAVADB_METHOD(DbEnv_lock_1id_1free, (JAVADB_ARGS, jint id), DB_ENV,
    lock_id_free, (c_this, id))

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_lock_1stat
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	DB_LOCK_STAT *statp = NULL;
	jobject retval = NULL;
	jclass dbclass;

	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->lock_stat(dbenv, &statp, (u_int32_t)flags);
	if (verify_return(jnienv, err, 0)) {
		if ((dbclass = get_class(jnienv, name_DB_LOCK_STAT)) == NULL ||
		    (retval =
		      create_default_object(jnienv, name_DB_LOCK_STAT)) == NULL)
			goto err;	/* An exception has been posted. */

		__jv_fill_lock_stat(jnienv, dbclass, retval, statp);

err:		__os_ufree(dbenv, statp);
	}
	return (retval);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_lock_1detect
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint atype, jint flags)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	int aborted;

	if (!verify_non_null(jnienv, dbenv))
		return (0);
	err = dbenv->lock_detect(dbenv, atype, flags, &aborted);
	verify_return(jnienv, err, 0);
	return (aborted);
}

JNIEXPORT /*DbLock*/ jobject JNICALL Java_com_sleepycat_db_DbEnv_lock_1get
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*u_int32_t*/ jint locker,
   jint flags, /*const Dbt*/ jobject obj, /*db_lockmode_t*/ jint lock_mode)
{
	int err;
	DB_ENV *dbenv;
	DB_LOCK *dblock;
	LOCKED_DBT lobj;
	/*DbLock*/ jobject retval;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	if ((err = __os_malloc(dbenv, sizeof(DB_LOCK), &dblock)) != 0)
		if (!verify_return(jnienv, err, 0))
			return (NULL);

	memset(dblock, 0, sizeof(DB_LOCK));
	err = 0;
	retval = NULL;
	if (locked_dbt_get(&lobj, jnienv, dbenv, obj, inOp) != 0)
		goto out;

	err = dbenv->lock_get(dbenv, locker, flags, &lobj.javainfo->dbt,
		       (db_lockmode_t)lock_mode, dblock);

	if (err == DB_LOCK_NOTGRANTED)
		report_notgranted_exception(jnienv,
					    "DbEnv.lock_get not granted",
					    DB_LOCK_GET, lock_mode, obj,
					    NULL, -1);
	else if (verify_return(jnienv, err, 0)) {
		retval = create_default_object(jnienv, name_DB_LOCK);
		set_private_dbobj(jnienv, name_DB_LOCK, retval, dblock);
	}

 out:
	locked_dbt_put(&lobj, jnienv, dbenv);
	return (retval);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_lock_1vec
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*u_int32_t*/ jint locker,
   jint flags, /*const Dbt*/ jobjectArray list, jint offset, jint count)
{
	DB_ENV *dbenv;
	DB_LOCKREQ *lockreq;
	DB_LOCKREQ *prereq;	/* preprocessed requests */
	DB_LOCKREQ *failedreq;
	DB_LOCK *lockp;
	LOCKED_DBT *locked_dbts;
	int err;
	int alloc_err;
	int i;
	size_t bytesize;
	size_t ldbtsize;
	jobject jlockreq;
	db_lockop_t op;
	jobject jobj;
	jobject jlock;
	int completed;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		goto out0;

	if ((*jnienv)->GetArrayLength(jnienv, list) < offset + count) {
		report_exception(jnienv,
				 "DbEnv.lock_vec array not large enough",
				 0, 0);
		goto out0;
	}

	bytesize = sizeof(DB_LOCKREQ) * count;
	if ((err = __os_malloc(dbenv, bytesize, &lockreq)) != 0) {
		verify_return(jnienv, err, 0);
		goto out0;
	}
	memset(lockreq, 0, bytesize);

	ldbtsize = sizeof(LOCKED_DBT) * count;
	if ((err = __os_malloc(dbenv, ldbtsize, &locked_dbts)) != 0) {
		verify_return(jnienv, err, 0);
		goto out1;
	}
	memset(lockreq, 0, ldbtsize);
	prereq = &lockreq[0];

	/* fill in the lockreq array */
	for (i = 0, prereq = &lockreq[0]; i < count; i++, prereq++) {
		jlockreq = (*jnienv)->GetObjectArrayElement(jnienv, list,
		    offset + i);
		if (jlockreq == NULL) {
			report_exception(jnienv,
					 "DbEnv.lock_vec list entry is null",
					 0, 0);
			goto out2;
		}
		op = (*jnienv)->GetIntField(jnienv, jlockreq,
		    fid_DbLockRequest_op);
		prereq->op = op;

		switch (op) {
		case DB_LOCK_GET_TIMEOUT:
			/* Needed: mode, timeout, obj.  Returned: lock. */
			prereq->op = (*jnienv)->GetIntField(jnienv, jlockreq,
			    fid_DbLockRequest_timeout);
			/* FALLTHROUGH */
		case DB_LOCK_GET:
			/* Needed: mode, obj.  Returned: lock. */
			prereq->mode = (*jnienv)->GetIntField(jnienv, jlockreq,
			    fid_DbLockRequest_mode);
			jobj = (*jnienv)->GetObjectField(jnienv, jlockreq,
			    fid_DbLockRequest_obj);
			if ((err = locked_dbt_get(&locked_dbts[i], jnienv,
			    dbenv, jobj, inOp)) != 0)
				goto out2;
			prereq->obj = &locked_dbts[i].javainfo->dbt;
			break;
		case DB_LOCK_PUT:
			/* Needed: lock.  Ignored: mode, obj. */
			jlock = (*jnienv)->GetObjectField(jnienv, jlockreq,
				fid_DbLockRequest_lock);
			if (!verify_non_null(jnienv, jlock))
				goto out2;
			lockp = get_DB_LOCK(jnienv, jlock);
			if (!verify_non_null(jnienv, lockp))
				goto out2;

			prereq->lock = *lockp;
			break;
		case DB_LOCK_PUT_ALL:
		case DB_LOCK_TIMEOUT:
			/* Needed: (none).  Ignored: lock, mode, obj. */
			break;
		case DB_LOCK_PUT_OBJ:
			/* Needed: obj.  Ignored: lock, mode. */
			jobj = (*jnienv)->GetObjectField(jnienv, jlockreq,
				fid_DbLockRequest_obj);
			if ((err = locked_dbt_get(&locked_dbts[i], jnienv,
					   dbenv, jobj, inOp)) != 0)
				goto out2;
			prereq->obj = &locked_dbts[i].javainfo->dbt;
			break;
		default:
			report_exception(jnienv,
					 "DbEnv.lock_vec bad op value",
					 0, 0);
			goto out2;
		}
	}

	err = dbenv->lock_vec(dbenv, locker, flags, lockreq, count, &failedreq);
	if (err == 0)
		completed = count;
	else
		completed = failedreq - lockreq;

	/* do post processing for any and all requests that completed */
	for (i = 0; i < completed; i++) {
		op = lockreq[i].op;
		if (op == DB_LOCK_PUT) {
			/*
			 * After a successful put, the DbLock can no longer
			 * be used, so we release the storage related to it.
			 */
			jlockreq = (*jnienv)->GetObjectArrayElement(jnienv,
			    list, i + offset);
			jlock = (*jnienv)->GetObjectField(jnienv, jlockreq,
				fid_DbLockRequest_lock);
			lockp = get_DB_LOCK(jnienv, jlock);
			__os_free(NULL, lockp);
			set_private_dbobj(jnienv, name_DB_LOCK, jlock, 0);
		}
		else if (op == DB_LOCK_GET) {
			/*
			 * Store the lock that was obtained.
			 * We need to create storage for it since
			 * the lockreq array only exists during this
			 * method call.
			 */
			alloc_err = __os_malloc(dbenv, sizeof(DB_LOCK), &lockp);
			if (!verify_return(jnienv, alloc_err, 0))
				goto out2;

			*lockp = lockreq[i].lock;

			jlockreq = (*jnienv)->GetObjectArrayElement(jnienv,
			    list, i + offset);
			jlock = create_default_object(jnienv, name_DB_LOCK);
			set_private_dbobj(jnienv, name_DB_LOCK, jlock, lockp);
			(*jnienv)->SetObjectField(jnienv, jlockreq,
						  fid_DbLockRequest_lock,
						  jlock);
		}
	}

	/* If one of the locks was not granted, build the exception now. */
	if (err == DB_LOCK_NOTGRANTED && i < count) {
		jlockreq = (*jnienv)->GetObjectArrayElement(jnienv,
							    list, i + offset);
		jobj = (*jnienv)->GetObjectField(jnienv, jlockreq,
						 fid_DbLockRequest_obj);
		jlock = (*jnienv)->GetObjectField(jnienv, jlockreq,
						  fid_DbLockRequest_lock);
		report_notgranted_exception(jnienv,
					    "DbEnv.lock_vec incomplete",
					    lockreq[i].op,
					    lockreq[i].mode,
					    jobj,
					    jlock,
					    i);
	}
	else
		verify_return(jnienv, err, 0);

 out2:
	/* Free the dbts that we have locked */
	for (i = 0 ; i < (prereq - lockreq); i++) {
		if ((op = lockreq[i].op) == DB_LOCK_GET ||
		    op == DB_LOCK_PUT_OBJ)
			locked_dbt_put(&locked_dbts[i], jnienv, dbenv);
	}
	__os_free(dbenv, locked_dbts);

 out1:
	__os_free(dbenv, lockreq);

 out0:
	return;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_lock_1put
  (JNIEnv *jnienv, jobject jthis, /*DbLock*/ jobject jlock)
{
	int err;
	DB_ENV *dbenv;
	DB_LOCK *dblock;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return;

	dblock = get_DB_LOCK(jnienv, jlock);
	if (!verify_non_null(jnienv, dblock))
		return;

	err = dbenv->lock_put(dbenv, dblock);
	if (verify_return(jnienv, err, 0)) {
		/*
		 * After a successful put, the DbLock can no longer
		 * be used, so we release the storage related to it
		 * (allocated in DbEnv.lock_get()).
		 */
		__os_free(NULL, dblock);

		set_private_dbobj(jnienv, name_DB_LOCK, jlock, 0);
	}
}

JNIEXPORT jobjectArray JNICALL Java_com_sleepycat_db_DbEnv_log_1archive
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err, len, i;
	char** ret;
	jclass stringClass;
	jobjectArray strarray;
	DB_ENV *dbenv;

	dbenv = get_DB_ENV(jnienv, jthis);
	strarray = NULL;
	if (!verify_non_null(jnienv, dbenv))
		return (0);
	err = dbenv->log_archive(dbenv, &ret, flags);
	if (!verify_return(jnienv, err, 0))
		return (0);

	if (ret != NULL) {
		len = 0;
		while (ret[len] != NULL)
			len++;
		stringClass = (*jnienv)->FindClass(jnienv, "java/lang/String");
		if ((strarray = (*jnienv)->NewObjectArray(jnienv,
		    len, stringClass, 0)) == NULL)
			goto out;
		for (i=0; i<len; i++) {
			jstring str = (*jnienv)->NewStringUTF(jnienv, ret[i]);
			(*jnienv)->SetObjectArrayElement(jnienv, strarray,
			     i, str);
		}
	}
out:	return (strarray);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_log_1compare
  (JNIEnv *jnienv, jclass jthis_class,
   /*DbLsn*/ jobject lsn0, /*DbLsn*/ jobject lsn1)
{
	DB_LSN *dblsn0;
	DB_LSN *dblsn1;

	COMPQUIET(jthis_class, NULL);
	dblsn0 = get_DB_LSN(jnienv, lsn0);
	dblsn1 = get_DB_LSN(jnienv, lsn1);

	return (log_compare(dblsn0, dblsn1));
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_log_1cursor
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	DB_LOGC *dblogc;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

	if (!verify_non_null(jnienv, dbenv))
		return (NULL);
	err = dbenv->log_cursor(dbenv, &dblogc, flags);
	verify_return(jnienv, err, 0);
	return (get_DbLogc(jnienv, dblogc));
}

JNIEXPORT jstring JNICALL Java_com_sleepycat_db_DbEnv_log_1file
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbLsn*/ jobject lsn)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	DB_LSN *dblsn = get_DB_LSN(jnienv, lsn);
	char filename[FILENAME_MAX+1] = "";

	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->log_file(dbenv, dblsn, filename, FILENAME_MAX);
	verify_return(jnienv, err, 0);
	filename[FILENAME_MAX] = '\0'; /* just to be sure */
	return (get_java_string(jnienv, filename));
}

JAVADB_METHOD(DbEnv_log_1flush,
    (JAVADB_ARGS, /*DbLsn*/ jobject lsn), DB_ENV,
    log_flush, (c_this, get_DB_LSN(jnienv, lsn)))

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_log_1put
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbLsn*/ jobject lsn,
   /*DbDbt*/ jobject data, jint flags)
{
	int err;
	DB_ENV *dbenv;
	DB_LSN *dblsn;
	LOCKED_DBT ldata;

	dbenv = get_DB_ENV(jnienv, jthis);
	dblsn = get_DB_LSN(jnienv, lsn);
	if (!verify_non_null(jnienv, dbenv))
		return;

	/* log_put's DB_LSN argument may not be NULL. */
	if (!verify_non_null(jnienv, dblsn))
		return;

	if (locked_dbt_get(&ldata, jnienv, dbenv, data, inOp) != 0)
		goto out;

	err = dbenv->log_put(dbenv, dblsn, &ldata.javainfo->dbt, flags);
	verify_return(jnienv, err, 0);
 out:
	locked_dbt_put(&ldata, jnienv, dbenv);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_log_1stat
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	DB_ENV *dbenv;
	DB_LOG_STAT *statp;
	jobject retval;
	jclass dbclass;

	retval = NULL;
	statp = NULL;
	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->log_stat(dbenv, &statp, (u_int32_t)flags);
	if (verify_return(jnienv, err, 0)) {
		if ((dbclass = get_class(jnienv, name_DB_LOG_STAT)) == NULL ||
		    (retval =
		       create_default_object(jnienv, name_DB_LOG_STAT)) == NULL)
			goto err;	/* An exception has been posted. */

		__jv_fill_log_stat(jnienv, dbclass, retval, statp);

err:		__os_ufree(dbenv, statp);
	}
	return (retval);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_memp_1stat
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	jclass dbclass;
	DB_ENV *dbenv;
	DB_MPOOL_STAT *statp;
	jobject retval;

	retval = NULL;
	statp = NULL;
	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->memp_stat(dbenv, &statp, 0, (u_int32_t)flags);
	if (verify_return(jnienv, err, 0)) {
		if ((dbclass = get_class(jnienv, name_DB_MPOOL_STAT)) == NULL ||
		    (retval =
		     create_default_object(jnienv, name_DB_MPOOL_STAT)) == NULL)
			goto err;	/* An exception has been posted. */

		__jv_fill_mpool_stat(jnienv, dbclass, retval, statp);

err:		__os_ufree(dbenv, statp);
	}
	return (retval);
}

JNIEXPORT jobjectArray JNICALL Java_com_sleepycat_db_DbEnv_memp_1fstat
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err, i, len;
	jclass fstat_class;
	DB_ENV *dbenv;
	DB_MPOOL_FSTAT **fstatp;
	jobjectArray retval;
	jfieldID filename_id;
	jstring jfilename;

	fstatp = NULL;
	retval = NULL;
	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->memp_stat(dbenv, 0, &fstatp, (u_int32_t)flags);
	if (verify_return(jnienv, err, 0)) {
		len = 0;
		while (fstatp[len] != NULL)
			len++;
		if ((fstat_class =
			get_class(jnienv, name_DB_MPOOL_FSTAT)) == NULL ||
		    (retval = (*jnienv)->NewObjectArray(jnienv, len,
			fstat_class, 0)) == NULL)
			goto err;
		for (i=0; i<len; i++) {
			jobject obj;
			if ((obj = create_default_object(jnienv,
			    name_DB_MPOOL_FSTAT)) == NULL)
				goto err;
			(*jnienv)->SetObjectArrayElement(jnienv, retval,
			    i, obj);

			/* Set the string field. */
			filename_id = (*jnienv)->GetFieldID(jnienv,
			    fstat_class, "file_name", string_signature);
			jfilename = get_java_string(jnienv,
			    fstatp[i]->file_name);
			(*jnienv)->SetObjectField(jnienv, obj,
			    filename_id, jfilename);
			set_int_field(jnienv, fstat_class, obj,
			    "st_pagesize", fstatp[i]->st_pagesize);
			set_int_field(jnienv, fstat_class, obj,
			    "st_cache_hit", fstatp[i]->st_cache_hit);
			set_int_field(jnienv, fstat_class, obj,
			    "st_cache_miss", fstatp[i]->st_cache_miss);
			set_int_field(jnienv, fstat_class, obj,
			    "st_map", fstatp[i]->st_map);
			set_int_field(jnienv, fstat_class, obj,
			    "st_page_create", fstatp[i]->st_page_create);
			set_int_field(jnienv, fstat_class, obj,
			    "st_page_in", fstatp[i]->st_page_in);
			set_int_field(jnienv, fstat_class, obj,
			    "st_page_out", fstatp[i]->st_page_out);
			__os_ufree(dbenv, fstatp[i]);
		}
err:		__os_ufree(dbenv, fstatp);
	}
	return (retval);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_memp_1trickle
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint pct)
{
	int err;
	DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
	int result = 0;

	if (verify_non_null(jnienv, dbenv)) {
		err = dbenv->memp_trickle(dbenv, pct, &result);
		verify_return(jnienv, err, 0);
	}
	return (result);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_txn_1begin
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbTxn*/ jobject pid, jint flags)
{
	int err;
	DB_TXN *dbpid, *result;
	DB_ENV *dbenv;

	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (0);

	dbpid = get_DB_TXN(jnienv, pid);
	result = 0;

	err = dbenv->txn_begin(dbenv, dbpid, &result, flags);
	if (!verify_return(jnienv, err, 0))
		return (0);
	return (get_DbTxn(jnienv, result));
}

JAVADB_METHOD(DbEnv_txn_1checkpoint,
    (JAVADB_ARGS, jint kbyte, jint min, jint flags), DB_ENV,
    txn_checkpoint, (c_this, kbyte, min, flags))

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_app_1dispatch_1changed
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, /*DbFeedback*/ jobject jappdispatch)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv) ||
	    !verify_non_null(jnienv, dbenvinfo))
		return;

	dbjie_set_app_dispatch_object(dbenvinfo, jnienv, dbenv, jappdispatch);
}

JNIEXPORT jobjectArray JNICALL Java_com_sleepycat_db_DbEnv_txn_1recover
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint count, jint flags)
{
	int err;
	DB_ENV *dbenv;
	DB_PREPLIST *preps;
	long retcount;
	int i;
	char signature[128];
	size_t bytesize;
	jobject retval;
	jobject obj;
	jobject txnobj;
	jbyteArray bytearr;
	jclass preplist_class;
	jfieldID txn_fieldid;
	jfieldID gid_fieldid;

	retval = NULL;
	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	/*
	 * We need to allocate some local storage for the
	 * returned preplist, and that requires us to do
	 * our own argument validation.
	 */
	if (count <= 0) {
		verify_return(jnienv, EINVAL, 0);
		goto out;
	}

	bytesize = sizeof(DB_PREPLIST) * count;
	if ((err = __os_malloc(dbenv, bytesize, &preps)) != 0) {
		verify_return(jnienv, err, 0);
		goto out;
	}

	err = dbenv->txn_recover(dbenv, preps, count, &retcount, flags);

	if (verify_return(jnienv, err, 0)) {
		if ((preplist_class =
		    get_class(jnienv, name_DB_PREPLIST)) == NULL ||
		    (retval = (*jnienv)->NewObjectArray(jnienv, retcount,
		    preplist_class, 0)) == NULL)
			goto err;

		(void)snprintf(signature, sizeof(signature),
		    "L%s%s;", DB_PACKAGE_NAME, name_DB_TXN);
		txn_fieldid = (*jnienv)->GetFieldID(jnienv, preplist_class,
						    "txn", signature);
		gid_fieldid = (*jnienv)->GetFieldID(jnienv, preplist_class,
						    "gid", "[B");

		for (i=0; i<retcount; i++) {
			/*
			 * First, make a blank DbPreplist object
			 * and set the array entry.
			 */
			if ((obj = create_default_object(jnienv,
			    name_DB_PREPLIST)) == NULL)
				goto err;
			(*jnienv)->SetObjectArrayElement(jnienv,
			    retval, i, obj);

			/* Set the txn field. */
			txnobj = get_DbTxn(jnienv, preps[i].txn);
			(*jnienv)->SetObjectField(jnienv,
			    obj, txn_fieldid, txnobj);

			/* Build the gid array and set the field. */
			if ((bytearr = (*jnienv)->NewByteArray(jnienv,
			    sizeof(preps[i].gid))) == NULL)
				goto err;
			(*jnienv)->SetByteArrayRegion(jnienv, bytearr, 0,
			    sizeof(preps[i].gid), (jbyte *)&preps[i].gid[0]);
			(*jnienv)->SetObjectField(jnienv, obj,
			    gid_fieldid, bytearr);
		}
	}
err:	__os_free(dbenv, preps);
out:	return (retval);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_txn_1stat
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jint flags)
{
	int err;
	DB_ENV *dbenv;
	DB_TXN_STAT *statp;
	jobject retval, obj;
	jclass dbclass, active_class;
	char active_signature[512];
	jfieldID arrid;
	jobjectArray actives;
	unsigned int i;

	retval = NULL;
	statp = NULL;
	dbenv = get_DB_ENV(jnienv, jthis);
	if (!verify_non_null(jnienv, dbenv))
		return (NULL);

	err = dbenv->txn_stat(dbenv, &statp, (u_int32_t)flags);
	if (verify_return(jnienv, err, 0)) {
		if ((dbclass = get_class(jnienv, name_DB_TXN_STAT)) == NULL ||
		    (retval =
		       create_default_object(jnienv, name_DB_TXN_STAT)) == NULL)
			goto err;

		/* Set the individual fields */
		__jv_fill_txn_stat(jnienv, dbclass, retval, statp);

		if ((active_class =
		    get_class(jnienv, name_DB_TXN_STAT_ACTIVE)) == NULL ||
		    (actives = (*jnienv)->NewObjectArray(jnienv,
		    statp->st_nactive, active_class, 0)) == NULL)
			goto err;

		/*
		 * Set the st_txnarray field.  This is a little more involved
		 * than other fields, since the type is an array, so none
		 * of our utility functions help.
		 */
		(void)snprintf(active_signature, sizeof(active_signature),
		    "[L%s%s;", DB_PACKAGE_NAME, name_DB_TXN_STAT_ACTIVE);

		arrid = (*jnienv)->GetFieldID(jnienv, dbclass, "st_txnarray",
					      active_signature);
		(*jnienv)->SetObjectField(jnienv, retval, arrid, actives);

		/* Now fill the in the elements of st_txnarray. */
		for (i=0; i<statp->st_nactive; i++) {
			obj = create_default_object(jnienv,
						name_DB_TXN_STAT_ACTIVE);
			(*jnienv)->SetObjectArrayElement(jnienv,
						actives, i, obj);

			set_int_field(jnienv, active_class, obj,
				      "txnid", statp->st_txnarray[i].txnid);
			set_int_field(jnienv, active_class, obj, "parentid",
				      statp->st_txnarray[i].parentid);
			set_lsn_field(jnienv, active_class, obj,
				      "lsn", statp->st_txnarray[i].lsn);
		}

err:		__os_ufree(dbenv, statp);
	}
	return (retval);
}

/* See discussion on errpfx, errcall in DB_ENV_JAVAINFO */
JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1set_1errcall
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jobject errcall)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);

	if (verify_non_null(jnienv, dbenv) &&
	    verify_non_null(jnienv, dbenvinfo)) {
		dbjie_set_errcall(dbenvinfo, jnienv, errcall);
	}
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1set_1errpfx
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jstring str)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *dbenvinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	dbenvinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);

	if (verify_non_null(jnienv, dbenv) &&
	    verify_non_null(jnienv, dbenvinfo)) {
		dbjie_set_errpfx(dbenvinfo, jnienv, str);
	}
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv__1finalize
    (JNIEnv *jnienv, /*DbEnv*/ jobject jthis,
     jobject /*DbErrcall*/ errcall, jstring errpfx)
{
	DB_ENV *dbenv;
	DB_ENV_JAVAINFO *envinfo;

	dbenv = get_DB_ENV(jnienv, jthis);
	envinfo = get_DB_ENV_JAVAINFO(jnienv, jthis);
	DB_ASSERT(envinfo != NULL);

	/* Note:  We detect and report unclosed DbEnvs. */
	if (dbenv != NULL && envinfo != NULL && !dbjie_is_dbopen(envinfo)) {

		/* If this error occurs, this object was never closed. */
		report_errcall(jnienv, errcall, errpfx,
			       "DbEnv.finalize: open DbEnv object destroyed");
	}

	/* Shouldn't see this object again, but just in case */
	set_private_dbobj(jnienv, name_DB_ENV, jthis, 0);
	set_private_info(jnienv, name_DB_ENV, jthis, 0);

	dbjie_destroy(envinfo, jnienv);
}
