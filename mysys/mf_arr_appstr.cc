/* Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/mf_arr_appstr.cc
*/

/**
  Append str to array, or move to the end if it already exists

  @param str    String to be appended
  @param array  The array, terminated by a NULL element, all unused elements
                pre-initialized to NULL
  @param size   Size of the array; array must be terminated by a NULL
                pointer, so can hold size - 1 elements

  @retval false  Success
  @retval true   Failure, array is full
*/

#include <assert.h>
#include <string.h>

#include "my_inttypes.h"

bool array_append_string_unique(const char *str, const char **array,
                                size_t size) {
  const char **p;
  /* end points at the terminating NULL element */
  const char **end = array + size - 1;
  assert(*end == nullptr);

  for (p = array; *p; ++p) {
    if (strcmp(*p, str) == 0) break;
  }
  if (p >= end) return true; /* Array is full */

  assert(*p == nullptr || strcmp(*p, str) == 0);

  while (*(p + 1)) {
    *p = *(p + 1);
    ++p;
  }

  assert(p < end);
  *p = str;

  return false; /* Success */
}
