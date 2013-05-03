/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/** Persistent cursor */
struct btr_pcur_t;
/** B-tree cursor */
struct btr_cur_t;
/** B-tree search information for the adaptive hash index */
struct btr_search_t;

#ifndef UNIV_HOTBACKUP

/** @brief The latch protecting the adaptive search system

This latch protects the
(1) hash index;
(2) columns of a record to which we have a pointer in the hash index;

but does NOT protect:

(3) next record offset field in a record;
(4) next or previous records on the same page.

Bear in mind (3) and (4) when using the hash index.
*/
extern rw_lock_t*	btr_search_latch_temp;

#endif /* UNIV_HOTBACKUP */

/** The latch protecting the adaptive search system */
#define btr_search_latch	(*btr_search_latch_temp)

/** Flag: has the search system been enabled?
Protected by btr_search_latch. */
extern char	btr_search_enabled;

/** The size of a reference to data stored on a different page.
The reference is stored at the end of the prefix of the field
in the index record. */
#define BTR_EXTERN_FIELD_REF_SIZE	20

/** A BLOB field reference full of zero, for use in assertions and tests.
Initially, BLOB field references are set to zero, in
dtuple_convert_big_rec(). */
extern const byte field_ref_zero[BTR_EXTERN_FIELD_REF_SIZE];

#endif
