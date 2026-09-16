/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
#pragma once

#include "trx0types.h"
class Read_view_interface;

/** The MVCC read view manager.
A read view object can be in "open" or "closed" state.
The "open" state is meant to mean that a transaction is using it at the moment.
The "closed" state is meant to mean that it is not currently used by any
transaction, but wasn't freed either, for performance reasons - it can be
cheaply reopened by using view_open(view,..) on it again.
A read view obtained by view_open(view,..) will be "open".
It can then be closed by view_close(view,..) in which case it might also get
freed.
If it wasn't freed, then view will still be not nullptr, but is_view_open(view)
will return false.
You should free such "closed" views with view_free(view).
All "open" read-views are tracked by MVCC_interface instance, and the oldest of
them can be cloned using clone_oldest_view(), but the clone itself will be
"closed". A cloned view is special in that it can't be opened with view_open(..)
and should not be passed to is_view_open(..) - it's implicitly guaranteed to be
always closed, so no need to check.
The ownership of the view is passed to the caller of view_open(view,..) or
clone_oldest_view(view,..), and the view remains in the caller ownership as long
as the pointer is not-null. The view_close(view,...) should be called for views
obtained with view_open(..) and it may set view to nullptr if it has chosen to
free the object. Closed read views, such that those resulting from calling
view_close(view,..) or clone_oldest_view(view,..), should be freed with
view_free(view,..) which always sets view to nullptr.

So, a typical usage would be something like:

    trx->read_view = nullptr;
    trx_sys->mvcc->view_open(trx->read_view, trx);
    ut_a(trx->read_view != nullptr);
    ut_a(trx_sys->mvcc->is_view_open(trx->read_view));

    //...use the view...

    trx_sys->mvcc->view_close(trx->read_view, ...);
    // trx->read_view will now be closed, perhaps even nullptr
    ut_a(!trx_sys->mvcc->is_view_open(trx->read_view));

    // When you are sure trx will never reopen trx->read_view
    // say, because you're going to free the trx object itself,
    // then make sure to free the read view, after closing it.
    // (You don't have to do that if trx->read_view is already
    // nullptr, but it wouldn't hurt).
    trx_sys->view_free(trx->read_view);
    ut_a(trx->read_view == nullptr);

And usage in purge is for example:

    purge_sys->view = nullptr;
    trx_sys->mvcc->clone_oldest_view(purge_sys->view);
    ut_a(purge_sys->view != nullptr);

    //...use the view...

    trx_sys->mvcc->view_free(purge_sys->view);
    ut_a(purge_sys->view == nullptr);
*/
class MVCC_interface {
 public:
  /** Initializes the MVCC once the max_committed_trx_id estimate is learned
  from reading system tablespace's header TRX_SYS_TRX_ID_STORE field.
  @param[in]     max_assigned_trx_id
                     The upper-bound on highest assigned trx id. No record in
                     any B-tree can currently have DB_TRX_ID larger than this.
                     No Undo Log can have TRX_UNDO_TRX_NO or TRX_UNDO_TRX_ID
                     larger than this.
  @param[in]     active_ids
                     The set of ids of currently active transactions.
                     They should all be at <= max_committed_trx_id.
  */
  virtual void initialize(trx_id_t max_assigned_trx_id,
                          trx_ids_t active_ids) = 0;

  /** Destructor.*/
  virtual ~MVCC_interface() = default;

  /** If view is nullptr then allocates a view, otherwise, reuses the one
  provided, in both cases opening it. That is, after the call view is not null,
  view members can be accessed, and view->is_closed() == false.
  It is guaranteed that clone_oldest_view(v2) calls which happen-before the call
  to view_close(view,...) for this view, will clone a v2 which is not fresher
  than this view.
  @param[in,out] view
                     Must be either nullptr or a result of view_close(..), which
                     in turn should be a result of an earlier view_open(..). It
                     must not be a result of clone_oldest_view(..).
                     Upon return it will be not null and open.
                     Must be closed by calling view_close(..). If it is still
                     not nullptr after view_close(..) it must be passed to
                     view_free(..) to free it.
  @param[in]     trx
                     Transaction instance of caller */
  virtual void view_open(Read_view_interface *&view, trx_t *trx) = 0;

