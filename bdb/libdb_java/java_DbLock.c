/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbLock.c,v 11.4 2000/11/30 00:58:39 ubell Exp $";
#endif /* not lint */

#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbLock.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLock_put
  (JNIEnv *jnienv, jobject jthis, /*DbEnv*/ jobject env)
{
	int err;
	DB_LOCK *dblock = get_DB_LOCK(jnienv, jthis);
	DB_ENV *dbenv = get_DB_ENV(jnienv, env);

	if (!verify_non_null(jnienv, dbenv))
		return;

	if (!verify_non_null(jnienv, dblock))
		return;

	err = lock_put(dbenv, dblock);
	if (verify_return(jnienv, err, 0)) {
		/* After a successful put, the DbLock can no longer
		 * be used, so we release the storage related to it
		 * (allocated in DbEnv.lock_get() or lock_tget()).
		 */
		free(dblock);

		set_private_dbobj(jnienv, name_DB_LOCK, jthis, 0);
	}
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLock_finalize
  (JNIEnv *jnienv, jobject jthis)
{
	DB_LOCK *dblock = get_DB_LOCK(jnienv, jthis);
	if (dblock) {
		/* Free any data related to DB_LOCK here */
		free(dblock);
	}
	set_private_dbobj(jnienv, name_DB_LOCK, jthis, 0); /* paranoia */
}
