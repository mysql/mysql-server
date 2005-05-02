/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_info.c,v 11.46 2002/08/29 14:22:23 margo Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db_int.h"
#include "java_util.h"

/****************************************************************
 *
 * Callback functions
 */

static int Db_assoc_callback(DB *db,
			     const DBT *key,
			     const DBT *data,
			     DBT *retval)
{
	DB_JAVAINFO *dbinfo;

	DB_ASSERT(db != NULL);
	dbinfo = (DB_JAVAINFO *)db->api_internal;
	return (dbji_call_assoc(dbinfo, db, dbinfo->jdbref,
	    key, data, retval));
}

static void Db_feedback_callback(DB *db, int opcode, int percent)
{
	DB_JAVAINFO *dbinfo;

	DB_ASSERT(db != NULL);
	dbinfo = (DB_JAVAINFO *)db->api_internal;
	dbji_call_feedback(dbinfo, db, dbinfo->jdbref, opcode, percent);
}

static int Db_append_recno_callback(DB *db, DBT *dbt, db_recno_t recno)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->api_internal;
	return (dbji_call_append_recno(dbinfo, db, dbinfo->jdbref, dbt, recno));
}

static int Db_bt_compare_callback(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->api_internal;
	return (dbji_call_bt_compare(dbinfo, db, dbinfo->jdbref, dbt1, dbt2));
}

static size_t Db_bt_prefix_callback(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->api_internal;
	return (dbji_call_bt_prefix(dbinfo, db, dbinfo->jdbref, dbt1, dbt2));
}

static int Db_dup_compare_callback(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->api_internal;
	return (dbji_call_dup_compare(dbinfo, db, dbinfo->jdbref, dbt1, dbt2));
}

static u_int32_t Db_h_hash_callback(DB *db, const void *data, u_int32_t len)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->api_internal;
	return (dbji_call_h_hash(dbinfo, db, dbinfo->jdbref, data, len));
}

static void DbEnv_feedback_callback(DB_ENV *dbenv, int opcode, int percent)
{
	DB_ENV_JAVAINFO *dbinfo;

	DB_ASSERT(dbenv != NULL);
	dbinfo = (DB_ENV_JAVAINFO *)dbenv->api2_internal;
	dbjie_call_feedback(dbinfo, dbenv, dbinfo->jenvref, opcode, percent);
}

static int DbEnv_rep_transport_callback(DB_ENV *dbenv,
					const DBT *control, const DBT *rec,
					int envid, u_int32_t flags)
{
	DB_ENV_JAVAINFO *dbinfo;

	dbinfo = (DB_ENV_JAVAINFO *)dbenv->api2_internal;
	return (dbjie_call_rep_transport(dbinfo, dbenv,
	    dbinfo->jenvref, control, rec, envid, (int)flags));
}

static int DbEnv_app_dispatch_callback(DB_ENV *dbenv, DBT *dbt,
				     DB_LSN *lsn, db_recops recops)
{
	DB_ENV_JAVAINFO *dbinfo;

	DB_ASSERT(dbenv != NULL);
	dbinfo = (DB_ENV_JAVAINFO *)dbenv->api2_internal;
	return (dbjie_call_app_dispatch(dbinfo, dbenv, dbinfo->jenvref, dbt,
	    lsn, recops));
}

/****************************************************************
 *
 * Implementation of class DBT_javainfo
 */
DBT_JAVAINFO *
dbjit_construct()
{
	DBT_JAVAINFO *dbjit;
	int err;

	/*XXX should return err*/
	if ((err = __os_malloc(NULL, sizeof(DBT_JAVAINFO), &dbjit)) != 0)
		return (NULL);

	memset(dbjit, 0, sizeof(DBT_JAVAINFO));
	return (dbjit);
}

