/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

#pragma once

#include <string>
#include "sql/sql_class.h"

namespace myduck {

/** Get the corresponding duckdb collation according to mysql CHARSET_INFO.
  Unfortunately, duckdb's collation is not completely consistent with mysql.
  We only focus on the two behaviors of NOCASE and NOACCENT.
  TODO: In the future we may support MySQL collation in duckdb.
  @param[in]  cs        Pointer to mysql CHARSET_INFO structure
  @param[in]  warn_msg  Warn message if there is any warning
  @return  DuckDB collation
*/
std::string get_duckdb_collation(const CHARSET_INFO *cs, std::string &warn_msg);
/* Charsets other than utf8mb3 and utf8mb4 use POSIX Collation directly
Duckdb treats posix same as binary. We cannot use binary because binary is
a keyword, so we use POSIX instead. */
static std::string COLLATION_BINARY = "POSIX";
static std::string COLLATION_NOCASE = "NOCASE";
static std::string COLLATION_NOCASE_NOACCENT = "NOCASE.NOACCENT";

/** DuckDB store latin1 column's data as utf8mb4. Get the corresponding
  utf8mb4 CHARSET_INFO according to latin1 CHARSET_INFO.
   @param[in]  cs  Pointer to latin1 CHARSET_INFO structure
   @return  utf8mb4 CHARSET_INFO
*/
const CHARSET_INFO *get_utf8mb4_charset_for_latin1(const CHARSET_INFO *cs);
}  // namespace myduck
