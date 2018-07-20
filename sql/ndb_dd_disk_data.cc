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

// Implements the functions defined in ndb_dd_disk_data.h
#include "sql/ndb_dd_disk_data.h"

#include "sql/dd/string_type.h"
#include "sql/dd/types/tablespace.h"
#include "sql/dd/types/tablespace_file.h"

// The keys used to store the id, version, and type of object
// in se_private_data field of DD
static const char* object_id_key = "object_id";
static const char* object_version_key = "object_version";
static const char* object_type_key = "object_type";


void
ndb_dd_disk_data_set_object_id_and_version(dd::Tablespace* object_def,
                                           int object_id,
                                           int object_version)
{
  DBUG_ENTER("ndb_dd_disk_data_set_object_id_and_version");
  DBUG_PRINT("enter", ("object_id: %d, object_version: %d",
                       object_id, object_version));

  object_def->se_private_data().set_int32(object_id_key,
                                          object_id);
  object_def->se_private_data().set_int32(object_version_key,
                                          object_version);
  DBUG_VOID_RETURN;
}


bool
ndb_dd_disk_data_get_object_id_and_version(const dd::Tablespace* object_def,
                                           int& object_id,
                                           int& object_version)
{
  DBUG_ENTER("ndb_dd_disk_data_get_object_id_and_version");

  if (!object_def->se_private_data().exists(object_id_key))
  {
    DBUG_PRINT("error", ("Disk data definition didn't contain property '%s'",
                         object_id_key));
    DBUG_RETURN(false);
  }

  if (object_def->se_private_data().get_int32(object_id_key, &object_id))
  {
    DBUG_PRINT("error", ("Disk data definition didn't have a valid number "
                         "for '%s'", object_id_key));
    DBUG_RETURN(false);
  }

  if (!object_def->se_private_data().exists(object_version_key))
  {
    DBUG_PRINT("error", ("Disk data definition didn't contain property '%s'",
                         object_version_key));
    DBUG_RETURN(false);
  }

  if (object_def->se_private_data().get_int32(object_version_key,
                                              &object_version))
  {
    DBUG_PRINT("error", ("Disk data definition didn't have a valid number "
                         "for '%s'", object_version_key));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("exit", ("object_id: %d, object_version: %d",
                      object_id, object_version));

  DBUG_RETURN(true);
}


void ndb_dd_disk_data_set_object_type(dd::Properties &se_private_data,
                                      const enum object_type type)
{
  DBUG_ENTER("ndb_dd_disk_data_set_object_type");

  dd::String_type type_str;
  if (type == object_type::TABLESPACE)
  {
    type_str = "tablespace";
  }
  else if (type == object_type::LOGFILE_GROUP)
  {
    type_str = "logfile_group";
  }
  else
  {
    // Should never reach here
    DBUG_ASSERT(false);
  }

  DBUG_PRINT("enter", ("object_type: %s", type_str.c_str()));

  se_private_data.set(object_type_key,
                      type_str.c_str());
  DBUG_VOID_RETURN;
}


void ndb_dd_disk_data_set_object_type(dd::Tablespace *object_def,
                                      enum object_type type)
{
  ndb_dd_disk_data_set_object_type(object_def->se_private_data(), type);
}


bool
ndb_dd_disk_data_get_object_type(const dd::Properties &se_private_data,
                                 enum object_type &type)
{
  DBUG_ENTER("ndb_dd_disk_data_get_object_type");

  if (!se_private_data.exists(object_type_key))
  {
    DBUG_PRINT("error", ("Disk data definition didn't contain property '%s'",
                         object_type_key));
    DBUG_RETURN(false);
  }

  dd::String_type type_str;
  if (se_private_data.get(object_type_key,
                          type_str))
  {
    DBUG_PRINT("error", ("Disk data definition didn't have a valid value for"
                         " '%s'", object_type_key));
    DBUG_RETURN(false);
  }

  if (type_str == "tablespace")
  {
    type = object_type::TABLESPACE;
  }
  else if (type_str == "logfile_group")
  {
    type = object_type::LOGFILE_GROUP;
  }
  else
  {
    // Should never reach here
    DBUG_ASSERT(false);
    DBUG_RETURN(false);
  }

  DBUG_PRINT("exit", ("object_type: %s", type_str.c_str()));

  DBUG_RETURN(true);
}


void
ndb_dd_disk_data_add_file(dd::Tablespace* object_def,
                          const char* file_name)
{
  object_def->add_file()->set_filename(file_name);
}


void ndb_dd_disk_data_get_file_names(const dd::Tablespace* object_def,
                                     std::vector<std::string>& file_names)
{
  for(const auto file : object_def->files())
  {
    file_names.push_back((file->filename()).c_str());
  }
}

