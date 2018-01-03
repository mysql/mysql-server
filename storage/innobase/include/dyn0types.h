/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file include/dyn0types.h
The dynamically allocated buffer types and constants

Created 2013-03-16 Sunny Bains
*******************************************************/

#ifndef dyn0types_h
#define dyn0types_h

/** Value of dyn_block_t::magic_n */
#define DYN_BLOCK_MAGIC_N	375767

/** This is the initial 'payload' size of a dynamic array;
this must be > MLOG_BUF_MARGIN + 30! */
#define	DYN_ARRAY_DATA_SIZE	512

/** Flag for dyn_block_t::used that indicates a full block */
#define DYN_BLOCK_FULL_FLAG	0x1000000UL

#endif /* dyn0types_h */
