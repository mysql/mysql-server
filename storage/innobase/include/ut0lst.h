/*****************************************************************************

Copyright (c) 1995, 2015, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file include/ut0lst.h
List utilities

Created 9/10/1995 Heikki Tuuri
Rewritten by Sunny Bains Dec 2011.
***********************************************************************/

#ifndef ut0lst_h
#define ut0lst_h

/* Do not include univ.i because univ.i includes this. */

#include "ut0dbg.h"

/* This module implements the two-way linear list. Note that a single
list node may belong to two or more lists, but is only on one list
at a time. */

/*******************************************************************//**
The two way list node.
@param TYPE the list node type name */
template <typename Type>
struct ut_list_node {
	Type*		prev;			/*!< pointer to the previous
						node, NULL if start of list */
	Type*		next;			/*!< pointer to next node,
						NULL if end of list */

	void reverse()
	{
		Type*	tmp = prev;
		prev = next;
		next = tmp;
	}
};

/** Macro used for legacy reasons */
#define UT_LIST_NODE_T(t)		ut_list_node<t>

/*******************************************************************//**
The two-way list base node. The base node contains pointers to both ends
of the list and a count of nodes in the list (excluding the base node
from the count). We also store a pointer to the member field so that it
doesn't have to be specified when doing list operations.
@param Type the type of the list element
@param NodePtr field member pointer that points to the list node */
template <typename Type, typename NodePtr>
struct ut_list_base {
	typedef Type elem_type;
	typedef NodePtr node_ptr;
	typedef ut_list_node<Type> node_type;

	ulint		count;			/*!< count of nodes in list */
	elem_type*	start;			/*!< pointer to list start,
						NULL if empty */
	elem_type*	end;			/*!< pointer to list end,
						NULL if empty */
	node_ptr	node;			/*!< Pointer to member field
						that is used as a link node */
#ifdef UNIV_DEBUG
	ulint		init;			/*!< UT_LIST_INITIALISED if
						the list was initialised with
						UT_LIST_INIT() */
#endif /* UNIV_DEBUG */

	void reverse()
	{
		Type*	tmp = start;
		start = end;
		end = tmp;
	}
};

#define UT_LIST_BASE_NODE_T(t)	ut_list_base<t, ut_list_node<t> t::*>

#ifdef UNIV_DEBUG
# define UT_LIST_INITIALISED		0xCAFE
# define UT_LIST_INITIALISE(b)		(b).init = UT_LIST_INITIALISED
# define UT_LIST_IS_INITIALISED(b)	ut_a(((b).init == UT_LIST_INITIALISED))
#else
# define UT_LIST_INITIALISE(b)
# define UT_LIST_IS_INITIALISED(b)
#endif /* UNIV_DEBUG */

/*******************************************************************//**
Note: This is really the list constructor. We should be able to use
placement new here.
Initializes the base node of a two-way list.
@param b the list base node
@param pmf point to member field that will be used as the link node */
#define UT_LIST_INIT(b, pmf)						\
{									\
	(b).count = 0;							\
	(b).start = 0;							\
	(b).end   = 0;							\
	(b).node  = pmf;						\
	UT_LIST_INITIALISE(b);						\
}

/** Functor for accessing the embedded node within a list element. This is
required because some lists can have the node emebedded inside a nested
struct/union. See lock0priv.h (table locks) for an example. It provides a
specialised functor to grant access to the list node. */
template <typename Type>
struct GenericGetNode {

	typedef ut_list_node<Type> node_type;

	GenericGetNode(node_type Type::* node) : m_node(node) {}

	node_type& operator() (Type& elem)
	{
		return(elem.*m_node);
	}

	node_type	Type::*m_node;
};

/*******************************************************************//**
Adds the node as the first element in a two-way linked list.
@param list the base node (not a pointer to it)
@param elem the element to add */
template <typename List>
void
ut_list_prepend(
	List&				list,
	typename List::elem_type*	elem)
{
	typename List::node_type&	elem_node = elem->*list.node;

	UT_LIST_IS_INITIALISED(list);

	elem_node.prev = 0;
	elem_node.next = list.start;

	if (list.start != 0) {
		typename List::node_type&	base_node =
			list.start->*list.node;

		ut_ad(list.start != elem);

		base_node.prev = elem;
	}

	list.start = elem;

	if (list.end == 0) {
		list.end = elem;
	}

	++list.count;
}

