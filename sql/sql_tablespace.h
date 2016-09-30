/* Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.

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


/**
  Check if tablespace name is valid

  @param tablespace_name        Name of the tablespace

  @note Tablespace names are not reflected in the file system, so
        character case conversion or consideration is not relevant.

  @note Checking for path characters or ending space is not done.
        The only checks are for identifier length, both in terms of
        number of characters and number of bytes.

  @retval  Ident_name_check::OK       Identifier name is ok (Success)
  @retval  Ident_name_check::WRONG    Identifier name is wrong, if length == 0
                                      (ER_WRONG_TABLESPACE_NAME)
  @retval  Ident_name_check::TOO_LONG Identifier name is too long if it is
                                      greater than 64 characters
                                      (ER_TOO_LONG_IDENT)

  @note In case of Ident_name_check::TOO_LONG or Ident_name_check::WRONG, the
        function reports an error (using my_error()).
*/

Ident_name_check check_tablespace_name(const char *tablespace_name);

bool mysql_alter_tablespace(THD* thd, st_alter_tablespace *ts_info);

#endif /* SQL_TABLESPACE_INCLUDED */
