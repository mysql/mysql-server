/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_locked.c,v 11.11 2000/10/25 19:54:55 dda Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"

/****************************************************************
 *
 * Implementation of class LockedDBT
 *
 */
int
jdbt_lock(JDBT *jdbt, JNIEnv *jnienv, jobject obj, OpKind kind)
{
	DBT *dbt;

	jdbt->obj_ = obj;
	jdbt->do_realloc_ = 0;
	jdbt->kind_ = kind;
	jdbt->java_array_len_= 0;
	jdbt->java_data_ = 0;
	jdbt->before_data_ = 0;
	jdbt->has_error_ = 0;
	jdbt->dbt = (DBT_JAVAINFO *)get_private_dbobj(jnienv, name_DBT, obj);

	if (!verify_non_null(jnienv, jdbt->dbt)) {
		jdbt->has_error_ = 1;
		return (EINVAL);
	}
	dbt = &jdbt->dbt->dbt;

	if (kind == outOp &&
	    (dbt->flags & (DB_DBT_USERMEM | DB_DBT_MALLOC | DB_DBT_REALLOC)) == 0) {
		report_exception(jnienv,
				 "Dbt.flags must be set to Db.DB_DBT_USERMEM, "
				 "Db.DB_DBT_MALLOC or Db.DB_DBT_REALLOC",
				 0, 0);
		jdbt->has_error_ = 1;
		return (EINVAL);
	}

	/* If this is requested to be realloc, we cannot use the
	 * underlying realloc, because the array we will pass in
	 * is not allocated by us, but the Java VM, so it cannot
	 * be successfully realloced.  We simulate the reallocation,
	 * by using USERMEM and reallocating the java array when a
	 * ENOMEM error occurs.  We change the flags during the operation,
	 * and they are reset when the operation completes (in the
	 * LockedDBT destructor.
	 */
	if ((dbt->flags & DB_DBT_REALLOC) != 0) {
		dbt->flags &= ~DB_DBT_REALLOC;
		dbt->flags |= DB_DBT_USERMEM;
		jdbt->do_realloc_ = 1;
	}

	if ((dbt->flags & DB_DBT_USERMEM) || kind != outOp) {

		/* If writing with DB_DBT_USERMEM/REALLOC
		 * or it's a set (or get/set) operation,
		 * then the data should point to a java array.
		 * Note that outOp means data is coming out of the database
		 * (it's a get).  inOp means data is going into the database
		 * (either a put, or a key input).
		 */
		if (!jdbt->dbt->array_) {
			report_exception(jnienv, "Dbt.data is null", 0, 0);
			jdbt->has_error_ = 1;
			return (EINVAL);
		}

		/* Verify other parameters */
		jdbt->java_array_len_ = (*jnienv)->GetArrayLength(jnienv, jdbt->dbt->array_);
		if (jdbt->dbt->offset_ < 0 ) {
			report_exception(jnienv, "Dbt.offset illegal", 0, 0);
			jdbt->has_error_ = 1;
			return (EINVAL);
		}
		if (dbt->ulen + jdbt->dbt->offset_ > jdbt->java_array_len_) {
			report_exception(jnienv,
			 "Dbt.ulen + Dbt.offset greater than array length", 0, 0);
			jdbt->has_error_ = 1;
			return (EINVAL);
		}

		jdbt->java_data_ = (*jnienv)->GetByteArrayElements(jnienv, jdbt->dbt->array_,
							  (jboolean *)0);
		dbt->data = jdbt->before_data_ = jdbt->java_data_ + jdbt->dbt->offset_;
	}
	else {

		/* If writing with DB_DBT_MALLOC, then the data is
		 * allocated by DB.
		 */
		dbt->data = jdbt->before_data_ = 0;
	}
	return (0);
}

/* The LockedDBT destructor is called when the java handler returns
 * to the user, since that's when the LockedDBT objects go out of scope.
 * Since it is thus called after any call to the underlying database,
 * it copies any information from temporary structures back to user
 * accessible arrays, and of course must free memory and remove references.
 */
