/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: java_info.h,v 11.35 2002/08/29 14:22:23 margo Exp $
 */

#ifndef _JAVA_INFO_H_
#define	_JAVA_INFO_H_

/*
 * "Info" classes for Java implementation of Berkeley DB API.
 * These classes hold extra information for which there is
 * no room or counterpart in the base classes used in the C API.
 * In the case of a DBT, the DBT_javainfo class is stored in the
 * 'private' variable of the java Dbt, and the DBT_javainfo is subclassed
 * from a DBT.  In the case of DB and DB_ENV, the appropriate
 * info objects are pointed to by the DB and DB_ENV objects.
 * This is convenient to implement callbacks.
 */

/****************************************************************
 *
 * Declaration of class DBT_javainfo
 *
 * A DBT_javainfo is created whenever a Dbt (java) object is created,
 * and a pointer to it is stored in its private info storage.
 * It is subclassed from DBT, because we must retain some extra
 * information in it while it is in use.  In particular, when
 * a java array is associated with it, we need to keep a Globally
 * Locked reference to it so it is not GC'd.  This reference is
 * destroyed when the Dbt is GC'd.
 */
typedef struct _dbt_javainfo
{
	DBT dbt;
	DB *db;			/* associated DB */
	jobject dbtref;		/* the java Dbt object */
	jbyteArray array;	/* the java array object -
				   this is only valid during the API call */
	int offset;		/* offset into the Java array */

#define	DBT_JAVAINFO_LOCKED	0x01	/* a LOCKED_DBT has been created */
	u_int32_t flags;
}
DBT_JAVAINFO;	/* used with all 'dbtji' functions */

/* create/initialize a DBT_JAVAINFO object */
extern DBT_JAVAINFO *dbjit_construct();

/* free this DBT_JAVAINFO, releasing anything allocated on its behalf */
extern void dbjit_destroy(DBT_JAVAINFO *dbjit);

/****************************************************************
 *
 * Declaration of class DB_ENV_JAVAINFO
 *
 * A DB_ENV_JAVAINFO is allocated and stuffed into the cj_internal
 * and the db_errpfx for every DB_ENV created.  It holds a
 * little extra info that is needed to support callbacks.
 *
 * There's a bit of trickery here, because we have built this
 * above a layer that has a C function callback that gets
 * invoked when an error occurs.  One of the C callback's arguments
 * is the prefix from the DB_ENV, but since we stuffed a pointer
 * to our own DB_ENV_JAVAINFO into the prefix, we get that object as an
 * argument to the C callback.  Thus, the C callback can have
 * access to much more than just the prefix, and it needs that
 * to call back into the Java enviroment.
 *
 * The DB_ENV_JAVAINFO object holds a copy of the Java Virtual Machine,
 * which is needed to attach to the current running thread
 * whenever we need to make a callback.  (This is more reliable
 * than our previous approach, which was to save the thread
 * that created the DbEnv).  It also has the Java callback object,
 * as well as a 'default' callback object that is used when the
 * caller sets the callback to null.  It also has the original
 * error prefix, since we overwrote the one in the DB_ENV.
 * There are also fields that are unrelated to the handling
 * of callbacks, but are convenient to attach to a DB_ENV.
 *
 * Note: We assume that the Java layer is the only one
 * fiddling with the contents of db_errpfx, db_errcall, cj_internal
 * for a DB_ENV that was created via Java.  Since the Java layer should
 * have the only pointer to such a DB_ENV, this should be true.
 */
typedef struct _db_env_javainfo
{
	JavaVM *javavm;
	int is_dbopen;
	char *errpfx;
	jobject jenvref;	/* global reference */
	jobject default_errcall; /* global reference */
	jobject errcall;	/* global reference */
	jobject feedback;	/* global reference */
	jobject rep_transport;	/* global reference */
	jobject app_dispatch;	/* global reference */
	jobject recovery_init;	/* global reference */
	u_char *conflict;
	size_t conflict_size;
	jint construct_flags;
}
DB_ENV_JAVAINFO;	/* used with all 'dbjie' functions */

/* create/initialize an object */
extern DB_ENV_JAVAINFO *dbjie_construct(JNIEnv *jnienv,
		       jobject jenv,
		       jobject default_errcall,
		       int is_dbopen);

/* release all objects held by this this one */
extern void dbjie_dealloc(DB_ENV_JAVAINFO *, JNIEnv *jnienv);

/* free this object, releasing anything allocated on its behalf */
extern void dbjie_destroy(DB_ENV_JAVAINFO *, JNIEnv *jnienv);

/* This gets the environment for the current thread */
extern JNIEnv *dbjie_get_jnienv(DB_ENV_JAVAINFO *);

extern void dbjie_set_errpfx(DB_ENV_JAVAINFO *, JNIEnv *jnienv,
			     jstring errpfx);
extern jstring dbjie_get_errpfx(DB_ENV_JAVAINFO *, JNIEnv *jnienv);
extern void dbjie_set_errcall(DB_ENV_JAVAINFO *, JNIEnv *jnienv,
			      jobject new_errcall);
extern void dbjie_set_conflict(DB_ENV_JAVAINFO *, u_char *v, size_t sz);
extern void dbjie_set_feedback_object(DB_ENV_JAVAINFO *, JNIEnv *jnienv,
				      DB_ENV *dbenv, jobject value);
