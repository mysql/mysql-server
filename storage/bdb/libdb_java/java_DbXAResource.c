/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2001
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbXAResource.c,v 11.6 2002/08/06 05:19:06 bostic Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef DIAGNOSTIC
#include <stdio.h>
#endif

#include "db_int.h"
#include "java_util.h"
#include "dbinc/xa.h"
#include "dbinc_auto/xa_ext.h"
#include "com_sleepycat_db_xa_DbXAResource.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1init
  (JNIEnv *jnienv, jobject jthis, jstring home, jint rmid, jint flags)
{
	int err;
	LOCKED_STRING ls_home;
	jclass cl;
	jmethodID mid;

	COMPQUIET(jthis, NULL);
	if (locked_string_get(&ls_home, jnienv, home) != 0)
		goto out;
	if ((err = __db_xa_open((char *)ls_home.string,
				rmid, flags)) != XA_OK) {
		verify_return(jnienv, err, EXCEPTION_XA);
	}

	/*
	 * Now create the DbEnv object, it will get attached
	 * to the DB_ENV just made in __db_xa_open.
	 */
	if ((cl = get_class(jnienv, name_DB_ENV)) == NULL)
		goto out;

	mid = (*jnienv)->GetStaticMethodID(jnienv, cl,
					   "_create_DbEnv_for_XA", "(II)V");
	(*jnienv)->CallStaticVoidMethod(jnienv, cl, mid, 0, rmid);

 out:
	locked_string_put(&ls_home, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1close
  (JNIEnv *jnienv, jobject jthis, jstring home, jint rmid, jint flags)
{
	int err;
	LOCKED_STRING ls_home;

	COMPQUIET(jthis, NULL);
	if (locked_string_get(&ls_home, jnienv, home) != 0)
		goto out;
	if ((err = __db_xa_close((char *)ls_home.string,
				 rmid, flags)) != XA_OK)
		verify_return(jnienv, err, EXCEPTION_XA);
 out:
	locked_string_put(&ls_home, jnienv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1commit
  (JNIEnv *jnienv, jobject jthis, jobject jxid, jint rmid,
   jboolean onePhase)
{
	XID xid;
	long flags;
	int err;

	COMPQUIET(jthis, NULL);
	if (!get_XID(jnienv, jxid, &xid))
		return;
	flags = 0;
	if (onePhase == JNI_TRUE)
		flags |= TMONEPHASE;
	if ((err = __db_xa_commit(&xid, rmid, flags)) != XA_OK)
		verify_return(jnienv, err, EXCEPTION_XA);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1end
  (JNIEnv *jnienv, jobject jthis, jobject jxid, jint rmid, jint flags)
{
	XID xid;
	int err;

	COMPQUIET(jthis, NULL);
	if (!get_XID(jnienv, jxid, &xid))
		return;
	if ((err = __db_xa_end(&xid, rmid, flags)) != XA_OK)
		verify_return(jnienv, err, EXCEPTION_XA);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1forget
  (JNIEnv *jnienv, jobject jthis, jobject jxid, jint rmid)
{
	XID xid;
	int err;

	COMPQUIET(jthis, NULL);
	if (!get_XID(jnienv, jxid, &xid))
		return;
	if ((err = __db_xa_forget(&xid, rmid, 0)) != XA_OK)
		verify_return(jnienv, err, EXCEPTION_XA);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_xa_DbXAResource__1prepare
  (JNIEnv *jnienv, jobject jthis, jobject jxid, jint rmid)
{
	XID xid;
	int err;

	COMPQUIET(jthis, NULL);
	if (!get_XID(jnienv, jxid, &xid))
		return (0);
	err = __db_xa_prepare(&xid, rmid, 0);
	if (err != XA_OK && err != XA_RDONLY)
		verify_return(jnienv, err, EXCEPTION_XA);

	return (err);
}

JNIEXPORT jobjectArray JNICALL Java_com_sleepycat_db_xa_DbXAResource__1recover
  (JNIEnv *jnienv, jobject jthis, jint rmid, jint flags)
{
	XID *xids;
	int err;
	int total;
	int cnt;
	int i;
	int curflags;
	size_t nbytes;
	jclass xid_class;
	jmethodID mid;
	jobject obj;
	jobjectArray retval;

	COMPQUIET(jthis, NULL);
	total = 0;
	cnt = 0;
	xids = NULL;
	flags &= ~(DB_FIRST | DB_LAST | DB_NEXT);

	/* Repeatedly call __db_xa_recover to fill up an array of XIDs */
	curflags = flags | DB_FIRST;
	do {
		total += cnt;
		nbytes = sizeof(XID) * (total + 10);
		if ((err = __os_realloc(NULL, nbytes, &xids)) != 0) {
			if (xids != NULL)
				__os_free(NULL, xids);
			verify_return(jnienv, XAER_NOTA, EXCEPTION_XA);
			return (NULL);
		}
		cnt = __db_xa_recover(&xids[total], 10, rmid, curflags);
		curflags = flags | DB_NEXT;
	} while (cnt > 0);

	if (xids != NULL)
		__os_free(NULL, xids);

	if (cnt < 0) {
		verify_return(jnienv, cnt, EXCEPTION_XA);
		return (NULL);
	}

	/* Create the java DbXid array and fill it up */
	if ((xid_class = get_class(jnienv, name_DB_XID)) == NULL)
		return (NULL);
	mid = (*jnienv)->GetMethodID(jnienv, xid_class, "<init>",
				     "(I[B[B)V");
	if ((retval = (*jnienv)->NewObjectArray(jnienv, total, xid_class, 0))
	    == NULL)
		goto out;

	for (i = 0; i < total; i++) {
		jobject gtrid;
		jobject bqual;
		jsize gtrid_len;
		jsize bqual_len;

		gtrid_len = (jsize)xids[i].gtrid_length;
		bqual_len = (jsize)xids[i].bqual_length;
		gtrid = (*jnienv)->NewByteArray(jnienv, gtrid_len);
		bqual = (*jnienv)->NewByteArray(jnienv, bqual_len);
		if (gtrid == NULL || bqual == NULL)
			goto out;
		(*jnienv)->SetByteArrayRegion(jnienv, gtrid, 0, gtrid_len,
		    (jbyte *)&xids[i].data[0]);
		(*jnienv)->SetByteArrayRegion(jnienv, bqual, 0, bqual_len,
		    (jbyte *)&xids[i].data[gtrid_len]);
		if ((obj = (*jnienv)->NewObject(jnienv, xid_class, mid,
		    (jint)xids[i].formatID, gtrid, bqual)) == NULL)
			goto out;
		(*jnienv)->SetObjectArrayElement(jnienv, retval, i, obj);
	}
out:	return (retval);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1rollback
  (JNIEnv *jnienv, jobject jthis, jobject jxid, jint rmid)
{
	XID xid;
	int err;

	COMPQUIET(jthis, NULL);
	if (!get_XID(jnienv, jxid, &xid))
		return;
	if ((err = __db_xa_rollback(&xid, rmid, 0)) != XA_OK)
		verify_return(jnienv, err, EXCEPTION_XA);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_xa_DbXAResource__1start
  (JNIEnv *jnienv, jobject jthis, jobject jxid, jint rmid, jint flags)
{
	XID xid;
	int err;

	COMPQUIET(jthis, NULL);
	if (!get_XID(jnienv, jxid, &xid))
		return;

	if ((err = __db_xa_start(&xid, rmid, flags)) != XA_OK)
		verify_return(jnienv, err, EXCEPTION_XA);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_xa_DbXAResource_xa_1attach
  (JNIEnv *jnienv, jclass jthisclass, jobject jxid, jobject jrmid)
{
	XID xid;
	XID *xidp;
	int ret;
	DB_ENV *env;
	DB_TXN *txn;
	int rmid;
	int *rmidp;
	jobject jtxn;
	jobject jenv;
	jclass cl;
	jmethodID mid;

	COMPQUIET(jthisclass, NULL);
	if (jxid == NULL) {
		xidp = NULL;
	}
	else {
		xidp = &xid;
		if (!get_XID(jnienv, jxid, &xid))
			return (NULL);
	}
	if (jrmid == NULL) {
		rmidp = NULL;
	}
	else {
		rmidp = &rmid;
		rmid = (int)(*jnienv)->CallIntMethod(jnienv, jrmid,
						     mid_Integer_intValue);
	}

	if ((ret = db_env_xa_attach(rmidp, xidp, &env, &txn)) != 0) {
		/*
		 * DB_NOTFOUND is a normal return, it means we
		 * have no current transaction,
		 */
		if (ret != DB_NOTFOUND)
			verify_return(jnienv, ret, 0);
		return (NULL);
	}

	jenv = ((DB_ENV_JAVAINFO *)env->api2_internal)->jenvref;
	jtxn = get_DbTxn(jnienv, txn);
	if ((cl = get_class(jnienv, name_DB_XAATTACH)) == NULL)
		return (NULL);
	mid = (*jnienv)->GetMethodID(jnienv, cl, "<init>",
		     "(Lcom/sleepycat/db/DbEnv;Lcom/sleepycat/db/DbTxn;)V");
	return (*jnienv)->NewObject(jnienv, cl, mid, jenv, jtxn);
}
