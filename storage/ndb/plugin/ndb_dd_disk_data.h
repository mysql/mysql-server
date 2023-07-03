/*
   Copyright (c) 2018, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_DD_DISK_DATA_H
#define NDB_DD_DISK_DATA_H

#include <vector>

#include "sql/dd/properties.h"

namespace dd {
class Tablespace;
struct Tablespace_table_ref;
}  // namespace dd

class THD;

/*
  Functions operating on disk data i.e. logfile
  groups and tablespaces. All functions are
  prefixed with ndb_dd_disk_data
*/

/*
   Save the disk data object's id and version in the definition
*/
void ndb_dd_disk_data_set_object_id_and_version(dd::Tablespace *object_def,
                                                int object_id,
                                                int object_version);

/*
  Return the definition's object id and version
*/
bool ndb_dd_disk_data_get_object_id_and_version(
    const dd::Tablespace *object_def, int &object_id, int &object_version);

enum object_type { TABLESPACE, LOGFILE_GROUP };
/*
   Save the type of the disk data object
*/
void ndb_dd_disk_data_set_object_type(dd::Properties &se_private_data,
                                      const enum object_type type);

void ndb_dd_disk_data_set_object_type(dd::Tablespace *object_def,
                                      const enum object_type type);

/*
  Return the disk data object type
*/
bool ndb_dd_disk_data_get_object_type(const dd::Properties &se_private_data,
                                      enum object_type &type);

/*
  Add undo/data file to logfile group/tablespace
*/
void ndb_dd_disk_data_add_file(dd::Tablespace *object_def,
                               const char *file_name);

/*
  Retrieve file names belonging to the disk data object
*/
void ndb_dd_disk_data_get_file_names(const dd::Tablespace *object_def,
                                     std::vector<std::string> &file_names);

/*
  Fetch information about tables in a tablespace
*/
bool ndb_dd_disk_data_get_table_refs(
    THD *thd, const dd::Tablespace &object_def,
    std::vector<dd::Tablespace_table_ref> &table_refs);

#endif
