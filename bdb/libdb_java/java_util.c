/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_util.c,v 11.49 2002/09/13 03:09:30 mjc Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>

#include "db_int.h"
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
const char * const name_DB_LOCKNOTGRANTED_EX = "DbLockNotGrantedException";
const char * const name_DB_LOGC            = "DbLogc";
const char * const name_DB_LOG_STAT        = "DbLogStat";
const char * const name_DB_LSN             = "DbLsn";
const char * const name_DB_MEMORY_EX       = "DbMemoryException";
const char * const name_DB_MPOOL_FSTAT     = "DbMpoolFStat";
const char * const name_DB_MPOOL_STAT      = "DbMpoolStat";
const char * const name_DB_PREPLIST        = "DbPreplist";
const char * const name_DB_QUEUE_STAT      = "DbQueueStat";
const char * const name_DB_REP_STAT        = "DbRepStat";
const char * const name_DB_RUNRECOVERY_EX  = "DbRunRecoveryException";
const char * const name_DBT                = "Dbt";
const char * const name_DB_TXN             = "DbTxn";
const char * const name_DB_TXN_STAT        = "DbTxnStat";
const char * const name_DB_TXN_STAT_ACTIVE = "DbTxnStat$Active";
const char * const name_DB_UTIL            = "DbUtil";
const char * const name_DbAppendRecno      = "DbAppendRecno";
const char * const name_DbBtreeCompare     = "DbBtreeCompare";
const char * const name_DbBtreePrefix      = "DbBtreePrefix";
const char * const name_DbDupCompare       = "DbDupCompare";
const char * const name_DbEnvFeedback      = "DbEnvFeedback";
const char * const name_DbErrcall          = "DbErrcall";
const char * const name_DbHash             = "DbHash";
const char * const name_DbLockRequest      = "DbLockRequest";
const char * const name_DbFeedback         = "DbFeedback";
const char * const name_DbRecoveryInit     = "DbRecoveryInit";
const char * const name_DbRepTransport	   = "DbRepTransport";
const char * const name_DbSecondaryKeyCreate = "DbSecondaryKeyCreate";
const char * const name_DbTxnRecover       = "DbTxnRecover";
const char * const name_RepElectResult = "DbEnv$RepElectResult";
const char * const name_RepProcessMessage = "DbEnv$RepProcessMessage";

const char * const string_signature    = "Ljava/lang/String;";

jfieldID fid_Dbt_data;
jfieldID fid_Dbt_offset;
jfieldID fid_Dbt_size;
jfieldID fid_Dbt_ulen;
jfieldID fid_Dbt_dlen;
jfieldID fid_Dbt_doff;
jfieldID fid_Dbt_flags;
jfieldID fid_Dbt_private_dbobj_;
jfieldID fid_Dbt_must_create_data;
jfieldID fid_DbLockRequest_op;
jfieldID fid_DbLockRequest_mode;
jfieldID fid_DbLockRequest_timeout;
jfieldID fid_DbLockRequest_obj;
jfieldID fid_DbLockRequest_lock;
jfieldID fid_RepProcessMessage_envid;

/****************************************************************
 *
 * Utility functions used by "glue" functions.
 */

/*
 * Do any one time initialization, especially initializing any
 * unchanging methodIds, fieldIds, etc.
 */
