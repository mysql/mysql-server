/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_util.c,v 11.17 2000/10/28 13:09:39 dda Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"

#ifdef DB_WIN32
#define	sys_errlist _sys_errlist
#define	sys_nerr _sys_nerr
#endif

const char * const name_DB                 = "Db";
const char * const name_DB_BTREE_STAT      = "DbBtreeStat";
const char * const name_DBC                = "Dbc";
const char * const name_DB_DEADLOCK_EX     = "DbDeadlockException";
const char * const name_DB_ENV             = "DbEnv";
const char * const name_DB_EXCEPTION       = "DbException";
const char * const name_DB_HASH_STAT       = "DbHashStat";
const char * const name_DB_LOCK            = "DbLock";
const char * const name_DB_LOCK_STAT       = "DbLockStat";
const char * const name_DB_LOG_STAT        = "DbLogStat";
const char * const name_DB_LSN             = "DbLsn";
const char * const name_DB_MEMORY_EX       = "DbMemoryException";
const char * const name_DB_MPOOL_FSTAT     = "DbMpoolFStat";
const char * const name_DB_MPOOL_STAT      = "DbMpoolStat";
const char * const name_DB_QUEUE_STAT      = "DbQueueStat";
const char * const name_DB_RUNRECOVERY_EX  = "DbRunRecoveryException";
const char * const name_DBT                = "Dbt";
const char * const name_DB_TXN             = "DbTxn";
const char * const name_DB_TXN_STAT        = "DbTxnStat";
const char * const name_DB_TXN_STAT_ACTIVE = "DbTxnStat$Active";
const char * const name_DbAppendRecno      = "DbAppendRecno";
const char * const name_DbBtreeCompare     = "DbBtreeCompare";
const char * const name_DbBtreePrefix      = "DbBtreePrefix";
const char * const name_DbDupCompare       = "DbDupCompare";
const char * const name_DbEnvFeedback      = "DbEnvFeedback";
const char * const name_DbErrcall          = "DbErrcall";
const char * const name_DbHash             = "DbHash";
const char * const name_DbFeedback         = "DbFeedback";
const char * const name_DbRecoveryInit     = "DbRecoveryInit";
const char * const name_DbTxnRecover       = "DbTxnRecover";

const char * const string_signature    = "Ljava/lang/String;";

/****************************************************************
 *
 * Utility functions used by "glue" functions.
 *
 */

/* Get the private data from a Db* object that points back to a C DB_* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_dbobj(JNIEnv *jnienv, const char *classname,
		       jobject obj)
{
	jclass dbClass;
	jfieldID id;
	long_to_ptr lp;

	if (!obj)
		return (0);

	dbClass = get_class(jnienv, classname);
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_dbobj_", "J");
	lp.java_long = (*jnienv)->GetLongField(jnienv, obj, id);
	return (lp.ptr);
}

/* Set the private data in a Db* object that points back to a C DB_* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_dbobj(JNIEnv *jnienv, const char *classname,
		      jobject obj, void *value)
{
	long_to_ptr lp;
	jclass dbClass;
	jfieldID id;

	lp.java_long = 0;	/* no junk in case sizes mismatch */
	lp.ptr = value;
	dbClass = get_class(jnienv, classname);
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_dbobj_", "J");
	(*jnienv)->SetLongField(jnienv, obj, id, lp.java_long);
}

/* Get the private data in a Db/DbEnv object that holds additional 'side data'.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_info(JNIEnv *jnienv, const char *classname,
		       jobject obj)
{
	jclass dbClass;
	jfieldID id;
	long_to_ptr lp;

	if (!obj)
		return (0);

	dbClass = get_class(jnienv, classname);
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_info_", "J");
	lp.java_long = (*jnienv)->GetLongField(jnienv, obj, id);
	return (lp.ptr);
}

/* Set the private data in a Db/DbEnv object that holds additional 'side data'.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_info(JNIEnv *jnienv, const char *classname,
		      jobject obj, void *value)
{
	long_to_ptr lp;
	jclass dbClass;
	jfieldID id;

	lp.java_long = 0;	/* no junk in case sizes mismatch */
	lp.ptr = value;
	dbClass = get_class(jnienv, classname);
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_info_", "J");
	(*jnienv)->SetLongField(jnienv, obj, id, lp.java_long);
}

/*
 * Given a non-qualified name (e.g. "foo"), get the class handle
 * for the fully qualified name (e.g. "com.sleepycat.db.foo")
 */
