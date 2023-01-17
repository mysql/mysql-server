/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

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

/** @file include/row0types.h
 Row operation global types

 Created 12/27/1996 Heikki Tuuri
 *******************************************************/

#ifndef row0types_h
#define row0types_h

struct plan_t;

struct upd_t;
struct upd_field_t;
struct upd_node_t;
struct del_node_t;
struct ins_node_t;
struct sel_node_t;
struct open_node_t;
struct fetch_node_t;

struct sel_buf_t;

struct undo_node_t;

struct purge_node_t;

struct row_ext_t;

/** Buffer for logging modifications during online index creation */
struct row_log_t;

/* MySQL data types */
struct TABLE;

#endif