void one_time_init(JNIEnv *jnienv)
{
    jclass cl;

    if ((cl = get_class(jnienv, name_DBT)) == NULL)
	return;	/* An exception has been posted. */
    fid_Dbt_data = (*jnienv)->GetFieldID(jnienv, cl, "data", "[B");
    fid_Dbt_offset = (*jnienv)->GetFieldID(jnienv, cl, "offset", "I");
    fid_Dbt_size = (*jnienv)->GetFieldID(jnienv, cl, "size", "I");
    fid_Dbt_ulen = (*jnienv)->GetFieldID(jnienv, cl, "ulen", "I");
    fid_Dbt_dlen = (*jnienv)->GetFieldID(jnienv, cl, "dlen", "I");
    fid_Dbt_doff = (*jnienv)->GetFieldID(jnienv, cl, "doff", "I");
    fid_Dbt_flags = (*jnienv)->GetFieldID(jnienv, cl, "flags", "I");
    fid_Dbt_must_create_data = (*jnienv)->GetFieldID(jnienv, cl,
						     "must_create_data", "Z");
    fid_Dbt_private_dbobj_ =
	(*jnienv)->GetFieldID(jnienv, cl, "private_dbobj_", "J");

    if ((cl = get_class(jnienv, name_DbLockRequest)) == NULL)
	return;	/* An exception has been posted. */
    fid_DbLockRequest_op = (*jnienv)->GetFieldID(jnienv, cl, "op", "I");
    fid_DbLockRequest_mode = (*jnienv)->GetFieldID(jnienv, cl, "mode", "I");
    fid_DbLockRequest_timeout =
	(*jnienv)->GetFieldID(jnienv, cl, "timeout", "I");
    fid_DbLockRequest_obj = (*jnienv)->GetFieldID(jnienv, cl, "obj",
						  "Lcom/sleepycat/db/Dbt;");
    fid_DbLockRequest_lock = (*jnienv)->GetFieldID(jnienv, cl, "lock",
						   "Lcom/sleepycat/db/DbLock;");

    if ((cl = get_class(jnienv, name_RepProcessMessage)) == NULL)
	return;	/* An exception has been posted. */
    fid_RepProcessMessage_envid =
	(*jnienv)->GetFieldID(jnienv, cl, "envid", "I");
}

/*
 * Get the private data from a Db* object that points back to a C DB_* object.
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

	if ((dbClass = get_class(jnienv, classname)) == NULL)
		return (NULL);	/* An exception has been posted. */
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_dbobj_", "J");
	lp.java_long = (*jnienv)->GetLongField(jnienv, obj, id);
	return (lp.ptr);
}

/*
 * Set the private data in a Db* object that points back to a C DB_* object.
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
	if ((dbClass = get_class(jnienv, classname)) == NULL)
		return;	/* An exception has been posted. */
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_dbobj_", "J");
	(*jnienv)->SetLongField(jnienv, obj, id, lp.java_long);
}

/*
 * Get the private data in a Db/DbEnv object that holds additional 'side data'.
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
		return (NULL);

	if ((dbClass = get_class(jnienv, classname)) == NULL)
		return (NULL);	/* An exception has been posted. */
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_info_", "J");
	lp.java_long = (*jnienv)->GetLongField(jnienv, obj, id);
	return (lp.ptr);
}

/*
 * Set the private data in a Db/DbEnv object that holds additional 'side data'.
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
	if ((dbClass = get_class(jnienv, classname)) == NULL)
		return;	/* An exception has been posted. */
	id = (*jnienv)->GetFieldID(jnienv, dbClass, "private_info_", "J");
	(*jnienv)->SetLongField(jnienv, obj, id, lp.java_long);
}

/*
 * Given a non-qualified name (e.g. "foo"), get the class handle
 * for the fully qualified name (e.g. "com.sleepycat.db.foo")
 */
jclass get_class(JNIEnv *jnienv, const char *classname)
{
	/*
	 * Note: PERFORMANCE: It should be possible to cache jclass's.
	 * If we do a NewGlobalRef on each one, we can keep them
	 * around in a table.  A jclass is a jobject, and
	 * since NewGlobalRef returns a jobject, it isn't
	 * technically right, but it would likely work with
	 * most implementations.  Possibly make it configurable.
	 */
	char fullname[128];

	(void)snprintf(fullname, sizeof(fullname),
	    "%s%s", DB_PACKAGE_NAME, classname);
	return ((*jnienv)->FindClass(jnienv, fullname));
}

/*
 * Given a fully qualified name (e.g. "java.util.Hashtable")
 * return the jclass object.  If it can't be found, an
 * exception is raised and NULL is return.
 * This is appropriate to be used for classes that may
 * not be present.
 */
jclass get_fully_qualified_class(JNIEnv *jnienv, const char *classname)
{
	jclass result;

	result = ((*jnienv)->FindClass(jnienv, classname));
	if (result == NULL) {
		jclass cnfe;
		char message[1024];

		cnfe = (*jnienv)->FindClass(jnienv,
				    "java/lang/ClassNotFoundException");
		strncpy(message, classname, sizeof(message));
		strncat(message, ": class not found", sizeof(message));
		(*jnienv)->ThrowNew(jnienv, cnfe, message);
	}
	return (result);
}

