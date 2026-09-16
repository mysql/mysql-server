/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
*/

#pragma once

#include "fil0tablespaces_nodes_interface.h" /* Tablespaces_nodes_interface::Tablespace_id */
#include "log0common.h"                      /* ib::redo::Lsn */
#include "ut0expected.h"                     /* ut::Expected */
#include "ut0function_reference.h" /* ut::Function_reference */
#include "ut0new.h"                /* ut::unique_pt */

class Flush_observer;
struct buf_block_t;

namespace ib {
class Monitoring_interface;
class Sys_var_handler_interface;
}  // namespace ib

namespace ib::fil {

/** Interface for handling operations related to page persistence */
class Pages_persistence_interface {
 public:
  /** Type used for numbering the pages in the tablespace. */
  using Page_number = Tablespace_node_handle_interface::Page_number;

  /** Type used for giving the tablespaces unique number. */
  using Tablespace_id = Tablespaces_nodes_interface::Tablespace_id;
  using Lsn = ib::redo::Lsn;

  enum class Status { SUCCESS = 0, IO_ERROR };

  /** @name Lifecycle methods

  In InnoDB Persistence of Pages involves several modules and operations which
  are started gradually during server boot, which is reflected in this API.

  The page cleaner threads - the buf_flush_page_coordinator_thread() and several
  instances of buf_flush_page_cleaner_thread() spawned by it - are responsible
  for writing dirty pages from BP to tablespaces. They do this already during
  recovery, as recovery is applying changes from redo log to pages in BP. They
  continue to do so during runtime, reacting to changes made by mtrs. The
  "speed" at which they do so, and the source of pages to flush (flush lists or
  LRU) may change over time but their main goal is to make sure changes which we
  redo log are also persisted to tablespaces storage.
  They get started in init().
  They get stopped implicitly by changing the srv_shutdown_state to
  SRV_SHUTDOWN_FLUSH_PHASE in srv_shutdown_page_cleaners() and is considered
  done once there are no active page cleaners.

  @note It is page cleaners who do the heavy work of actually writing pages, and
  this way making sure the state of persisted pages marches forward.
  However, it is not their job to "make a checkpoint" (store the minimum needed
  lsn value in durable place) nor to "bump the Checkpoint LSN" (compute and
  announce to other modules of the code the minimum needed lsn value). They
  merely afford doing so, by ensuring that all changes up to a given LSN are
  already persisted to tablespaces, so that a higher value of LSN is now
  "available for checkpoint", and thus Checkpoint LSN can be bumped and its
  value can be stored in a place which would survive a crash - but this is a job
  of log checkpointer thread.

  The log checkpointer - the Log_checkpointing::do_work thread is monitoring the
  lsn available for checkpoint reported by page cleaners and if it decides it is
  a good moment to persist that value as Checkpoint LSN, then it does so, and
  additionally calls
  ib::redo::handler->do_not_need_smaller_than(new_checkpoint_lsn), to inform
  Redo log handler that descriptions of changes below this lsn are no longer
  needed. We call this operation "checkpointing", or "making a checkpoint". How
  often to make it is determined by configuration and stage of bootstrap and
  explicit requests to do so from code. It doesn't do so during recovery, to not
  truncate a redo log prefix containing Persistent Table Metadata which it might
  need to reread in case we crash again during recovery. It's started after
  recover_tables() has stored PTM info to B-tree, using enable_checkpointing().
  From this moment the Checkpoint LSN value can be bumped and stored.
  It's further encouraged by enable_periodical_checkpoints() to occur on timely
  fashion even if nobody requested it.
  The disable_checkpointing() stops the checkpointing (and log checkpointer).

  In order to properly track the lsn available for checkpoint, InnoDB uses a
  Link_buf data structure to know what ranges of changes got already reported
  by mtr_has_dirtied_pages(). However this data structure needs to be:
  - constructed in init(),
  - informed from what value to start in assume_checkpoint_lsn(lsn) or decided
    automatically at the end of recover_pages(..),
  - and destructed in deinit().

  The class can also have a constructor and destructor, however be aware that
  the destructor might get called during abort() and thus it might be difficult
  to execute any code which depends on already destructed infrastructure like
  performance schema (used by mutexes, events etc.). */
  /** @{ */

  virtual ~Pages_persistence_interface() = default;

  /** Redo a create tablespace storage operation for a tablespace.
  This operation is done when REDOs are replayed and must be successful.
  @param[in]  space_id  Tablespace id
  @param[in]  flags     Tablespace flags
  @param[in]  path      path of tablespace storage */
  virtual void redo_create_tablespace(
      Tablespaces_nodes_interface::Tablespace_id space_id, uint32_t flags,
      const char *path) = 0;