/*******************************************************************//**
Adds the node as the first element in a two-way linked list.
@param LIST the base node (not a pointer to it)
@param ELEM the element to add */
#define UT_LIST_ADD_FIRST(LIST, ELEM)	ut_list_prepend(LIST, ELEM)

/*******************************************************************//**
Adds the node as the last element in a two-way linked list.
@param list list
@param elem the element to add
@param get_node to get the list node for that element */
template <typename List, typename Functor>
void
ut_list_append(
	List&				list,
	typename List::elem_type*	elem,
	Functor				get_node)
{
	typename List::node_type&	node = get_node(*elem);

	UT_LIST_IS_INITIALISED(list);

	node.next = 0;
	node.prev = list.end;

	if (list.end != 0) {
		typename List::node_type&	base_node = get_node(*list.end);

		ut_ad(list.end != elem);

		base_node.next = elem;
	}

	list.end = elem;

	if (list.start == 0) {
		list.start = elem;
	}

	++list.count;
}

/*******************************************************************//**
Adds the node as the last element in a two-way linked list.
@param list list
@param elem the element to add */
template <typename List>
void
ut_list_append(
	List&				list,
	typename List::elem_type*	elem)
{
	ut_list_append(
		list, elem,
		GenericGetNode<typename List::elem_type>(list.node));
}

/*******************************************************************//**
Adds the node as the last element in a two-way linked list.
@param LIST list base node (not a pointer to it)
@param ELEM the element to add */
#define UT_LIST_ADD_LAST(LIST, ELEM)	ut_list_append(LIST, ELEM)

/*******************************************************************//**
Inserts a ELEM2 after ELEM1 in a list.
@param list the base node
@param elem1 node after which ELEM2 is inserted
@param elem2 node being inserted after ELEM1 */
template <typename List>
void
ut_list_insert(
	List&				list,
	typename List::elem_type*	elem1,
	typename List::elem_type*	elem2)
{
	ut_ad(elem1 != elem2);
	UT_LIST_IS_INITIALISED(list);

	typename List::node_type&	elem1_node = elem1->*list.node;
	typename List::node_type&	elem2_node = elem2->*list.node;

	elem2_node.prev = elem1;
	elem2_node.next = elem1_node.next;

	if (elem1_node.next != NULL) {
		typename List::node_type&	next_node =
			elem1_node.next->*list.node;

		next_node.prev = elem2;
	}

	elem1_node.next = elem2;

	if (list.end == elem1) {
		list.end = elem2;
	}

	++list.count;
}

/*******************************************************************//**
Inserts a ELEM2 after ELEM1 in a list.
@param LIST list base node (not a pointer to it)
@param ELEM1 node after which ELEM2 is inserted
@param ELEM2 node being inserted after ELEM1 */
#define UT_LIST_INSERT_AFTER(LIST, ELEM1, ELEM2)			\
	ut_list_insert(LIST, ELEM1, ELEM2)

/*******************************************************************//**
Removes a node from a two-way linked list.
@param list the base node (not a pointer to it)
@param node member node within list element that is to be removed
@param get_node functor to get the list node from elem */
template <typename List, typename Functor>
void
ut_list_remove(
	List&				list,
	typename List::node_type&	node,
	Functor				get_node)
{
	ut_a(list.count > 0);
	UT_LIST_IS_INITIALISED(list);

	if (node.next != NULL) {
		typename List::node_type&	next_node =
			get_node(*node.next);

		next_node.prev = node.prev;
	} else {
		list.end = node.prev;
	}

	if (node.prev != NULL) {
		typename List::node_type&	prev_node =
			get_node(*node.prev);

		prev_node.next = node.next;
	} else {
		list.start = node.next;
	}

	node.next = 0;
	node.prev = 0;

	--list.count;
}

/*******************************************************************//**
Removes a node from a two-way linked list.
@param list the base node (not a pointer to it)
@param elem element to be removed from the list
@param get_node functor to get the list node from elem */
template <typename List, typename Functor>
void
ut_list_remove(
	List&				list,
	typename List::elem_type*	elem,
	Functor				get_node)
{
	ut_list_remove(list, get_node(*elem), get_node);
}

/*******************************************************************//**
Removes a node from a two-way linked list.
@param list the base node (not a pointer to it)
@param elem element to be removed from the list */
template <typename List>
void
ut_list_remove(
	List&				list,
	typename List::elem_type*	elem)
{
	ut_list_remove(
		list, elem->*list.node,
		GenericGetNode<typename List::elem_type>(list.node));
}

