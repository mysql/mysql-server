/*****************************************************************************

Copyright (c) 1994, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"

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
	ut_print_timestamp(stderr);
#ifdef UNIV_HOTBACKUP
	fprintf(stderr, "  InnoDB: Assertion failure in file %s line %lu\n",
		file, line);
#else /* UNIV_HOTBACKUP */
	fprintf(stderr,
		"  InnoDB: Assertion failure in thread " ULINTPF
		" in file %s line " ULINTPF "\n",
		os_thread_pf(os_thread_get_curr_id()),
		innobase_basename(file), line);
#endif /* UNIV_HOTBACKUP */
	if (expr) {
		fprintf(stderr,
			"InnoDB: Failing assertion: %s\n", expr);
	}

	fputs("InnoDB: We intentionally generate a memory trap.\n"
	      "InnoDB: Submit a detailed bug report"
	      " to http://bugs.mysql.com.\n"
	      "InnoDB: If you get repeated assertion failures"
	      " or crashes, even\n"
	      "InnoDB: immediately after the mysqld startup, there may be\n"
	      "InnoDB: corruption in the InnoDB tablespace. Please refer to\n"
	      "InnoDB: " REFMAN "forcing-innodb-recovery.html\n"
	      "InnoDB: about forcing recovery.\n", stderr);

	fflush(stderr);
	fflush(stdout);
	abort();
}