  /** Redo a delete tablespace storage operation for a tablespace.
  This operation is done when REDOs are replayed and must be successful.
  @param[in]  space_id  Tablespace id
  @param[in]  path      path of tablespace storage */
  virtual void redo_delete_tablespace(Tablespace_id space_id,
                                      const char *path) = 0;

  /** Redo a rename operation for a tablespace.
  This operation is done when REDOs are replayed and must be successful.
  @param[in]  space_id   Tablespace id
  @param[in]  old_path   current path tablespace storage
  @param[in]  new_path   target path of tablespace storage */
  virtual void redo_rename_tablespace(Tablespace_id space_id,
                                      const char *old_path,
                                      const char *new_path) = 0;

  /** Allocate and construct helper data structures and start persisting changes
  to pages, but do not start persisting the progress of that operation: the
  values reported by get_checkpoint_lsn() should not change, and
  ib::redo::handler->do_not_need_smaller_than(lsn) should not be called.
  That is: page cleaners can start now, but log check pointer can not.
  @return IO_ERROR due to one of plenty possible IO errors, SUCCESS otherwise */
  [[nodiscard]] virtual Status init() = 0;

  /** The caller asks the implementation to assume that redo log below
  min_needed_lsn is not available and is not needed anymore because pages have
  all changes below this lsn already persisted. Also, the next change to come
  will be min_needed_lsn. In other words the redo log is logically empty:
  all changes were already persisted to pages.
  That is, the checkpoint lsn is at min_needed_lsn and no dirty page will be
  added with a lower lsn in future.
  This is currently used only when initializing a new instance.
  @param[in]     min_needed_lsn
                     the current snapshot of the database (its tablespaces),
                     the start and the end of the redo log,
                     the only available version of it
  @retval SUCCESS
              everything went fine: the min_needed_lsn value was stored in
              Redo log handler's metadata, and get_checkpoint_lsn() would
              return min_needed_lsn if called
  @retval IO_ERROR
              could not persist the value of min needed lsn (make a checkpoint)
  */
  [[nodiscard]] virtual Status assume_checkpoint_lsn(Lsn min_needed_lsn) = 0;

  /** From now on, the implementation is permitted to bump the min needed lsn
  value reported and persisted - that is, not only can it persist changes to
  pages but moreover it can persist the state of progress of this operation.
  That is, the log checkpointer can be started now. */
  virtual void enable_checkpointing() = 0;

  /** From now on, not only advancing the min needed lsn is permitted, but also
  encouraged. Before a call to this function it should only be done if really
  needed for correctness, but avoided otherwise to make the behaviour more
  deterministic. From now on, unsolicited, periodical advancements are no
  longer a problem and can actually help in reclaiming redo log space.
  That is, the log checkpointer can start periodical checkpoints.*/
  virtual void enable_periodical_checkpoints() = 0;

  /** From now on, the implementation is no longer permitted to bump the min
  needed lsn.
  That is, the log checkpointer thread should be stopped now. */
  virtual void disable_checkpointing() = 0;

  /** Deinitialize whatever was initialized in init(). This is called when
  shutting down in a planned way (as opposed to the destructor which might be
  called indirectly from mysql_exit on abort, in which case the order of
  destruction can be problematic - for example PFS or some other piece of
  infrastructure might be no longer available) */
  virtual void deinit() = 0;

  /** @} */

  /** @name Handling pages dirtied by mtrs */
  /** @{ */

  /** The caller informs that it is now committing an mtr which has dirtied
  some pages. The pages are still latched in BP, but it will not dirty any more
  pages, and the range of lsns is already assigned to it (unless the redo
  logging is not enabled for this mtr, i.e. MTR_LOG_SHORT_INSERTS,
  MTR_LOG_NO_REDO or MTR_LOG_NONE was used). This is a good moment to add
  dirtied pages to flush lists.
  @param[in]     start_lsn
                     the start lsn of the committing mtr. It is 0 iff this mtr
                     is not redo logged.
  @param[in]     end_lsn
                     the end lsn of the committing mtr. It is 0 iff this mtr
                     is not redo logged.
  @param[in]     observer
                     the Flush observer (if any) attached to this mtr
  @param[in]     iterate_over_dirty_pages
                     a function that can be used to iterate over all pages
                     dirtied committing mtr by passing a visitor callback to it.
                     The visitor will be called AT LEAST once for each dirtied
                     page.
  */
  virtual void mtr_has_dirtied_pages(
      Lsn start_lsn, Lsn end_lsn, ::Flush_observer *observer,
      ut::Function_reference<void(ut::Function_reference<void(buf_block_t *)>)>
          iterate_over_dirty_pages) = 0;

