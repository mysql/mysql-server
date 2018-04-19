/*
   Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_NAME_UTIL_H
#define NDB_NAME_UTIL_H


void ndb_set_dbname(const char *pathname, char *dbname);
void ndb_set_tabname(const char *pathname, char *tabname);

/*
  Return true if name starts with the prefix used for temporary name
  (normally this is "#sql")
*/
bool ndb_name_is_temp(const char* name);

/*
  Return true if name starts with the prefix used for NDB blob
  tables.

  NOTE! Those tables are internal but still returned in the public
  parts of NdbApi so they may need to be filtered in various places.
*/
bool ndb_name_is_blob_prefix(const char* name);

#endif
