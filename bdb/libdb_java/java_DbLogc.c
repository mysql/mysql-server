/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbLogc.c,v 11.6 2002/07/02 12:03:03 mjc Exp $";
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
#include "com_sleepycat_db_DbLogc.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLogc_close
  (JNIEnv *jnienv, jobject jthis, jint flags)
{
	int err;
	DB_LOGC *dblogc = get_DB_LOGC(jnienv, jthis);

	if (!verify_non_null(jnienv, dblogc))
		return;
	err = dblogc->close(dblogc, flags);
	if (verify_return(jnienv, err, 0)) {
		set_private_dbobj(jnienv, name_DB_LOGC, jthis, 0);
	}
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbLogc_get
  (JNIEnv *jnienv, jobject jthis,
   /*DbLsn*/ jobject lsn, /*Dbt*/ jobject data, jint flags)
{
	int err, retry;
	DB_LOGC *dblogc;
	DB_LSN *dblsn;
	LOCKED_DBT ldata;
	OpKind dataop;

	/*
	 * Depending on flags, the user may be supplying the key,
	 * or else we may have to retrieve it.
	 */
	err = 0;
	dataop = outOp;

	dblogc = get_DB_LOGC(jnienv, jthis);
	dblsn = get_DB_LSN(jnienv, lsn);
	if (locked_dbt_get(&ldata, jnienv, dblogc->dbenv, data, dataop) != 0)
		goto out1;

	if (!verify_non_null(jnienv, dblogc))
		goto out1;

	for (retry = 0; retry < 3; retry++) {
		err = dblogc->get(dblogc, dblsn, &ldata.javainfo->dbt, flags);

		/*
		 * If we failed due to lack of memory in our DBT arrays,
		 * retry.
		 */
		if (err != ENOMEM)
			break;
		if (!locked_dbt_realloc(&ldata, jnienv, dblogc->dbenv))
			break;
	}
 out1:
	locked_dbt_put(&ldata, jnienv, dblogc->dbenv);
	if (!DB_RETOK_LGGET(err)) {
		if (verify_dbt(jnienv, err, &ldata))
			verify_return(jnienv, err, 0);
	}
	return (err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLogc_finalize
  (JNIEnv *jnienv, jobject jthis)
{
	/*
	 * Free any data related to DB_LOGC here.
	 * If we ever have java-only data embedded in the DB_LOGC
	 * and need to do this, we'll have to track DbLogc's
	 * according to which DbEnv owns them, just as
	 * we track Db's according to which DbEnv owns them.
	 * That's necessary to avoid double freeing that
	 * comes about when closes interact with GC.
	 */

#ifdef DIAGNOSTIC
	DB_LOGC *dblogc;

	dblogc = get_DB_LOGC(jnienv, jthis);
	if (dblogc != NULL)
		fprintf(stderr, "Java API: DbLogc has not been closed\n");
#else

	COMPQUIET(jnienv, NULL);
	COMPQUIET(jthis, NULL);

#endif
}