jclass get_class(JNIEnv *jnienv, const char *classname)
{
	/* Note: PERFORMANCE: It should be possible to cache jclass's.
	 * If we do a NewGlobalRef on each one, we can keep them
	 * around in a table.  A jclass is a jobject, and
	 * since NewGlobalRef returns a jobject, it isn't
	 * technically right, but it would likely work with
	 * most implementations.  Possibly make it configurable.
	 */
	char fullname[128] = DB_PACKAGE_NAME;
	strncat(fullname, classname, sizeof(fullname));
	return ((*jnienv)->FindClass(jnienv, fullname));
}

/* Set an individual field in a Db* object.
 * The field must be a DB object type.
 */
void set_object_field(JNIEnv *jnienv, jclass class_of_this,
		      jobject jthis, const char *object_classname,
		      const char *name_of_field, jobject obj)
{
	char signature[512];
	jfieldID id;

	strncpy(signature, "L", sizeof(signature));
	strncat(signature, DB_PACKAGE_NAME, sizeof(signature));
	strncat(signature, object_classname, sizeof(signature));
	strncat(signature, ";", sizeof(signature));

	id  = (*jnienv)->GetFieldID(jnienv, class_of_this, name_of_field, signature);
	(*jnienv)->SetObjectField(jnienv, jthis, id, obj);
}

/* Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_int_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, jint value)
{
	jfieldID id  = (*jnienv)->GetFieldID(jnienv, class_of_this, name_of_field, "I");
	(*jnienv)->SetIntField(jnienv, jthis, id, value);
}

/* Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_long_field(JNIEnv *jnienv, jclass class_of_this,
		    jobject jthis, const char *name_of_field, jlong value)
{
	jfieldID id  = (*jnienv)->GetFieldID(jnienv, class_of_this, name_of_field, "J");
	(*jnienv)->SetLongField(jnienv, jthis, id, value);
}

/* Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_lsn_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, DB_LSN value)
{
	set_object_field(jnienv, class_of_this, jthis, name_DB_LSN,
			 name_of_field, get_DbLsn(jnienv, value));
}

/* Report an exception back to the java side.
 */
void report_exception(JNIEnv *jnienv, const char *text, int err,
		      unsigned long expect_mask)
{
	jstring textString;
	jclass dbexcept;
	jclass javaexcept;
	jmethodID constructId;
	jthrowable obj;

	textString = NULL;
	dbexcept = NULL;
	javaexcept = NULL;
	constructId = NULL;
	obj = NULL;

	switch (err) {
	/* DB_JAVA_CALLBACK is returned by dbji_call_append_recno()
	 * (the append_recno callback) when the Java version of the
	 * callback has thrown an exception, and we want to pass the
	 * exception on.  The exception has already been thrown, we
	 * don't want to throw a new one.
	 */
	case DB_JAVA_CALLBACK:
		break;
	case ENOMEM:
		dbexcept = get_class(jnienv, name_DB_MEMORY_EX);
		break;
	case ENOENT:
		/* In this case there is a corresponding standard java
		 * exception type that we'll use.  First we make sure
		 * that the calling function expected this kind of error,
		 * if not we give an 'internal error' DbException, since
		 * we must not throw an exception type that isn't
		 * declared in the signature.
		 *
		 * We'll make this a little more general if/when we add
		 * more java standard exceptions.
		 */
		if ((expect_mask & EXCEPTION_FILE_NOT_FOUND) == 0) {
			char errstr[1024];

			strncpy(errstr, "internal error: unexpected errno: ",
				sizeof(errstr));
			strncat(errstr, text, sizeof(errstr));
			textString = get_java_string(jnienv, errstr);
			dbexcept = get_class(jnienv, name_DB_EXCEPTION);
		}
		else {
			javaexcept =
			  (*jnienv)->FindClass(jnienv, "java/io/FileNotFoundException");
		}
		break;
	case DB_RUNRECOVERY:
		dbexcept = get_class(jnienv, name_DB_RUNRECOVERY_EX);
		break;
	case DB_LOCK_DEADLOCK:
		dbexcept = get_class(jnienv, name_DB_DEADLOCK_EX);
		break;
	default:
		dbexcept = get_class(jnienv, name_DB_EXCEPTION);
		break;
	}
	if (dbexcept != NULL) {
		if (textString == NULL)
			textString = get_java_string(jnienv, text);
		constructId = (*jnienv)->GetMethodID(jnienv, dbexcept,
						     "<init>",
						     "(Ljava/lang/String;I)V");
		obj = (jthrowable)(*jnienv)->NewObject(jnienv, dbexcept,
						       constructId, textString,
						       err);
		(*jnienv)->Throw(jnienv, obj);
	}
	else if (javaexcept != NULL) {
		javaexcept =
			(*jnienv)->FindClass(jnienv, "java/io/FileNotFoundException");
		(*jnienv)->ThrowNew(jnienv, javaexcept, text);
	}
}

