/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_info.c,v 11.18 2000/10/28 13:09:39 dda Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "db_int.h"
#include "java_util.h"

/****************************************************************
 *
 * Callback functions
 *
 */

static void Db_feedback_callback(DB *db, int opcode, int percent)
{
	DB_JAVAINFO *dbinfo;

	DB_ASSERT(db != NULL);
	dbinfo = (DB_JAVAINFO *)db->cj_internal;
	dbji_call_feedback(dbinfo, db, dbinfo->jdbref_, opcode, percent);
}

static int Db_append_recno_callback(DB *db, DBT *dbt, db_recno_t recno)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->cj_internal;
	return (dbji_call_append_recno(dbinfo, db, dbinfo->jdbref_, dbt, recno));
}

static int Db_bt_compare_callback(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->cj_internal;
	return (dbji_call_bt_compare(dbinfo, db, dbinfo->jdbref_, dbt1, dbt2));
}

static size_t Db_bt_prefix_callback(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->cj_internal;
	return (dbji_call_bt_prefix(dbinfo, db, dbinfo->jdbref_, dbt1, dbt2));
}

static int Db_dup_compare_callback(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->cj_internal;
	return (dbji_call_dup_compare(dbinfo, db, dbinfo->jdbref_, dbt1, dbt2));
}

static u_int32_t Db_h_hash_callback(DB *db, const void *data, u_int32_t len)
{
	DB_JAVAINFO *dbinfo;

	dbinfo = (DB_JAVAINFO *)db->cj_internal;
	return (dbji_call_h_hash(dbinfo, db, dbinfo->jdbref_, data, len));
}

static void DbEnv_feedback_callback(DB_ENV *dbenv, int opcode, int percent)
{
	DB_ENV_JAVAINFO *dbinfo;

	DB_ASSERT(dbenv != NULL);
	dbinfo = (DB_ENV_JAVAINFO *)dbenv->cj_internal;
	dbjie_call_feedback(dbinfo, dbenv, dbinfo->jenvref_, opcode, percent);
}

static int DbEnv_recovery_init_callback(DB_ENV *dbenv)
{
	DB_ENV_JAVAINFO *dbinfo;

	dbinfo = (DB_ENV_JAVAINFO *)dbenv->cj_internal;
	return (dbjie_call_recovery_init(dbinfo, dbenv, dbinfo->jenvref_));
}

static int DbEnv_tx_recover_callback(DB_ENV *dbenv, DBT *dbt,
				     DB_LSN *lsn, db_recops recops)
{
	DB_ENV_JAVAINFO *dbinfo;

	DB_ASSERT(dbenv != NULL);
	dbinfo = (DB_ENV_JAVAINFO *)dbenv->cj_internal;
	return dbjie_call_tx_recover(dbinfo, dbenv, dbinfo->jenvref_, dbt,
				     lsn, recops);
}

/****************************************************************
 *
 * Implementation of class DBT_javainfo
 *
 */
DBT_JAVAINFO *
dbjit_construct()
{
	DBT_JAVAINFO *dbjit;

	dbjit = (DBT_JAVAINFO *)malloc(sizeof(DBT_JAVAINFO));
	memset(dbjit, 0, sizeof(DBT_JAVAINFO));
	return (dbjit);
}

void dbjit_destroy(DBT_JAVAINFO *dbjit)
{
	/* Sanity check:
	 * We cannot delete the global ref because we don't have a JNIEnv.
	 */
	if (dbjit->array_ != NULL) {
		fprintf(stderr, "object is not freed\n");
	}

	/* Extra paranoia */
	memset(dbjit, 0, sizeof(DB_JAVAINFO));
	free(dbjit);
}

void dbjit_release(DBT_JAVAINFO *dbjit, JNIEnv *jnienv)
{
	if (dbjit->array_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjit->array_);
		dbjit->array_ = NULL;
	}
}

/****************************************************************
 *
 * Implementation of class DB_ENV_JAVAINFO
 *
 */

