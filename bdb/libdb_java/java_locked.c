/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_locked.c,v 11.32 2002/08/06 05:19:07 bostic Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db_int.h"
#include "java_util.h"

/****************************************************************
 *
 * Implementation of functions to manipulate LOCKED_DBT.
 */
int
locked_dbt_get(LOCKED_DBT *ldbt, JNIEnv *jnienv, DB_ENV *dbenv,
	       jobject jdbt, OpKind kind)
{
	DBT *dbt;

	COMPQUIET(dbenv, NULL);
	ldbt->jdbt = jdbt;
	ldbt->java_array_len = 0;
	ldbt->flags = 0;
	ldbt->kind = kind;
	ldbt->java_data = 0;
	ldbt->before_data = 0;
	ldbt->javainfo =
		(DBT_JAVAINFO *)get_private_dbobj(jnienv, name_DBT, jdbt);

	if (!verify_non_null(jnienv, ldbt->javainfo)) {
		report_exception(jnienv, "Dbt is gc'ed?", 0, 0);
		F_SET(ldbt, LOCKED_ERROR);
		return (EINVAL);
	}
	if (F_ISSET(ldbt->javainfo, DBT_JAVAINFO_LOCKED)) {
		report_exception(jnienv, "Dbt is already in use", 0, 0);
		F_SET(ldbt, LOCKED_ERROR);
		return (EINVAL);
	}
	dbt = &ldbt->javainfo->dbt;

	if ((*jnienv)->GetBooleanField(jnienv,
	    jdbt, fid_Dbt_must_create_data) != 0)
		F_SET(ldbt, LOCKED_CREATE_DATA);
	else
		ldbt->javainfo->array =
			(*jnienv)->GetObjectField(jnienv, jdbt, fid_Dbt_data);

	dbt->size = (*jnienv)->GetIntField(jnienv, jdbt, fid_Dbt_size);
	dbt->ulen = (*jnienv)->GetIntField(jnienv, jdbt, fid_Dbt_ulen);
	dbt->dlen = (*jnienv)->GetIntField(jnienv, jdbt, fid_Dbt_dlen);
	dbt->doff = (*jnienv)->GetIntField(jnienv, jdbt, fid_Dbt_doff);
	dbt->flags = (*jnienv)->GetIntField(jnienv, jdbt, fid_Dbt_flags);
	ldbt->javainfo->offset = (*jnienv)->GetIntField(jnienv, jdbt,
						    fid_Dbt_offset);

	/*
	 * If no flags are set, use default behavior of DB_DBT_MALLOC.
	 * We can safely set dbt->flags because flags will never be copied
	 * back to the Java Dbt.
	 */
	if (kind != inOp &&
	    !F_ISSET(dbt, DB_DBT_USERMEM | DB_DBT_MALLOC | DB_DBT_REALLOC))
		F_SET(dbt, DB_DBT_MALLOC);

	/*
	 * If this is requested to be realloc with an existing array,
	 * we cannot use the underlying realloc, because the array we
	 * will pass in is allocated by the Java VM, not us, so it
	 * cannot be realloced.  We simulate the reallocation by using
	 * USERMEM and reallocating the java array when a ENOMEM error
	 * occurs.  We change the flags during the operation, and they
	 * are reset when the operation completes (in locked_dbt_put).
	 */
	if (F_ISSET(dbt, DB_DBT_REALLOC) && ldbt->javainfo->array != NULL) {
		F_CLR(dbt, DB_DBT_REALLOC);
		F_SET(dbt, DB_DBT_USERMEM);
		F_SET(ldbt, LOCKED_REALLOC_NONNULL);
	}

	if ((F_ISSET(dbt, DB_DBT_USERMEM) || kind != outOp) &&
	    !F_ISSET(ldbt, LOCKED_CREATE_DATA)) {

		/*
		 * If writing with DB_DBT_USERMEM
		 * or it's a set (or get/set) operation,
		 * then the data should point to a java array.
		 * Note that outOp means data is coming out of the database
		 * (it's a get).  inOp means data is going into the database
		 * (either a put, or a key input).
		 */
		if (!ldbt->javainfo->array) {
			report_exception(jnienv, "Dbt.data is null", 0, 0);
			F_SET(ldbt, LOCKED_ERROR);
			return (EINVAL);
		}

		/* Verify other parameters */
		ldbt->java_array_len = (*jnienv)->GetArrayLength(jnienv,
							ldbt->javainfo->array);
		if (ldbt->javainfo->offset < 0 ) {
			report_exception(jnienv, "Dbt.offset illegal", 0, 0);
			F_SET(ldbt, LOCKED_ERROR);
			return (EINVAL);
		}
		if (dbt->size + ldbt->javainfo->offset > ldbt->java_array_len) {
			report_exception(jnienv,
			 "Dbt.size + Dbt.offset greater than array length",
					 0, 0);
			F_SET(ldbt, LOCKED_ERROR);
			return (EINVAL);
		}

		ldbt->java_data = (*jnienv)->GetByteArrayElements(jnienv,
						ldbt->javainfo->array,
						(jboolean *)0);

		dbt->data = ldbt->before_data = ldbt->java_data +
			ldbt->javainfo->offset;
	}
	else if (!F_ISSET(ldbt, LOCKED_CREATE_DATA)) {

		/*
		 * If writing with DB_DBT_MALLOC or DB_DBT_REALLOC with
		 * a null array, then the data is allocated by DB.
		 */
		dbt->data = ldbt->before_data = 0;
	}

	/*
	 * RPC makes the assumption that if dbt->size is non-zero, there
	 * is data to copy from dbt->data.  We may have set dbt->size
	 * to a non-zero integer above but decided not to point
	 * dbt->data at anything.  (One example is if we're doing an outOp
	 * with an already-used Dbt whose values we expect to just
	 * overwrite.)
	 *
	 * Clean up the dbt fields so we don't run into trouble.
	 * (Note that doff, dlen, and flags all may contain meaningful
	 * values.)
	 */
	if (dbt->data == NULL)
		dbt->size = dbt->ulen = 0;

	F_SET(ldbt->javainfo, DBT_JAVAINFO_LOCKED);
	return (0);
}