/*
 * Set an individual field in a Db* object.
 * The field must be a DB object type.
 */
void set_object_field(JNIEnv *jnienv, jclass class_of_this,
		      jobject jthis, const char *object_classname,
		      const char *name_of_field, jobject obj)
{
	char signature[512];
	jfieldID id;

	(void)snprintf(signature, sizeof(signature),
	    "L%s%s;", DB_PACKAGE_NAME, object_classname);
	id  = (*jnienv)->GetFieldID(
	    jnienv, class_of_this, name_of_field, signature);
	(*jnienv)->SetObjectField(jnienv, jthis, id, obj);
}

/*
 * Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_int_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, jint value)
{
	jfieldID id  =
	    (*jnienv)->GetFieldID(jnienv, class_of_this, name_of_field, "I");
	(*jnienv)->SetIntField(jnienv, jthis, id, value);
}

/*
 * Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_long_field(JNIEnv *jnienv, jclass class_of_this,
		    jobject jthis, const char *name_of_field, jlong value)
{
	jfieldID id  = (*jnienv)->GetFieldID(jnienv, class_of_this,
					     name_of_field, "J");
	(*jnienv)->SetLongField(jnienv, jthis, id, value);
}

/*
 * Set an individual field in a Db* object.
 * The field must be an integer type.
 */
void set_lsn_field(JNIEnv *jnienv, jclass class_of_this,
		   jobject jthis, const char *name_of_field, DB_LSN value)
{
	set_object_field(jnienv, class_of_this, jthis, name_DB_LSN,
			 name_of_field, get_DbLsn(jnienv, value));
}

/*
 * Report an exception back to the java side.
 */
void report_exception(JNIEnv *jnienv, const char *text,
		      int err, unsigned long expect_mask)
{
	jstring textString;
	jclass dbexcept;
	jclass javaexcept;
	jthrowable obj;

	textString = NULL;
	dbexcept = NULL;
	javaexcept = NULL;

	switch (err) {
	/*
	 * DB_JAVA_CALLBACK is returned by
	 * dbji_call_append_recno() (the append_recno callback)
	 * when the Java version of the callback has thrown
	 * an exception, and we want to pass the exception on.
	 * The exception has already been thrown, we
	 * don't want to throw a new one.
	 */
		case DB_JAVA_CALLBACK:
			break;
		case ENOENT:
			/*
			 * In this case there is a corresponding
			 * standard java exception type that we'll use.
			 * First we make sure that the calling function
			 * expected this kind of error, if not we give
			 * an 'internal error' DbException, since
			 * we must not throw an exception type that isn't
			 * declared in the signature.
			 *
			 * We'll make this a little more general if/when
			 * we add more java standard exceptions.
			 */
			if ((expect_mask & EXCEPTION_FILE_NOT_FOUND) != 0) {
				javaexcept = (*jnienv)->FindClass(jnienv,
				    "java/io/FileNotFoundException");
			}
			else {
				char errstr[1024];

				snprintf(errstr, sizeof(errstr),
				  "internal error: unexpected errno: %s",
					 text);
				textString = get_java_string(jnienv,
							     errstr);
				dbexcept = get_class(jnienv,
						     name_DB_EXCEPTION);
			}
			break;
		case DB_RUNRECOVERY:
			dbexcept = get_class(jnienv,
					     name_DB_RUNRECOVERY_EX);
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
		if ((obj = create_exception(jnienv, textString, err, dbexcept))
		    != NULL)
			(*jnienv)->Throw(jnienv, obj);
		/* Otherwise, an exception has been posted. */
	}
	else if (javaexcept != NULL)
		(*jnienv)->ThrowNew(jnienv, javaexcept, text);
	else
		fprintf(stderr,
		    "report_exception: failed to create an exception\n");
}

/*
 * Report an exception back to the java side, for the specific
 * case of DB_LOCK_NOTGRANTED, as more things are added to the
 * constructor of this type of exception.
 */
