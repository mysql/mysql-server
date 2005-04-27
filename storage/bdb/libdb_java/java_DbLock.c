/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbLock.c,v 11.12 2002/02/28 21:27:38 ubell Exp $";
#endif /* not lint */

#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "db_int.h"
#include "java_util.h"
#include "com_sleepycat_db_DbLock.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLock_finalize
  (JNIEnv *jnienv, jobject jthis)
{
	DB_LOCK *dblock = get_DB_LOCK(jnienv, jthis);
	if (dblock) {
		/* Free any data related to DB_LOCK here */
		__os_free(NULL, dblock);
	}
	set_private_dbobj(jnienv, name_DB_LOCK, jthis, 0); /* paranoia */
}
