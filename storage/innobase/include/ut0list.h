/*****************************************************************************

Copyright (c) 2006, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/ut0list.h
 A double-linked list

 Created 4/26/2006 Osku Salerma
 ************************************************************************/

/** A double-linked list. This differs from the one in ut0lst.h in that in this
 one, each list node contains a pointer to the data, whereas the one in
 ut0lst.h uses a strategy where the list pointers are embedded in the data
 items themselves.

 Use this one when you need to store arbitrary data in the list where you
 can't embed the list pointers in the data, if a data item needs to be
 stored in multiple lists, etc.

 Note about the memory management: ib_list_t is a fixed-size struct whose
 allocation/deallocation is done through ib_list_create/ib_list_free, but the
 memory for the list nodes is allocated through a user-given memory heap,
 which can either be the same for all nodes or vary per node. Most users will
 probably want to create a memory heap to store the item-specific data, and
 pass in this same heap to the list node creation functions, thus
 automatically freeing the list node when the item's heap is freed.

 ************************************************************************/

#ifndef IB_LIST_H
#define IB_LIST_H

#include "mem0mem.h"

struct ib_list_t;
struct ib_list_node_t;

/** Create a new list using mem_alloc. Lists created with this function must be
 freed with ib_list_free.
 @return list */
ib_list_t *ib_list_create(void);

/** Free a list. */
void ib_list_free(ib_list_t *list); /*!< in: list */

/** Add the data to the end of the list.
 @return new list node */
ib_list_node_t *ib_list_add_last(
    ib_list_t *list,   /*!< in: list */
    void *data,        /*!< in: data */
    mem_heap_t *heap); /*!< in: memory heap to use */

/** Remove the node from the list. */
void ib_list_remove(ib_list_t *list,       /*!< in: list */
                    ib_list_node_t *node); /*!< in: node to remove */

/** Get the first node in the list.
 @return first node, or NULL */
UNIV_INLINE
ib_list_node_t *ib_list_get_first(ib_list_t *list); /*!< in: list */

/** Get the last node in the list.
 @return last node, or NULL */
UNIV_INLINE
ib_list_node_t *ib_list_get_last(ib_list_t *list); /*!< in: list */

/********************************************************************
Check if list is empty. */
UNIV_INLINE
ibool ib_list_is_empty(
    /* out: TRUE if empty else  */
    const ib_list_t *list); /* in: list */

/* List. */
struct ib_list_t {
  ib_list_node_t *first; /*!< first node */
  ib_list_node_t *last;  /*!< last node */
};

/* A list node. */
struct ib_list_node_t {
  ib_list_node_t *prev; /*!< previous node */
  ib_list_node_t *next; /*!< next node */
  void *data;           /*!< user data */
};

/* Quite often, the only additional piece of data you need is the per-item
memory heap, so we have this generic struct available to use in those
cases. */
struct ib_list_helper_t {
  mem_heap_t *heap; /*!< memory heap */
  void *data;       /*!< user data */
};

#include "ut0list.ic"

#endif
