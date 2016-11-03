/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <memory>                    // unique_ptr

#include "lock.h"                    // Tablespace_hash_set
#include "my_alloc.h"
#include "my_global.h"


class THD;
class st_alter_tablespace;
struct handlerton;

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
  @param commit_dd_changes  Indicates that we need to commit
                            changes to data-dictionary.

  @returns Uncached dd::Tablespace object for tablespace created
           (nullptr in case of failure).
*/
dd::Tablespace* create_tablespace(THD *thd,
                                  st_alter_tablespace *ts_info,
                                  handlerton *hton,
                                  bool commit_dd_changes);

/**
  Drop Tablespace from Data Dictionary.

  @param thd                Thread executing the operation.
  @param tablespace         Uncached tablespace object for
                            the tablespace to be dropped.
  @param commit_dd_changes  Indicates that we need to commit
                            changes to data-dictionary.
  @param uncached           Indicates whether dd::Tablespace
                            object is uncached.

  @return false - On success.
  @return true - On failure.
*/
bool drop_tablespace(THD *thd, const Tablespace *tablespace,
                     bool commit_dd_changes, bool uncached);

/**
  Update tablespace description in Data Dictionary.

  @param thd                Thread executing the operation.
  @param tablespace         Uncached tablespace object for
                            the tablespace.
  @param commit_dd_changes  Indicates that we need to commit
                            changes to data-dictionary.

  @return false - On success.
  @return true - On failure.
*/
bool update_tablespace(THD *thd, const Tablespace *old_tablespace,
                       Tablespace *tablespace,
                       bool commit_dd_changes);

/**
  Add/Drop Tablespace file for a tablespace from Data Dictionary.

  @param thd          Thread executing the operation.
  @param ts_info      Tablespace metadata from the DDL.
  @param old_ts_def   Old version of tablespace definition.
  @param new_ts_def   New version of tablespace definition.

  @return false - On success.
  @return true - On failure.
*/
bool alter_tablespace(THD *thd, st_alter_tablespace *ts_info,
                      const dd::Tablespace *old_ts_def,
                      dd::Tablespace *new_ts_def);

} // namespace dd
#endif // DD_TABLESPACE_INCLUDED