  /** A callback that is called from buf_flush_note_oldest_modification() once
  for each page that became dirty, assuming buf_flush_note_oldest_modification()
  is called at all from mtr_has_dirtied_pages(), for example by calling
  buf_flush_note_modification() from visitor passed to iterate_over_dirty_pages.
  TODO: is it really needed if we have mtr_has_dirtied_pages() ?
  @param[in]      buf_block        Block for Page that became dirty.
  */
  virtual void page_became_dirty(buf_block_t *buf_block) = 0;

  /** Makes sure that for a specified tablespace all changes to pages are
  persisted. This requires all pages from the Buffer Pool (for the
  specified tablespace) that were dirty before this call, to be made clean. This
  will remove such pages from the flush_list, but not from the Buffer Pool
  itself (and thus will not remove from LRU list). In case the specified
  transaction is interrupted before or during the call, whatever pages were not
  yet processed, will not be touched and thus will remain in both lists.
  @param[in]     space_id
                     The id of the tablespace to persist its pages.
  @param[in]     trx
                     Transaction, if any, to monitor for interrupted operation.
  */
  virtual void persist_tablespace(Tablespace_id space_id, const trx_t *trx) = 0;

  /** Makes sure that all changes to pages that are observed by a specified
  flush observer are persisted. This requires all such pages from the Buffer
  Pool that were dirty before this call, to be made clean. This will remove such
  pages from the flush_list, but not from the Buffer Pool itself (and thus will
  not remove from LRU list). In case the transaction that is linked to the
  specified FlushObserver instance is interrupted before or during the call, all
  pages that were not flushed before noticing the interruption, will be removed
  from the flush_list, causing them to be considered clean. They will not be
  ever written back even when evicted from the LRU. This means that the changes
  observed by the FlushObserver must not be redologged, and any future
  modifications of this page must start from initializing it before use.
  @param[in]     observer
                     The Flush observer used to monitor the flushing process.
  */
  virtual void persist_tablespaces(::Flush_observer *observer) = 0;

  /** @} */

  /** @name Handling recovery of changes to pages after a crash */
  /** @{ */

  /** Run recovery of all tablespaces assuming they already contain all the
  changes at least up to the provided clean_shutdown_lsn.
  Note that this is *not* the value of min needed lsn which the persistence
  implementation keeps track of more precisely by itself, nor even the (perhaps
  lower) value stored when making a checkpoint via store_metadata(0,..) - it is
  just the last value InnoDB was aware of at last clean shutdown obtained via
  get_checkpoint_lsn() and stored in system tablespace's header.
  The init() method must be called before attempting recover_pages(..).
  If CLONE is supported, then it should call:
  - arch_init(),
  - Arch_page_sys::post_recovery_init()
  as well.
  @param[in,out]     clean_shutdown_lsn
                         The lsn at the last clean shutdown the InnoDB knows
                         about. This method will set it to the recovered lsn.
  @retval List of tablespaces IDs found for all discovered tablespaces
  @retval IO_ERROR if could not perform recovery due to one of plenty possible
          IO errors */
  [[nodiscard]] virtual ut::Expected<
      std::vector<Tablespaces_nodes_interface::Tablespace_id>, Status>
  recover_pages(Lsn &clean_shutdown_lsn) = 0;

  /** Run recovery of tables (in particular: mysql.innodb_dynamic_metadata)
  based on information gathered during recover_pages().
  Used to update PTM's B-tree using the information from redo log.
  TODO: recover_tables() does not really belong to pages persistence, as tables
  are a high-level concept, and recovering them is basically running SQL on top
  of already recovered B-tree. However, this is how InnoDB currently handles PTM
  - the redo log contains logical records which are interpreted as requests to
  REPLACE INTO mysql.innodb_dynamic_metadata. Therefore recovery is two-stage:
  first we recover_pages() to get physically consistent B-tree, then we apply
  collected PTM changes extracted from those records. Once we implement WL#15550
  we will no longer produce this kind of redo log records. If we additionally
  assume that upgrade will require a clean shutdown then the redo log will no
  longer contain such PTM redo log records, and we will no longer have to handle
  them and will be able to remove this method. */
  [[nodiscard]] virtual Status recover_tables() = 0;

  /** @} */

  /** @name Handling evictions of pages from buffer pool */
  /** @{ */

