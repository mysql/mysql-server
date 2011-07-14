/*****************************************************************************

Copyright (c) 1995, 2010, Innobase Oy. All Rights Reserved.

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

/******************************************************************//**
@file include/ut0lst.h
List utilities

Created 9/10/1995 Heikki Tuuri
***********************************************************************/

#ifndef ut0lst_h
#define ut0lst_h

#include "univ.i"

/* This module implements the two-way linear list which should be used
if a list is used in the database. Note that a single struct may belong
to two or more lists, provided that the list are given different names.
An example of the usage of the lists can be found in fil0fil.c. */

/*******************************************************************//**
This macro expands to the unnamed type definition of a struct which acts
as the two-way list base node. The base node contains pointers
to both ends of the list and a count of nodes in the list (excluding
the base node from the count).
@param TYPE	the name of the list node data type */
#define UT_LIST_BASE_NODE_T(TYPE)\
struct {\
	ulint	count;	/*!< count of nodes in list */\
	TYPE *	start;	/*!< pointer to list start, NULL if empty */\
	TYPE *	end;	/*!< pointer to list end, NULL if empty */\
}\

/*******************************************************************//**
This macro expands to the unnamed type definition of a struct which
should be embedded in the nodes of the list, the node type must be a struct.
This struct contains the pointers to next and previous nodes in the list.
The name of the field in the node struct should be the name given
to the list.
@param TYPE	the list node type name */
/* Example:
typedef struct LRU_node_struct	LRU_node_t;
struct LRU_node_struct {
	UT_LIST_NODE_T(LRU_node_t)	LRU_list;
	...
}
The example implements an LRU list of name LRU_list. Its nodes are of type
LRU_node_t. */

#define UT_LIST_NODE_T(TYPE)\
struct {\
	TYPE *	prev;	/*!< pointer to the previous node,\
			NULL if start of list */\
	TYPE *	next;	/*!< pointer to next node, NULL if end of list */\
}\

/*******************************************************************//**
Initializes the base node of a two-way list.
@param BASE	the list base node
*/
#define UT_LIST_INIT(BASE)\
{\
	(BASE).count = 0;\
	(BASE).start = NULL;\
	(BASE).end   = NULL;\
}\

/*******************************************************************//**
Adds the node as the first element in a two-way linked list.
@param NAME	list name
@param BASE	the base node (not a pointer to it)
@param N	pointer to the node to be added to the list.
*/
#define UT_LIST_ADD_FIRST(NAME, BASE, N)\
{\
	ut_ad(N);\
	((BASE).count)++;\
	((N)->NAME).next = (BASE).start;\
	((N)->NAME).prev = NULL;\
	if (UNIV_LIKELY((BASE).start != NULL)) {\
		ut_ad((BASE).start != (N));\
		(((BASE).start)->NAME).prev = (N);\
	}\
	(BASE).start = (N);\
	if (UNIV_UNLIKELY((BASE).end == NULL)) {\
		(BASE).end = (N);\
	}\
}\

/*******************************************************************//**
Adds the node as the last element in a two-way linked list.
@param NAME	list name
@param BASE	the base node (not a pointer to it)
@param N	pointer to the node to be added to the list
*/
#define UT_LIST_ADD_LAST(NAME, BASE, N)\
{\
	ut_ad(N != NULL);\
	((BASE).count)++;\
	((N)->NAME).prev = (BASE).end;\
	((N)->NAME).next = NULL;\
	if ((BASE).end != NULL) {\
		ut_ad((BASE).end != (N));\
		(((BASE).end)->NAME).next = (N);\
	}\
	(BASE).end = (N);\
	if ((BASE).start == NULL) {\
		(BASE).start = (N);\
	}\
}\

/*******************************************************************//**
Inserts a NODE2 after NODE1 in a list.
@param NAME	list name
@param BASE	the base node (not a pointer to it)
@param NODE1	pointer to node after which NODE2 is inserted
@param NODE2	pointer to node being inserted after NODE1
*/
#define UT_LIST_INSERT_AFTER(NAME, BASE, NODE1, NODE2)\
{\
	ut_ad(NODE1);\
	ut_ad(NODE2);\
	ut_ad((NODE1) != (NODE2));\
	((BASE).count)++;\
	((NODE2)->NAME).prev = (NODE1);\
	((NODE2)->NAME).next = ((NODE1)->NAME).next;\
	if (((NODE1)->NAME).next != NULL) {\
		((((NODE1)->NAME).next)->NAME).prev = (NODE2);\
	}\
	((NODE1)->NAME).next = (NODE2);\
	if ((BASE).end == (NODE1)) {\
		(BASE).end = (NODE2);\
	}\
}\

