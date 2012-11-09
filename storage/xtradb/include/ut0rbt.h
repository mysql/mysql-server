/*****************************************************************************
Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.

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

/*******************************************************************//**
@file include/ut0rbt.h
Red-Black tree implementation.

Created 2007-03-20 Sunny Bains
************************************************************************/

#ifndef INNOBASE_UT0RBT_H
#define INNOBASE_UT0RBT_H

#if !defined(IB_RBT_TESTING)
#include "univ.i"
#include "ut0mem.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define	ut_malloc	malloc
#define	ut_free		free
#define	ulint		unsigned long
#define	ut_a(c)		assert(c)
#define ut_error	assert(0)
#define	ibool		unsigned int
#define	TRUE		1
#define	FALSE		0
#endif

/* Red black tree typedefs */
typedef struct ib_rbt_struct ib_rbt_t;
typedef struct ib_rbt_node_struct ib_rbt_node_t;
/* FIXME: Iterator is a better name than _bound_ */
typedef struct ib_rbt_bound_struct ib_rbt_bound_t;
typedef void (*ib_rbt_print_node)(const ib_rbt_node_t* node);
typedef int (*ib_rbt_compare)(const void* p1, const void* p2);

/* Red black tree color types */
enum ib_rbt_color_enum {
	IB_RBT_RED,
	IB_RBT_BLACK
};

typedef enum ib_rbt_color_enum ib_rbt_color_t;

/* Red black tree node */
struct ib_rbt_node_struct {
	ib_rbt_color_t	color;			/* color of this node */

	ib_rbt_node_t*	left;			/* points left child */
	ib_rbt_node_t*	right;			/* points right child */
	ib_rbt_node_t*	parent;			/* points parent node */

	char		value[1];		/* Data value */
};

/* Red black tree instance.*/
struct	ib_rbt_struct {
	ib_rbt_node_t*	nil;			/* Black colored node that is
						used as a sentinel. This is
						pre-allocated too.*/

	ib_rbt_node_t*	root;			/* Root of the tree, this is
						pre-allocated and the first
						data node is the left child.*/

	ulint		n_nodes;		/* Total number of data nodes */

	ib_rbt_compare	compare;		/* Fn. to use for comparison */
	ulint		sizeof_value;		/* Sizeof the item in bytes */
};

/* The result of searching for a key in the tree, this is useful for
a speedy lookup and insert if key doesn't exist.*/
struct ib_rbt_bound_struct {
	const ib_rbt_node_t*
			last;			/* Last node visited */

	int		result;			/* Result of comparing with
						the last non-nil node that
						was visited */
};

/* Size in elements (t is an rb tree instance) */
#define rbt_size(t)	(t->n_nodes)

/* Check whether the rb tree is empty (t is an rb tree instance) */
#define rbt_empty(t)	(rbt_size(t) == 0)

/* Get data value (t is the data type, n is an rb tree node instance) */
#define rbt_value(t, n) ((t*) &n->value[0])

/* Compare a key with the node value (t is tree, k is key, n is node)*/
#define rbt_compare(t, k, n) (t->compare(k, n->value))

/* Node size. FIXME: name might clash, but currently it does not, so for easier
maintenance do not rename it for now. */
#define	SIZEOF_NODE(t)	((sizeof(ib_rbt_node_t) + t->sizeof_value) - 1)

/****************************************************************//**
Free an instance of  a red black tree */
UNIV_INTERN
void
rbt_free(
/*=====*/
	ib_rbt_t*	tree);		/*!< in: rb tree to free */
/****************************************************************//**
Create an instance of a red black tree
@return	rb tree instance */
UNIV_INTERN
ib_rbt_t*
rbt_create(
/*=======*/
	size_t		sizeof_value,	/*!< in: size in bytes */
	ib_rbt_compare	compare);	/*!< in: comparator */
/****************************************************************//**
Delete a node from the red black tree, identified by key.
@return TRUE if success FALSE if not found */
UNIV_INTERN
ibool
rbt_delete(
/*=======*/
	ib_rbt_t*	tree,		/*!< in: rb tree */
	const void*	key);		/*!< in: key to delete */
/****************************************************************//**
Remove a node from the rb tree, the node is not free'd, that is the
callers responsibility.
@return	the deleted node with the const. */
UNIV_INTERN
ib_rbt_node_t*
rbt_remove_node(
/*============*/
	ib_rbt_t*	tree,		/*!< in: rb tree */
	const ib_rbt_node_t*
			node);		/*!< in: node to delete, this
					is a fudge and declared const
					because the caller has access
					only to const nodes.*/
/****************************************************************//**
Find a matching node in the rb tree.
@return	node if found else return NULL */
UNIV_INTERN
const ib_rbt_node_t*
rbt_lookup(
/*=======*/
	const ib_rbt_t*	tree,		/*!< in: rb tree to search */
	const void*	key);		/*!< in: key to lookup */
/****************************************************************//**
Generic insert of a value in the rb tree.
@return	inserted node */
UNIV_INTERN
const ib_rbt_node_t*
rbt_insert(
/*=======*/
	ib_rbt_t*	tree,		/*!< in: rb tree */
	const void*	key,		/*!< in: key for ordering */
	const void*	value);		/*!< in: data that will be
					copied to the node.*/
