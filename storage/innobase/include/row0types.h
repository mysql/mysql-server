/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0types.h
Row operation global types

Created 12/27/1996 Heikki Tuuri
*******************************************************/

#ifndef row0types_h
#define row0types_h

typedef struct plan_struct plan_t;

typedef	struct upd_struct upd_t;

typedef struct upd_field_struct upd_field_t;

typedef	struct upd_node_struct upd_node_t;

typedef	struct del_node_struct del_node_t;

typedef	struct ins_node_struct ins_node_t;

typedef struct sel_node_struct	sel_node_t;

typedef struct open_node_struct	open_node_t;

typedef struct fetch_node_struct fetch_node_t;

typedef struct row_printf_node_struct	row_printf_node_t;
typedef struct sel_buf_struct	sel_buf_t;

typedef	struct undo_node_struct undo_node_t;

typedef	struct purge_node_struct purge_node_t;

typedef struct row_ext_struct row_ext_t;

/** Index record modification operations during online index creation */
enum row_op {
	/** Insert a record */
	ROW_OP_INSERT,
	/** Delete-mark a record */
	ROW_OP_DELETE_MARK,
	/** Unmark a delete-marked record */
	ROW_OP_DELETE_UNMARK,
	/** Purge a delete-marked record */
	ROW_OP_PURGE,
	/** Purge a record that may not be delete-marked */
	ROW_OP_DELETE_PURGE
};

/** Buffer for logging modifications during online index creation */
struct row_log_t;

/* MySQL data types */
struct TABLE;

#endif
