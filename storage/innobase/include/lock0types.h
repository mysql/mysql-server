/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/lock0types.h
The transaction lock system global types

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#include "ut0lst.h"

#ifndef lock0types_h
#define lock0types_h

#define lock_t ib_lock_t

struct lock_t;
struct lock_sys_t;
struct lock_table_t;

enum select_mode {
	SELECT_ORDINARY,	/* default behaviour */
	SELECT_SKIP_LOCKED,	/* skip the row if row is locked */
	SELECT_NOWAIT		/* return immediately if row is locked */
};

/* Basic lock modes */
enum lock_mode {
	LOCK_IS = 0,	/* intention shared */
	LOCK_IX,	/* intention exclusive */
	LOCK_S,		/* shared */
	LOCK_X,		/* exclusive */
	LOCK_AUTO_INC,	/* locks the auto-inc counter of a table
			in an exclusive mode */
	LOCK_NONE,	/* this is used elsewhere to note consistent read */
	LOCK_NUM = LOCK_NONE, /* number of lock modes */
	LOCK_NONE_UNSET = 255
};

/** Convert the given enum value into string.
@param[in]	mode	the lock mode
@return human readable string of the given enum value */
inline
const char* lock_mode_string(enum lock_mode mode)
{
	switch (mode) {
	case LOCK_IS:
		return("LOCK_IS");
	case LOCK_IX:
		return("LOCK_IX");
	case LOCK_S:
		return("LOCK_S");
	case LOCK_X:
		return("LOCK_X");
	case LOCK_AUTO_INC:
		return("LOCK_AUTO_INC");
	case LOCK_NONE:
		return("LOCK_NONE");
	case LOCK_NONE_UNSET:
		return("LOCK_NONE_UNSET");
	default:
		ut_error;
	}
}

typedef UT_LIST_BASE_NODE_T(lock_t) trx_lock_list_t;

#endif /* lock0types_h */