/* create/initialize an object */
DB_ENV_JAVAINFO *
dbjie_construct(JNIEnv *jnienv,
		jobject default_errcall,
		int is_dbopen)
{
	DB_ENV_JAVAINFO *dbjie;

	dbjie = (DB_ENV_JAVAINFO *)malloc(sizeof(DB_ENV_JAVAINFO));
	memset(dbjie, 0, sizeof(DB_ENV_JAVAINFO));
	dbjie->is_dbopen_ = is_dbopen;

	if ((*jnienv)->GetJavaVM(jnienv, &dbjie->javavm_) != 0) {
		free(dbjie);
		report_exception(jnienv, "cannot get Java VM", 0, 0);
		return (NULL);
	}

	/* The default error call just prints to the 'System.err'
	 * stream.  If the user does set_errcall to null, we'll
	 * want to have a reference to set it back to.
	 *
	 * Why do we have always set db_errcall to our own callback?
	 * Because it makes the interaction between setting the
	 * error prefix, error stream, and user's error callback
	 * that much easier.
	 */
	dbjie->default_errcall_ = NEW_GLOBAL_REF(jnienv, default_errcall);
	dbjie->errcall_ = NEW_GLOBAL_REF(jnienv, default_errcall);
	return (dbjie);
}

/* release all objects held by this this one */
void dbjie_dealloc(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv)
{
	if (dbjie->recovery_init_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->recovery_init_);
		dbjie->recovery_init_ = NULL;
	}
	if (dbjie->feedback_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->feedback_);
		dbjie->feedback_ = NULL;
	}
	if (dbjie->tx_recover_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->tx_recover_);
		dbjie->tx_recover_ = NULL;
	}
	if (dbjie->errcall_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->errcall_);
		dbjie->errcall_ = NULL;
	}
	if (dbjie->default_errcall_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->default_errcall_);
		dbjie->default_errcall_ = NULL;
	}

	if (dbjie->conflict_ != NULL) {
		free(dbjie->conflict_);
		dbjie->conflict_ = NULL;
	}
	if (dbjie->errpfx_ != NULL) {
		free(dbjie->errpfx_);
		dbjie->errpfx_ = NULL;
	}
}

/* free this object, releasing anything allocated on its behalf */
void dbjie_destroy(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv)
{
	dbjie_dealloc(dbjie, jnienv);

	/* Extra paranoia */
	memset(dbjie, 0, sizeof(DB_ENV_JAVAINFO));
	free(dbjie);
}

/* Attach to the current thread that is running and
 * return that.  We use the java virtual machine
 * that we saved in the constructor.
 */
JNIEnv *
dbjie_get_jnienv(DB_ENV_JAVAINFO *dbjie)
{
	/* Note:
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

	/* This should always succeed, as we are called via
	 * some Java activity.  I think therefore I am (a thread).
	 */
	if ((*dbjie->javavm_)->AttachCurrentThread(dbjie->javavm_, &attachret, 0) != 0)
		return (0);

	return ((JNIEnv *)attachret);
}

jstring
dbjie_get_errpfx(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv)
{
	return (get_java_string(jnienv, dbjie->errpfx_));
}

void
dbjie_set_errcall(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv, jobject new_errcall)
{
	/* If the new_errcall is null, we'll set the error call
	 * to the default one.
	 */
	if (new_errcall == NULL)
		new_errcall = dbjie->default_errcall_;

	DELETE_GLOBAL_REF(jnienv, dbjie->errcall_);
	dbjie->errcall_ = NEW_GLOBAL_REF(jnienv, new_errcall);
}

void
dbjie_set_errpfx(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv, jstring errpfx)
{
	if (dbjie->errpfx_ != NULL)
		free(dbjie->errpfx_);

	if (errpfx)
		dbjie->errpfx_ = get_c_string(jnienv, errpfx);
	else
		dbjie->errpfx_ = NULL;
}

void
dbjie_set_conflict(DB_ENV_JAVAINFO *dbjie, unsigned char *newarr)
{
	if (dbjie->conflict_)
		free(dbjie->conflict_);
	dbjie->conflict_ = newarr;
}

