/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef NDBT_FIND_HPP
#define NDBT_FIND_HPP

#include <BaseString.hpp>

/*
  Look for the binary named 'binary_name' in any of the
  given paths. Adds platform specific searc locations when
  necessary.
  Returns the full absolute path to the binary in 'name' if
  found, otherwise print error message and 'abort'
*/
void NDBT_find_binary(BaseString &name, const char *binary_name,
                      const char *first_path, ...);

/*
  Wrapper around 'NDBT_find_binary' hardcoded to find ndb_mgmd
*/
void NDBT_find_ndb_mgmd(BaseString &path);

/*
  Wrapper to find ndbd
*/
void NDBT_find_ndbd(BaseString &path);

/*
  Wrapper to find ndb_sign_keys in a test environment
*/
void NDBT_find_sign_keys(BaseString &path);

#endif
