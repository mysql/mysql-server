/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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

/** @file buf/buf.h
 The database buffer pool high-level routines

 Created 11/5/1995 Heikki Tuuri
 *******************************************************/

#ifndef buf_h
#define buf_h

#include "univ.i"

#include "page0size.h"

/** Checks if a page contains only zeroes.
@param[in]      read_buf        database page
@param[in]      page_size       page size
@return true if page is filled with zeroes */
bool buf_page_is_zeroes(const byte *read_buf, const page_size_t &page_size);
#endif
