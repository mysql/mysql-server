/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_TABLESPACE_INCLUDED
#define SQL_TABLESPACE_INCLUDED

enum class Ident_name_check;
class THD;
class st_alter_tablespace;
struct handlerton;


/**
  Check if tablespace name has valid length.

  @param tablespace_name        Name of the tablespace

  @note Tablespace names are not reflected in the file system, so
        character case conversion or consideration is not relevant.

  @note Checking for path characters or ending space is not done.
        The checks are for identifier length, both in terms of
        number of characters and number of bytes.

  @retval  false   No error encountered while checking length.
  @retval  true    Error encountered and reported.
*/

bool validate_tablespace_name_length(const char *tablespace_name);


/**
  Check if a tablespace name is valid.

  SE specific validation is done by the SE by invoking a handlerton method.

  @param tablespace_ddl         Whether this is tablespace DDL or not.
  @param tablespace_name        Name of the tablespace
  @param engine                 Handlerton for the tablespace.

  @retval  false   No error encountered while checking the name.
  @retval  true    Error encountered and reported.
*/

bool validate_tablespace_name(bool tablespace_ddl,
                              const char *tablespace_name,
                              const handlerton *engine);


bool mysql_alter_tablespace(THD* thd, st_alter_tablespace *ts_info);

#endif /* SQL_TABLESPACE_INCLUDED */