/* Report an error via the errcall mechanism.
 */
void report_errcall(JNIEnv *jnienv, jobject errcall,
		    jstring prefix, const char *message)
{
	jmethodID id;
	jclass errcall_class;
	jstring msg;

	errcall_class = get_class(jnienv, name_DbErrcall);
	msg = get_java_string(jnienv, message);

	id = (*jnienv)->GetMethodID(jnienv, errcall_class,
				 "errcall",
				 "(Ljava/lang/String;Ljava/lang/String;)V");
	if (id == NULL) {
		fprintf(stderr, "Cannot get errcall methodID!\n");
		fprintf(stderr, "error: %s\n", message);
		return;
	}

	(*jnienv)->CallVoidMethod(jnienv, errcall, id, prefix, msg);
}

/* If the object is null, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_non_null(JNIEnv *jnienv, void *obj)
{
	if (obj == NULL) {
		report_exception(jnienv, "null object", EINVAL, 0);
		return (0);
	}
	return (1);
}

/* If the error code is non-zero, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_return(JNIEnv *jnienv, int err, unsigned long expect_mask)
{
	if (err == 0)
		return 1;

	report_exception(jnienv, db_strerror(err), err, expect_mask);
	return 0;
}

/* Create an object of the given class, calling its default constructor.
 */
jobject create_default_object(JNIEnv *jnienv, const char *class_name)
{
	jclass dbclass = get_class(jnienv, class_name);
	jmethodID id = (*jnienv)->GetMethodID(jnienv, dbclass, "<init>", "()V");
	jobject object = (*jnienv)->NewObject(jnienv, dbclass, id);
	return (object);
}

/* Convert an DB object to a Java encapsulation of that object.
 * Note: This implementation creates a new Java object on each call,
 * so it is generally useful when a new DB object has just been created.
 */
jobject convert_object(JNIEnv *jnienv, const char *class_name, void *dbobj)
{
	jobject jo;

	if (!dbobj)
		return (0);

	jo = create_default_object(jnienv, class_name);
	set_private_dbobj(jnienv, class_name, jo, dbobj);
	return (jo);
}

/* Create a copy of the string
 */
char *dup_string(const char *str)
{
	int len;
	char *retval;

	len = strlen(str) + 1;
	retval = (char *)malloc(sizeof(char)*len);
	strncpy(retval, str, len);
	return (retval);
}

/* Create a java string from the given string
 */
jstring get_java_string(JNIEnv *jnienv, const char* string)
{
	if (string == 0)
		return (0);
	return ((*jnienv)->NewStringUTF(jnienv, string));
}

/* Create a malloc'ed copy of the java string.
 * Caller must free it.
 */
char *get_c_string(JNIEnv *jnienv, jstring jstr)
{
	const jbyte *utf;
	char *retval;

	utf = (*jnienv)->GetStringUTFChars(jnienv, jstr, NULL);
	retval = dup_string((const char *)utf);
	(*jnienv)->ReleaseStringUTFChars(jnienv, jstr, utf);
	return retval;
}

/* Convert a java object to the various C pointers they represent.
 */
DB *get_DB(JNIEnv *jnienv, jobject obj)
{
	return ((DB *)get_private_dbobj(jnienv, name_DB, obj));
}

DB_BTREE_STAT *get_DB_BTREE_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_BTREE_STAT *)get_private_dbobj(jnienv, name_DB_BTREE_STAT, obj));
}

DBC *get_DBC(JNIEnv *jnienv, jobject obj)
{
	return ((DBC *)get_private_dbobj(jnienv, name_DBC, obj));
}

DB_ENV *get_DB_ENV(JNIEnv *jnienv, jobject obj)
{
	return ((DB_ENV *)get_private_dbobj(jnienv, name_DB_ENV, obj));
}

DB_ENV_JAVAINFO *get_DB_ENV_JAVAINFO(JNIEnv *jnienv, jobject obj)
{
	return ((DB_ENV_JAVAINFO *)get_private_info(jnienv, name_DB_ENV, obj));
}