#ifdef UNIV_LIST_DEBUG
/** Invalidate the pointers in a list node.
@param NAME	list name
@param N	pointer to the node that was removed */
# define UT_LIST_REMOVE_CLEAR(NAME, N)		\
((N)->NAME.prev = (N)->NAME.next = (void*) -1)
#else
/** Invalidate the pointers in a list node.
@param NAME	list name
@param N	pointer to the node that was removed */
# define UT_LIST_REMOVE_CLEAR(NAME, N) while (0)
#endif

/*******************************************************************//**
Removes a node from a two-way linked list.
@param NAME	list name
@param BASE	the base node (not a pointer to it)
@param N	pointer to the node to be removed from the list
*/
#define UT_LIST_REMOVE(NAME, BASE, N)					\
do {									\
	ut_ad(N);							\
	ut_a((BASE).count > 0);						\
	((BASE).count)--;						\
	if (((N)->NAME).next != NULL) {					\
		((((N)->NAME).next)->NAME).prev = ((N)->NAME).prev;	\
	} else {							\
		(BASE).end = ((N)->NAME).prev;				\
	}								\
	if (((N)->NAME).prev != NULL) {					\
		((((N)->NAME).prev)->NAME).next = ((N)->NAME).next;	\
	} else {							\
		(BASE).start = ((N)->NAME).next;			\
	}								\
	UT_LIST_REMOVE_CLEAR(NAME, N);					\
} while (0)

/********************************************************************//**
Gets the next node in a two-way list.
@param NAME	list name
@param N	pointer to a node
@return		the successor of N in NAME, or NULL */
#define UT_LIST_GET_NEXT(NAME, N)\
	(((N)->NAME).next)

/********************************************************************//**
Gets the previous node in a two-way list.
@param NAME	list name
@param N	pointer to a node
@return		the predecessor of N in NAME, or NULL */
#define UT_LIST_GET_PREV(NAME, N)\
	(((N)->NAME).prev)

/********************************************************************//**
Alternative macro to get the number of nodes in a two-way list, i.e.,
its length.
@param BASE	the base node (not a pointer to it).
@return		the number of nodes in the list */
#define UT_LIST_GET_LEN(BASE)\
	(BASE).count

/********************************************************************//**
Gets the first node in a two-way list.
@param BASE	the base node (not a pointer to it)
@return		first node, or NULL if the list is empty */
#define UT_LIST_GET_FIRST(BASE)\
	(BASE).start

/********************************************************************//**
Gets the last node in a two-way list.
@param BASE	the base node (not a pointer to it)
@return		last node, or NULL if the list is empty */
#define UT_LIST_GET_LAST(BASE)\
	(BASE).end

/********************************************************************//**
Checks the consistency of a two-way list.
@param NAME		the name of the list
@param TYPE		node type
@param BASE		base node (not a pointer to it)
@param ASSERTION	a condition on ut_list_node_313 */
#define UT_LIST_VALIDATE(NAME, TYPE, BASE, ASSERTION)			\
do {									\
	ulint	ut_list_i_313;						\
	TYPE*	ut_list_node_313;					\
									\
	ut_list_node_313 = (BASE).start;				\
									\
	for (ut_list_i_313 = (BASE).count; ut_list_i_313--; ) {		\
		ut_a(ut_list_node_313);					\
		ASSERTION;						\
		ut_ad((ut_list_node_313->NAME).next || !ut_list_i_313);	\
		ut_list_node_313 = (ut_list_node_313->NAME).next;	\
	}								\
									\
	ut_a(ut_list_node_313 == NULL);					\
									\
	ut_list_node_313 = (BASE).end;					\
									\
	for (ut_list_i_313 = (BASE).count; ut_list_i_313--; ) {		\
		ut_a(ut_list_node_313);					\
		ASSERTION;						\
		ut_ad((ut_list_node_313->NAME).prev || !ut_list_i_313);	\
		ut_list_node_313 = (ut_list_node_313->NAME).prev;	\
	}								\
									\
	ut_a(ut_list_node_313 == NULL);					\
} while (0)

#endif

