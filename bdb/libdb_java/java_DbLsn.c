/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: java_DbLsn.c,v 11.5 2000/11/30 00:58:39 ubell Exp $";
#endif /* not lint */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>              /* needed for FILENAME_MAX */

#include "db.h"
#include "db_int.h"
#include "java_util.h"
#include "com_sleepycat_db_DbLsn.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLsn_init_1lsn
  (JNIEnv *jnienv, /*DbLsn*/ jobject jthis)
{
	/* Note: the DB_LSN object stored in the private_dbobj_
	 * is allocated in get_DbLsn().
	 */

	COMPQUIET(jnienv, NULL);
	COMPQUIET(jthis, NULL);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLsn_finalize
  (JNIEnv *jnienv, jobject jthis)
{
	DB_LSN *dblsn;

	dblsn = get_DB_LSN(jnienv, jthis);
	if (dblsn) {
		free(dblsn);
	}
}