void dbjie_set_feedback_object(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv,
			       DB_ENV *dbenv, jobject jfeedback)
{
	int err;

	if (dbjie->feedback_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->feedback_);
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

	dbjie->feedback_ = NEW_GLOBAL_REF(jnienv, jfeedback);
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

	feedback_class = get_class(jnienv, name_DbEnvFeedback);
	id = (*jnienv)->GetMethodID(jnienv, feedback_class,
				    "feedback",
				    "(Lcom/sleepycat/db/DbEnv;II)V");
	if (!id) {
		fprintf(stderr, "Cannot find callback class\n");
		return;
	}

	(*jnienv)->CallVoidMethod(jnienv, dbjie->feedback_, id,
				  jenv, (jint)opcode, (jint)percent);
}

void dbjie_set_recovery_init_object(DB_ENV_JAVAINFO *dbjie,
				    JNIEnv *jnienv, DB_ENV *dbenv,
				    jobject jrecovery_init)
{
	int err;

	if (dbjie->recovery_init_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->recovery_init_);
	}
	if (jrecovery_init == NULL) {
		if ((err = dbenv->set_recovery_init(dbenv, NULL)) != 0)
			report_exception(jnienv, "set_recovery_init failed",
					 err, 0);
	}
	else {
		if ((err = dbenv->set_recovery_init(dbenv,
					DbEnv_recovery_init_callback)) != 0)
			report_exception(jnienv, "set_recovery_init failed",
					 err, 0);
	}

	dbjie->recovery_init_ = NEW_GLOBAL_REF(jnienv, jrecovery_init);
}

int dbjie_call_recovery_init(DB_ENV_JAVAINFO *dbjie, DB_ENV *dbenv,
			     jobject jenv)
{
	JNIEnv *jnienv;
	jclass recovery_init_class;
	jmethodID id;

	COMPQUIET(dbenv, NULL);
	jnienv = dbjie_get_jnienv(dbjie);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (EINVAL);
	}

	recovery_init_class = get_class(jnienv, name_DbRecoveryInit);
	id = (*jnienv)->GetMethodID(jnienv, recovery_init_class,
				    "recovery_init",
				    "(Lcom/sleepycat/db/DbEnv;)V");
	if (!id) {
		fprintf(stderr, "Cannot find callback class\n");
		return (EINVAL);
	}
	return (*jnienv)->CallIntMethod(jnienv, dbjie->recovery_init_,
					id, jenv);
}

void dbjie_set_tx_recover_object(DB_ENV_JAVAINFO *dbjie, JNIEnv *jnienv,
				 DB_ENV *dbenv, jobject jtx_recover)
{
	int err;

	if (dbjie->tx_recover_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbjie->tx_recover_);
	}
	if (jtx_recover == NULL) {
		if ((err = dbenv->set_tx_recover(dbenv, NULL)) != 0)
			report_exception(jnienv, "set_tx_recover failed",
					 err, 0);
	}
	else {
		if ((err = dbenv->set_tx_recover(dbenv,
						 DbEnv_tx_recover_callback)) != 0)
			report_exception(jnienv, "set_tx_recover failed",
					 err, 0);
	}

	dbjie->tx_recover_ = NEW_GLOBAL_REF(jnienv, jtx_recover);
}