void dbjit_destroy(DBT_JAVAINFO *dbjit)
{
	DB_ASSERT(!F_ISSET(dbjit, DBT_JAVAINFO_LOCKED));
	/* Extra paranoia */
	memset(dbjit, 0, sizeof(DBT_JAVAINFO));
	(void)__os_free(NULL, dbjit);
}

/****************************************************************
 *
 * Implementation of class DB_ENV_JAVAINFO
 */

/* create/initialize an object */
DB_ENV_JAVAINFO *
dbjie_construct(JNIEnv *jnienv,
		jobject jenv,
		jobject default_errcall,
		int is_dbopen)
{
	DB_ENV_JAVAINFO *dbjie;
	int err;

	/*XXX should return err*/
	if ((err = __os_malloc(NULL, sizeof(DB_ENV_JAVAINFO), &dbjie)) != 0)
		return (NULL);
	memset(dbjie, 0, sizeof(DB_ENV_JAVAINFO));
	dbjie->is_dbopen = is_dbopen;

	if ((*jnienv)->GetJavaVM(jnienv, &dbjie->javavm) != 0) {
		__os_free(NULL, dbjie);
		report_exception(jnienv, "cannot get Java VM", 0, 0);
		return (NULL);
	}

	/*
	 * The default error call just prints to the 'System.err'
	 * stream.  If the user does set_errcall to null, we'll
	 * want to have a reference to set it back to.
	 *
	 * Why do we have always set db_errcall to our own callback?
	 * Because it makes the interaction between setting the
	 * error prefix, error stream, and user's error callback
	 * that much easier.
	 */
	dbjie->default_errcall = NEW_GLOBAL_REF(jnienv, default_errcall);
	dbjie->errcall = NEW_GLOBAL_REF(jnienv, default_errcall);
	dbjie->jenvref = NEW_GLOBAL_REF(jnienv, jenv);
	return (dbjie);
}

/* release all objects held by this this one */
void dbjie_dealloc(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv)
{
	if (dbjie->feedback != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->feedback);
		dbjie->feedback = NULL;
	}
	if (dbjie->app_dispatch != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->app_dispatch);
		dbjie->app_dispatch = NULL;
	}
	if (dbjie->errcall != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->errcall);
		dbjie->errcall = NULL;
	}
	if (dbjie->default_errcall != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->default_errcall);
		dbjie->default_errcall = NULL;
	}
	if (dbjie->jenvref != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->jenvref);
		dbjie->jenvref = NULL;
	}

	if (dbjie->conflict != NULL) {
		__os_free(NULL, dbjie->conflict);
		dbjie->conflict = NULL;
		dbjie->conflict_size = 0;
	}
	if (dbjie->errpfx != NULL) {
		__os_free(NULL, dbjie->errpfx);
		dbjie->errpfx = NULL;
	}
}

/* free this object, releasing anything allocated on its behalf */
void dbjie_destroy(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv)
{
	dbjie_dealloc(dbjie, jnienv);

	/* Extra paranoia */
	memset(dbjie, 0, sizeof(DB_ENV_JAVAINFO));
	(void)__os_free(NULL, dbjie);
}

/*
 * Attach to the current thread that is running and
 * return that.  We use the java virtual machine
 * that we saved in the constructor.
 */
JNIEnv *
dbjie_get_jnienv(DB_ENV_JAVAINFO *dbjie)
{
	/*
	 * Note:
	 * Different versions of the JNI disagree on the signature
	 * for AttachCurrentThread.  The most recent documentation
	 * seems to say that (JNIEnv **) is correct, but newer
	 * JNIs seem to use (void **), oddly enough.
	 */
#ifdef JNI_VERSION_1_2
	void *attachret = 0;
#else
	JNIEnv *attachret = 0;
#endif

	/*
	 * This should always succeed, as we are called via
	 * some Java activity.  I think therefore I am (a thread).
	 */
	if ((*dbjie->javavm)->AttachCurrentThread(dbjie->javavm, &attachret, 0)
	    != 0)
		return (0);

	return ((JNIEnv *)attachret);
}

