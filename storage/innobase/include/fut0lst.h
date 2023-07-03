/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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

/** @file include/fut0lst.h
 File-based list utilities

 Created 11/28/1995 Heikki Tuuri
 ***********************************************************************/

#ifndef fut0lst_h
#define fut0lst_h

#include "univ.i"

#include "fil0fil.h"
#include "mtr0mtr.h"

/* The C 'types' of base node and list node: these should be used to
write self-documenting code. Of course, the sizeof macro cannot be
applied to these types! */

typedef byte flst_base_node_t;
typedef byte flst_node_t;

/* The physical size of a list base node in bytes */
constexpr ulint FLST_BASE_NODE_SIZE = 4 + 2 * FIL_ADDR_SIZE;

/* The physical size of a list node in bytes */
constexpr ulint FLST_NODE_SIZE = 2 * FIL_ADDR_SIZE;

/** Initializes a list base node.
@param[in]      base    Pointer to base node
@param[in]      mtr     Mini-transaction handle */
static inline void flst_init(flst_base_node_t *base, mtr_t *mtr);

/** Adds a node as the last node in a list.
@param[in] base Pointer to base node of list
@param[in] node Node to add
@param[in] mtr Mini-transaction handle */
void flst_add_last(flst_base_node_t *base, flst_node_t *node, mtr_t *mtr);

/** Adds a node as the first node in a list.
@param[in] base Pointer to base node of list
@param[in] node Node to add
@param[in] mtr Mini-transaction handle */
void flst_add_first(flst_base_node_t *base, flst_node_t *node, mtr_t *mtr);

/** Removes a node.
@param[in] base Pointer to base node of list
@param[in] node2 Node to remove
@param[in] mtr Mini-transaction handle */
void flst_remove(flst_base_node_t *base, flst_node_t *node2, mtr_t *mtr);

/** Get the length of a list.
@param[in]      base    base node
@return length */
static inline ulint flst_get_len(const flst_base_node_t *base);

/** Gets list first node address.
@param[in]      base    Pointer to base node
@param[in]      mtr     Mini-transaction handle
@return file address */
static inline fil_addr_t flst_get_first(const flst_base_node_t *base,
                                        mtr_t *mtr);

/** Gets list last node address.
@param[in]      base    Pointer to base node
@param[in]      mtr     Mini-transaction handle
@return file address */
static inline fil_addr_t flst_get_last(const flst_base_node_t *base,
                                       mtr_t *mtr);

/** Gets list next node address.
@param[in]      node    Pointer to node
@param[in]      mtr     Mini-transaction handle
@return file address */
static inline fil_addr_t flst_get_next_addr(const flst_node_t *node,
                                            mtr_t *mtr);

/** Gets list prev node address.
@param[in]      node    Pointer to node
@param[in]      mtr     Mini-transaction handle
@return file address */
static inline fil_addr_t flst_get_prev_addr(const flst_node_t *node,
                                            mtr_t *mtr);

/** Writes a file address.
@param[in]      faddr   Pointer to file faddress
@param[in]      addr    File address
@param[in]      mtr     Mini-transaction handle */
static inline void flst_write_addr(fil_faddr_t *faddr, fil_addr_t addr,
                                   mtr_t *mtr);

/** Reads a file address.
@param[in]      faddr   Pointer to file faddress
@param[in]      mtr     Mini-transaction handle
@return file address */
static inline fil_addr_t flst_read_addr(const fil_faddr_t *faddr, mtr_t *mtr);

/** Validates a file-based list.
 @param[in] base pointer to base node of list
 @param[in] mtr1 mtr */
void flst_validate(const flst_base_node_t *base, mtr_t *mtr1);

/** Inserts a node after another in a list.
@param[in]      base    Pointer to base node of list
@param[in]      node1   Node to insert after
@param[in]      node2   Node to add
@param[in]      mtr     Mini-transaction handle. */
void flst_insert_after(flst_base_node_t *base, flst_node_t *node1,
                       flst_node_t *node2, mtr_t *mtr);

/** Inserts a node before another in a list.
@param[in]      base    Pointer to base node of list
@param[in]      node2   Node to insert
@param[in]      node3   Node to insert before
@param[in]      mtr     Mini-transaction handle. */
void flst_insert_before(flst_base_node_t *base, flst_node_t *node2,
                        flst_node_t *node3, mtr_t *mtr);

#include "fut0lst.ic"

/** In-memory representation of flst_base_node_t */
struct flst_bnode_t {
  ulint len;
  fil_addr_t first;
  fil_addr_t last;

  flst_bnode_t() : len(0) {}

  flst_bnode_t(const flst_base_node_t *base, mtr_t *mtr)
      : len(flst_get_len(base)),
        first(flst_get_first(base, mtr)),
        last(flst_get_last(base, mtr)) {}

  void set(const flst_base_node_t *base, mtr_t *mtr) {
    len = flst_get_len(base);
    first = flst_get_first(base, mtr);
    last = flst_get_last(base, mtr);
  }

  void reset() {
    len = 0;
    first = fil_addr_null;
    last = fil_addr_null;
  }

  std::ostream &print(std::ostream &out) const {
    out << "[flst_base_node_t: len=" << len << ", first=" << first
        << ", last=" << last << "]";
    return (out);
  }
};

inline std::ostream &operator<<(std::ostream &out, const flst_bnode_t &obj) {
  return (obj.print(out));
}

#endif /* fut0lst_h */
