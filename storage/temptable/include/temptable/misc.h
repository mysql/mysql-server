/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/misc.h
TempTable miscellaneous helper utilities declarations. */

#ifndef TEMPTABLE_MISC_H
#define TEMPTABLE_MISC_H

#include <cstddef>

#include "my_compiler.h"

#define TEMPTABLE_UNUSED MY_ATTRIBUTE((unused))

#ifdef DBUG_OFF
#define TEMPTABLE_UNUSED_NODBUG MY_ATTRIBUTE((unused))
#else /* DBUG_OFF */
#define TEMPTABLE_UNUSED_NODBUG
#endif /* DBUG_OFF */

namespace temptable {

/** Check if a given buffer is inside another buffer.
@return true if inside */
inline bool buf_is_inside_another(
    /** [in] First buffer, that should be inside the other. */
    const unsigned char *small,
    /** [in] First buffer length in bytes. */
    size_t small_length,
    /** [in] Second buffer, that should contain the other. */
    const unsigned char *big,
    /** [in] Second buffer length in bytes. */
    size_t big_length) {
  const unsigned char *small_after_last = small + small_length;
  const unsigned char *big_after_last = big + big_length;

  return small >= big && small_after_last <= big_after_last;
}

} /* namespace temptable */

#endif /* TEMPTABLE_MISC_H */
