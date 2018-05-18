/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file ut/ut0dbg.cc
 Debug utilities for Innobase.

 Created 1/30/1994 Heikki Tuuri
 **********************************************************************/

#include <stdlib.h>

#include "univ.i"

#ifndef UNIV_HOTBACKUP
#include "ha_prototypes.h"
#include "sql/log.h"
#endif /* !UNIV_HOTBACKUP */

#include "ut0dbg.h"

/** Report a failed assertion. */
[[noreturn]] void ut_dbg_assertion_failed(
    const char *expr, /*!< in: the failed assertion (optional) */
    const char *file, /*!< in: source file containing the assertion */
    ulint line)       /*!< in: line number of the assertion */
{
#if !defined(UNIV_HOTBACKUP) && !defined(UNIV_NO_ERR_MSGS)
  ib::error(ER_IB_MSG_1273)
      << "Assertion failure: " << innobase_basename(file) << ":" << line
      << ((expr != nullptr) ? ":" : "") << ((expr != nullptr) ? expr : "")
      << " thread " << os_thread_handle();

#else  /* !UNIV_HOTBACKUP && !defined(UNIV_NO_ERR_MSGS) */
  auto filename = base_name(file);

  if (filename == nullptr) {
    filename = "null";
  }

  fprintf(stderr,
          "InnoDB: Assertion failure: %s:" ULINTPF
          "%s%s\n"
          "InnoDB: thread " UINT64PF,
          filename, line, expr != nullptr ? ":" : "",
          expr != nullptr ? expr : "", os_thread_handle());
#endif /* !UNIV_HOTBACKUP */

  fputs(
      "InnoDB: We intentionally generate a memory trap.\n"
      "InnoDB: Submit a detailed bug report"
      " to http://bugs.mysql.com.\n"
      "InnoDB: If you get repeated assertion failures"
      " or crashes, even\n"
      "InnoDB: immediately after the mysqld startup, there may be\n"
      "InnoDB: corruption in the InnoDB tablespace. Please refer to\n"
      "InnoDB: " REFMAN
      "forcing-innodb-recovery.html\n"
      "InnoDB: about forcing recovery.\n",
      stderr);

#ifndef DBUG_OFF
  dump_trace();
#endif /* DBUG_OFF */

  fflush(stderr);
  fflush(stdout);
  abort();
}
