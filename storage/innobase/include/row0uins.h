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

#include "my_compiler.h"

/** @file include/row0uins.h
 Fresh insert undo

 Created 2/25/1997 Heikki Tuuri
 *******************************************************/

#ifndef row0uins_h
#define row0uins_h

#include "data0data.h"
#include "dict0types.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "univ.i"

/** Undoes a fresh insert of a row to a table. A fresh insert means that
 the same clustered index unique key did not have any record, even delete
 marked, at the time of the insert.  InnoDB is eager in a rollback:
 if it figures out that an index record will be removed in the purge
 anyway, it will remove it in the rollback.
 @return DB_SUCCESS */
dberr_t row_undo_ins(undo_node_t *node, /*!< in: row undo node */
                     que_thr_t *thr)    /*!< in: query thread */
    MY_ATTRIBUTE((warn_unused_result));

#include "row0uins.ic"

#endif