jstring
dbjie_get_errpfx(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv)
{
	return (get_java_string(jnienv, dbjie->errpfx));
}

void
dbjie_set_errcall(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv, jobject new_errcall)
{
	/*
	 * If the new_errcall is null, we'll set the error call
	 * to the default one.
	 */
	if (new_errcall == NULL)
		new_errcall = dbjie->default_errcall;

	DELETE_GLOBAL_REF(jnienv, dbjie->errcall);
	dbjie->errcall = NEW_GLOBAL_REF(jnienv, new_errcall);
}

void
dbjie_set_errpfx(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv, jstring errpfx)
{
	if (dbjie->errpfx != NULL)
		__os_free(NULL, dbjie->errpfx);

	if (errpfx)
		dbjie->errpfx = get_c_string(jnienv, errpfx);
	else
		dbjie->errpfx = NULL;
}

void
dbjie_set_conflict(DB_ENV_JAVAINFO *dbjie, u_char *newarr, size_t size)
{
	if (dbjie->conflict != NULL)
		(void)__os_free(NULL, dbjie->conflict);
	dbjie->conflict = newarr;
	dbjie->conflict_size = size;
}

void dbjie_set_feedback_object(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv,
			       DB_ENV *dbenv, jobject jfeedback)
{
	int err;

	if (dbjie->feedback != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->feedback);
	}
	if (jfeedback == NULL) {
		if ((err = dbenv->set_feedback(dbenv, NULL)) != 0)
			report_exception(jnienv, "set_feedback failed",
					 err, 0);
	}
	else {
		if ((err = dbenv->set_feedback(dbenv,
					       DbEnv_feedback_callback)) != 0)
			report_exception(jnienv, "set_feedback failed",
					 err, 0);
	}

	dbjie->feedback = NEW_GLOBAL_REF(jnienv, jfeedback);
}

void dbjie_call_feedback(DB_ENV_JAVAINFO *dbjie, DB_ENV *dbenv, jobject jenv,
			 int opcode, int percent)
{
	JNIEnv *jnienv;
	jclass feedback_class;
	jmethodID id;

	COMPQUIET(dbenv, NULL);
	jnienv = dbjie_get_jnienv(dbjie);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return;
	}

	if ((feedback_class =
	    get_class(jnienv, name_DbEnvFeedback)) == NULL) {
		fprintf(stderr, "Cannot find callback class %s\n",
		    name_DbEnvFeedback);
		return;	/* An exception has been posted. */
	}
	id = (*jnienv)->GetMethodID(jnienv, feedback_class,
				    "feedback",
				    "(Lcom/sleepycat/db/DbEnv;II)V");
	if (!id) {
		fprintf(stderr, "Cannot find callback method feedback\n");
		return;
	}

	(*jnienv)->CallVoidMethod(jnienv, dbjie->feedback, id,
				  jenv, (jint)opcode, (jint)percent);
}

void dbjie_set_rep_transport_object(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv,
				    DB_ENV *dbenv, int id, jobject jtransport)
{
	int err;

	if (dbjie->rep_transport != NULL)
		DELETE_GLOBAL_REF(jnienv, dbjie->rep_transport);

	err = dbenv->set_rep_transport(dbenv, id,
	    DbEnv_rep_transport_callback);
	verify_return(jnienv, err, 0);

	dbjie->rep_transport = NEW_GLOBAL_REF(jnienv, jtransport);
}

