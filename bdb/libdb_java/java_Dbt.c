/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_Dbt.c,v 11.10 2000/10/25 19:54:55 dda Exp $";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_Dbt.h"

JAVADB_RW_ACCESS(Dbt, jint, size, DBT, size)
JAVADB_RW_ACCESS(Dbt, jint, ulen, DBT, ulen)
JAVADB_RW_ACCESS(Dbt, jint, dlen, DBT, dlen)
JAVADB_RW_ACCESS(Dbt, jint, doff, DBT, doff)
JAVADB_RW_ACCESS(Dbt, jint, flags, DBT, flags)

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_init
  (JNIEnv *jnienv, jobject jthis)
{
	DBT_JAVAINFO *dbtji;

	dbtji = dbjit_construct();
	set_private_dbobj(jnienv, name_DBT, jthis, dbtji);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_internal_1set_1data
  (JNIEnv *jnienv, jobject jthis, jbyteArray array)
{
	DBT_JAVAINFO *db_this;

	db_this = get_DBT_JAVAINFO(jnienv, jthis);
	if (verify_non_null(jnienv, db_this)) {

		/* If we previously allocated an array for java,
		 * must release reference.
		 */
		dbjit_release(db_this, jnienv);

		/* Make the array a global ref,
		 * it won't be GC'd till we release it.
		 */
		if (array)
			array = (jbyteArray)NEW_GLOBAL_REF(jnienv, array);
		db_this->array_ = array;
	}
}

JNIEXPORT jbyteArray JNICALL Java_com_sleepycat_db_Dbt_get_1data
  (JNIEnv *jnienv, jobject jthis)
{
	DBT_JAVAINFO *db_this;
	jbyteArray arr;
	int len;

	db_this = get_DBT_JAVAINFO(jnienv, jthis);
	if (verify_non_null(jnienv, db_this)) {
		/* XXX this will copy the data on each call to get_data,
		 * even if it is unchanged.
		 */
		if (db_this->create_array_ != 0) {
			/* XXX we should reuse the existing array if we can */
			len = db_this->dbt.size;
			if (db_this->array_ != NULL)
				DELETE_GLOBAL_REF(jnienv, db_this->array_);
			arr = (*jnienv)->NewByteArray(jnienv, len);
			db_this->array_ =
				(jbyteArray)NEW_GLOBAL_REF(jnienv, arr);
			(*jnienv)->SetByteArrayRegion(jnienv, arr, 0, len,
						      db_this->dbt.data);
		}
		return (db_this->array_);
	}
	return (0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_set_1offset
  (JNIEnv *jnienv, jobject jthis, jint offset)
{
	DBT_JAVAINFO *db_this;

	db_this = get_DBT_JAVAINFO(jnienv, jthis);
	if (verify_non_null(jnienv, db_this)) {
		db_this->offset_ = offset;
	}
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Dbt_get_1offset
  (JNIEnv *jnienv, jobject jthis)
{
	DBT_JAVAINFO *db_this;

	db_this = get_DBT_JAVAINFO(jnienv, jthis);
	if (verify_non_null(jnienv, db_this)) {
		return db_this->offset_;
	}
	return (0);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_set_1recno_1key_1data(JNIEnv *jnienv, jobject jthis, jint value)
{
	JDBT jdbt;

	if (jdbt_lock(&jdbt, jnienv, jthis, inOp) != 0)
		goto out;

	if (!jdbt.dbt->dbt.data ||
	    jdbt.java_array_len_ < sizeof(db_recno_t)) {
		char buf[200];
		sprintf(buf, "set_recno_key_data error: %p %p %d %d",
			&jdbt.dbt->dbt, jdbt.dbt->dbt.data,
			jdbt.dbt->dbt.ulen, sizeof(db_recno_t));
		report_exception(jnienv, buf, 0, 0);
	}
	else {
		*(db_recno_t*)(jdbt.dbt->dbt.data) = value;
	}
 out:
	jdbt_unlock(&jdbt, jnienv);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Dbt_get_1recno_1key_1data(JNIEnv *jnienv, jobject jthis)
{
	jint ret;
	JDBT jdbt;

	ret = 0;

	/* Although this is kind of like "retrieve", we don't support
	 * DB_DBT_MALLOC for this operation, so we tell jdbt_lock
	 * that is not a retrieve.
	 */
	if (jdbt_lock(&jdbt, jnienv, jthis, inOp) != 0)
		goto out;

	if (!jdbt.dbt->dbt.data ||
	    jdbt.java_array_len_ < sizeof(db_recno_t)) {
		char buf[200];
		sprintf(buf, "get_recno_key_data error: %p %p %d %d",
			&jdbt.dbt->dbt, jdbt.dbt->dbt.data,
			jdbt.dbt->dbt.ulen, sizeof(db_recno_t));
		report_exception(jnienv, buf, 0, 0);
	}
	else {
		ret = *(db_recno_t*)(jdbt.dbt->dbt.data);
	}
 out:
	jdbt_unlock(&jdbt, jnienv);
	return (ret);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_finalize
  (JNIEnv *jnienv, jobject jthis)
{
	DBT_JAVAINFO *dbtji;

	dbtji = get_DBT_JAVAINFO(jnienv, jthis);
	if (dbtji) {
		/* Free any data related to DBT here */
		dbjit_release(dbtji, jnienv);

		/* Extra paranoia */
		memset(dbtji, 0, sizeof(DBT_JAVAINFO));
		free(dbtji);
	}
}