extern void dbjie_call_feedback(DB_ENV_JAVAINFO *, DB_ENV *dbenv, jobject jenv,
				int opcode, int percent);
extern void dbjie_set_recovery_init_object(DB_ENV_JAVAINFO *, JNIEnv *jnienv,
					   DB_ENV *dbenv, jobject value);
extern int dbjie_call_recovery_init(DB_ENV_JAVAINFO *, DB_ENV *dbenv,
				    jobject jenv);
extern void dbjie_set_rep_transport_object(DB_ENV_JAVAINFO *, JNIEnv *jnienv,
					   DB_ENV *dbenv, int id, jobject obj);
extern int dbjie_call_rep_transport(DB_ENV_JAVAINFO *, DB_ENV *dbenv,
				    jobject jenv, const DBT *control,
				    const DBT *rec, int envid, int flags);
extern void dbjie_set_app_dispatch_object(DB_ENV_JAVAINFO *, JNIEnv *jnienv,
					DB_ENV *dbenv, jobject value);
extern int dbjie_call_app_dispatch(DB_ENV_JAVAINFO *,
				 DB_ENV *dbenv, jobject jenv,
				 DBT *dbt, DB_LSN *lsn, int recops);
extern jobject dbjie_get_errcall(DB_ENV_JAVAINFO *) ;
extern jint dbjie_is_dbopen(DB_ENV_JAVAINFO *);

/****************************************************************
 *
 * Declaration of class DB_JAVAINFO
 *
 * A DB_JAVAINFO is allocated and stuffed into the cj_internal field
 * for every DB created.  It holds a little extra info that is needed
 * to support callbacks.
 *
 * Note: We assume that the Java layer is the only one
 * fiddling with the contents of cj_internal
 * for a DB that was created via Java.  Since the Java layer should
 * have the only pointer to such a DB, this should be true.
 */
typedef struct _db_javainfo
{
	JavaVM *javavm;
	jobject jdbref;		/* global reference */
	jobject append_recno;	/* global reference */
	jobject assoc;		/* global reference */
	jobject bt_compare;	/* global reference */
	jobject bt_prefix;	/* global reference */
	jobject dup_compare;	/* global reference */
	jobject feedback;	/* global reference */
	jobject h_hash;		/* global reference */
	jmethodID append_recno_method_id;
	jmethodID assoc_method_id;
	jmethodID bt_compare_method_id;
	jmethodID bt_prefix_method_id;
	jmethodID dup_compare_method_id;
	jmethodID feedback_method_id;
	jmethodID h_hash_method_id;
	jint construct_flags;
} DB_JAVAINFO;

/* create/initialize an object */
extern DB_JAVAINFO *dbji_construct(JNIEnv *jnienv, jobject jdb, jint flags);

/* release all objects held by this this one */
extern void dbji_dealloc(DB_JAVAINFO *, JNIEnv *jnienv);

/* free this object, releasing anything allocated on its behalf */
extern void dbji_destroy(DB_JAVAINFO *, JNIEnv *jnienv);

/* This gets the environment for the current thread */
extern JNIEnv *dbji_get_jnienv();
extern jint dbji_get_flags();

extern void dbji_set_feedback_object(DB_JAVAINFO *, JNIEnv *jnienv, DB *db, jobject value);
extern void dbji_call_feedback(DB_JAVAINFO *, DB *db, jobject jdb,
			       int opcode, int percent);

extern void dbji_set_append_recno_object(DB_JAVAINFO *, JNIEnv *jnienv, DB *db, jobject value);
extern int dbji_call_append_recno(DB_JAVAINFO *, DB *db, jobject jdb,
				  DBT *dbt, jint recno);
extern void dbji_set_assoc_object(DB_JAVAINFO *, JNIEnv *jnienv,
				  DB *db, DB_TXN *txn, DB *second,
				  jobject value, int flags);
extern int dbji_call_assoc(DB_JAVAINFO *, DB *db, jobject jdb,
			   const DBT *key, const DBT* data, DBT *result);
extern void dbji_set_bt_compare_object(DB_JAVAINFO *, JNIEnv *jnienv, DB *db, jobject value);
extern int dbji_call_bt_compare(DB_JAVAINFO *, DB *db, jobject jdb,
				const DBT *dbt1, const DBT *dbt2);
extern void dbji_set_bt_prefix_object(DB_JAVAINFO *, JNIEnv *jnienv, DB *db, jobject value);
extern size_t dbji_call_bt_prefix(DB_JAVAINFO *, DB *db, jobject jdb,
				  const DBT *dbt1, const DBT *dbt2);
extern void dbji_set_dup_compare_object(DB_JAVAINFO *, JNIEnv *jnienv, DB *db, jobject value);
extern int dbji_call_dup_compare(DB_JAVAINFO *, DB *db, jobject jdb,
				 const DBT *dbt1, const DBT *dbt2);
extern void dbji_set_h_hash_object(DB_JAVAINFO *, JNIEnv *jnienv, DB *db, jobject value);
extern int dbji_call_h_hash(DB_JAVAINFO *, DB *db, jobject jdb,
			    const void *data, int len);

#endif /* !_JAVA_INFO_H_ */
