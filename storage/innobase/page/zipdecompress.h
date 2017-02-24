/*****************************************************************************

Copyright (c) 2005, 2017, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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
@file page/zipdecompress.h
Compressed page interface

Created June 2005 by Marko Makela
*******************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#ifndef zip_decompress_h
#define zip_decompress_h

#include "page0types.h"
#include "btr0types.h"
#include "fil0types.h"

#include "page/page.ic"
#include "page/zipdecompress.ic"
/**********************************************************************//**
Decompress a page.  This function should tolerate errors on the compressed
page.  Instead of letting assertions fail, it will return FALSE if an
inconsistency is detected.
@return true on success, false on failure */
ibool
page_zip_decompress_low(
/*====================*/
	page_zip_des_t*	page_zip,/*!< in: data, ssize;
				out: m_start, m_end, m_nonempty, n_blobs */
	page_t*		page,	/*!< out: uncompressed page, may be trashed */
	ibool		all);	/*!< in: TRUE=decompress the whole page;
				FALSE=verify but do not copy some
				page header fields that should not change
				after page creation */

#endif
