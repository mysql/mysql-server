/*****************************************************************************

Copyright (c) 1997, 2026, Oracle and/or its affiliates.

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

/** @file include/read0read.h
 Cursor read

 Created 2/16/1997 Heikki Tuuri
 *******************************************************/

#ifndef read0read_h
#define read0read_h

#include <stddef.h>
#include <algorithm>

#include "read0types.h"
#include "univ.i"

/** The MVCC read view manager */
class MVCC {
 public:
  /** Constructor
  @param size           Number of views to pre-allocate */
  explicit MVCC(ulint size);

  /** Destructor.
  Free all the views in the m_free list */
  ~MVCC();

  /** Allocate and create a view.
  @param view   View owned by this class created for the caller. Must be
  freed by calling view_close()
  @param trx    Transaction instance of caller */
  void view_open(ReadView *&view, trx_t *trx);

  /**
  Close a view created by the above function.
  @param view           view allocated by trx_open.
  @param own_mutex      true if caller owns trx_sys_t::mutex */
  void view_close(ReadView *&view, bool own_mutex);

  /** Clones the oldest view into the provided view, unless the function
  determines that the provided view is already a good enough lower bound.
  The caller owns the view that is passed in, which is interpreted to be a
  previous lower bound known to the caller.
  No need to call view_close(view,..).

  Note: This function is called by Purge to determine the purge_sys->view used
  to distinguish which transactions are considered committed by everybody, and
  thus their undo logs can be purged.
  Purge mainly uses purge_sys->view->low_limit_no(), which is a safe
  lower-bound on what can be purged based on NO, and further limits it to the
  lowest needed NO reported by GTID Persistor. But other places like ROLLBACK
  use purge_sys->view->changes_visible(ID,..).
  @param[in,out] view   Preallocated view, owned by the caller. Can be either
                        default constructed (m_low_limit_no is 0) or a fully
                        initialized ReadView object. */
  void clone_oldest_view(ReadView *view);

  /**
  @return the number of active views */
  ulint size() const;

  /**
  @return true if the view is active and valid */
  static bool is_view_active(ReadView *view) {
    ut_a(view != reinterpret_cast<ReadView *>(0x1));

    return (view != nullptr && !(intptr_t(view) & 0x1));
  }

  /**
  Set the view creator transaction id. Note: This should be set only
  for views created by RW transactions.
  @param view   Set the creator trx id for this view
  @param id     Transaction id to set */
  static void set_view_creator_trx_id(ReadView *view, trx_id_t id) {
    ut_ad(id > 0);

    view->creator_trx_id(id);
  }

 private:
  /**
  Validates a read view list. */
  bool validate() const;

  /**
  Find a free view from the active list, if none found then allocate
  a new view. This function will also attempt to move delete marked
  views from the active list to the freed list.
  @return a view to use */
  inline ReadView *get_view();

 private:
  // Prevent copying
  MVCC(const MVCC &);
  MVCC &operator=(const MVCC &);

 private:
  typedef UT_LIST_BASE_NODE_T(ReadView, m_view_list) view_list_t;

  /** Free views ready for reuse. */
  view_list_t m_free;

  /** Active and closed views, the closed views will have the
  creator trx id set to TRX_ID_MAX */
  view_list_t m_views;
};

#endif /* read0read_h */