DB_HASH_STAT *get_DB_HASH_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_HASH_STAT *)get_private_dbobj(jnienv, name_DB_HASH_STAT, obj));
}

DB_JAVAINFO *get_DB_JAVAINFO(JNIEnv *jnienv, jobject obj)
{
	return ((DB_JAVAINFO *)get_private_info(jnienv, name_DB, obj));
}

DB_LOCK *get_DB_LOCK(JNIEnv *jnienv, jobject obj)
{
	return ((DB_LOCK *)get_private_dbobj(jnienv, name_DB_LOCK, obj));
}

DB_LOG_STAT *get_DB_LOG_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_LOG_STAT *)get_private_dbobj(jnienv, name_DB_LOG_STAT, obj));
}

DB_LSN *get_DB_LSN(JNIEnv *jnienv, jobject obj)
{
	return ((DB_LSN *)get_private_dbobj(jnienv, name_DB_LSN, obj));
}

DB_MPOOL_FSTAT *get_DB_MPOOL_FSTAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_MPOOL_FSTAT *)get_private_dbobj(jnienv, name_DB_MPOOL_FSTAT, obj));
}

DB_MPOOL_STAT *get_DB_MPOOL_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_MPOOL_STAT *)get_private_dbobj(jnienv, name_DB_MPOOL_STAT, obj));
}

DB_QUEUE_STAT *get_DB_QUEUE_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_QUEUE_STAT *)get_private_dbobj(jnienv, name_DB_QUEUE_STAT, obj));
}

DB_TXN *get_DB_TXN(JNIEnv *jnienv, jobject obj)
{
	return ((DB_TXN *)get_private_dbobj(jnienv, name_DB_TXN, obj));
}

DB_TXN_STAT *get_DB_TXN_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_TXN_STAT *)get_private_dbobj(jnienv, name_DB_TXN_STAT, obj));
}

DBT *get_DBT(JNIEnv *jnienv, jobject obj)
{
	DBT_JAVAINFO *ji;

	ji = (DBT_JAVAINFO *)get_private_dbobj(jnienv, name_DBT, obj);
	if (ji == NULL)
		return (NULL);
	else
		return (&ji->dbt);
}

DBT_JAVAINFO *get_DBT_JAVAINFO(JNIEnv *jnienv, jobject obj)
{
	return ((DBT_JAVAINFO *)get_private_dbobj(jnienv, name_DBT, obj));
}

/* Convert a C pointer to the various Java objects they represent.
 */
jobject get_DbBtreeStat(JNIEnv *jnienv, DB_BTREE_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_BTREE_STAT, dbobj));
}

jobject get_Dbc(JNIEnv *jnienv, DBC *dbobj)
{
	return (convert_object(jnienv, name_DBC, dbobj));
}

jobject get_DbHashStat(JNIEnv *jnienv, DB_HASH_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_HASH_STAT, dbobj));
}

jobject get_DbLogStat(JNIEnv *jnienv, DB_LOG_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_LOG_STAT, dbobj));
}

/* LSNs are different since they are really normally
 * treated as by-value objects.  We actually create
 * a pointer to the LSN and store that, deleting it
 * when the LSN is GC'd.
 */
jobject get_DbLsn(JNIEnv *jnienv, DB_LSN dbobj)
{
	DB_LSN *lsnp = (DB_LSN *)malloc(sizeof(DB_LSN));
	memset(lsnp, 0, sizeof(DB_LSN));
	*lsnp = dbobj;
	return (convert_object(jnienv, name_DB_LSN, lsnp));
}

jobject get_Dbt(JNIEnv *jnienv, DBT *dbt)
{
	return (convert_object(jnienv, name_DBT, dbt));
}

jobject get_DbMpoolFStat(JNIEnv *jnienv, DB_MPOOL_FSTAT *dbobj)
{
	return (convert_object(jnienv, name_DB_MPOOL_FSTAT, dbobj));
}

jobject get_DbMpoolStat(JNIEnv *jnienv, DB_MPOOL_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_MPOOL_STAT, dbobj));
}

jobject get_DbQueueStat(JNIEnv *jnienv, DB_QUEUE_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_QUEUE_STAT, dbobj));
}

jobject get_DbTxn(JNIEnv *jnienv, DB_TXN *dbobj)
{
	return (convert_object(jnienv, name_DB_TXN, dbobj));
}

jobject get_DbTxnStat(JNIEnv *jnienv, DB_TXN_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_TXN_STAT, dbobj));
}
