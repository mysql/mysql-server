/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
***********************************************************************/

#ifndef ut0lst_h
#define ut0lst_h

#include "univ.i"

/*******************************************************************//**
Return offset of F in POD T.
@param T	- POD pointer
@param F	- Field in T */
#define IB_OFFSETOF(T, F)						\
	(reinterpret_cast<byte*>(&(T)->F) - reinterpret_cast<byte*>(T))

/* This module implements the two-way linear list which should be used
if a list is used in the database. Note that a single struct may belong
to two or more lists, provided that the list are given different names.
An example of the usage of the lists can be found in fil0fil.cc. */

/*******************************************************************//**
This macro expands to the unnamed type definition of a struct which acts
as the two-way list base node. The base node contains pointers
to both ends of the list and a count of nodes in the list (excluding
the base node from the count).
@param TYPE	the name of the list node data type */
template <typename TYPE>
struct ut_list_base {
	typedef TYPE elem_type;

	ulint	count;	/*!< count of nodes in list */
	TYPE*	start;	/*!< pointer to list start, NULL if empty */
	TYPE*	end;	/*!< pointer to list end, NULL if empty */
};

#define UT_LIST_BASE_NODE_T(TYPE)	ut_list_base<TYPE>

/*******************************************************************//**
This macro expands to the unnamed type definition of a struct which
should be embedded in the nodes of the list, the node type must be a struct.
This struct contains the pointers to next and previous nodes in the list.
The name of the field in the node struct should be the name given
to the list.
@param TYPE	the list node type name */
/* Example:
struct LRU_node_t {
	UT_LIST_NODE_T(LRU_node_t)	LRU_list;
	...
}
The example implements an LRU list of name LRU_list. Its nodes are of type
LRU_node_t. */

template <typename TYPE>
struct ut_list_node {
	TYPE* 	prev;	/*!< pointer to the previous node,
			NULL if start of list */
	TYPE* 	next;	/*!< pointer to next node, NULL if end of list */
};

#define UT_LIST_NODE_T(TYPE)	ut_list_node<TYPE>

/*******************************************************************//**
Get the list node at offset.
@param elem	- list element
@param offset	- offset within element.
@return reference to list node. */
template <typename Type>
ut_list_node<Type>&
ut_elem_get_node(Type&	elem, size_t offset)
{
	ut_a(offset < sizeof(elem));

	return(*reinterpret_cast<ut_list_node<Type>*>(
		reinterpret_cast<byte*>(&elem) + offset));
}

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
@param list	the base node (not a pointer to it)
@param elem	the element to add
@param offset	offset of list node in elem. */
template <typename List, typename Type>
void
ut_list_prepend(
	List&		list,
	Type&		elem,
	size_t		offset)
{
	ut_list_node<Type>&	elem_node = ut_elem_get_node(elem, offset);

 	elem_node.prev = 0;
 	elem_node.next = list.start;

	if (list.start != 0) {
		ut_list_node<Type>&	base_node =
			ut_elem_get_node(*list.start, offset);

		ut_ad(list.start != &elem);

		base_node.prev = &elem;
	}

	list.start = &elem;

	if (list.end == 0) {
		list.end = &elem;
	}

	++list.count;
}

/*******************************************************************//**
Adds the node as the first element in a two-way linked list.
@param NAME	list name
@param LIST	the base node (not a pointer to it)
@param ELEM	the element to add */
#define UT_LIST_ADD_FIRST(NAME, LIST, ELEM)	\
	ut_list_prepend(LIST, *ELEM, IB_OFFSETOF(ELEM, NAME))

/*******************************************************************//**
Adds the node as the last element in a two-way linked list.
@param list	list
@param elem	the element to add
@param offset	offset of list node in elem */
template <typename List, typename Type>
void
ut_list_append(
	List&		list,
	Type&		elem,
	size_t		offset)
{
	ut_list_node<Type>&	elem_node = ut_elem_get_node(elem, offset);

	elem_node.next = 0;
	elem_node.prev = list.end;

	if (list.end != 0) {
		ut_list_node<Type>&	base_node =
			ut_elem_get_node(*list.end, offset);

		ut_ad(list.end != &elem);

		base_node.next = &elem;
	}

	list.end = &elem;

	if (list.start == 0) {
		list.start = &elem;
	}

	++list.count;
}

/*******************************************************************//**
Adds the node as the last element in a two-way linked list.
@param NAME	list name
@param LIST	list
@param ELEM	the element to add */
#define UT_LIST_ADD_LAST(NAME, LIST, ELEM)\
	ut_list_append(LIST, *ELEM, IB_OFFSETOF(ELEM, NAME))

