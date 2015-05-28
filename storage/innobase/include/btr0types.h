/*****************************************************************************

Copyright (c) 1996, 2015, Oracle and/or its affiliates. All Rights Reserved.

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

/********************************************************************//**
@file include/btr0types.h
The index tree general types

Created 2/17/1996 Heikki Tuuri
*************************************************************************/

#ifndef btr0types_h
#define btr0types_h

#include "univ.i"

#include "rem0types.h"
#include "page0types.h"
#include "sync0rw.h"
#include "page0size.h"

/** Persistent cursor */
struct btr_pcur_t;
/** B-tree cursor */
struct btr_cur_t;
/** B-tree search information for the adaptive hash index */
struct btr_search_t;

/** Is search system enabled.
Search system is protected by array of latches. */
extern char	btr_search_enabled;

/** Number of adaptive hash index partition. */
extern ulong	btr_ahi_parts;

/** The size of a reference to data stored on a different page.
The reference is stored at the end of the prefix of the field
in the index record. */
#define BTR_EXTERN_FIELD_REF_SIZE	FIELD_REF_SIZE

/** If the data don't exceed the size, the data are stored locally. */
#define BTR_EXTERN_LOCAL_STORED_MAX_SIZE	\
	(BTR_EXTERN_FIELD_REF_SIZE * 2)

/** The information is used for creating a new index tree when
applying TRUNCATE log record during recovery */
struct btr_create_t {

	explicit btr_create_t(const byte* const ptr)
		:
		format_flags(),
		n_fields(),
		field_len(),
		fields(ptr),
		trx_id_pos(ULINT_UNDEFINED)
	{
		/* Do nothing */
	}

	/** Page format */
	ulint			format_flags;

	/** Numbr of index fields */
	ulint			n_fields;

	/** The length of the encoded meta-data */
	ulint			field_len;

	/** Field meta-data, encoded. */
	const byte* const	fields;

	/** Position of trx-id column. */
	ulint			trx_id_pos;
};

#endif