int dbjie_call_tx_recover(DB_ENV_JAVAINFO *dbjie, DB_ENV *dbenv, jobject jenv,
			  DBT *dbt, DB_LSN *lsn, int recops)
{
	JNIEnv *jnienv;
	jclass tx_recover_class;
	jmethodID id;
	jobject jdbt;
	jobject jlsn;

	COMPQUIET(dbenv, NULL);
	jnienv = dbjie_get_jnienv(dbjie);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	tx_recover_class = get_class(jnienv, name_DbTxnRecover);
	id = (*jnienv)->GetMethodID(jnienv, tx_recover_class,
				    "tx_recover",
				    "(Lcom/sleepycat/db/DbEnv;"
				    "Lcom/sleepycat/db/Dbt;"
				    "Lcom/sleepycat/db/DbLsn;"
				    "I)I");
	if (!id) {
		fprintf(stderr, "Cannot find callback class\n");
		return (0);
	}

	if (dbt == NULL)
		jdbt = NULL;
	else
		jdbt = get_Dbt(jnienv, dbt);

	if (lsn == NULL)
		jlsn = NULL;
	else
		jlsn = get_DbLsn(jnienv, *lsn);

	return (*jnienv)->CallIntMethod(jnienv, dbjie->tx_recover_, id, jenv,
					jdbt, jlsn, recops);
}

jobject dbjie_get_errcall(DB_ENV_JAVAINFO *dbjie)
{
	return (dbjie->errcall_);
}

int dbjie_is_dbopen(DB_ENV_JAVAINFO *dbjie)
{
	return (dbjie->is_dbopen_);
}

/****************************************************************
 *
 * Implementation of class DB_JAVAINFO
 *
 */

DB_JAVAINFO *dbji_construct(JNIEnv *jnienv, jint flags)
{
	DB_JAVAINFO *dbji;

	dbji = (DB_JAVAINFO *)malloc(sizeof(DB_JAVAINFO));
	memset(dbji, 0, sizeof(DB_JAVAINFO));

	if ((*jnienv)->GetJavaVM(jnienv, &dbji->javavm_) != 0) {
		report_exception(jnienv, "cannot get Java VM", 0, 0);
		free(dbji);
		return (NULL);
	}
	dbji->construct_flags_ = flags;
	return (dbji);
}

void
dbji_dealloc(DB_JAVAINFO *dbji, JNIEnv *jnienv)
{
	if (dbji->append_recno_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->append_recno_);
		dbji->append_recno_ = NULL;
	}
	if (dbji->bt_compare_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_compare_);
		dbji->bt_compare_ = NULL;
	}
	if (dbji->bt_prefix_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_prefix_);
		dbji->bt_prefix_ = NULL;
	}
	if (dbji->dup_compare_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->dup_compare_);
		dbji->dup_compare_ = NULL;
	}
	if (dbji->feedback_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->feedback_);
		dbji->feedback_ = NULL;
	}
	if (dbji->h_hash_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->h_hash_);
		dbji->h_hash_ = NULL;
	}
}

void
dbji_destroy(DB_JAVAINFO *dbji, JNIEnv *jnienv)
{
	dbji_dealloc(dbji, jnienv);
	free(dbji);
}

JNIEnv *dbji_get_jnienv(DB_JAVAINFO *dbji)
{
	/* Note:
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

	/* This should always succeed, as we are called via
	 * some Java activity.  I think therefore I am (a thread).
	 */
	if ((*dbji->javavm_)->AttachCurrentThread(dbji->javavm_, &attachret, 0) != 0)
		return (0);

	return ((JNIEnv *)attachret);
}

jint dbji_get_flags(DB_JAVAINFO *dbji)
{
	return (dbji->construct_flags_);
}

