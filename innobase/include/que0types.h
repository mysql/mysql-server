/******************************************************
Query graph global types

(c) 1996 Innobase Oy

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
	ulint		type;	/* query node type */
	que_node_t*	parent;	/* back pointer to parent node, or NULL */
	que_node_t*	brother;/* pointer to a possible brother node */
	dfield_t	val;	/* evaluated value for an expression */
	ulint		val_buf_size;
				/* buffer size for the evaluated value data,
				if the buffer has been allocated dynamically:
				if this field is != 0, and the node is a
				symbol node or a function node, then we
				have to free the data field in val explicitly */
};

#endif