  /** A callback that is called once the page is freed from the BufferPool. The
  page is already removed from the Page Hash table, but the mutex that guards
  the page object and latch protecting the Page Hash table, are still held, so
  this page will not be read in again while this method is executing.

  @param[in]     space_id
                     The id of the tablespace containing the evicted page
  @param[in]     page_no
                     Zero-based number of the evicted page within its tablespace
  @param[in]     modification_lsn
                     The highest modification LSN of the page that is being
                     evicted - that is there are no modifications to this page
                     in the range [modification_lsn, peek_first_unassigned_lsn),
                     as otherwise the page would be dirty or latched and could
                     not be evicted. In other words, if you want to bring this
                     page back to BP in the same state, you need to ensure it
                     is recovered at least up to modification_lsn. */
  virtual void page_is_to_be_evicted(Tablespace_id space_id,
                                     Page_number page_no,
                                     Lsn modification_lsn) = 0;
  /** @} */

  /** @name Handling checkpoints
  What is the Checkpoint LSN? The oldest lsn position in the redo log that this
  implementation still needs. One reason it might need the redo log is to ensure
  that database state is durable - that is, after a crash, it could reconstruct
  the pages in the latest version even if redo log below that lsn is lost.
  We define Checkpoint LSN to be whatever get_checkpoint_lsn() returns, which in
  general can be a value smaller than the real answer to the abstract question
  "what is the smallest needed lsn?" - for example, the page cleaners could have
  already persisted newer changes to tablespaces, but did not yet announce that
  higher lsn is available for checkpoint, or log checkpointer has not yet
  noticed that, or haven't yet got chance to persist this higher value.
  Note that erring on this side is safe, as having too much redo log is not a
  correctness problem.
  */
  /** @{ */

  /** Returns the checkpoint lsn.
  This function should be monotone in time.
  The returned value itself should be crash-resistant, that is, after the crash
  the implementation should return get_checkpoint_lsn() value at least as
  large as returned now.
  @return the oldest lsn in the redo log still needed by this implementation of
  pages persistence
  */
  [[nodiscard]] virtual Lsn get_checkpoint_lsn() const = 0;

  /** The caller request the implementation to ensure that when it returns, it
  no longer needs the redo log generated so far, i.e. the value returned by
  subsequent calls to get_checkpoint_lsn() must be at least the value of
  ib::redo::handler->peek_first_unassigned_lsn() would have returned before the
  call to request_sharp_checkpoint(). This method is used from:
  - various debug hooks
  - places which want to ensure redo log is logically empty because:
     - it's MEB or other "archival" situation where we need a clear start of log
     - it is a slow or normal shutdown
  - after we did some non-redo-logged changes to tablespaces
  - we've imported a new tablespace
  - we've initialized legacy double-write pages and want them written to disc,
    and would not like to have to "recover" changes to them
  - we've initialized new undo log rsegs - TODO: I don't understand this part
  */
  virtual void request_sharp_checkpoint() = 0;
  /** @} */

  /** @name Handling of sys vars */
  /** @{ */

  /** Returns an object responsible for handling updates of dynamic sys-vars
  related to pages persistence, such as:
    - innodb_log_checkpoint_fuzzy_now
    - innodb_checkpoint_disabled
  */
  [[nodiscard]] virtual ib::Sys_var_handler_interface &config_handler() = 0;

  /** @} */

  /** @name Handling of INFORMATION_SCHEMA.INNODB_METRICS  */
  /** @{ */

  /** Returns an object responsible for reporting values of monitors related to
  pages persistence, such as:
    - MONITOR_OVLD_LSN_BUF_DIRTY_PAGES_ADDED
    - MONITOR_OVLD_BUF_OLDEST_LSN_APPROX
    - MONITOR_OVLD_BUF_OLDEST_LSN_LWM
    - MONITOR_OVLD_MAX_AGE_ASYNC
    - MONITOR_OVLD_MAX_AGE_SYNC
  @return The instance to use to obtain values of relevant monitors */
  [[nodiscard]] virtual ib::Monitoring_interface &get_monitoring() = 0;

  /** @} */
};

/** Sets the implementation of the Pages_persistence_interface. It can be called
only once, that is, once set, it is impossible to set other implementation.
@param[in] new_persistence New implementation to use. */
void set_pages_persistence(
    ut::unique_ptr<Pages_persistence_interface> new_persistence);

} /* namespace ib::fil */

/** The implementation of persistence for the pages stored in tablespaces'
nodes' storage. */
extern ut::unique_ptr<ib::fil::Pages_persistence_interface> pages_persistence;
