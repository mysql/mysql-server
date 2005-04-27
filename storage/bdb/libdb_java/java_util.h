/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: java_util.h,v 11.44 2002/08/29 14:22:24 margo Exp $
 */

#ifndef _JAVA_UTIL_H_
#define	_JAVA_UTIL_H_

#ifdef _MSC_VER

/*
 * These are level 4 warnings that are explicitly disabled.
 * With Visual C++, by default you do not see above level 3 unless
 * you use /W4.  But we like to compile with the highest level
 * warnings to catch other errors.
 *
 * 4201: nameless struct/union
 *       triggered by standard include file <winnt.h>
 *
 * 4244: '=' : convert from '__int64' to 'unsigned int', possible loss of data
 *       results from making size_t data members correspond to jlongs
 *
 * 4514: unreferenced inline function has been removed
 *       jni.h defines methods that are not called
 *
 * 4127: conditional expression is constant
 *       occurs because of arg in JAVADB_RW_ACCESS_STRING macro
 */
#pragma warning(disable: 4244 4201 4514 4127)

#endif

#include "db_config.h"
#include "db.h"
#include "db_int.h"
#include <jni.h>
#include "java_info.h"
#include "java_locked.h"
#include <string.h>             /* needed for memset */

#define	DB_PACKAGE_NAME "com/sleepycat/db/"

/* Union to convert longs to pointers (see {get,set}_private_dbobj). */
typedef union {
    jlong java_long;
    void *ptr;
} long_to_ptr;

/****************************************************************
 *
 * Utility functions and definitions used by "glue" functions.
 */

#define	NOT_IMPLEMENTED(str) \
	report_exception(jnienv, str /*concatenate*/ ": not implemented", 0)

/*
 * Get, delete a global reference.
 * Making this operation a function call allows for
 * easier tracking for debugging.  Global references
 * are mostly grabbed at 'open' and 'close' points,
 * so there shouldn't be a big performance hit.
 *
 * Macro-izing this makes it easier to add debugging code
 * to track unreleased references.
 */
#ifdef DBJAVA_DEBUG
#include <unistd.h>
static void wrdebug(const char *str)
{
	write(2, str, strlen(str));
	write(2, "\n", 1);
}

static jobject debug_new_global_ref(JNIEnv *jnienv, jobject obj, const char *s)
{
	wrdebug(s);
	return ((*jnienv)->NewGlobalRef(jnienv, obj));
}

static void debug_delete_global_ref(JNIEnv *jnienv, jobject obj, const char *s)
{
	wrdebug(s);
	(*jnienv)->DeleteGlobalRef(jnienv, obj);
}