void dbji_set_feedback_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
			      DB *db, jobject jfeedback)
{
	jclass feedback_class;

	if (dbji->feedback_method_id_ == NULL) {
		feedback_class = get_class(jnienv, name_DbFeedback);
		dbji->feedback_method_id_ =
			(*jnienv)->GetMethodID(jnienv, feedback_class,
					       "feedback",
					       "(Lcom/sleepycat/db/Db;II)V");
		if (dbji->feedback_method_id_ != NULL) {
			/* XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->feedback_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->feedback_);
	}
	if (jfeedback == NULL) {
		db->set_feedback(db, NULL);
	}
	else {
		db->set_feedback(db, Db_feedback_callback);
	}

	dbji->feedback_ = NEW_GLOBAL_REF(jnienv, jfeedback);

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

	DB_ASSERT(dbji->feedback_method_id_ != NULL);
	(*jnienv)->CallVoidMethod(jnienv, dbji->feedback_,
				  dbji->feedback_method_id_,
				  jdb, (jint)opcode, (jint)percent);
}

void dbji_set_append_recno_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				  DB *db, jobject jcallback)
{
	jclass append_recno_class;

	if (dbji->append_recno_method_id_ == NULL) {
		append_recno_class = get_class(jnienv, name_DbAppendRecno);
		dbji->append_recno_method_id_ =
			(*jnienv)->GetMethodID(jnienv, append_recno_class,
					       "db_append_recno",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;I)V");
		if (dbji->append_recno_method_id_ == NULL) {
			/* XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->append_recno_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->append_recno_);
	}
	if (jcallback == NULL) {
		db->set_append_recno(db, NULL);
	}
	else {
		db->set_append_recno(db, Db_append_recno_callback);
	}

	dbji->append_recno_ = NEW_GLOBAL_REF(jnienv, jcallback);
}

extern int dbji_call_append_recno(DB_JAVAINFO *dbji, DB *db, jobject jdb,
				  DBT *dbt, jint recno)
{
	JNIEnv *jnienv;
	jobject jdbt;
	DBT_JAVAINFO *dbtji;
	jbyteArray arr;
	unsigned int arraylen;
	unsigned char *data;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	/* XXX
	 * We should have a pool of Dbt objects used for this purpose
	 * instead of creating new ones each time.  Because of
	 * multithreading, we may need an arbitrary number (more than two).
	 * We might also have a byte arrays that grow as needed,
	 * so we don't need to allocate those either.
	 *
	 * Note, we do not set the 'create_array_' flag as on other
	 * callbacks as we are creating the array here.
	 */
	jdbt = create_default_object(jnienv, name_DBT);
	dbtji = get_DBT_JAVAINFO(jnienv, jdbt);
	memcpy(&dbtji->dbt, dbt, sizeof(DBT));
	dbtji->dbt.data = NULL;
	arr = (*jnienv)->NewByteArray(jnienv, dbt->size);
	(*jnienv)->SetByteArrayRegion(jnienv, arr, 0, dbt->size,
				      (jbyte *)dbt->data);
	dbtji->array_ = (jbyteArray)NEW_GLOBAL_REF(jnienv, arr);

	DB_ASSERT(dbji->append_recno_method_id_ != NULL);
	(*jnienv)->CallVoidMethod(jnienv, dbji->append_recno_,
				  dbji->append_recno_method_id_,
				  jdb, jdbt, recno);

	/* The underlying C API requires that an errno be returned
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

	if (dbtji->array_ == NULL) {
		report_exception(jnienv, "Dbt.data is null", 0, 0);
		return (EFAULT);
	}

	arraylen = (*jnienv)->GetArrayLength(jnienv, dbtji->array_);
	if (dbtji->offset_ < 0 ) {
		report_exception(jnienv, "Dbt.offset illegal", 0, 0);
		return (EFAULT);
	}
	if (dbt->ulen + dbtji->offset_ > arraylen) {
		report_exception(jnienv,
		   "Dbt.ulen + Dbt.offset greater than array length", 0, 0);
		return (EFAULT);
	}

	data = (*jnienv)->GetByteArrayElements(jnienv, dbtji->array_,
					       (jboolean *)0);
	dbt->data = data + dbtji->offset_;
	return (0);
}

void dbji_set_bt_compare_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jcompare)
{
	jclass bt_compare_class;

	if (dbji->bt_compare_method_id_ == NULL) {
		bt_compare_class = get_class(jnienv, name_DbBtreeCompare);
		dbji->bt_compare_method_id_ =
			(*jnienv)->GetMethodID(jnienv, bt_compare_class,
					       "bt_compare",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->bt_compare_method_id_ == NULL) {
			/* XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->bt_compare_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_compare_);
	}
	if (jcompare == NULL) {
		db->set_bt_compare(db, NULL);
	}
	else {
		db->set_bt_compare(db, Db_bt_compare_callback);
	}

	dbji->bt_compare_ = NEW_GLOBAL_REF(jnienv, jcompare);
}

int dbji_call_bt_compare(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			 const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jnienv;
	jobject jdbt1, jdbt2;
	DBT_JAVAINFO *dbtji1, *dbtji2;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	/* XXX
	 * We should have a pool of Dbt objects used for this purpose
	 * instead of creating new ones each time.  Because of
	 * multithreading, we may need an arbitrary number (more than two).
	 * We might also have a byte arrays that grow as needed,
	 * so we don't need to allocate those either.
	 */
	jdbt1 = create_default_object(jnienv, name_DBT);
	jdbt2 = create_default_object(jnienv, name_DBT);
	dbtji1 = get_DBT_JAVAINFO(jnienv, jdbt1);
	memcpy(&dbtji1->dbt, dbt1, sizeof(DBT));
	dbtji1->create_array_ = 1;
	dbtji2 = get_DBT_JAVAINFO(jnienv, jdbt2);
	memcpy(&dbtji2->dbt, dbt2, sizeof(DBT));
	dbtji2->create_array_ = 1;

	DB_ASSERT(dbji->bt_compare_method_id_ != NULL);
	return (*jnienv)->CallIntMethod(jnienv, dbji->bt_compare_,
					dbji->bt_compare_method_id_,
					jdb, jdbt1, jdbt2);
}

void dbji_set_bt_prefix_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jprefix)
{
	jclass bt_prefix_class;

	if (dbji->bt_prefix_method_id_ == NULL) {
		bt_prefix_class = get_class(jnienv, name_DbBtreePrefix);
		dbji->bt_prefix_method_id_ =
			(*jnienv)->GetMethodID(jnienv, bt_prefix_class,
					       "bt_prefix",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->bt_prefix_method_id_ == NULL) {
			/* XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->bt_prefix_ != NULL) {
		DELETE_GLOBAL_REF(jnienv, dbji->bt_prefix_);
	}
	if (jprefix == NULL) {
		db->set_bt_prefix(db, NULL);
	}
	else {
		db->set_bt_prefix(db, Db_bt_prefix_callback);
	}

	dbji->bt_prefix_ = NEW_GLOBAL_REF(jnienv, jprefix);
}

size_t dbji_call_bt_prefix(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			   const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jnienv;
	jobject jdbt1, jdbt2;
	DBT_JAVAINFO *dbtji1, *dbtji2;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	/* XXX
	 * We should have a pool of Dbt objects used for this purpose
	 * instead of creating new ones each time.  Because of
	 * multithreading, we may need an arbitrary number (more than two).
	 * We might also have a byte arrays that grow as needed,
	 * so we don't need to allocate those either.
	 */
	jdbt1 = create_default_object(jnienv, name_DBT);
	jdbt2 = create_default_object(jnienv, name_DBT);
	dbtji1 = get_DBT_JAVAINFO(jnienv, jdbt1);
	memcpy(&dbtji1->dbt, dbt1, sizeof(DBT));
	dbtji1->create_array_ = 1;
	dbtji2 = get_DBT_JAVAINFO(jnienv, jdbt2);
	memcpy(&dbtji2->dbt, dbt2, sizeof(DBT));
	dbtji2->create_array_ = 1;

	DB_ASSERT(dbji->bt_prefix_method_id_ != NULL);
	return (size_t)(*jnienv)->CallIntMethod(jnienv, dbji->bt_prefix_,
						dbji->bt_prefix_method_id_,
						jdb, jdbt1, jdbt2);
}

void dbji_set_dup_compare_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jcompare)
{
	jclass dup_compare_class;

	if (dbji->dup_compare_method_id_ == NULL) {
		dup_compare_class = get_class(jnienv, name_DbDupCompare);
		dbji->dup_compare_method_id_ =
			(*jnienv)->GetMethodID(jnienv, dup_compare_class,
					       "dup_compare",
					       "(Lcom/sleepycat/db/Db;"
					       "Lcom/sleepycat/db/Dbt;"
					       "Lcom/sleepycat/db/Dbt;)I");
		if (dbji->dup_compare_method_id_ == NULL) {
			/* XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->dup_compare_ != NULL)
		DELETE_GLOBAL_REF(jnienv, dbji->dup_compare_);

	if (jcompare == NULL)
		db->set_dup_compare(db, NULL);
	else
		db->set_dup_compare(db, Db_dup_compare_callback);

	dbji->dup_compare_ = NEW_GLOBAL_REF(jnienv, jcompare);
}

int dbji_call_dup_compare(DB_JAVAINFO *dbji, DB *db, jobject jdb,
			 const DBT *dbt1, const DBT *dbt2)
{
	JNIEnv *jnienv;
	jobject jdbt1, jdbt2;
	DBT_JAVAINFO *dbtji1, *dbtji2;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	/* XXX
	 * We should have a pool of Dbt objects used for this purpose
	 * instead of creating new ones each time.  Because of
	 * multithreading, we may need an arbitrary number (more than two).
	 * We might also have a byte arrays that grow as needed,
	 * so we don't need to allocate those either.
	 */
	jdbt1 = create_default_object(jnienv, name_DBT);
	jdbt2 = create_default_object(jnienv, name_DBT);
	dbtji1 = get_DBT_JAVAINFO(jnienv, jdbt1);
	memcpy(&dbtji1->dbt, dbt1, sizeof(DBT));
	dbtji1->create_array_ = 1;
	dbtji2 = get_DBT_JAVAINFO(jnienv, jdbt2);
	memcpy(&dbtji2->dbt, dbt2, sizeof(DBT));
	dbtji2->create_array_ = 1;

	DB_ASSERT(dbji->dup_compare_method_id_ != NULL);
	return (*jnienv)->CallIntMethod(jnienv, dbji->dup_compare_,
					dbji->dup_compare_method_id_,
					jdb, jdbt1, jdbt2);
}

void dbji_set_h_hash_object(DB_JAVAINFO *dbji, JNIEnv *jnienv,
				DB *db, jobject jhash)
{
	jclass h_hash_class;

	if (dbji->h_hash_method_id_ == NULL) {
		h_hash_class = get_class(jnienv, name_DbHash);
		dbji->h_hash_method_id_ =
			(*jnienv)->GetMethodID(jnienv, h_hash_class,
					       "hash",
					       "(Lcom/sleepycat/db/Db;"
					       "[BI)I");
		if (dbji->h_hash_method_id_ == NULL) {
			/* XXX
			 * We should really have a better way
			 * to translate this to a Java exception class.
			 * In theory, it shouldn't happen.
			 */
			report_exception(jnienv, "Cannot find callback method",
					 EFAULT, 0);
			return;
		}
	}

	if (dbji->h_hash_ != NULL)
		DELETE_GLOBAL_REF(jnienv, dbji->h_hash_);

	if (jhash == NULL)
		db->set_h_hash(db, NULL);
	else
		db->set_h_hash(db, Db_h_hash_callback);

	dbji->h_hash_ = NEW_GLOBAL_REF(jnienv, jhash);
}

int dbji_call_h_hash(DB_JAVAINFO *dbji, DB *db, jobject jdb,
		     const void *data, int len)
{
	JNIEnv *jnienv;
	jbyteArray jarray;

	COMPQUIET(db, NULL);
	jnienv = dbji_get_jnienv(dbji);
	if (jnienv == NULL) {
		fprintf(stderr, "Cannot attach to current thread!\n");
		return (0);
	}

	DB_ASSERT(dbji->h_hash_method_id_ != NULL);

	jarray = (*jnienv)->NewByteArray(jnienv, len);
	(*jnienv)->SetByteArrayRegion(jnienv, jarray, 0, len, (void *)data);
	return (*jnienv)->CallIntMethod(jnienv, dbji->h_hash_,
					dbji->h_hash_method_id_,
					jdb, jarray, len);
}
