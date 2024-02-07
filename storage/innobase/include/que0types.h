/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/que0types.h
 Query graph global types

 Created 5/27/1996 Heikki Tuuri
 *******************************************************/

#ifndef que0types_h
#define que0types_h

#include "data0data.h"
#include "dict0types.h"

/* Pseudotype for all graph nodes */
typedef void que_node_t;

/* Query graph root is a fork node */
typedef struct que_fork_t que_t;

struct que_thr_t;

/* Common struct at the beginning of each query graph node; the name of this
substruct must be 'common' */

struct que_common_t {
  ulint type;          /*!< query node type */
  que_node_t *parent;  /*!< back pointer to parent node, or NULL */
  que_node_t *brother; /* pointer to a possible brother node */
  dfield_t val;        /*!< evaluated value for an expression */
  ulint val_buf_size;
  /* buffer size for the evaluated value data,
  if the buffer has been allocated dynamically:
  if this field is != 0, and the node is a
  symbol node or a function node, then we
  have to free the data field in val
  explicitly */
};

#endif
