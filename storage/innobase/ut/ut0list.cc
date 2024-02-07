/*****************************************************************************

Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

/** @file ut/ut0list.cc
 A double-linked list

 Created 4/26/2006 Osku Salerma
 ************************************************************************/

#include "ut0list.h"

#include <stddef.h>

/** Create a new list.
 @return list */
ib_list_t *ib_list_create(void) {
  return (static_cast<ib_list_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(ib_list_t))));
}

/** Free a list. */
void ib_list_free(ib_list_t *list) /*!< in: list */
{
  /* We don't check that the list is empty because it's entirely valid
  to e.g. have all the nodes allocated from a single heap that is then
  freed after the list itself is freed. */

  ut::free(list);
}

/** Add the data after the indicated node.
 @return new list node */
static ib_list_node_t *ib_list_add_after(
    ib_list_t *list,           /*!< in: list */
    ib_list_node_t *prev_node, /*!< in: node preceding new node (can
                               be NULL) */
    void *data,                /*!< in: data */
    mem_heap_t *heap)          /*!< in: memory heap to use */
{
  ib_list_node_t *node;

  node = static_cast<ib_list_node_t *>(mem_heap_alloc(heap, sizeof(*node)));

  node->data = data;

  if (!list->first) {
    /* Empty list. */

    ut_a(!prev_node);

    node->prev = nullptr;
    node->next = nullptr;

    list->first = node;
    list->last = node;
  } else if (!prev_node) {
    /* Start of list. */

    node->prev = nullptr;
    node->next = list->first;

    list->first->prev = node;

    list->first = node;
  } else {
    /* Middle or end of list. */

    node->prev = prev_node;
    node->next = prev_node->next;

    prev_node->next = node;

    if (node->next) {
      node->next->prev = node;
    } else {
      list->last = node;
    }
  }

  return (node);
}

/** Add the data to the end of the list.
 @return new list node */
ib_list_node_t *ib_list_add_last(
    ib_list_t *list,  /*!< in: list */
    void *data,       /*!< in: data */
    mem_heap_t *heap) /*!< in: memory heap to use */
{
  return (ib_list_add_after(list, ib_list_get_last(list), data, heap));
}

/** Remove the node from the list.
@param[in] list List
@param[in] node Node to remove */
void ib_list_remove(ib_list_t *list, ib_list_node_t *node) {
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    /* First item in list. */

    ut_ad(list->first == node);

    list->first = node->next;
  }

  if (node->next) {
    node->next->prev = node->prev;
  } else {
    /* Last item in list. */

    ut_ad(list->last == node);

    list->last = node->prev;
  }

  node->prev = node->next = nullptr;
}
