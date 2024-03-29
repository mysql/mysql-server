/*****************************************************************************

Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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
/** @file include/ut0rbt.h
 Various utilities

 Created 2007-03-20 Sunny Bains
 *******************************************************/

#ifndef INNOBASE_UT0RBT_H
#define INNOBASE_UT0RBT_H

#if !defined(IB_RBT_TESTING)
#include "univ.i"
#include "ut0mem.h"
#else
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ut_malloc malloc
#define ut_free free
#define ulint unsigned long
#define ut_a(c) assert(c)
#define ut_error assert(0)
#endif

struct ib_rbt_node_t;
typedef void (*ib_rbt_print_node)(const ib_rbt_node_t *node);
typedef int (*ib_rbt_compare)(const void *p1, const void *p2);
typedef int (*ib_rbt_arg_compare)(const void *, const void *p1, const void *p2);

/** Red black tree color types */
enum ib_rbt_color_t { IB_RBT_RED, IB_RBT_BLACK };

/** Red black tree node */
struct ib_rbt_node_t {
  ib_rbt_color_t color; /* color of this node */

  ib_rbt_node_t *left;   /* points left child */
  ib_rbt_node_t *right;  /* points right child */
  ib_rbt_node_t *parent; /* points parent node */

  char value[1]; /* Data value */
};

/** Red black tree instance.*/
struct ib_rbt_t {
  ib_rbt_node_t *nil; /* Black colored node that is
                      used as a sentinel. This is
                      pre-allocated too.*/

  ib_rbt_node_t *root; /* Root of the tree, this is
                       pre-allocated and the first
                       data node is the left child.*/

  ulint n_nodes; /* Total number of data nodes */

  ib_rbt_compare compare;              /* Fn. to use for comparison */
  ib_rbt_arg_compare compare_with_arg; /* Fn. to use for comparison
                                       with argument */
  ulint sizeof_value;                  /* Sizeof the item in bytes */
  void *cmp_arg;                       /* Compare func argument */
};

/** The result of searching for a key in the tree, this is useful for
a speedy lookup and insert if key doesn't exist.*/
struct ib_rbt_bound_t {
  const ib_rbt_node_t *last; /* Last node visited */

  int result; /* Result of comparing with
              the last non-nil node that
              was visited */
};

/* Size in elements (t is an rb tree instance) */
#define rbt_size(t) (t->n_nodes)

/* Check whether the rb tree is empty (t is an rb tree instance) */
#define rbt_empty(t) (rbt_size(t) == 0)

/* Get data value (t is the data type, n is an rb tree node instance) */
#define rbt_value(t, n) ((t *)&n->value[0])

/* Compare a key with the node value (t is tree, k is key, n is node)*/
#define rbt_compare(t, k, n) (t->compare(k, n->value))

/** Free an instance of  a red black tree */
void rbt_free(ib_rbt_t *tree); /*!< in: rb tree to free */
/** Create an instance of a red black tree
 @return rb tree instance */
ib_rbt_t *rbt_create(size_t sizeof_value,     /*!< in: size in bytes */
                     ib_rbt_compare compare); /*!< in: comparator */
/** Create an instance of a red black tree, whose comparison function takes
 an argument
 @return rb tree instance */
ib_rbt_t *rbt_create_arg_cmp(size_t sizeof_value, /*!< in: size in bytes */
                             ib_rbt_arg_compare compare, /*!< in: comparator */
                             void *cmp_arg); /*!< in: compare fn arg */
/** Delete a node from the red black tree, identified by key */
bool rbt_delete(
    /* in: true on success */
    ib_rbt_t *tree,   /* in: rb tree */
    const void *key); /* in: key to delete */
/** Remove a node from the red black tree, NOTE: This function will not delete
 the node instance, THAT IS THE CALLERS RESPONSIBILITY.
 @return the deleted node with the const. */
ib_rbt_node_t *rbt_remove_node(
    ib_rbt_t *tree,             /*!< in: rb tree */
    const ib_rbt_node_t *node); /*!< in: node to delete, this
                                is a fudge and declared const
                                because the caller has access
                                only to const nodes.*/
/** Add data to the red black tree, identified by key (no dups yet!)
 @return inserted node */
const ib_rbt_node_t *rbt_insert(ib_rbt_t *tree,     /*!< in: rb tree */
                                const void *key,    /*!< in: key for ordering */
                                const void *value); /*!< in: data that will be
                                                    copied to the node.*/
/** Add a new node to the tree, useful for data that is pre-sorted.
 @return appended node */
const ib_rbt_node_t *rbt_add_node(ib_rbt_t *tree,         /*!< in: rb tree */
                                  ib_rbt_bound_t *parent, /*!< in: parent */
                                  const void *value);     /*!< in: this value is
                                                          copied     to the node */
/** Return the left most data node in the tree
 @return left most node */
const ib_rbt_node_t *rbt_first(const ib_rbt_t *tree); /*!< in: rb tree */
/** Return the right most data node in the tree
 @return right most node */
const ib_rbt_node_t *rbt_last(const ib_rbt_t *tree); /*!< in: rb tree */
/** Return the next node from current.
 @return successor node to current that is passed in. */
const ib_rbt_node_t *rbt_next(const ib_rbt_t *tree, /*!< in: rb tree */
                              const ib_rbt_node_t * /* in: current node */
                                  current);
/** Return the prev node from current.
 @return predecessor node to current that is passed in */
const ib_rbt_node_t *rbt_prev(const ib_rbt_t *tree, /*!< in: rb tree */
                              const ib_rbt_node_t * /* in: current node */
                                  current);
/** Search for the key, a node will be returned in parent.last, whether it
 was found or not. If not found then parent.last will contain the
 parent node for the possibly new key otherwise the matching node.
 @return result of last comparison */
int rbt_search(const ib_rbt_t *tree,   /*!< in: rb tree */
               ib_rbt_bound_t *parent, /*!< in: search bounds */
               const void *key);       /*!< in: key to search */
/** Search for the key, a node will be returned in parent.last, whether it
 was found or not. If not found then parent.last will contain the
 parent node for the possibly new key otherwise the matching node.
 @return result of last comparison */
int rbt_search_cmp(const ib_rbt_t *tree,            /*!< in: rb tree */
                   ib_rbt_bound_t *parent,          /*!< in: search bounds */
                   const void *key,                 /*!< in: key to search */
                   ib_rbt_compare compare,          /*!< in: comparator */
                   ib_rbt_arg_compare arg_compare); /*!< in: fn to compare items
                                                    with argument */
/** Merge the node from dst into src. Return the number of nodes merged.
 @return no. of recs merged */
ulint rbt_merge_uniq(ib_rbt_t *dst,        /*!< in: dst rb tree */
                     const ib_rbt_t *src); /*!< in: src rb tree */
#if defined UNIV_DEBUG || defined IB_RBT_TESTING
/** Verify the integrity of the RB tree. For debugging. 0 failure else height
 of tree (in count of black nodes).
 @return true if OK false if tree invalid. */
bool rbt_validate(const ib_rbt_t *tree); /*!< in: tree to validate */
#endif                                   /* UNIV_DEBUG || IB_RBT_TESTING */

#endif /* INNOBASE_UT0RBT_H */
