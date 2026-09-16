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

#include "read0mvcc_interface.h"
#include "read0types.h"  // ReadView
#include "univ.i"
#include "ut0cpu_cache.h"

/** The MVCC read view manager */
class MVCC : public MVCC_interface {
  /* To protect cache line with vtable pointer from being dirtied via false
  sharing with other fields of this structure, we add padding here. We don't
  use alignas(ut::INNODB_CACHE_LINE_SIZE), as we don't know if the particular
  allocator used by the caller respect such large alignment requirements.
  This is not a premature optimization - false sharing caused 16% TPS drop in
  performance on sysbench OLTP uniform 64 users in READ COMMITTED. */
  char _padding[ut::INNODB_CACHE_LINE_SIZE];

 public:
  /** Constructor.
  @param size           Number of views to pre-allocate */
  explicit MVCC(ulint size);

  /** Destructor.
  Free all the views in the m_free list */
  ~MVCC() override;

  void initialize(trx_id_t max_committed_trx_id, trx_ids_t active_ids) override;
  void view_open(Read_view_interface *&view, trx_t *trx) override;
  void view_close(Read_view_interface *&view, bool own_mutex) override;
  void clone_oldest_view(Read_view_interface *&view) override;
  void view_free(Read_view_interface *&view) override;
  [[nodiscard]] size_t get_open_views_count() const override;
  void undo_purge_is_starting() override;
  void undo_purge_has_shutdown() override;

 private:
  /** A helper for the interface method with the same name, which makes it
  cleaner to assign to the referenced pointer while using the actual
  implementation-specific type.

  If trx is auto-commit non-locking transaction and view is not-null, it
  attempts to reopen it. This fast path succeeds if the view is still the
  freshest possible, in which case taking trx_sys mutex is avoided. Otherwise it
  falls back to the slow path, which requires a trx_sys mutex to (re)initialize
  the view.
  @see view_open(Read_view_interface*&,trx_t*) */
  void view_open(ReadView *&view, trx_t *trx);

  /** A helper for the interface method with the same name, which makes it
  cleaner to assign to the referenced pointer while using the actual
  implementation-specific type.

  In case own_mutex is true, it will move the view to the free list and assign
  nullptr to the argument. Otherwise it only closes the view, without freeing
  it.
  @see view_close(Read_view_interface*&,bool) */
  void view_close(ReadView *&view, bool own_mutex);

  /** A helper for the interface method with the same name, which makes it
  cleaner to assign to the referenced pointer while using the actual
  implementation-specific type.

  Clones the oldest view into the provided view, unless the function
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
                        initialized ReadView object.
  @see clone_oldest_view(Read_view_interface*&) */
  void clone_oldest_view(ReadView *&view);

 public:
  [[nodiscard]] bool is_view_open(
      const Read_view_interface *view) const override {
    return view != nullptr && !((ReadView *)view)->is_closed();
  }

  void set_view_creator_trx_id(Read_view_interface *&view,
                               trx_id_t id) override {
    ut_ad(id > 0);

    ((ReadView *)view)->creator_trx_id(id);
  }

 private:
  /** Asserts the read view list is sorted. */
  void validate() const;

  /** Get a view from the free list, or allocate a new one if it's empty.
  @return a view to use */
  inline ReadView *get_view();

  MVCC(const MVCC &) = delete;
  MVCC &operator=(const MVCC &) = delete;

 private:
  typedef UT_LIST_BASE_NODE_T(ReadView, m_view_list) view_list_t;

  /** Free views ready for reuse. */
  view_list_t m_free;

  /** Active and closed views. */
  view_list_t m_views;
};

#endif /* read0read_h */
