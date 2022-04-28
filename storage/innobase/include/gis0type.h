/*****************************************************************************

Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

/** @file include/gis0type.h
 R-tree header file

 Created 2013/03/27 Jimmy Yang
 ***********************************************************************/

#ifndef gis0type_h
#define gis0type_h

#include "univ.i"

#include "buf0buf.h"
#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "gis0geo.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "que0types.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0new.h"
#include "ut0vec.h"
#include "ut0wqueue.h"

#include <list>
#include <vector>

/* Node Sequence Number. Only updated when page splits */
typedef uint32_t node_seq_t;

/* RTree internal non-leaf Nodes to be searched, from root to leaf */
typedef struct node_visit {
  page_no_t page_no;  /*!< the page number */
  node_seq_t seq_no;  /*!< the SSN (split sequence number */
  ulint level;        /*!< the page's index level */
  page_no_t child_no; /*!< child page num if for parent
                      recording */
  btr_pcur_t *cursor; /*!< cursor structure if we positioned
                      FIXME: there is no need to use whole
                      btr_pcur_t, just the position related
                      members */
  double mbr_inc;     /*!< whether this node needs to be
                      enlarged for insertion */
} node_visit_t;

typedef std::vector<node_visit_t, ut::allocator<node_visit_t>> rtr_node_path_t;

typedef struct rtr_rec {
  rec_t *r_rec; /*!< matched record */
  bool locked;  /*!< whether the record locked */
} rtr_rec_t;

typedef std::vector<rtr_rec_t, ut::allocator<rtr_rec_t>> rtr_rec_vector;

/* Structure for matched records on the leaf page */
typedef struct matched_rec {
  byte *bufp; /*!< aligned buffer point */
  byte rec_buf[UNIV_PAGE_SIZE_MAX * 2];
  /*!< buffer used to copy matching rec */
  buf_block_t block;            /*!< the shadow buffer block */
  ulint used;                   /*!< memory used */
  rtr_rec_vector *matched_recs; /*!< vector holding the matching rec */
  ib_mutex_t rtr_match_mutex;   /*!< mutex protect the match_recs
                                vector */
  bool valid;                   /*!< whether result in matched_recs
                                or this search is valid (page not
                                dropped) */
  bool locked;                  /*!< whether these recs locked */
} matched_rec_t;

/* Maximum index level for R-Tree, this is consistent with BTR_MAX_LEVELS */
constexpr uint32_t RTR_MAX_LEVELS = 100;

/* Number of pages we latch at leaf level when there is possible Tree
modification (split, shrink), we always latch left, current
and right pages */
constexpr uint32_t RTR_LEAF_LATCH_NUM = 3;

/** Vectors holding the matching internal pages/nodes and leaf records */
typedef struct rtr_info {
  rtr_node_path_t *path; /*!< vector holding matching pages */
  rtr_node_path_t *parent_path;
  /*!< vector holding parent pages during
  search */
  matched_rec_t *matches; /*!< struct holding matching leaf records */
  ib_mutex_t rtr_path_mutex;
  /*!< mutex protect the "path" vector */
  buf_block_t *tree_blocks[RTR_MAX_LEVELS + RTR_LEAF_LATCH_NUM];
  /*!< tracking pages that would be locked
  at leaf level, for future free */
  ulint tree_savepoints[RTR_MAX_LEVELS + RTR_LEAF_LATCH_NUM];
  /*!< savepoint used to release latches/blocks
  on each level and leaf level */
  rtr_mbr_t mbr;       /*!< the search MBR */
  que_thr_t *thr;      /*!< the search thread */
  mem_heap_t *heap;    /*!< memory heap */
  btr_cur_t *cursor;   /*!< cursor used for search */
  dict_index_t *index; /*!< index it is searching */
  bool need_prdt_lock;
  /*!< whether we will need predicate lock
  the tree */
  bool need_page_lock;
  /*!< whether we will need predicate page lock
  the tree */
  bool allocated; /*!< whether this structure is allocate or
                on stack */
  bool mbr_adj;   /*!< whether mbr will need to be enlarged
                  for an insertion operation */
  bool fd_del;    /*!< found deleted row */
  const dtuple_t *search_tuple;
  /*!< search tuple being used */
  page_cur_mode_t search_mode;
  /*!< current search mode */

  /* TODO: This is for a temporary fix, will be removed later */
  bool *is_dup;
  /*!< whether the current rec is a duplicate record. */
} rtr_info_t;

typedef std::list<rtr_info_t *, ut::allocator<rtr_info_t *>> rtr_info_active;

/* Tracking structure for all onoging search for an index */
typedef struct rtr_info_track {
  rtr_info_active *rtr_active; /*!< Active search info */
  ib_mutex_t rtr_active_mutex;
  /*!< mutex to protect
  rtr_active */
} rtr_info_track_t;

/* Node Sequence Number and mutex protects it. */
typedef struct rtree_ssn {
  ib_mutex_t mutex;  /*!< mutex protect the seq num */
  node_seq_t seq_no; /*!< the SSN (node sequence number) */
} rtr_ssn_t;

/* This is to record the record movement between pages. Used for corresponding
lock movement */
typedef struct rtr_rec_move {
  rec_t *old_rec; /*!< record being moved in old page */
  rec_t *new_rec; /*!< new record location */
  bool moved;     /*!< whether lock are moved too */
} rtr_rec_move_t;
#endif /*!< gis0rtree.h */
