/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbTxn.c,v 11.16 2002/08/06 05:19:05 bostic Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db_int.h"
#include "java_util.h"
#include "com_sleepycat_db_DbTxn.h"

JAVADB_METHOD(DbTxn_abort, (JAVADB_ARGS), DB_TXN,
    abort, (c_this))
JAVADB_METHOD(DbTxn_commit, (JAVADB_ARGS, jint flags), DB_TXN,
    commit, (c_this, flags))
JAVADB_METHOD(DbTxn_discard, (JAVADB_ARGS, jint flags), DB_TXN,
    discard, (c_this, flags))

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbTxn_id
  (JNIEnv *jnienv, jobject jthis)
{
	int retval = 0;
	DB_TXN *dbtxn = get_DB_TXN(jnienv, jthis);
	if (!verify_non_null(jnienv, dbtxn))
		return (-1);

	/* No error to check for from DB_TXN->id */
	retval = dbtxn->id(dbtxn);
	return (retval);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxn_prepare
  (JNIEnv *jnienv, jobject jthis, jbyteArray gid)
{
	int err;
	DB_TXN *dbtxn;
	jbyte *c_array;

	dbtxn = get_DB_TXN(jnienv, jthis);
	if (!verify_non_null(jnienv, dbtxn))
		return;

	if (gid == NULL ||
	    (*jnienv)->GetArrayLength(jnienv, gid) < DB_XIDDATASIZE) {
		report_exception(jnienv, "DbTxn.prepare gid array "
				 "must be >= 128 bytes", EINVAL, 0);
		return;
	}
	c_array = (*jnienv)->GetByteArrayElements(jnienv, gid, NULL);
	err = dbtxn->prepare(dbtxn, (u_int8_t *)c_array);
	(*jnienv)->ReleaseByteArrayElements(jnienv, gid, c_array, 0);
	verify_return(jnienv, err, 0);
}

JAVADB_METHOD(DbTxn_set_1timeout,
    (JAVADB_ARGS, jlong timeout, jint flags), DB_TXN,
    set_timeout, (c_this, (u_int32_t)timeout, flags))
