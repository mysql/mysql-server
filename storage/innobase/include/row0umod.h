/*****************************************************************************

Copyright (c) 1997, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/row0umod.h
 Undo modify of a row

 Created 2/27/1997 Heikki Tuuri
 *******************************************************/

#ifndef row0umod_h
#define row0umod_h

#include "data0data.h"
#include "dict0types.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "univ.i"

/** Undoes a modify operation on a row of a table.
 @return DB_SUCCESS or error code */
dberr_t row_undo_mod(undo_node_t *node, /*!< in: row undo node */
                     que_thr_t *thr)    /*!< in: query thread */
    MY_ATTRIBUTE((warn_unused_result));

#include "row0umod.ic"

#endif
