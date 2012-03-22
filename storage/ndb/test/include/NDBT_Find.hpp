/*
   Copyright 2009 Sun Microsystems, Inc.

   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
  found, otherwise porint error mesage and 'abort' 
*/
void NDBT_find_binary(BaseString& name,
                     const char* binary_name,
                     const char* first_path, ...);


/*
  Wrapper around 'NDBT_find_binary' hardcoded to find ndb_mgmd
*/
void NDBT_find_ndb_mgmd(BaseString& path);

#endif