int dbjie_call_rep_transport(DB_ENV_JAVAINFO *dbjie, DB_ENV *dbenv,
			     jobject jenv, const DBT *control,
			     const DBT *rec, int flags, int envid)
{
	JNIEnv *jnienv;
	jclass rep_transport_class;
	jmethodID jid;
	jobject jcdbt, jrdbt;

	COMPQUIET(dbenv, NULL);
	jnienv = dbjie_get_jnienv(dbjie);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	if ((rep_transport_class =
	    get_class(jnienv, name_DbRepTransport)) == NULL) {
		fprintf(stderr, "Cannot find callback class %s\n",
		    name_DbRepTransport);
		return (0);	/* An exception has been posted. */
	}
	jid = (*jnienv)->GetMethodID(jnienv, rep_transport_class,
				     "send",
				     "(Lcom/sleepycat/db/DbEnv;"
				     "Lcom/sleepycat/db/Dbt;"
				     "Lcom/sleepycat/db/Dbt;II)I");

	if (!jid) {
		fprintf(stderr, "Cannot find callback method send\n");
		return (0);
	}

	jcdbt = get_const_Dbt(jnienv, control, NULL);
	jrdbt = get_const_Dbt(jnienv, rec, NULL);

	return (*jnienv)->CallIntMethod(jnienv, dbjie->rep_transport, jid, jenv,
					jcdbt, jrdbt, flags, envid);
}

void dbjie_set_app_dispatch_object(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv,
				 DB_ENV *dbenv, jobject japp_dispatch)
{
	int err;

	if (dbjie->app_dispatch != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->app_dispatch);
	}
	if (japp_dispatch == NULL) {
		if ((err = dbenv->set_app_dispatch(dbenv, NULL)) != 0)
			report_exception(jnienv, "set_app_dispatch failed",
					 err, 0);
	}
	else {
		if ((err = dbenv->set_app_dispatch(dbenv,
		    DbEnv_app_dispatch_callback)) != 0)
			report_exception(jnienv, "set_app_dispatch failed",
					 err, 0);
	}

	dbjie->app_dispatch = NEW_GLOBAL_REF(jnienv, japp_dispatch);
}

int dbjie_call_app_dispatch(DB_ENV_JAVAINFO *dbjie, DB_ENV *dbenv, jobject jenv,
			  DBT *dbt, DB_LSN *lsn, int recops)
{
	JNIEnv *jnienv;
	jclass app_dispatch_class;
	jmethodID id;
	jobject jdbt;
	jobject jlsn;

	COMPQUIET(dbenv, NULL);
	jnienv = dbjie_get_jnienv(dbjie);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	if ((app_dispatch_class =
	    get_class(jnienv, name_DbTxnRecover)) == NULL) {
		fprintf(stderr, "Cannot find callback class %s\n",
		    name_DbTxnRecover);
		return (0);	/* An exception has been posted. */
	}
	id = (*jnienv)->GetMethodID(jnienv, app_dispatch_class,
				    "app_dispatch",
				    "(Lcom/sleepycat/db/DbEnv;"
				    "Lcom/sleepycat/db/Dbt;"
				    "Lcom/sleepycat/db/DbLsn;"
				    "I)I");
	if (!id) {
		fprintf(stderr, "Cannot find callback method app_dispatch\n");
		return (0);
	}

	jdbt = get_Dbt(jnienv, dbt, NULL);

	if (lsn == NULL)
		jlsn = NULL;
	else
		jlsn = get_DbLsn(jnienv, *lsn);

	return (*jnienv)->CallIntMethod(jnienv, dbjie->app_dispatch, id, jenv,
					jdbt, jlsn, recops);
}

jobject dbjie_get_errcall(DB_ENV_JAVAINFO *dbjie)
{
	return (dbjie->errcall);
}

jint dbjie_is_dbopen(DB_ENV_JAVAINFO *dbjie)
{
	return (dbjie->is_dbopen);
}

/****************************************************************
 *
 * Implementation of class DB_JAVAINFO
 */

