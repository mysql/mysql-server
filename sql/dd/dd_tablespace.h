/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_TABLESPACE_INCLUDED
#define DD_TABLESPACE_INCLUDED

#include "lock.h"                    // Tablespace_hash_set

class THD;
class st_alter_tablespace;
struct handlerton;
typedef struct st_mem_root MEM_ROOT;

namespace dd {

class Tablespace;

/**
  Fill Tablespace_hash_set with tablespace names used by the
  given db_name.table_name.

  @param thd            - Thread invoking the function.
  @param db_name        - Database name.
  @param table_name     - Table name.
  @param tablespace_set - (OUT) Hash_set where tablespace names
                          are filled.

  @return true - On failure.
  @return false - On success.
*/
bool fill_table_and_parts_tablespace_names(THD *thd,
                                           const char *db_name,
                                           const char *table_name,
                                           Tablespace_hash_set *tablespace_set);

/**
  Read tablespace name of a tablespace_id.

  @param thd                   - Thread invoking this call.
  @param obj                   - Table/Partition object whose
                                 tablespace name is being read.
  @param tablespace_name (OUT) - Tablespace name of table.
  @param mem_root              - Memroot where tablespace name
                                 should be stored.

  @return true  - On failure.
  @return false - On success.
*/
template <typename T>
bool get_tablespace_name(THD *thd, const T *obj,
                         const char** tablespace_name,
                         MEM_ROOT *mem_root);

/**
  Create Tablespace in Data Dictionary.

  @note:
  We now impose tablespace names to be unique accross SE's.
  Which was not the case earlier.

  @param thd                Thread executing the operation.
  @param ts_info            Tablespace metadata from the DDL.
  @param hton               Handlerton in which tablespace reside.

  @note The caller must rollback both statement and transaction on
        failure, before any further accesses to DD. This is because
        such a failure might be caused by a deadlock, which requires
        rollback before any other operations on SE (including reads
        using attachable transactions) can be done.

  @return false - On success.
  @return true - On failure.
*/
bool create_tablespace(THD *thd, st_alter_tablespace *ts_info,
                       handlerton *hton);

} // namespace dd
#endif // DD_TABLESPACE_INCLUDED