/*
 * locked_dbt_put must be called for any LOCKED_DBT struct before a
 * java handler returns to the user.  It can be thought of as the
 * LOCKED_DBT destructor.  It copies any information from temporary
 * structures back to user accessible arrays, and of course must free
 * memory and remove references.  The LOCKED_DBT itself is not freed,
 * as it is expected to be a stack variable.
 *
 * Note that after this call, the LOCKED_DBT can still be used in
 * limited ways, e.g. to look at values in the C DBT.
 */
void
locked_dbt_put(LOCKED_DBT *ldbt, JNIEnv *jnienv, DB_ENV *dbenv)
{
	DBT *dbt;

	dbt = &ldbt->javainfo->dbt;

	/*
	 * If the error flag was set, we never succeeded
	 * in allocating storage.
	 */
	if (F_ISSET(ldbt, LOCKED_ERROR))
		return;

	if (((F_ISSET(dbt, DB_DBT_USERMEM) ||
	      F_ISSET(ldbt, LOCKED_REALLOC_NONNULL)) ||
	     ldbt->kind == inOp) && !F_ISSET(ldbt, LOCKED_CREATE_DATA)) {

		/*
		 * If writing with DB_DBT_USERMEM or it's a set
		 * (or get/set) operation, then the data may be already in
		 * the java array, in which case, we just need to release it.
		 * If DB didn't put it in the array (indicated by the
		 * dbt->data changing), we need to do that
		 */
		if (ldbt->before_data != ldbt->java_data) {
			(*jnienv)->SetByteArrayRegion(jnienv,
						      ldbt->javainfo->array,
						      ldbt->javainfo->offset,
						      dbt->ulen,
						      ldbt->before_data);
		}
		(*jnienv)->ReleaseByteArrayElements(jnienv,
						    ldbt->javainfo->array,
						    ldbt->java_data, 0);
		dbt->data = 0;
	}
	else if (F_ISSET(dbt, DB_DBT_MALLOC | DB_DBT_REALLOC) &&
	    ldbt->kind != inOp && !F_ISSET(ldbt, LOCKED_CREATE_DATA)) {

		/*
		 * If writing with DB_DBT_MALLOC, or DB_DBT_REALLOC
		 * with a zero buffer, then the data was allocated by
		 * DB.  If dbt->data is zero, it means an error
		 * occurred (and should have been already reported).
		 */
		if (dbt->data) {

			/*
			 * In the case of SET_RANGE, the key is inOutOp
			 * and when not found, its data will be left as
			 * its original value.  Only copy and free it
			 * here if it has been allocated by DB
			 * (dbt->data has changed).
			 */
			if (dbt->data != ldbt->before_data) {
				jbyteArray newarr;

				if ((newarr = (*jnienv)->NewByteArray(jnienv,
				    dbt->size)) == NULL) {
					/* The JVM has posted an exception. */
					F_SET(ldbt, LOCKED_ERROR);
					return;
				}
				(*jnienv)->SetObjectField(jnienv, ldbt->jdbt,
							  fid_Dbt_data,
							  newarr);
				ldbt->javainfo->offset = 0;
				(*jnienv)->SetByteArrayRegion(jnienv,
					      newarr, 0, dbt->size,
					      (jbyte *)dbt->data);
				(void)__os_ufree(dbenv, dbt->data);
				dbt->data = 0;
			}
		}
	}

	/*
	 * The size field may have changed after a DB API call,
	 * so we set that back too.
	 */
	(*jnienv)->SetIntField(jnienv, ldbt->jdbt, fid_Dbt_size, dbt->size);
	ldbt->javainfo->array = NULL;
	F_CLR(ldbt->javainfo, DBT_JAVAINFO_LOCKED);
}