DB_JAVAINFO *dbji_construct(JNIEnv *jnienv, jobject jdb, jint flags)
{
	DB_JAVAINFO *dbji;
	int err;

	/*XXX should return err*/
	if ((err = __os_malloc(NULL, sizeof(DB_JAVAINFO), &dbji)) != 0)
		return (NULL);

	memset(dbji, 0, sizeof(DB_JAVAINFO));

	if ((*jnienv)->GetJavaVM(jnienv, &dbji->javavm) != 0) {
		report_exception(jnienv, "cannot get Java VM", 0, 0);
		(void)__os_free(NULL, dbji);
		return (NULL);
	}
	dbji->jdbref = NEW_GLOBAL_REF(jnienv, jdb);
	dbji->construct_flags = flags;
	return (dbji);
}

void
dbji_dealloc(DB_JAVAINFO *dbji, JNIEnv *jnienv)
{
	if (dbji->append_recno != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->append_recno);
		dbji->append_recno = NULL;
	}
	if (dbji->assoc != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->assoc);
		dbji->assoc = NULL;
	}
	if (dbji->bt_compare != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_compare);
		dbji->bt_compare = NULL;
	}
	if (dbji->bt_prefix != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_prefix);
		dbji->bt_prefix = NULL;
	}
	if (dbji->dup_compare != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->dup_compare);
		dbji->dup_compare = NULL;
	}
	if (dbji->feedback != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->feedback);
		dbji->feedback = NULL;
	}
	if (dbji->h_hash != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->h_hash);
		dbji->h_hash = NULL;
	}
	if (dbji->jdbref != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->jdbref);
		dbji->jdbref = NULL;
	}
}

void
dbji_destroy(DB_JAVAINFO *dbji, JNIEnv *jnienv)
{
	dbji_dealloc(dbji, jnienv);
	__os_free(NULL, dbji);
}

JNIEnv *dbji_get_jnienv(DB_JAVAINFO *dbji)
{
	/*
	 * Note:
	 * Different versions of the JNI disagree on the signature
	 * for AttachCurrentThread.  The most recent documentation
	 * seems to say that (JNIEnv **) is correct, but newer
	 * JNIs seem to use (void **), oddly enough.
	 */
#ifdef JNI_VERSION_1_2
	void *attachret = 0;
#else
	JNIEnv *attachret = 0;
#endif

	/*
	 * This should always succeed, as we are called via
	 * some Java activity.  I think therefore I am (a thread).
	 */
	if ((*dbji->javavm)->AttachCurrentThread(dbji->javavm, &attachret, 0)
	    != 0)
		return (0);

	return ((JNIEnv *)attachret);
}

jint dbji_get_flags(DB_JAVAINFO *dbji)
{
	return (dbji->construct_flags);
}

void dbji_set_feedback_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
			      DB *db, jobject jfeedback)
{
	jclass feedback_class;

	if (dbji->feedback_method_id == NULL) {
		if ((feedback_class =
		    get_class(jnienv, name_DbFeedback)) == NULL)
			return;	/* An exception has been posted. */
		dbji->feedback_method_id =
			(*jnienv)->GetMethodID(jnienv, feedback_class,
					       "feedback",
					       "(Lcom/sleepycat/db/Db;II)V");
		if (dbji->feedback_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->feedback != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->feedback);
	}
	if (jfeedback == NULL) {
		db->set_feedback(db, NULL);
	}
	else {
		db->set_feedback(db, Db_feedback_callback);
	}

	dbji->feedback = NEW_GLOBAL_REF(jnienv, jfeedback);

}

void dbji_call_feedback(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			int opcode, int percent)
{
	JNIEnv *jnienv;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return;
	}

	DB_ASSERT(dbji->feedback_method_id != NULL);
	(*jnienv)->CallVoidMethod(jnienv, dbji->feedback,
				  dbji->feedback_method_id,
				  jdb, (jint)opcode, (jint)percent);
}