  /** Closes a view previously opened by view_open(..). It's safe to call it on
  a view which was already closed. After the call, it is unsafe to access
  view's members, because it might be freed by this function (in which case
  view is nullptr). Even if the view is still not null after the call, it is at
  least in the closed state - so can be reopen again with view_open(..) or freed
  with view_free(..).
  @param[in,out] view
                     Must not be nullptr. Must be a pointer obtained from
                     view_open(..) or view_close(..). Upon return it will be
                     either nullptr or pointing to a closed view.
  @param[in]     own_mutex
                     true if the caller owns the trx_sys->mutex */
  virtual void view_close(Read_view_interface *&view, bool own_mutex) = 0;

  /** Makes sure the view is properly freed: if view is already nullptr does
  nothing, otherwise it must be already closed, and this function will take
  trx_sys->mutex, free the view, and set it to nullptr.
  @param[in,out] view
                     nullptr or a closed view. Upon return it will be nullptr.
  */
  virtual void view_free(Read_view_interface *&view) = 0;

  /** Clones the oldest open view or if there is no open view at the moment,
  clones one which would be created if view_open(..) was called.
  The view provided by a caller must be either nullptr or a closed view obtained
  from an earlier call to clone_oldest_view(..).
  This function will either allocate a new view or clone into view provided.
  After the call view will be considered closed.
  It must be freed by the caller using view_free(view) when no longer needed.
  @param[in,out] view
                     A pointer to a closed view obtained from an earlier call to
                     this method or a nullptr. It must not be a result of
                     view_close(..) or view_open(..). Upon return it will
                     point to a closed view which is a clone of the oldest open
                     read view found during the call. It should be freed with
                     view_free(..). Do not call view_open(..) on it. */
  virtual void clone_oldest_view(Read_view_interface *&view) = 0;

  /** Instructs the MVCC that the Undo Purge is about to start working and MVCC
  will be asked to clone the oldest Read View. */
  virtual void undo_purge_is_starting() = 0;

  /** Instructs the MVCC that the Undo Purge was shutdown and will not be asking
  to clone the oldest Read View or any other MVCC anymore. To be called before
  trx_sys is being destroyed. */
  virtual void undo_purge_has_shutdown() = 0;

  /** Returns the number of open views. */
  [[nodiscard]] virtual size_t get_open_views_count() const = 0;

  /** Can be used only for nullptr or view assigned by view_open(view,..) or
  view_close(view,..) or clone_oldest_view(view,..).
  (note, though that clone_oldest_view always gives you a closed view,
  so no need to check).
  @param[in]     view
                     The view to check. Can be nullptr.
  @return true if the view is not nullptr and is open */
  [[nodiscard]] virtual bool is_view_open(
      const Read_view_interface *view) const = 0;

  /** Sets the view creator transaction id, when a transaction which previously
  called view_open(view,..) when it was read-only (and thus had trx->id==0), has
  transitioned to a RW state and got a trx->id assigned.
  One can imagine an implementation which could change the view instance to
  another (say: from one shared with other read-only transactions to one
  tailored to this particular RW transaction, which should see its own writes).
  @param[in,out] view
                     Set the creator trx id for this view.
                     The view should be already open first by
                     view_open(view, trx) when trx->id was 0.
  @param[in]     id
                     Transaction id to set, should be the trx->id assigned to
                     the same trx for which view_open(view, trx) was called
                     earlier.
  */
  virtual void set_view_creator_trx_id(Read_view_interface *&view,
                                       trx_id_t id) = 0;
};