/*******************************************************************//**
Inserts a ELEM2 after ELEM1 in a list.
@param list	the base node
@param elem1	node after which ELEM2 is inserted
@param elem2	node being inserted after NODE1
@param offset	offset of list node in elem1 and elem2 */
template <typename List, typename Type>
void
ut_list_insert(
	List&		list,
	Type&		elem1,
	Type&		elem2,
	size_t		offset)
{
	ut_ad(&elem1 != &elem2);

	ut_list_node<Type>&	elem1_node = ut_elem_get_node(elem1, offset);
	ut_list_node<Type>&	elem2_node = ut_elem_get_node(elem2, offset);

	elem2_node.prev = &elem1;
	elem2_node.next = elem1_node.next;

	if (elem1_node.next != NULL) {
		ut_list_node<Type>&	next_node =
			ut_elem_get_node(*elem1_node.next, offset);

		next_node.prev = &elem2;
	}

	elem1_node.next = &elem2;

	if (list.end == &elem1) {
		list.end = &elem2;
	}

	++list.count;
}

/*******************************************************************//**
Inserts a ELEM2 after ELEM1 in a list.
@param NAME	list name
@param LIST	the base node
@param ELEM1	node after which ELEM2 is inserted
@param ELEM2	node being inserted after ELEM1 */
#define UT_LIST_INSERT_AFTER(NAME, LIST, ELEM1, ELEM2)\
	ut_list_insert(LIST, *ELEM1, *ELEM2, IB_OFFSETOF(ELEM1, NAME))

#ifdef UNIV_LIST_DEBUG
/** Invalidate the pointers in a list node.
@param NAME	list name
@param N	pointer to the node that was removed */
# define UT_LIST_REMOVE_CLEAR(N)					\
	(N).next = (Type*) -1;						\
	(N).prev = (N).next
#else
/** Invalidate the pointers in a list node.
@param NAME	list name
@param N	pointer to the node that was removed */
# define UT_LIST_REMOVE_CLEAR(N)
#endif /* UNIV_LIST_DEBUG */

/*******************************************************************//**
Removes a node from a two-way linked list.
@param list	the base node (not a pointer to it)
@param elem	node to be removed from the list
@param offset	offset of list node within elem */
template <typename List, typename Type>
void
ut_list_remove(
	List&		list,
 	Type&		elem,
	size_t		offset)
{
	ut_list_node<Type>&	elem_node = ut_elem_get_node(elem, offset);

	ut_a(list.count > 0);

	if (elem_node.next != NULL) {
		ut_list_node<Type>&	next_node =
			ut_elem_get_node(*elem_node.next, offset);

		next_node.prev = elem_node.prev;
	} else {
		list.end = elem_node.prev;
	}

	if (elem_node.prev != NULL) {
		ut_list_node<Type>&	prev_node =
			ut_elem_get_node(*elem_node.prev, offset);

		prev_node.next = elem_node.next;
	} else {
		list.start = elem_node.next;
	}

	UT_LIST_REMOVE_CLEAR(elem_node);

	--list.count;
}

/*******************************************************************//**
Removes a node from a two-way linked list.
  aram NAME	list name
@param LIST	the base node (not a pointer to it)
@param ELEM	node to be removed from the list */
#define UT_LIST_REMOVE(NAME, LIST, ELEM)				\
	ut_list_remove(LIST, *ELEM, IB_OFFSETOF(ELEM, NAME))

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

struct	NullValidate { void operator()(const void* elem) { } };

/********************************************************************//**
Iterate over all the elements and call the functor for each element.
@param list	base node (not a pointer to it)
@param functor	Functor that is called for each element in the list
@parm  node	pointer to member node within list element */
template <typename List, class Functor>
void
ut_list_map(
	List&		list,
	ut_list_node<typename List::elem_type>
			List::elem_type::*node,
	Functor		functor)
{
	ulint		count = 0;

	for (typename List::elem_type* elem = list.start;
	     elem != 0;
	     elem = (elem->*node).next, ++count) {

		functor(elem);
	}

	ut_a(count == list.count);
}

/********************************************************************//**
Checks the consistency of a two-way list.
@param list	base node (not a pointer to it)
@param functor	Functor that is called for each element in the list
@parm  node	pointer to member node within list element */
template <typename List, class Functor>
void
ut_list_validate(
	List&		list,
	ut_list_node<typename List::elem_type>
			List::elem_type::*node,
	Functor		functor = NullValidate())
{
	ut_list_map(list, node, functor);

	ulint		count = 0;

	for (typename List::elem_type* elem = list.end;
	     elem != 0;
	     elem = (elem->*node).prev, ++count) {

		functor(elem);
	}

	ut_a(count == list.count);
}

/********************************************************************//**
Checks the consistency of a two-way list.
@param NAME		the name of the list
@param TYPE		node type
@param LIST		base node (not a pointer to it)
@param FUNCTOR		called for each list element */
#define UT_LIST_VALIDATE(NAME, TYPE, LIST, FUNCTOR)			\
	ut_list_validate(LIST, &TYPE::NAME, FUNCTOR)

#define UT_LIST_CHECK(NAME, TYPE, LIST)					\
	ut_list_validate(LIST, &TYPE::NAME, NullValidate())

#endif /* ut0lst.h */
