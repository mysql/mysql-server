/*****************************************************************************

Copyright (c) 1994, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/*****************************************************************//**
@file ut/ut0dbg.cc
Debug utilities for Innobase.

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#include <stdlib.h>

#include "ha_prototypes.h"
#include "sql/log.h"
#include "ut0dbg.h"

/*************************************************************//**
Report a failed assertion. */
void
ut_dbg_assertion_failed(
/*====================*/
	const char* expr,	/*!< in: the failed assertion (optional) */
	const char* file,	/*!< in: source file containing the assertion */
	ulint line)		/*!< in: line number of the assertion */
{
#ifndef UNIV_HOTBACKUP
	sql_print_error(
		"InnoDB: Assertion failure: %s:" ULINTPF "%s%s\n"
		"InnoDB: thread " UINT64PF,
		innobase_basename(file), line,
		expr != nullptr ? ":" : "",
		expr != nullptr ? expr : "",
		os_thread_handle());
#else /* !UNIV_HOTBACKUP */
	fprintf(stderr,
		"InnoDB: Assertion failure: %s:" ULINTPF "%s%s\n"
		"InnoDB: thread " UINT64PF,
		innobase_basename(file), line,
		expr != nullptr ? ":" : "",
		expr != nullptr ? expr : "",
		os_thread_handle());
#endif /* !UNIV_HOTBACKUP */

	fputs("InnoDB: We intentionally generate a memory trap.\n"
	      "InnoDB: Submit a detailed bug report"
	      " to http://bugs.mysql.com.\n"
	      "InnoDB: If you get repeated assertion failures"
	      " or crashes, even\n"
	      "InnoDB: immediately after the mysqld startup, there may be\n"
	      "InnoDB: corruption in the InnoDB tablespace. Please refer to\n"
	      "InnoDB: " REFMAN "forcing-innodb-recovery.html\n"
	      "InnoDB: about forcing recovery.\n", stderr);

#ifndef DBUG_OFF
	dump_trace();
#endif /* DBUG_OFF */

	fflush(stderr);
	fflush(stdout);
	abort();
}
