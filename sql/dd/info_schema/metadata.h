/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef SQL_DD_METADATA_H
#define SQL_DD_METADATA_H

#include <mysql/plugin.h>            // st_plugin_int

#include "sql/dd/string_type.h"      // dd::String_type

class THD;
struct st_plugin_int;

namespace dd {
namespace info_schema {

/**
  The version of the current information_schema system views.

  This version number is stored on disk in the data dictionary.
  Every time the information_schema structure changes,
  this version number must change.

  The numbering to use is the MySQL version number
  of the first MySQL version that published a given database schema.
  The format is Mmmdd with M=Major, m=minor, d=dot,
  so that MySQL 8.0.4 is encoded as 80004.
*/

static const uint IS_DD_VERSION= 80004;

/**
  Initialize INFORMATION_SCHEMA system views.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool initialize(THD *thd);


/**
  Create INFORMATION_SCHEMA system views.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool create_system_views(THD *thd);


/**
  Store the server I_S table metadata into dictionary, once during MySQL
  server bootstrap.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool store_server_I_S_metadata(THD *thd);


/**
  Store I_S table metadata into dictionary, during MySQL server startup.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool update_I_S_metadata(THD *thd);


/**
  Store dynamic I_S plugin table metadata into dictionary, during INSTALL
  command execution.

  @param thd         Thread context.
  @param plugin_int  I_S Plugin of which the metadata is to be stored.

  @return       Upon failure, return true, otherwise false.
*/
bool store_dynamic_plugin_I_S_metadata(THD *thd,
                                       st_plugin_int *plugin_int);


/**
  Remove I_S view metadata from dictionary. This is used
  UNINSTALL and server restart procedure when I_S version is changed.

  @param thd         Thread context.
  @param view_name   I_S view name of which the metadata is to be stored.

  @return       Upon failure, return true, otherwise false.
*/
bool remove_I_S_view_metadata(THD *thd,
                              const dd::String_type &view_name);

}
}

#endif // SQL_DD_METADATA_H
