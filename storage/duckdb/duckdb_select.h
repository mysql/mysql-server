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

#include <sys/types.h>

#include "delta_appender.h"
#include "duckdb_types.h"
#include "my_base.h" /* ha_rows */
#include "my_compiler.h"
#include "my_inttypes.h"
#include "mysql/plugin.h"
#include "sql/duckdb/duckdb_query.h"
#include "sql/handler.h" /* handler */
#include "sql/sql_class.h"
#include "sql/sql_show.h"
#include "sql/sql_table.h"
#include "thr_lock.h" /* THR_LOCK, THR_LOCK_DATA */

extern handlerton *duckdb_hton;

void store_duckdb_field_in_mysql_format(Field *field, duckdb::Value &value,
                                        THD *thd);