/****************************************************************//**
Add a new node to the tree, useful for data that is pre-sorted.
@return	appended node */
UNIV_INTERN
const ib_rbt_node_t*
rbt_add_node(
/*=========*/
	ib_rbt_t*	tree,		/*!< in: rb tree */
	ib_rbt_bound_t*	parent,		/*!< in: parent */
	const void*	value);		/*!< in: this value is copied
					to the node */
/****************************************************************//**
Add a new caller-provided node to tree at the specified position.
The node must have its key fields initialized correctly.
@return added node */
UNIV_INTERN
const ib_rbt_node_t*
rbt_add_preallocated_node(
/*======================*/
	ib_rbt_t*	tree,		/*!< in: rb tree */
	ib_rbt_bound_t*	parent,		/*!< in: parent */
	ib_rbt_node_t*	node);		/*!< in: node */

/****************************************************************//**
Return the left most data node in the tree
@return	left most node */
UNIV_INTERN
const ib_rbt_node_t*
rbt_first(
/*======*/
	const ib_rbt_t*	tree);		/*!< in: rb tree */
/****************************************************************//**
Return the right most data node in the tree
@return	right most node */
UNIV_INTERN
const ib_rbt_node_t*
rbt_last(
/*=====*/
	const ib_rbt_t*	tree);		/*!< in: rb tree */
/****************************************************************//**
Return the next node from current.
@return	successor node to current that is passed in. */
UNIV_INTERN
const ib_rbt_node_t*
rbt_next(
/*=====*/
	const ib_rbt_t*	tree,		/*!< in: rb tree */
	const ib_rbt_node_t*		/*!< in: current node */
			current);
/****************************************************************//**
Return the prev node from current.
@return	precedessor node to current that is passed in */
UNIV_INTERN
const ib_rbt_node_t*
rbt_prev(
/*=====*/
	const ib_rbt_t*	tree,		/*!< in: rb tree */
	const ib_rbt_node_t*		/*!< in: current node */
			current);
/****************************************************************//**
Find the node that has the lowest key that is >= key.
@return	node that satisfies the lower bound constraint or NULL */
UNIV_INTERN
const ib_rbt_node_t*
rbt_lower_bound(
/*============*/
	const ib_rbt_t*	tree,		/*!< in: rb tree */
	const void*	key);		/*!< in: key to search */
/****************************************************************//**
Find the node that has the greatest key that is <= key.
@return	node that satisifies the upper bound constraint or NULL */
UNIV_INTERN
const ib_rbt_node_t*
rbt_upper_bound(
/*============*/
	const ib_rbt_t*	tree,		/*!< in: rb tree */
	const void*	key);		/*!< in: key to search */
/****************************************************************//**
Search for the key, a node will be retuned in parent.last, whether it
was found or not. If not found then parent.last will contain the
parent node for the possibly new key otherwise the matching node.
@return	result of last comparison */
UNIV_INTERN
int
rbt_search(
/*=======*/
	const ib_rbt_t*	tree,		/*!< in: rb tree */
	ib_rbt_bound_t*	parent,		/*!< in: search bounds */
	const void*	key);		/*!< in: key to search */
/****************************************************************//**
Search for the key, a node will be retuned in parent.last, whether it
was found or not. If not found then parent.last will contain the
parent node for the possibly new key otherwise the matching node.
@return	result of last comparison */
UNIV_INTERN
int
rbt_search_cmp(
/*===========*/
	const ib_rbt_t*	tree,		/*!< in: rb tree */
	ib_rbt_bound_t*	parent,		/*!< in: search bounds */
	const void*	key,		/*!< in: key to search */
	ib_rbt_compare	compare);	/*!< in: comparator */
/****************************************************************//**
Clear the tree, deletes (and free's) all the nodes. */
UNIV_INTERN
void
rbt_clear(
/*======*/
	ib_rbt_t*	tree);		/*!< in: rb tree */
/****************************************************************//**
Clear the tree without deleting and freeing its nodes. */
UNIV_INTERN
void
rbt_reset(
/*======*/
	 ib_rbt_t*	tree);		/*!< in: rb tree */
/****************************************************************//**
Merge the node from dst into src. Return the number of nodes merged.
@return	no. of recs merged */
UNIV_INTERN
ulint
rbt_merge_uniq(
/*===========*/
	ib_rbt_t*	dst,		/*!< in: dst rb tree */
	const ib_rbt_t*	src);		/*!< in: src rb tree */
/****************************************************************//**
Merge the node from dst into src. Return the number of nodes merged.
Delete the nodes from src after copying node to dst. As a side effect
the duplicates will be left untouched in the src, since we don't support
duplicates (yet). NOTE: src and dst must be similar, the function doesn't
check for this condition (yet).
@return	no. of recs merged */
UNIV_INTERN
ulint
rbt_merge_uniq_destructive(
/*=======================*/
	ib_rbt_t*	dst,		/*!< in: dst rb tree */
	ib_rbt_t*	src);		/*!< in: src rb tree */
/****************************************************************//**
Verify the integrity of the RB tree. For debugging. 0 failure else height
of tree (in count of black nodes).
@return	TRUE if OK FALSE if tree invalid. */
UNIV_INTERN
ibool
rbt_validate(
/*=========*/
	const ib_rbt_t*	tree);		/*!< in: tree to validate */
/****************************************************************//**
Iterate over the tree in depth first order. */
UNIV_INTERN
void
rbt_print(
/*======*/
	const ib_rbt_t*		tree,	/*!< in: tree to traverse */
	ib_rbt_print_node	print);	/*!< in: print function */

#endif /* INNOBASE_UT0RBT_H */
