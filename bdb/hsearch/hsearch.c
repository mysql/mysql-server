/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993
 *	Margo Seltzer.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: hsearch.c,v 11.5 2000/11/30 00:58:37 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#define	DB_DBM_HSEARCH	1
#include "db_int.h"

static DB	*dbp;
static ENTRY	 retval;

int
__db_hcreate(nel)
	size_t nel;
{
	int ret;

	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		__os_set_errno(ret);
		return (1);
	}

	if ((ret = dbp->set_pagesize(dbp, 512)) != 0 ||
	    (ret = dbp->set_h_ffactor(dbp, 16)) != 0 ||
	    (ret = dbp->set_h_nelem(dbp, nel)) != 0 ||
	    (ret = dbp->open(dbp,
	    NULL, NULL, DB_HASH, DB_CREATE, __db_omode("rw----"))) != 0)
		__os_set_errno(ret);

	/*
	 * !!!
	 * Hsearch returns 0 on error, not 1.
	 */
	return (ret == 0 ? 1 : 0);
}

ENTRY *
__db_hsearch(item, action)
	ENTRY item;
	ACTION action;
{
	DBT key, val;
	int ret;

	if (dbp == NULL) {
		__os_set_errno(EINVAL);
		return (NULL);
	}
	memset(&key, 0, sizeof(key));
	memset(&val, 0, sizeof(val));
	key.data = item.key;
	key.size = strlen(item.key) + 1;

	switch (action) {
	case ENTER:
		val.data = item.data;
		val.size = strlen(item.data) + 1;

		/*
		 * Try and add the key to the database.  If we fail because
		 * the key already exists, return the existing key.
		 */
		if ((ret =
		    dbp->put(dbp, NULL, &key, &val, DB_NOOVERWRITE)) == 0)
			break;
		if (ret == DB_KEYEXIST &&
		    (ret = dbp->get(dbp, NULL, &key, &val, 0)) == 0)
			break;
		/*
		 * The only possible DB error is DB_NOTFOUND, and it can't
		 * happen.  Check for a DB error, and lie if we find one.
		 */
		__os_set_errno(ret > 0 ? ret : EINVAL);
		return (NULL);
	case FIND:
		if ((ret = dbp->get(dbp, NULL, &key, &val, 0)) != 0) {
			if (ret != DB_NOTFOUND)
				__os_set_errno(ret);
			return (NULL);
		}
		item.data = (char *)val.data;
		break;
	default:
		__os_set_errno(EINVAL);
		return (NULL);
	}
	retval.key = item.key;
	retval.data = item.data;
	return (&retval);
}

void
__db_hdestroy()
{
	if (dbp != NULL) {
		(void)dbp->close(dbp, 0);
		dbp = NULL;
	}
}