void dbji_set_append_recno_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				  DB *db, jobject jcallback)
{
	jclass append_recno_class;

	if (dbji->append_recno_method_id == NULL) {
		if ((append_recno_class =
		    get_class(jnienv, name_DbAppendRecno)) == NULL)
			return;	/* An exception has been posted. */
		dbji->append_recno_method_id =
			(*jnienv)->GetMethodID(jnienv, append_recno_class,
					       "db_append_recno",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;I)V");
		if (dbji->append_recno_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->append_recno != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->append_recno);
	}
	if (jcallback == NULL) {
		db->set_append_recno(db, NULL);
	}
	else {
		db->set_append_recno(db, Db_append_recno_callback);
	}

	dbji->append_recno = NEW_GLOBAL_REF(jnienv, jcallback);
}

extern int dbji_call_append_recno(DB_JAVAINFO *dbji, DB *db, jobject jdb,
				  DBT *dbt, jint recno)
{
	JNIEnv *jnienv;
	jobject jresult;
	DBT_JAVAINFO *dbtji;
	LOCKED_DBT lresult;
	DB_ENV *dbenv;
	u_char *bytearray;
	int err;

	jnienv = dbji_get_jnienv(dbji);
	dbenv = db->dbenv;
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	jresult = get_Dbt(jnienv, dbt, &dbtji);

	DB_ASSERT(dbji->append_recno_method_id != NULL);
	(*jnienv)->CallVoidMethod(jnienv, dbji->append_recno,
				  dbji->append_recno_method_id,
				  jdb, jresult, recno);

	/*
	 * The underlying C API requires that an errno be returned
	 * on error.  Java users know nothing of errnos, so we
	 * allow them to throw exceptions instead.  We leave the
	 * exception in place and return DB_JAVA_CALLBACK to the C API
	 * that called us.  Eventually the DB->get will fail and
	 * when java prepares to throw an exception in
	 * report_exception(), this will be spotted as a special case,
	 * and the original exception will be preserved.
	 *
	 * Note: we have sometimes noticed strange behavior with
	 * exceptions under Linux 1.1.7 JVM.  (i.e. multiple calls
	 * to ExceptionOccurred() may report different results).
	 * Currently we don't know of any problems related to this
	 * in our code, but if it pops up in the future, users are
	 * encouranged to get a more recent JVM.
	 */
	if ((*jnienv)->ExceptionOccurred(jnienv) != NULL)
		return (DB_JAVA_CALLBACK);

	/*
	 * Now get the DBT back from java, because the user probably
	 * changed it.  We'll have to copy back the array too and let
	 * our caller free it.
	 *
	 * We expect that the user *has* changed the DBT (why else would
	 * they set up an append_recno callback?) so we don't
	 * worry about optimizing the unchanged case.
	 */
	if ((err = locked_dbt_get(&lresult, jnienv, dbenv, jresult, inOp)) != 0)
		return (err);

	memcpy(dbt, &lresult.javainfo->dbt, sizeof(DBT));
	if ((err = __os_malloc(dbenv, dbt->size, &bytearray)) != 0)
		goto out;

	memcpy(bytearray, dbt->data, dbt->size);
	dbt->data = bytearray;
	dbt->flags |= DB_DBT_APPMALLOC;

 out:
	locked_dbt_put(&lresult, jnienv, dbenv);
	return (err);
}

void dbji_set_assoc_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
			       DB *db, DB_TXN *txn, DB *second,
			       jobject jcallback, int flags)
{
	jclass assoc_class;
	int err;

	if (dbji->assoc_method_id == NULL) {
		if ((assoc_class =
		    get_class(jnienv, name_DbSecondaryKeyCreate)) == NULL)
			return;	/* An exception has been posted. */
		dbji->assoc_method_id =
			(*jnienv)->GetMethodID(jnienv, assoc_class,
					       "secondary_key_create",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->assoc_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->assoc != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->assoc);
		dbji->assoc = NULL;
	}

	if (jcallback == NULL)
		err = db->associate(db, txn, second, NULL, flags);
	else
		err = db->associate(db, txn, second, Db_assoc_callback, flags);

	if (verify_return(jnienv, err, 0))
		dbji->assoc = NEW_GLOBAL_REF(jnienv, jcallback);
}