#define	NEW_GLOBAL_REF(jnienv, obj)  \
	debug_new_global_ref(jnienv, obj, "+Ref: " #obj)
#define	DELETE_GLOBAL_REF(jnienv, obj) \
	debug_delete_global_ref(jnienv, obj, "-Ref: " #obj)
#else
#define	NEW_GLOBAL_REF(jnienv, obj)     (*jnienv)->NewGlobalRef(jnienv, obj)
#define	DELETE_GLOBAL_REF(jnienv, obj)  (*jnienv)->DeleteGlobalRef(jnienv, obj)
#define	wrdebug(x)
#endif

/*
 * Do any one time initialization, especially initializing any
 * unchanging methodIds, fieldIds, etc.
 */
void one_time_init(JNIEnv *jnienv);

/*
 * Get the current JNIEnv from the java VM.
 * If the jvm argument is null, uses the default
 * jvm stored during the first invocation.
 */
JNIEnv *get_jnienv(JavaVM *jvm);

/*
 * Get the private data from a Db* object that points back to a C DB_* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_dbobj(JNIEnv *jnienv, const char *classname,
		       jobject obj);

/*
 * Set the private data in a Db* object that points back to a C DB_* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_dbobj(JNIEnv *jnienv, const char *classname,
		      jobject obj, void *value);

/*
 * Get the private data in a Db/DbEnv object that holds additional 'side data'.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_info(JNIEnv *jnienv, const char *classname,
		       jobject obj);

/*
 * Set the private data in a Db/DbEnv object that holds additional 'side data'.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_info(JNIEnv *jnienv, const char *classname,
		      jobject obj, void *value);

/*
 * Given a non-qualified name (e.g. "foo"), get the class handle
 * for the fully qualified name (e.g. "com.sleepycat.db.foo")
 */
jclass get_class(JNIEnv *jnienv, const char *classname);

/*
 * Set an individual field in a Db* object.
 * The field must be a DB object type.
 */
void set_object_field(JNIEnv *jnienv, jclass class_of_this,
		      jobject jthis, const char *object_classname,
		      const char *name_of_field, jobject obj);

/*
 * Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_int_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, jint value);

/*
 * Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_long_field(JNIEnv *jnienv, jclass class_of_this,
			jobject jthis, const char *name_of_field, jlong value);

/*
 * Set an individual field in a Db* object.
 * The field must be an DbLsn type.
 */
void set_lsn_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, DB_LSN value);

/*
 * Values of flags for verify_return() and report_exception().
 * These indicate what sort of exceptions the method may throw
 * (in addition to DbException).
 */
static const u_int32_t EXCEPTION_FILE_NOT_FOUND = 0x0001; /*FileNotFound*/

/*
 * Report an exception back to the java side.
 */
void report_exception(JNIEnv *jnienv, const char *text,
		      int err, unsigned long expect_mask);

/*
 * Report an exception back to the java side, for the specific
 * case of DB_LOCK_NOTGRANTED, as more things are added to the
 * constructor of this type of exception.
 */
void report_notgranted_exception(JNIEnv *jnienv, const char *text,
				 db_lockop_t op, db_lockmode_t mode,
				 jobject jdbt, jobject jlock, int index);

/*
 * Create an exception object and return it.
 * The given class must have a constructor that has a
 * constructor with args (java.lang.String text, int errno);
 * DbException and its subclasses fit this bill.
 */
jobject create_exception(JNIEnv *jnienv, jstring text,
			 int err, jclass dbexcept);

/*
 * Report an error via the errcall mechanism.
 */
void report_errcall(JNIEnv *jnienv, jobject errcall,
		    jstring prefix, const char *message);

/*
 * If the object is null, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_non_null(JNIEnv *jnienv, void *obj);

/*
 * If the error code is non-zero, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_return(JNIEnv *jnienv, int err, unsigned long flags);

/*
 * Verify that there was no memory error due to undersized Dbt.
 * If there is report a DbMemoryException, with the Dbt attached
 * and return false (0), otherwise return true (1).
 */
int verify_dbt(JNIEnv *jnienv, int err, LOCKED_DBT *locked_dbt);

/*
 * Create an object of the given class, calling its default constructor.
 */
jobject create_default_object(JNIEnv *jnienv, const char *class_name);

/*
 * Create a Dbt object, , calling its default constructor.
 */
jobject create_dbt(JNIEnv *jnienv, const char *class_name);

/*
 * Convert an DB object to a Java encapsulation of that object.
 * Note: This implementation creates a new Java object on each call,
 * so it is generally useful when a new DB object has just been created.
 */
jobject convert_object(JNIEnv *jnienv, const char *class_name, void *dbobj);

/*
 * Create a copy of the java string using __os_malloc.
 * Caller must free it.
 */
char *get_c_string(JNIEnv *jnienv, jstring jstr);

/*
 * Create a java string from the given string
 */
jstring get_java_string(JNIEnv *jnienv, const char* string);

/*
 * Convert a java object to the various C pointers they represent.
 */
DB             *get_DB            (JNIEnv *jnienv, jobject obj);
DB_BTREE_STAT  *get_DB_BTREE_STAT (JNIEnv *jnienv, jobject obj);
DBC            *get_DBC           (JNIEnv *jnienv, jobject obj);
DB_ENV         *get_DB_ENV        (JNIEnv *jnienv, jobject obj);
DB_ENV_JAVAINFO *get_DB_ENV_JAVAINFO (JNIEnv *jnienv, jobject obj);
DB_HASH_STAT   *get_DB_HASH_STAT  (JNIEnv *jnienv, jobject obj);
DB_JAVAINFO    *get_DB_JAVAINFO   (JNIEnv *jnienv, jobject obj);
DB_LOCK        *get_DB_LOCK       (JNIEnv *jnienv, jobject obj);
DB_LOGC        *get_DB_LOGC       (JNIEnv *jnienv, jobject obj);
DB_LOG_STAT    *get_DB_LOG_STAT   (JNIEnv *jnienv, jobject obj);
DB_LSN         *get_DB_LSN        (JNIEnv *jnienv, jobject obj);
DB_MPOOL_FSTAT *get_DB_MPOOL_FSTAT(JNIEnv *jnienv, jobject obj);
DB_MPOOL_STAT  *get_DB_MPOOL_STAT (JNIEnv *jnienv, jobject obj);
DB_QUEUE_STAT  *get_DB_QUEUE_STAT (JNIEnv *jnienv, jobject obj);
DB_TXN         *get_DB_TXN        (JNIEnv *jnienv, jobject obj);
DB_TXN_STAT    *get_DB_TXN_STAT   (JNIEnv *jnienv, jobject obj);
DBT            *get_DBT           (JNIEnv *jnienv, jobject obj);
DBT_JAVAINFO   *get_DBT_JAVAINFO  (JNIEnv *jnienv, jobject obj);

/*
 * From a C object, create a Java object.
 */
jobject get_DbBtreeStat  (JNIEnv *jnienv, DB_BTREE_STAT *dbobj);
jobject get_Dbc          (JNIEnv *jnienv, DBC *dbobj);
jobject get_DbHashStat   (JNIEnv *jnienv, DB_HASH_STAT *dbobj);
jobject get_DbLogc       (JNIEnv *jnienv, DB_LOGC *dbobj);
jobject get_DbLogStat    (JNIEnv *jnienv, DB_LOG_STAT *dbobj);
jobject get_DbLsn        (JNIEnv *jnienv, DB_LSN dbobj);
jobject get_DbMpoolStat  (JNIEnv *jnienv, DB_MPOOL_STAT *dbobj);
jobject get_DbMpoolFStat (JNIEnv *jnienv, DB_MPOOL_FSTAT *dbobj);
jobject get_DbQueueStat  (JNIEnv *jnienv, DB_QUEUE_STAT *dbobj);
jobject get_const_Dbt    (JNIEnv *jnienv, const DBT *dbt, DBT_JAVAINFO **retp);
jobject get_Dbt          (JNIEnv *jnienv, DBT *dbt, DBT_JAVAINFO **retp);
jobject get_DbTxn        (JNIEnv *jnienv, DB_TXN *dbobj);
jobject get_DbTxnStat    (JNIEnv *jnienv, DB_TXN_STAT *dbobj);

/* The java names of DB classes */
extern const char * const name_DB;
extern const char * const name_DB_BTREE_STAT;
extern const char * const name_DBC;
extern const char * const name_DB_DEADLOCK_EX;
extern const char * const name_DB_ENV;
extern const char * const name_DB_EXCEPTION;
extern const char * const name_DB_HASH_STAT;
extern const char * const name_DB_LOCK;
extern const char * const name_DB_LOCK_STAT;
extern const char * const name_DB_LOGC;
extern const char * const name_DB_LOG_STAT;
extern const char * const name_DB_LSN;
extern const char * const name_DB_MEMORY_EX;
extern const char * const name_DB_MPOOL_FSTAT;
extern const char * const name_DB_MPOOL_STAT;
extern const char * const name_DB_LOCKNOTGRANTED_EX;
extern const char * const name_DB_PREPLIST;
extern const char * const name_DB_QUEUE_STAT;
extern const char * const name_DB_REP_STAT;
extern const char * const name_DB_RUNRECOVERY_EX;
extern const char * const name_DBT;
extern const char * const name_DB_TXN;
extern const char * const name_DB_TXN_STAT;
extern const char * const name_DB_TXN_STAT_ACTIVE;
extern const char * const name_DB_UTIL;
extern const char * const name_DbAppendRecno;
extern const char * const name_DbBtreeCompare;
extern const char * const name_DbBtreePrefix;
extern const char * const name_DbDupCompare;
extern const char * const name_DbEnvFeedback;
extern const char * const name_DbErrcall;
extern const char * const name_DbFeedback;
extern const char * const name_DbHash;
extern const char * const name_DbRecoveryInit;
extern const char * const name_DbRepTransport;
extern const char * const name_DbSecondaryKeyCreate;
extern const char * const name_DbTxnRecover;
extern const char * const name_RepElectResult;
extern const char * const name_RepProcessMessage;

extern const char * const string_signature;

extern jfieldID fid_Dbt_data;
extern jfieldID fid_Dbt_offset;
extern jfieldID fid_Dbt_size;
extern jfieldID fid_Dbt_ulen;
extern jfieldID fid_Dbt_dlen;
extern jfieldID fid_Dbt_doff;
extern jfieldID fid_Dbt_flags;
extern jfieldID fid_Dbt_must_create_data;
extern jfieldID fid_DbLockRequest_op;
extern jfieldID fid_DbLockRequest_mode;
extern jfieldID fid_DbLockRequest_timeout;
extern jfieldID fid_DbLockRequest_obj;
extern jfieldID fid_DbLockRequest_lock;
extern jfieldID fid_RepProcessMessage_envid;

#define JAVADB_ARGS JNIEnv *jnienv, jobject jthis

#define	JAVADB_GET_FLD(j_class, j_fieldtype, j_field, c_type, c_field)	      \
JNIEXPORT j_fieldtype JNICALL						      \
  Java_com_sleepycat_db_##j_class##_get_1##j_field			      \
  (JAVADB_ARGS)								      \
{									      \
	c_type *db= get_##c_type(jnienv, jthis);			      \
									      \
	if (verify_non_null(jnienv, db))				      \
		return (db->c_field);					      \
	return (0);							      \
}

#define	JAVADB_SET_FLD(j_class, j_fieldtype, j_field, c_type, c_field)	      \
JNIEXPORT void JNICALL							      \
  Java_com_sleepycat_db_##j_class##_set_1##j_field			      \
  (JAVADB_ARGS, j_fieldtype value)					      \
{									      \
	c_type *db= get_##c_type(jnienv, jthis);			      \
									      \
	if (verify_non_null(jnienv, db))				      \
		db->c_field = value;					      \
}

#define	JAVADB_METHOD(_meth, _argspec, c_type, c_meth, _args)		      \
JNIEXPORT void JNICALL Java_com_sleepycat_db_##_meth _argspec		      \
{									      \
	c_type *c_this = get_##c_type(jnienv, jthis);			      \
	int ret;							      \
									      \
	if (!verify_non_null(jnienv, c_this))				      \
		return;							      \
	ret = c_this->c_meth _args;					      \
	if (!DB_RETOK_STD(ret))						      \
		report_exception(jnienv, db_strerror(ret), ret, 0);	      \
}

#define	JAVADB_METHOD_INT(_meth, _argspec, c_type, c_meth, _args, _retok)     \
JNIEXPORT jint JNICALL Java_com_sleepycat_db_##_meth _argspec		      \
{									      \
	c_type *c_this = get_##c_type(jnienv, jthis);			      \
	int ret;							      \
									      \
	if (!verify_non_null(jnienv, c_this))				      \
		return (0);						      \
	ret = c_this->c_meth _args;					      \
	if (!_retok(ret))						      \
		report_exception(jnienv, db_strerror(ret), ret, 0);	      \
	return ((jint)ret);						      \
}

#define	JAVADB_SET_METH(j_class, j_type, j_fld, c_type, c_field)	      \
    JAVADB_METHOD(j_class##_set_1##j_fld, (JAVADB_ARGS, j_type val), c_type,  \
    set_##c_field, (c_this, val))

#define	JAVADB_SET_METH_STR(j_class, j_fld, c_type, c_field)		      \
    JAVADB_METHOD(j_class##_set_1##j_fld, (JAVADB_ARGS, jstring val), c_type, \
    set_##c_field, (c_this, (*jnienv)->GetStringUTFChars(jnienv, val, NULL)))


/*
 * These macros are used by code generated by the s_java script.
 */
#define JAVADB_STAT_INT(env, cl, jobj, statp, name) \
	set_int_field(jnienv, cl, jobj, #name, statp->name)

#define JAVADB_STAT_LSN(env, cl, jobj, statp, name) \
	set_lsn_field(jnienv, cl, jobj, #name, statp->name)

#define JAVADB_STAT_LONG(env, cl, jobj, statp, name) \
	set_long_field(jnienv, cl, jobj, #name, statp->name)

/*
 * We build the active list separately.
 */
#define JAVADB_STAT_ACTIVE(env, cl, jobj, statp, name) \
	do {} while(0)

#endif /* !_JAVA_UTIL_H_ */
