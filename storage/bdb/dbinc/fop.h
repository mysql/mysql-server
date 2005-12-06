/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: fop.h,v 12.3 2005/10/04 18:22:22 bostic Exp $
 */

#ifndef	_FOP_H_
#define	_FOP_H_

#define	MAKE_INMEM(D) do {					\
	F_SET((D), DB_AM_INMEM);				\
	(void)__memp_set_flags((D)->mpf, DB_MPOOL_NOFILE, 1);	\
} while (0)

#include "dbinc_auto/fileops_auto.h"
#include "dbinc_auto/fileops_ext.h"

#endif /* !_FOP_H_ */