/*******************************************************************//**
Removes a node from a two-way linked list.
@param LIST the base node (not a pointer to it)
@param ELEM node to be removed from the list */
#define UT_LIST_REMOVE(LIST, ELEM)	ut_list_remove(LIST, ELEM)

/********************************************************************//**
Gets the next node in a two-way list.
@param NAME list name
@param N pointer to a node
@return the successor of N in NAME, or NULL */
#define UT_LIST_GET_NEXT(NAME, N)	(((N)->NAME).next)

/********************************************************************//**
Gets the previous node in a two-way list.
@param NAME list name
@param N pointer to a node
@return the predecessor of N in NAME, or NULL */
#define UT_LIST_GET_PREV(NAME, N)	(((N)->NAME).prev)

/********************************************************************//**
Alternative macro to get the number of nodes in a two-way list, i.e.,
its length.
@param BASE the base node (not a pointer to it).
@return the number of nodes in the list */
#define UT_LIST_GET_LEN(BASE)		(BASE).count

/********************************************************************//**
Gets the first node in a two-way list.
@param BASE the base node (not a pointer to it)
@return first node, or NULL if the list is empty */
#define UT_LIST_GET_FIRST(BASE)		(BASE).start

/********************************************************************//**
Gets the last node in a two-way list.
@param BASE the base node (not a pointer to it)
@return last node, or NULL if the list is empty */
#define UT_LIST_GET_LAST(BASE)		(BASE).end

struct	NullValidate { void operator()(const void* elem) { } };

/********************************************************************//**
Iterate over all the elements and call the functor for each element.
@param[in]	list	base node (not a pointer to it)
@param[in,out]	functor	Functor that is called for each element in the list */
template <typename List, class Functor>
void
ut_list_map(
	const List&	list,
	Functor&	functor)
{
	ulint		count = 0;

	UT_LIST_IS_INITIALISED(list);

	for (typename List::elem_type* elem = list.start;
	     elem != 0;
	     elem = (elem->*list.node).next, ++count) {

		functor(elem);
	}

	ut_a(count == list.count);
}

template <typename List>
void
ut_list_reverse(List& list)
{
	UT_LIST_IS_INITIALISED(list);

	for (typename List::elem_type* elem = list.start;
	     elem != 0;
	     elem = (elem->*list.node).prev) {
		(elem->*list.node).reverse();
	}

	list.reverse();
}

#define UT_LIST_REVERSE(LIST)	ut_list_reverse(LIST)

/********************************************************************//**
Checks the consistency of a two-way list.
@param[in]		list base node (not a pointer to it)
@param[in,out]		functor Functor that is called for each element in the list */
template <typename List, class Functor>
void
ut_list_validate(
	const List&	list,
	Functor&	functor)
{
	ut_list_map(list, functor);

	/* Validate the list backwards. */
	ulint		count = 0;

	for (typename List::elem_type* elem = list.end;
	     elem != 0;
	     elem = (elem->*list.node).prev) {
		++count;
	}

	ut_a(count == list.count);
}

/** Check the consistency of a two-way list.
@param[in] LIST base node reference */
#define UT_LIST_CHECK(LIST) do {		\
	NullValidate nullV;			\
	ut_list_validate(LIST, nullV);		\
} while (0)

/** Move the given element to the beginning of the list.
@param[in,out]	list	the list object
@param[in]	elem	the element of the list which will be moved
			to the beginning of the list. */
template <typename List>
void
ut_list_move_to_front(
	List&				list,
	typename List::elem_type*	elem)
{
	ut_ad(ut_list_exists(list, elem));

	if (UT_LIST_GET_FIRST(list) != elem) {
		ut_list_remove(list, elem);
		ut_list_prepend(list, elem);
	}
}

#ifdef UNIV_DEBUG
/** Check if the given element exists in the list.
@param[in,out]	list	the list object
@param[in]	elem	the element of the list which will be checked */
template <typename List>
bool
ut_list_exists(
	List&				list,
	typename List::elem_type*	elem)
{
	typename List::elem_type*	e1;

	for (e1 = UT_LIST_GET_FIRST(list); e1 != NULL;
	     e1 = (e1->*list.node).next) {
		if (elem == e1) {
			return(true);
		}
	}
	return(false);
}
#endif

#define UT_LIST_MOVE_TO_FRONT(LIST, ELEM) \
   ut_list_move_to_front(LIST, ELEM)

#endif /* ut0lst.h */
