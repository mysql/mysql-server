/*****************************************************************************

Copyright (c) 1995, 2015, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file buf/buf.cc
The database buffer buf_pool

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "univ.i"
#include "page0size.h"

/** Checks if a page contains only zeroes.
@param[in]	read_buf	database page
@param[in]	page_size	page size
@return true if page is filled with zeroes */
bool
buf_page_is_zeroes(
	const byte*		read_buf,
	const page_size_t&	page_size)
{
	for (ulint i = 0; i < page_size.logical(); i++) {
		if (read_buf[i] != 0) {
			return(false);
		}
	}
	return(true);
}
