/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: java_util.h,v 11.22 2001/01/11 18:19:53 bostic Exp $
 */

#ifndef _JAVA_UTIL_H_
#define	_JAVA_UTIL_H_

#ifdef _MSC_VER

/* These are level 4 warnings that are explicitly disabled.
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
#include "java_info.h"
#include "java_locked.h"
#include <jni.h>
#include <string.h>             /* needed for memset */

#define	DB_PACKAGE_NAME "com/sleepycat/db/"

/* Union to convert longs to pointers (see {get,set}_private_dbobj).
 */
typedef union {
    jlong java_long;
    void *ptr;
} long_to_ptr;

/****************************************************************
 *
 * Utility functions and definitions used by "glue" functions.
 *
 */

#define	NOT_IMPLEMENTED(str) \
	report_exception(jnienv, str /*concatenate*/ ": not implemented", 0)

/* Get, delete a global reference.
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
	return (*jnienv)->NewGlobalRef(jnienv, obj);
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

/* Get the private data from a Db* object that points back to a C DB_* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_dbobj(JNIEnv *jnienv, const char *classname,
		       jobject obj);

/* Set the private data in a Db* object that points back to a C DB_* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_dbobj(JNIEnv *jnienv, const char *classname,
		      jobject obj, void *value);

/* Get the private data in a Db/DbEnv object that holds additional 'side data'.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_info(JNIEnv *jnienv, const char *classname,
		       jobject obj);

/* Set the private data in a Db/DbEnv object that holds additional 'side data'.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_info(JNIEnv *jnienv, const char *classname,
		      jobject obj, void *value);

/*
 * Given a non-qualified name (e.g. "foo"), get the class handl
 * for the fully qualified name (e.g. "com.sleepycat.db.foo")
 */
jclass get_class(JNIEnv *jnienv, const char *classname);

/* Set an individual field in a Db* object.
 * The field must be a DB object type.
 */
void set_object_field(JNIEnv *jnienv, jclass class_of_this,
		      jobject jthis, const char *object_classname,
		      const char *name_of_field, jobject obj);

/* Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_int_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, jint value);

/* Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_long_field(JNIEnv *jnienv, jclass class_of_this,
			jobject jthis, const char *name_of_field, jlong value);

/* Set an individual field in a Db* object.
 * The field must be an DbLsn type.
 */
void set_lsn_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, DB_LSN value);

/* Values of expect_mask
 */
static const int EXCEPTION_FILE_NOT_FOUND = 0x0001;

/* Report an exception back to the java side.
 */
void report_exception(JNIEnv *jnienv, const char *text, int err,
		      unsigned long expect_mask);

/* Report an error via the errcall mechanism.
 */
void report_errcall(JNIEnv *jnienv, jobject errcall,
		    jstring prefix, const char *message);

/* If the object is null, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_non_null(JNIEnv *jnienv, void *obj);

/* If the error code is non-zero, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_return(JNIEnv *jnienv, int err, unsigned long expect_mask);

/* Create an object of the given class, calling its default constructor.
 */
jobject create_default_object(JNIEnv *jnienv, const char *class_name);

/* Convert an DB object to a Java encapsulation of that object.
 * Note: This implementation creates a new Java object on each call,
 * so it is generally useful when a new DB object has just been created.
 */
jobject convert_object(JNIEnv *jnienv, const char *class_name, void *dbobj);

/* Create a copy of the string
 */
char *dup_string(const char *str);

/* Create a malloc'ed copy of the java string.
 * Caller must free it.
 */
char *get_c_string(JNIEnv *jnienv, jstring jstr);

/* Create a java string from the given string
 */
jstring get_java_string(JNIEnv *jnienv, const char* string);

/* Convert a java object to the various C pointers they represent.
 */
DB             *get_DB            (JNIEnv *jnienv, jobject obj);
DB_BTREE_STAT  *get_DB_BTREE_STAT (JNIEnv *jnienv, jobject obj);
DBC            *get_DBC           (JNIEnv *jnienv, jobject obj);
DB_ENV         *get_DB_ENV        (JNIEnv *jnienv, jobject obj);
DB_ENV_JAVAINFO *get_DB_ENV_JAVAINFO (JNIEnv *jnienv, jobject obj);
DB_HASH_STAT   *get_DB_HASH_STAT  (JNIEnv *jnienv, jobject obj);
DB_JAVAINFO    *get_DB_JAVAINFO   (JNIEnv *jnienv, jobject obj);
DB_LOCK        *get_DB_LOCK       (JNIEnv *jnienv, jobject obj);
DB_LOG_STAT    *get_DB_LOG_STAT   (JNIEnv *jnienv, jobject obj);
DB_LSN         *get_DB_LSN        (JNIEnv *jnienv, jobject obj);
DB_MPOOL_FSTAT *get_DB_MPOOL_FSTAT(JNIEnv *jnienv, jobject obj);
DB_MPOOL_STAT  *get_DB_MPOOL_STAT (JNIEnv *jnienv, jobject obj);
DB_QUEUE_STAT  *get_DB_QUEUE_STAT (JNIEnv *jnienv, jobject obj);
DB_TXN         *get_DB_TXN        (JNIEnv *jnienv, jobject obj);
DB_TXN_STAT    *get_DB_TXN_STAT   (JNIEnv *jnienv, jobject obj);
DBT            *get_DBT           (JNIEnv *jnienv, jobject obj);
DBT_JAVAINFO   *get_DBT_JAVAINFO  (JNIEnv *jnienv, jobject obj);

/* From a C object, create a Java object.
 */
