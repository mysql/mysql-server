/******************************************************
Red-Black tree implementation.
(c) 2007 Oracle/Innobase Oy

Created 2007-03-20 Sunny Bains
*******************************************************/

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
// FIXME: Iterator is a better name than _bound_
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

/************************************************************************
Free an instance of  a red black tree */
UNIV_INTERN
void
rbt_free(
/*=====*/
	ib_rbt_t*	tree);			/* in: rb tree to free */
/************************************************************************
Create an instance of a red black tree */
UNIV_INTERN
ib_rbt_t*
rbt_create(
/*=======*/
						/* out: rb tree instance */
	size_t		sizeof_value,		/* in: size in bytes */
	ib_rbt_compare	compare);		/* in: comparator */
/************************************************************************
Delete a node from the red black tree, identified by key */
UNIV_INTERN
ibool
rbt_delete(
/*=======*/
						/* in: TRUE on success */
	ib_rbt_t*	tree,			/* in: rb tree */
	const void*	key);			/* in: key to delete */
/************************************************************************
Remove a node from the red black tree, NOTE: This function will not delete
the node instance, THAT IS THE CALLERS RESPONSIBILITY.*/
UNIV_INTERN
ib_rbt_node_t*
rbt_remove_node(
/*============*/
						/* out: the deleted node
						with the const.*/
	ib_rbt_t*	tree,			/* in: rb tree */
	const ib_rbt_node_t*
			node);			/* in: node to delete, this
						is a fudge and declared const
						because the caller has access
						only to const nodes.*/
/************************************************************************
Return a node from the red black tree, identified by
key, NULL if not found */
UNIV_INTERN
const ib_rbt_node_t*
rbt_lookup(
/*=======*/
						/* out: node if found else
						return NULL*/
	const ib_rbt_t*	tree,			/* in: rb tree to search */
	const void*	key);			/* in: key to lookup */
/************************************************************************
Add data to the red black tree, identified by key (no dups yet!)*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_insert(
/*=======*/
						/* out: inserted node */
	ib_rbt_t*	tree,			/* in: rb tree */
	const void*	key,			/* in: key for ordering */
	const void*	value);			/* in: data that will be
						copied to the node.*/
/************************************************************************
Add a new node to the tree, useful for data that is pre-sorted.*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_add_node(
/*=========*/
						/* out: appended node */
	ib_rbt_t*	tree,			/* in: rb tree */
	ib_rbt_bound_t*	parent,			/* in: parent */
	const void*	value);			/* in: this value is copied
						to the node */
/************************************************************************
Return the left most data node in the tree*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_first(
/*======*/
						/* out: left most node */
	const ib_rbt_t*	tree);			/* in: rb tree */
/************************************************************************
Return the right most data node in the tree*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_last(
/*=====*/
						/* out: right most node */
	const ib_rbt_t*	tree);			/* in: rb tree */
/************************************************************************
Return the next node from current.*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_next(
/*=====*/
						/* out: successor node to
						current that is passed in.*/
	const ib_rbt_t*	tree,			/* in: rb tree */
	const ib_rbt_node_t*			/* in: current node */
			current);
/************************************************************************
Return the prev node from current.*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_prev(
/*=====*/
						/* out: precedessor node to
						current that is passed in */
	const ib_rbt_t*	tree,			/* in: rb tree */
	const ib_rbt_node_t*			/* in: current node */
			current);
/************************************************************************
Find the node that has the lowest key that is >= key.*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_lower_bound(
/*============*/
						/* out: node that satisfies
						the lower bound constraint or
						NULL */
	const ib_rbt_t*	tree,			/* in: rb tree */
	const void*	key);			/* in: key to search */
/************************************************************************
Find the node that has the greatest key that is <= key.*/
UNIV_INTERN
const ib_rbt_node_t*
rbt_upper_bound(
/*============*/
						/* out: node that satisifies
						the upper bound constraint or
						NULL */
	const ib_rbt_t*	tree,			/* in: rb tree */
	const void*	key);			/* in: key to search */
/************************************************************************
Search for the key, a node will be retuned in parent.last, whether it
was found or not. If not found then parent.last will contain the
parent node for the possibly new key otherwise the matching node.*/
UNIV_INTERN
int
rbt_search(
/*=======*/
						/* out: result of last
						comparison */
	const ib_rbt_t*	tree,			/* in: rb tree */
	ib_rbt_bound_t*	parent,			/* in: search bounds */
	const void*	key);			/* in: key to search */
/************************************************************************
Search for the key, a node will be retuned in parent.last, whether it
was found or not. If not found then parent.last will contain the
parent node for the possibly new key otherwise the matching node.*/
UNIV_INTERN
int
rbt_search_cmp(
/*===========*/
						/* out: result of last
						comparison */
	const ib_rbt_t*	tree,			/* in: rb tree */
	ib_rbt_bound_t*	parent,			/* in: search bounds */
	const void*	key,			/* in: key to search */
	ib_rbt_compare	compare);		/* in: comparator */
/************************************************************************
Clear the tree, deletes (and free's) all the nodes.*/
UNIV_INTERN
void
rbt_clear(
/*======*/
	ib_rbt_t*	tree);			/* in: rb tree */
/************************************************************************
Merge the node from dst into src. Return the number of nodes merged.*/
UNIV_INTERN
ulint
rbt_merge_uniq(
/*===========*/
						/* out: no. of recs merged */
	ib_rbt_t*	dst,			/* in: dst rb tree */
	const ib_rbt_t*	src);			/* in: src rb tree */
/************************************************************************
Merge the node from dst into src. Return the number of nodes merged.
Delete the nodes from src after copying node to dst. As a side effect
the duplicates will be left untouched in the src, since we don't support
duplicates (yet). NOTE: src and dst must be similar, the function doesn't
check for this condition (yet).*/
UNIV_INTERN
ulint
rbt_merge_uniq_destructive(
/*=======================*/
						/* out: no. of recs merged */
	ib_rbt_t*	dst,			/* in: dst rb tree */
	ib_rbt_t*	src);			/* in: src rb tree */
/************************************************************************
Verify the integrity of the RB tree. For debugging. 0 failure else height
of tree (in count of black nodes).*/
UNIV_INTERN
ibool
rbt_validate(
/*=========*/
						/* out: TRUE if OK
						FALSE if tree invalid.*/
	const ib_rbt_t*	tree);			/* in: tree to validate */
/************************************************************************
Iterate over the tree in depth first order.*/
UNIV_INTERN
void
rbt_print(
/*======*/
	const ib_rbt_t*		tree,		/* in: tree to traverse */
	ib_rbt_print_node	print);		/* in: print function */

#endif /* INNOBASE_UT0RBT_H */