extern int dbji_call_assoc(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			   const DBT *key, const DBT *value, DBT *result)
{
	JNIEnv *jnienv;
	jobject jresult;
	LOCKED_DBT lresult;
	DB_ENV *dbenv;
	int err;
	int sz;
	u_char *bytearray;
	jint retval;

	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	DB_ASSERT(dbji->assoc_method_id != NULL);

	dbenv = db->dbenv;
	jresult = create_default_object(jnienv, name_DBT);

	retval = (*jnienv)->CallIntMethod(jnienv, dbji->assoc,
					  dbji->assoc_method_id, jdb,
					  get_const_Dbt(jnienv, key, NULL),
					  get_const_Dbt(jnienv, value, NULL),
					  jresult);
	if (retval != 0)
		return (retval);

	if ((*jnienv)->ExceptionOccurred(jnienv) != NULL)
		return (DB_JAVA_CALLBACK);

	if ((err = locked_dbt_get(&lresult, jnienv, dbenv, jresult, inOp)) != 0)
		return (err);

	sz = lresult.javainfo->dbt.size;
	if (sz > 0) {
		bytearray = (u_char *)lresult.javainfo->dbt.data;

		/*
		 * If the byte array is in the range of one of the
		 * arrays passed to us we can use it directly.
		 * If not, we must create our own array and
		 * fill it in with the java array.  Since
		 * the java array may disappear and we don't
		 * want to keep its memory locked indefinitely,
		 * we cannot just pin the array.
		 *
		 * XXX consider pinning the array, and having
		 * some way for the C layer to notify the java
		 * layer when it can be unpinned.
		 */
		if ((bytearray < (u_char *)key->data ||
		     bytearray + sz > (u_char *)key->data + key->size) &&
		    (bytearray < (u_char *)value->data ||
		     bytearray + sz > (u_char *)value->data + value->size)) {

			result->flags |= DB_DBT_APPMALLOC;
			if ((err = __os_malloc(dbenv, sz, &bytearray)) != 0)
				goto out;
			memcpy(bytearray, lresult.javainfo->dbt.data, sz);
		}
		result->data = bytearray;
		result->size = sz;
	}
 out:
	locked_dbt_put(&lresult, jnienv, dbenv);
	return (err);
}

void dbji_set_bt_compare_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jcompare)
{
	jclass bt_compare_class;

	if (dbji->bt_compare_method_id == NULL) {
		if ((bt_compare_class =
		    get_class(jnienv, name_DbBtreeCompare)) == NULL)
			return;	/* An exception has been posted. */
		dbji->bt_compare_method_id =
			(*jnienv)->GetMethodID(jnienv, bt_compare_class,
					       "bt_compare",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->bt_compare_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->bt_compare != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_compare);
	}
	if (jcompare == NULL) {
		db->set_bt_compare(db, NULL);
	}
	else {
		db->set_bt_compare(db, Db_bt_compare_callback);
	}

	dbji->bt_compare = NEW_GLOBAL_REF(jnienv, jcompare);
}

int dbji_call_bt_compare(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			 const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jnienv;
	jobject jdbt1, jdbt2;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	jdbt1 = get_const_Dbt(jnienv, dbt1, NULL);
	jdbt2 = get_const_Dbt(jnienv, dbt2, NULL);

	DB_ASSERT(dbji->bt_compare_method_id != NULL);
	return (*jnienv)->CallIntMethod(jnienv, dbji->bt_compare,
					dbji->bt_compare_method_id,
					jdb, jdbt1, jdbt2);
}

