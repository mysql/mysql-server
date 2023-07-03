/*****************************************************************************

Copyright (c) 2001, 2023, Oracle and/or its affiliates.

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

/** @file include/fts0opt.h
 Full Text Search optimize thread

 Created 2011-02-15 Jimmy Yang
 ***********************************************************************/
#ifndef INNODB_FTS0OPT_H
#define INNODB_FTS0OPT_H

/********************************************************************
Callback function to fetch the rows in an FTS INDEX record. */
bool fts_optimize_index_fetch_node(
    /* out: always returns non-NULL */
    void *row,       /* in: sel_node_t* */
    void *user_arg); /* in: pointer to ib_vector_t */
#endif