void report_notgranted_exception(JNIEnv *jnienv, const char *text,
				 db_lockop_t op, db_lockmode_t mode,
				 jobject jdbt, jobject jlock, int index)
{
	jstring textString;
	jclass dbexcept;
	jthrowable obj;
	jmethodID mid;

	if ((dbexcept = get_class(jnienv, name_DB_LOCKNOTGRANTED_EX)) == NULL)
		return;	/* An exception has been posted. */
	textString = get_java_string(jnienv, text);

	mid = (*jnienv)->GetMethodID(jnienv, dbexcept, "<init>",
				     "(Ljava/lang/String;II"
				     "Lcom/sleepycat/db/Dbt;"
				     "Lcom/sleepycat/db/DbLock;I)V");
	if ((obj = (jthrowable)(*jnienv)->NewObject(jnienv, dbexcept,
	    mid, textString, op, mode, jdbt, jlock, index)) != NULL)
		(*jnienv)->Throw(jnienv, obj);
	else
		fprintf(stderr,
	    "report_notgranted_exception: failed to create an exception\n");
}

/*
 * Create an exception object and return it.
 * The given class must have a constructor that has a
 * constructor with args (java.lang.String text, int errno);
 * DbException and its subclasses fit this bill.
 */
jobject create_exception(JNIEnv *jnienv, jstring text,
				  int err, jclass dbexcept)
{
	jthrowable obj;
	jmethodID mid;

	mid = (*jnienv)->GetMethodID(jnienv, dbexcept, "<init>",
				     "(Ljava/lang/String;I)V");
	if (mid != NULL)
		obj = (jthrowable)(*jnienv)->NewObject(jnienv, dbexcept, mid,
					       text, err);
	else {
		fprintf(stderr, "Cannot get exception init method ID!\n");
		obj = NULL;
	}

	return (obj);
}

/*
 * Report an error via the errcall mechanism.
 */
void report_errcall(JNIEnv *jnienv, jobject errcall,
		    jstring prefix, const char *message)
{
	jmethodID id;
	jclass errcall_class;
	jstring msg;

	if ((errcall_class = get_class(jnienv, name_DbErrcall)) == NULL)
		return;	/* An exception has been posted. */
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

/*
 * If the object is null, report an exception and return false (0),
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

/*
 * If the error code is non-zero, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_return(JNIEnv *jnienv, int err, unsigned long expect_mask)
{
	if (err == 0)
		return (1);

	report_exception(jnienv, db_strerror(err), err, expect_mask);
	return (0);
}

/*
 * Verify that there was no memory error due to undersized Dbt.
 * If there is report a DbMemoryException, with the Dbt attached
 * and return false (0), otherwise return true (1).
 */
int verify_dbt(JNIEnv *jnienv, int err, LOCKED_DBT *ldbt)
{
	DBT *dbt;
	jobject exception;
	jstring text;
	jclass dbexcept;
	jmethodID mid;

	if (err != ENOMEM)
		return (1);

	dbt = &ldbt->javainfo->dbt;
	if (!F_ISSET(dbt, DB_DBT_USERMEM) || dbt->size <= dbt->ulen)
		return (1);

	/* Create/throw an exception of type DbMemoryException */
	if ((dbexcept = get_class(jnienv, name_DB_MEMORY_EX)) == NULL)
		return (1);	/* An exception has been posted. */
	text = get_java_string(jnienv,
			       "Dbt not large enough for available data");
	exception = create_exception(jnienv, text, ENOMEM, dbexcept);

	/* Attach the dbt to the exception */
	mid = (*jnienv)->GetMethodID(jnienv, dbexcept, "set_dbt",
				     "(L" DB_PACKAGE_NAME "Dbt;)V");
	(*jnienv)->CallVoidMethod(jnienv, exception, mid, ldbt->jdbt);
	(*jnienv)->Throw(jnienv, exception);
	return (0);
}

/*
 * Create an object of the given class, calling its default constructor.
 */
jobject create_default_object(JNIEnv *jnienv, const char *class_name)
{
	jmethodID id;
	jclass dbclass;

	if ((dbclass = get_class(jnienv, class_name)) == NULL)
		return (NULL);	/* An exception has been posted. */
	id = (*jnienv)->GetMethodID(jnienv, dbclass, "<init>", "()V");
	return ((*jnienv)->NewObject(jnienv, dbclass, id));
}

/*
 * Convert an DB object to a Java encapsulation of that object.
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

/*
 * Create a copy of the string
 */
char *dup_string(const char *str)
{
	int len;
	char *retval;
	int err;

	len = strlen(str) + 1;
	if ((err = __os_malloc(NULL, sizeof(char)*len, &retval)) != 0)
		return (NULL);
	strncpy(retval, str, len);
	return (retval);
}

/*
 * Create a java string from the given string
 */
jstring get_java_string(JNIEnv *jnienv, const char* string)
{
	if (string == 0)
		return (0);
	return ((*jnienv)->NewStringUTF(jnienv, string));
}

/*
 * Create a copy of the java string using __os_malloc.
 * Caller must free it.
 */
char *get_c_string(JNIEnv *jnienv, jstring jstr)
{
	const char *utf;
	char *retval;

	utf = (*jnienv)->GetStringUTFChars(jnienv, jstr, NULL);
	retval = dup_string(utf);
	(*jnienv)->ReleaseStringUTFChars(jnienv, jstr, utf);
	return (retval);
}

/*
 * Convert a java object to the various C pointers they represent.
 */
DB *get_DB(JNIEnv *jnienv, jobject obj)
{
	return ((DB *)get_private_dbobj(jnienv, name_DB, obj));
}

DB_BTREE_STAT *get_DB_BTREE_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_BTREE_STAT *)
	    get_private_dbobj(jnienv, name_DB_BTREE_STAT, obj));
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
	return ((DB_HASH_STAT *)
	    get_private_dbobj(jnienv, name_DB_HASH_STAT, obj));
}