void dbji_set_bt_prefix_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jprefix)
{
	jclass bt_prefix_class;

	if (dbji->bt_prefix_method_id == NULL) {
		if ((bt_prefix_class =
		    get_class(jnienv, name_DbBtreePrefix)) == NULL)
			return;	/* An exception has been posted. */
		dbji->bt_prefix_method_id =
			(*jnienv)->GetMethodID(jnienv, bt_prefix_class,
					       "bt_prefix",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->bt_prefix_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->bt_prefix != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_prefix);
	}
	if (jprefix == NULL) {
		db->set_bt_prefix(db, NULL);
	}
	else {
		db->set_bt_prefix(db, Db_bt_prefix_callback);
	}

	dbji->bt_prefix = NEW_GLOBAL_REF(jnienv, jprefix);
}

size_t dbji_call_bt_prefix(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			   const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jnienv;
	jobject jdbt1, jdbt2;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	jdbt1 = get_const_Dbt(jnienv, dbt1, NULL);
	jdbt2 = get_const_Dbt(jnienv, dbt2, NULL);

	DB_ASSERT(dbji->bt_prefix_method_id != NULL);
	return (size_t)(*jnienv)->CallIntMethod(jnienv, dbji->bt_prefix,
						dbji->bt_prefix_method_id,
						jdb, jdbt1, jdbt2);
}

void dbji_set_dup_compare_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jcompare)
{
	jclass dup_compare_class;

	if (dbji->dup_compare_method_id == NULL) {
		if ((dup_compare_class =
		    get_class(jnienv, name_DbDupCompare)) == NULL)
			return;	/* An exception has been posted. */
		dbji->dup_compare_method_id =
			(*jnienv)->GetMethodID(jnienv, dup_compare_class,
					       "dup_compare",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->dup_compare_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->dup_compare != NULL)
		DELETE_GLOBAL_REF(jnienv, dbji->dup_compare);

	if (jcompare == NULL)
		db->set_dup_compare(db, NULL);
	else
		db->set_dup_compare(db, Db_dup_compare_callback);

	dbji->dup_compare = NEW_GLOBAL_REF(jnienv, jcompare);
}

int dbji_call_dup_compare(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			 const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jnienv;
	jobject jdbt1, jdbt2;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	jdbt1 = get_const_Dbt(jnienv, dbt1, NULL);
	jdbt2 = get_const_Dbt(jnienv, dbt2, NULL);

	DB_ASSERT(dbji->dup_compare_method_id != NULL);
	return (*jnienv)->CallIntMethod(jnienv, dbji->dup_compare,
					dbji->dup_compare_method_id,
					jdb, jdbt1, jdbt2);
}

void dbji_set_h_hash_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jhash)
{
	jclass h_hash_class;

	if (dbji->h_hash_method_id == NULL) {
		if ((h_hash_class =
		    get_class(jnienv, name_DbHash)) == NULL)
			return;	/* An exception has been posted. */
		dbji->h_hash_method_id =
			(*jnienv)->GetMethodID(jnienv, h_hash_class,
					       "hash",
					       "(Lcom/sleepycat/db/Db;"
					       "[BI)I");
		if (dbji->h_hash_method_id == NULL) {
			/*
			 * XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->h_hash != NULL)
		DELETE_GLOBAL_REF(jnienv, dbji->h_hash);

	if (jhash == NULL)
		db->set_h_hash(db, NULL);
	else
		db->set_h_hash(db, Db_h_hash_callback);

	dbji->h_hash = NEW_GLOBAL_REF(jnienv, jhash);
}

int dbji_call_h_hash(DB_JAVAINFO *dbji, DB *db, jobject jdb,
		     const void *data, int len)
{
	JNIEnv *jnienv;
	jbyteArray jdata;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	DB_ASSERT(dbji->h_hash_method_id != NULL);

	if ((jdata = (*jnienv)->NewByteArray(jnienv, len)) == NULL)
		return (0);	/* An exception has been posted by the JVM */
	(*jnienv)->SetByteArrayRegion(jnienv, jdata, 0, len, (void *)data);
	return (*jnienv)->CallIntMethod(jnienv, dbji->h_hash,
					dbji->h_hash_method_id,
					jdb, jdata, len);
}