void
jdbt_unlock(JDBT *jdbt, JNIEnv *jnienv)
{
	DBT *dbt;

	dbt = &jdbt->dbt->dbt;

	/* Fix up the flags if we changed them. */
	if (jdbt->do_realloc_) {
		dbt->flags &= ~DB_DBT_USERMEM;
		dbt->flags |= DB_DBT_REALLOC;
	}

	if ((dbt->flags & (DB_DBT_USERMEM | DB_DBT_REALLOC)) ||
	    jdbt->kind_ == inOp) {

		/* If writing with DB_DBT_USERMEM/REALLOC or it's a set
		 * (or get/set) operation, then the data may be already in
		 * the java array, in which case, we just need to release it.
		 * If DB didn't put it in the array (indicated by the
		 * dbt->data changing), we need to do that
		 */
		if (jdbt->before_data_ != jdbt->java_data_) {
			(*jnienv)->SetByteArrayRegion(jnienv,
						      jdbt->dbt->array_,
						      jdbt->dbt->offset_,
						      dbt->ulen,
						      jdbt->before_data_);
		}
		(*jnienv)->ReleaseByteArrayElements(jnienv, jdbt->dbt->array_, jdbt->java_data_, 0);
		dbt->data = 0;
	}
	if ((dbt->flags & DB_DBT_MALLOC) && jdbt->kind_ != inOp) {

		/* If writing with DB_DBT_MALLOC, then the data was allocated
		 * by DB.  If dbt->data is zero, it means an error occurred
		 * (and should have been already reported).
		 */
		if (dbt->data) {

			/* Release any old references. */
			dbjit_release(jdbt->dbt, jnienv);

			/* In the case of SET_RANGE, the key is inOutOp
			 * and when not found, its data will be left as
			 * its original value.  Only copy and free it
			 * here if it has been allocated by DB
			 * (dbt->data has changed).
			 */
			if (dbt->data != jdbt->before_data_) {
				jdbt->dbt->array_ = (jbyteArray)
					NEW_GLOBAL_REF(jnienv,
					   (*jnienv)->NewByteArray(jnienv,
								   dbt->size));
				jdbt->dbt->offset_ = 0;
				(*jnienv)->SetByteArrayRegion(jnienv,
					      jdbt->dbt->array_, 0, dbt->size,
					      (jbyte *)dbt->data);
				free(dbt->data);
				dbt->data = 0;
			}
		}
	}
}

/* Realloc the java array to receive data if the DBT was marked
 * for realloc, and the last operation set the size field to an
 * amount greater than ulen.
 */
int jdbt_realloc(JDBT *jdbt, JNIEnv *jnienv)
{
	DBT *dbt;

	dbt = &jdbt->dbt->dbt;

	if (!jdbt->do_realloc_ || jdbt->has_error_ || dbt->size <= dbt->ulen)
		return (0);

	(*jnienv)->ReleaseByteArrayElements(jnienv, jdbt->dbt->array_, jdbt->java_data_, 0);
	dbjit_release(jdbt->dbt, jnienv);

	/* We allocate a new array of the needed size.
	 * We'll set the offset to 0, as the old offset
	 * really doesn't make any sense.
	 */
	jdbt->java_array_len_ = dbt->ulen = dbt->size;
	jdbt->dbt->offset_ = 0;
	jdbt->dbt->array_ = (jbyteArray)
		NEW_GLOBAL_REF(jnienv, (*jnienv)->NewByteArray(jnienv, dbt->size));

	jdbt->java_data_ = (*jnienv)->GetByteArrayElements(jnienv,
						     jdbt->dbt->array_,
						     (jboolean *)0);
	dbt->data = jdbt->before_data_ = jdbt->java_data_;
	return (1);
}

/****************************************************************
 *
 * Implementation of class JSTR
 *
 */
int
jstr_lock(JSTR *js, JNIEnv *jnienv, jstring jstr)
{
	js->jstr_ = jstr;

	if (jstr == 0)
		js->string = 0;
	else
		js->string = (*jnienv)->GetStringUTFChars(jnienv, jstr,
							  (jboolean *)0);
	return (0);
}

void jstr_unlock(JSTR *js, JNIEnv *jnienv)
{
	if (js->jstr_)
		(*jnienv)->ReleaseStringUTFChars(jnienv, js->jstr_, js->string);
}

/****************************************************************
 *
 * Implementation of class JSTRARRAY
 *
 */
int
jstrarray_lock(JSTRARRAY *jsa, JNIEnv *jnienv, jobjectArray arr)
{
	int i;

	jsa->arr_ = arr;
	jsa->array = 0;

	if (arr != 0) {
		int count = (*jnienv)->GetArrayLength(jnienv, arr);
		const char **new_array =
			(const char **)malloc((sizeof(const char *))*(count+1));
		for (i=0; i<count; i++) {
			jstring jstr = (jstring)(*jnienv)->GetObjectArrayElement(jnienv, arr, i);
			if (jstr == 0) {
				/*
				 * An embedded null in the string array
				 * is treated as an endpoint.
				 */
				new_array[i] = 0;
				break;
			}
			else {
				new_array[i] =
					(*jnienv)->GetStringUTFChars(jnienv, jstr, (jboolean *)0);
			}
		}
		new_array[count] = 0;
		jsa->array = new_array;
	}
	return (0);
}

void jstrarray_unlock(JSTRARRAY *jsa, JNIEnv *jnienv)
{
	int i;
	jstring jstr;

	if (jsa->arr_) {
		int count = (*jnienv)->GetArrayLength(jnienv, jsa->arr_);
		for (i=0; i<count; i++) {
			if (jsa->array[i] == 0)
				break;
			jstr = (jstring)(*jnienv)->GetObjectArrayElement(jnienv, jsa->arr_, i);
			(*jnienv)->ReleaseStringUTFChars(jnienv, jstr, jsa->array[i]);
		}
		free((void*)jsa->array);
	}
}
