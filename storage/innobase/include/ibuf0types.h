/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/ibuf0types.h
Insert buffer global types

Created 7/29/1997 Heikki Tuuri
*******************************************************/

#ifndef ibuf0types_h
#define ibuf0types_h

/* The insert buffer tree itself is always located in space 0. */
#define IBUF_SPACE_ID		static_cast<space_id_t>(0)

struct ibuf_t;

#endif