/*
 * Realloc the java array to receive data if the DBT used
 * DB_DBT_REALLOC flag with a non-null data array, and the last
 * operation set the size field to an amount greater than ulen.
 * Return 1 if these conditions are met, otherwise 0.  This is used
 * internally to simulate the operations needed for DB_DBT_REALLOC.
 */
int locked_dbt_realloc(LOCKED_DBT *ldbt, JNIEnv *jnienv, DB_ENV *dbenv)
{
	DBT *dbt;

	COMPQUIET(dbenv, NULL);
	dbt = &ldbt->javainfo->dbt;

	if (!F_ISSET(ldbt, LOCKED_REALLOC_NONNULL) ||
	    F_ISSET(ldbt, LOCKED_ERROR) || dbt->size <= dbt->ulen)
		return (0);

	(*jnienv)->ReleaseByteArrayElements(jnienv, ldbt->javainfo->array,
					    ldbt->java_data, 0);

	/*
	 * We allocate a new array of the needed size.
	 * We'll set the offset to 0, as the old offset
	 * really doesn't make any sense.
	 */
	if ((ldbt->javainfo->array = (*jnienv)->NewByteArray(jnienv,
	    dbt->size)) == NULL) {
		F_SET(ldbt, LOCKED_ERROR);
		return (0);
	}

	ldbt->java_array_len = dbt->ulen = dbt->size;
	ldbt->javainfo->offset = 0;
	(*jnienv)->SetObjectField(jnienv, ldbt->jdbt, fid_Dbt_data,
	    ldbt->javainfo->array);
	ldbt->java_data = (*jnienv)->GetByteArrayElements(jnienv,
	    ldbt->javainfo->array, (jboolean *)0);
	memcpy(ldbt->java_data, ldbt->before_data, dbt->ulen);
	dbt->data = ldbt->before_data = ldbt->java_data;
	return (1);
}

/****************************************************************
 *
 * Implementation of functions to manipulate LOCKED_STRING.
 */
int
locked_string_get(LOCKED_STRING *ls, JNIEnv *jnienv, jstring jstr)
{
	ls->jstr = jstr;

	if (jstr == 0)
		ls->string = 0;
	else
		ls->string = (*jnienv)->GetStringUTFChars(jnienv, jstr,
							  (jboolean *)0);
	return (0);
}

void locked_string_put(LOCKED_STRING *ls, JNIEnv *jnienv)
{
	if (ls->jstr)
		(*jnienv)->ReleaseStringUTFChars(jnienv, ls->jstr, ls->string);
}