DB_JAVAINFO *get_DB_JAVAINFO(JNIEnv *jnienv, jobject obj)
{
	return ((DB_JAVAINFO *)get_private_info(jnienv, name_DB, obj));
}

DB_LOCK *get_DB_LOCK(JNIEnv *jnienv, jobject obj)
{
	return ((DB_LOCK *)get_private_dbobj(jnienv, name_DB_LOCK, obj));
}

DB_LOGC *get_DB_LOGC(JNIEnv *jnienv, jobject obj)
{
	return ((DB_LOGC *)get_private_dbobj(jnienv, name_DB_LOGC, obj));
}

DB_LOG_STAT *get_DB_LOG_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_LOG_STAT *)
	    get_private_dbobj(jnienv, name_DB_LOG_STAT, obj));
}

DB_LSN *get_DB_LSN(JNIEnv *jnienv, /* DbLsn */ jobject obj) {
	/*
	 * DbLsns that are created from within java (new DbLsn()) rather
	 * than from within C (get_DbLsn()) may not have a "private" DB_LSN
	 * structure allocated for them yet.  We can't do this in the
	 * actual constructor (init_lsn()), because there's no way to pass
	 * in an initializing value in, and because the get_DbLsn()/
	 * convert_object() code path needs a copy of the pointer before
	 * the constructor gets called.  Thus, get_DbLsn() allocates and
	 * fills a DB_LSN for the object it's about to create.
	 *
	 * Since "new DbLsn()" may reasonably be passed as an argument to
	 * functions such as DbEnv.log_put(), though, we need to make sure
	 * that DB_LSN's get allocated when the object was created from
	 * Java, too.  Here, we lazily allocate a new private DB_LSN if
	 * and only if it turns out that we don't already have one.
	 *
	 * The only exception is if the DbLsn object is a Java null
	 * (in which case the jobject will also be NULL). Then a NULL
	 * DB_LSN is legitimate.
	 */
	DB_LSN *lsnp;
	int err;

	if (obj == NULL)
		return (NULL);

	lsnp = (DB_LSN *)get_private_dbobj(jnienv, name_DB_LSN, obj);
	if (lsnp == NULL) {
		if ((err = __os_malloc(NULL, sizeof(DB_LSN), &lsnp)) != 0)
			return (NULL);
		memset(lsnp, 0, sizeof(DB_LSN));
		set_private_dbobj(jnienv, name_DB_LSN, obj, lsnp);
	}

	return (lsnp);
}

DB_MPOOL_FSTAT *get_DB_MPOOL_FSTAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_MPOOL_FSTAT *)
	    get_private_dbobj(jnienv, name_DB_MPOOL_FSTAT, obj));
}

DB_MPOOL_STAT *get_DB_MPOOL_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_MPOOL_STAT *)
	    get_private_dbobj(jnienv, name_DB_MPOOL_STAT, obj));
}

DB_QUEUE_STAT *get_DB_QUEUE_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_QUEUE_STAT *)
	    get_private_dbobj(jnienv, name_DB_QUEUE_STAT, obj));
}

