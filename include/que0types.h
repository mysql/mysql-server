/*****************************************************************************

Copyright (c) 1996, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/que0types.h
Query graph global types

Created 5/27/1996 Heikki Tuuri
*******************************************************/

#ifndef que0types_h
#define que0types_h

#include "data0data.h"
#include "dict0types.h"

/* Pseudotype for all graph nodes */
typedef void	que_node_t;

typedef struct que_fork_struct	que_fork_t;

/* Query graph root is a fork node */
typedef	que_fork_t	que_t;

typedef struct que_thr_struct		que_thr_t;
typedef struct que_common_struct	que_common_t;

/* Common struct at the beginning of each query graph node; the name of this
substruct must be 'common' */

struct que_common_struct{
	ulint		type;	/*!< query node type */
	que_node_t*	parent;	/*!< back pointer to parent node, or NULL */
	que_node_t*	brother;/* pointer to a possible brother node */
	dfield_t	val;	/*!< evaluated value for an expression */
	ulint		val_buf_size;
				/* buffer size for the evaluated value data,
				if the buffer has been allocated dynamically:
				if this field is != 0, and the node is a
				symbol node or a function node, then we
				have to free the data field in val
				explicitly */
};

#endif
