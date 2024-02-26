/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file buf/buf.cc
 The database buffer buf_pool

 Created 11/5/1995 Heikki Tuuri
 *******************************************************/

#include "page0size.h"
#include "univ.i"

/** Checks if a page contains only zeroes.
@param[in]      read_buf        database page
@param[in]      page_size       page size
@return true if page is filled with zeroes */
bool buf_page_is_zeroes(const byte *read_buf, const page_size_t &page_size) {
  for (ulint i = 0; i < page_size.logical(); i++) {
    if (read_buf[i] != 0) {
      return (false);
    }
  }
  return (true);
}
