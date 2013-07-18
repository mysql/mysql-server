/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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
