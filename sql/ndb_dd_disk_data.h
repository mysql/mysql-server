/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/properties.h"

namespace dd {
  class Tablespace;
}

/*
  Functions operating on disk data i.e. logfile
  groups and tablespaces. All functions are
  prefixed with ndb_dd_disk_data
*/

enum object_type
{
  TABLESPACE,
  LOGFILE_GROUP
};
/*
   Save the type of the disk data object
*/
void ndb_dd_disk_data_set_object_type(dd::Properties &se_private_data,
                                      const enum object_type type);

void ndb_dd_disk_data_set_object_type(dd::Tablespace* tablespace_def,
                                      const enum object_type type);

/*
  Return the disk data object type
*/
bool
ndb_dd_disk_data_get_object_type(const dd::Properties &se_private_data,
                                 enum object_type &type);

/*
  Add undo log file to the logfile group
*/
void ndb_dd_disk_data_add_undo_file(dd::Tablespace* logfile_group,
                                    const char* undo_file_name);

#endif