DB_TXN *get_DB_TXN(JNIEnv *jnienv, jobject obj)
{
	return ((DB_TXN *)get_private_dbobj(jnienv, name_DB_TXN, obj));
}

DB_TXN_STAT *get_DB_TXN_STAT(JNIEnv *jnienv, jobject obj)
{
	return ((DB_TXN_STAT *)
	    get_private_dbobj(jnienv, name_DB_TXN_STAT, obj));
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

/*
 * Convert a C pointer to the various Java objects they represent.
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

jobject get_DbLogc(JNIEnv *jnienv, DB_LOGC *dbobj)
{
	return (convert_object(jnienv, name_DB_LOGC, dbobj));
}

jobject get_DbLogStat(JNIEnv *jnienv, DB_LOG_STAT *dbobj)
{
	return (convert_object(jnienv, name_DB_LOG_STAT, dbobj));
}

/*
 * LSNs are different since they are really normally
 * treated as by-value objects.  We actually create
 * a pointer to the LSN and store that, deleting it
 * when the LSN is GC'd.
 */
jobject get_DbLsn(JNIEnv *jnienv, DB_LSN dbobj)
{
	DB_LSN *lsnp;
	int err;

	if ((err = __os_malloc(NULL, sizeof(DB_LSN), &lsnp)) != 0)
		return (NULL);

	memset(lsnp, 0, sizeof(DB_LSN));
	*lsnp = dbobj;
	return (convert_object(jnienv, name_DB_LSN, lsnp));
}

/*
 * Shared code for get_Dbt and get_const_Dbt.
 *
 * XXX
 * Currently we make no distinction in implementation of these
 * two kinds of Dbts, although in the future we may want to.
 * (It's probably easier to make the optimizations listed below
 * with readonly Dbts).
 *
 * Dbt's created via this function are only used for a short lifetime,
 * during callback functions.  In the future, we should consider taking
 * advantage of this by having a pool of Dbt objects instead of creating
 * new ones each time.   Because of multithreading, we may need an
 * arbitrary number.  We might also have sharing of the byte arrays
 * used by the Dbts.
 */
static jobject get_Dbt_shared(JNIEnv *jnienv, const DBT *dbt, int readonly,
			      DBT_JAVAINFO **ret_info)
{
	jobject jdbt;
	DBT_JAVAINFO *dbtji;

	COMPQUIET(readonly, 0);

	/* A NULL DBT should become a null Dbt. */
	if (dbt == NULL)
		return (NULL);

	/*
	 * Note that a side effect of creating a Dbt object
	 * is the creation of the attached DBT_JAVAINFO object
	 * (see the native implementation of Dbt.init())
	 * A DBT_JAVAINFO object contains its own DBT.
	 */
	jdbt = create_default_object(jnienv, name_DBT);
	dbtji = get_DBT_JAVAINFO(jnienv, jdbt);
	memcpy(&dbtji->dbt, dbt, sizeof(DBT));

	/*
	 * Set the boolean indicator so that the Java side knows to
	 * call back when it wants to look at the array.  This avoids
	 * needlessly creating/copying arrays that may never be looked at.
	 */
	(*jnienv)->SetBooleanField(jnienv, jdbt, fid_Dbt_must_create_data, 1);
	(*jnienv)->SetIntField(jnienv, jdbt, fid_Dbt_size, dbt->size);

	if (ret_info != NULL)
	    *ret_info = dbtji;
	return (jdbt);
}

/*
 * Get a writeable Dbt.
 *
 * Currently we're sharing code with get_const_Dbt.
 * It really shouldn't be this way, we have a DBT that we can
 * change, and have some mechanism for copying back
 * any changes to the original DBT.
 */
jobject get_Dbt(JNIEnv *jnienv, DBT *dbt,
		DBT_JAVAINFO **ret_info)
{
	return (get_Dbt_shared(jnienv, dbt, 0, ret_info));
}

/*
 * Get a Dbt that we promise not to change, or at least
 * if there are changes, they don't matter and won't get
 * seen by anyone.
 */
jobject get_const_Dbt(JNIEnv *jnienv, const DBT *dbt,
		      DBT_JAVAINFO **ret_info)
{
	return (get_Dbt_shared(jnienv, dbt, 1, ret_info));
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
