#ifndef HISTOGRAMS_HISTOGRAM_UTILITY_INCLUDED
#define HISTOGRAMS_HISTOGRAM_UTILITY_INCLUDED

/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_alloc.h"
#include "sql_string.h"

namespace histograms {

/**
  Returns a deep copy of the input argument. In case T has heap-allocated data
  it is copied onto the supplied mem_root.

  @note This function is only intended to be used to copy the values in
  histogram buckets and does not provide general support for deep copying
  arbitrary types.

  @param src         The value to be copied.
  @param mem_root    The MEM_ROOT to copy heap-allocated data onto.
  @param[out] error  Set to true if an error occurs.

  @return A deep copy of the input argument.
*/
template <typename T>
T DeepCopy(const T &src, [[maybe_unused]] MEM_ROOT *mem_root,
           [[maybe_unused]] bool *error) {
  return src;
}

template <>
inline String DeepCopy(const String &src, MEM_ROOT *mem_root, bool *error) {
  char *dst = src.dup(mem_root);
  if (dst == nullptr) {
    *error = true;
    return String();
  }
  return String(dst, src.length(), src.charset());
}

}  // namespace histograms

#endif