jobject get_DbBtreeStat  (JNIEnv *jnienv, DB_BTREE_STAT *dbobj);
jobject get_Dbc          (JNIEnv *jnienv, DBC *dbobj);
jobject get_DbHashStat   (JNIEnv *jnienv, DB_HASH_STAT *dbobj);
jobject get_DbLogStat    (JNIEnv *jnienv, DB_LOG_STAT *dbobj);
jobject get_DbLsn        (JNIEnv *jnienv, DB_LSN dbobj);
jobject get_DbMpoolStat  (JNIEnv *jnienv, DB_MPOOL_STAT *dbobj);
jobject get_DbMpoolFStat (JNIEnv *jnienv, DB_MPOOL_FSTAT *dbobj);
jobject get_DbQueueStat  (JNIEnv *jnienv, DB_QUEUE_STAT *dbobj);
jobject get_Dbt          (JNIEnv *jnienv, DBT *dbt);
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
extern const char * const name_DB_LOG_STAT;
extern const char * const name_DB_LSN;
extern const char * const name_DB_MEMORY_EX;
extern const char * const name_DB_MPOOL_FSTAT;
extern const char * const name_DB_MPOOL_STAT;
extern const char * const name_DB_QUEUE_STAT;
extern const char * const name_DB_RUNRECOVERY_EX;
extern const char * const name_DBT;
extern const char * const name_DB_TXN;
extern const char * const name_DB_TXN_STAT;
extern const char * const name_DB_TXN_STAT_ACTIVE;
extern const char * const name_DbAppendRecno;
extern const char * const name_DbBtreeCompare;
extern const char * const name_DbBtreePrefix;
extern const char * const name_DbDupCompare;
extern const char * const name_DbEnvFeedback;
extern const char * const name_DbErrcall;
extern const char * const name_DbFeedback;
extern const char * const name_DbHash;
extern const char * const name_DbRecoveryInit;
extern const char * const name_DbTxnRecover;

extern const char * const string_signature;

#define	JAVADB_RO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
JNIEXPORT j_fieldtype JNICALL                                               \
  Java_com_sleepycat_db_##j_class##_get_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis)                                           \
{                                                                           \
	c_type *db_this = get_##c_type(jnienv, jthis);                      \
									    \
	if (verify_non_null(jnienv, db_this)) {                             \
		return db_this->c_field;                                    \
	}                                                                   \
	return 0;                                                           \
}

#define	JAVADB_WO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
JNIEXPORT void JNICALL                                                      \
  Java_com_sleepycat_db_##j_class##_set_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis, j_fieldtype value)                        \
{                                                                           \
	c_type *db_this = get_##c_type(jnienv, jthis);                      \
									    \
	if (verify_non_null(jnienv, db_this)) {                             \
		db_this->c_field = value;                                   \
	}                                                                   \
}

/* This is a variant of the JAVADB_WO_ACCESS macro to define a simple set_
 * method using a C "method" call.  These should be used with set_
 * methods that cannot invoke java 'callbacks' (no set_ method currently
 * does that).  That assumption allows us to optimize (and simplify)
 * by not calling API_BEGIN/END macros.
 */
#define	JAVADB_WO_ACCESS_METHOD(j_class, j_fieldtype,                       \
				j_field, c_type, c_field)		    \
JNIEXPORT void JNICALL                                                      \
  Java_com_sleepycat_db_##j_class##_set_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis, j_fieldtype value)                        \
{                                                                           \
	c_type *db_this;                                                    \
	int err;                                                            \
									    \
	db_this = get_##c_type(jnienv, jthis);                              \
	if (verify_non_null(jnienv, db_this)) {                             \
		err = db_this->set_##c_field(db_this, value);               \
		verify_return(jnienv, err, 0);                              \
	}                                                                   \
}

#define	JAVADB_RW_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
	JAVADB_RO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
	JAVADB_WO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)

#define	JAVADB_WO_ACCESS_STRING(j_class, j_field, c_type, c_field)          \
JNIEXPORT void JNICALL                                                      \
  Java_com_sleepycat_db_##j_class##_set_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis, jstring value)                            \
{                                                                           \
	c_type *db_this;                                                    \
	int err;                                                            \
									    \
	db_this = get_##c_type(jnienv, jthis);                              \
	if (verify_non_null(jnienv, db_this)) {                             \
		err = db_this->set_##c_field(db_this,                       \
			  (*jnienv)->GetStringUTFChars(jnienv, value, NULL)); \
		verify_return(jnienv, err, 0);                              \
	}                                                                   \
}

#define	JAVADB_API_BEGIN(db, jthis) \
	if ((db) != NULL) \
	  ((DB_JAVAINFO*)(db)->cj_internal)->jdbref_ = \
	  ((DB_ENV_JAVAINFO*)((db)->dbenv->cj_internal))->jdbref_ = (jthis)

#define	JAVADB_API_END(db) \
	if ((db) != NULL) \
	  ((DB_JAVAINFO*)(db)->cj_internal)->jdbref_ = \
	  ((DB_ENV_JAVAINFO*)((db)->dbenv->cj_internal))->jdbref_ = 0

#define	JAVADB_ENV_API_BEGIN(dbenv, jthis) \
	if ((dbenv) != NULL) \
	  ((DB_ENV_JAVAINFO*)((dbenv)->cj_internal))->jenvref_ = (jthis)

#define	JAVADB_ENV_API_END(dbenv) \
	if ((dbenv) != NULL) \
	  ((DB_ENV_JAVAINFO*)((dbenv)->cj_internal))->jenvref_ = 0

#endif /* !_JAVA_UTIL_H_ */
