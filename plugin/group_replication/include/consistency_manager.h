/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONSISTENCY_MANAGER_INCLUDED
#define CONSISTENCY_MANAGER_INCLUDED

#define CONSISTENCY_INFO_OUTCOME_OK 0
#define CONSISTENCY_INFO_OUTCOME_ERROR 1
#define CONSISTENCY_INFO_OUTCOME_COMMIT 2

#include <mysql/group_replication_priv.h>
#include <mysql/plugin_group_replication.h>
#include <atomic>
#include <list>
#include <map>
#include <utility>

#include "plugin/group_replication/include/hold_transactions.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/pipeline_interfaces.h"
#include "plugin/group_replication/include/plugin_observers/group_transaction_observation_manager.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/**
  @class Transaction_consistency_info

  The consistency information of a transaction, including its
  configuration and state.
*/
class Transaction_consistency_info {
 public:
  /*
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated
    @param[in] nothrow When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size, const std::nothrow_t &) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_consistent_transactions, size, MYF(MY_WME));
  }

  /*
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
    @param[in] nothrow When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.
  */
  void operator delete(void *ptr, const std::nothrow_t &) noexcept {
    my_free(ptr);
  }

  /**
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_consistent_transactions, size, MYF(MY_WME));
  }

  /**
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
  */
  void operator delete(void *ptr) noexcept { my_free(ptr); }

  /**
    Constructor

    @param[in]  thread_id         the thread that is executing the transaction
    @param[in]  local_transaction true if this transaction did originate from
                                  this server
    @param[in]  sid               transaction sid
    @param[in]  sidno             transaction sidno
    @param[in]  gno               transaction gno
    @param[in]  consistency_level the transaction consistency
    @param[in]  members_that_must_prepare_the_transaction
                                  list of the members that must prepare the
                                  transaction before it is allowed to commit
  */
  Transaction_consistency_info(
      my_thread_id thread_id, bool local_transaction, const rpl_sid *sid,
      rpl_sidno sidno, rpl_gno gno,
      enum_group_replication_consistency_level consistency_level,
      Members_list *members_that_must_prepare_the_transaction);

  virtual ~Transaction_consistency_info();

  /**
    Get the thread id that is executing the transaction.

    @return the thread id
  */
  my_thread_id get_thread_id();

  /**
    Is the transaction from this server?

   @return  true   yes
            false  otherwise
  */
  bool is_local_transaction();

  /**
    Is the transaction prepared locally?

   @return  true   yes
            false  otherwise
  */
  bool is_transaction_prepared_locally();

  /**
    Get the transaction sidno.

   @return the sidno
  */
  rpl_sidno get_sidno();

  /**
    Get the transaction gno.

    @return the gno
  */
  rpl_gno get_gno();

  /**
    Get the transaction consistency.

    @return the consistency
  */
  enum_group_replication_consistency_level get_consistency_level();

  /**
    Is this transaction running on a single member group?

    @return  true   yes
             false  otherwise
  */
  bool is_a_single_member_group();

  /**
    Did all other ONLINE members already prepared the transaction?

    @return  true   yes
             false  otherwise
  */
  bool is_the_transaction_prepared_remotely();

  /**
    Call action after this transaction being prepared on this member
    applier.

    @param[in]  thread_id      the applier thread id
    @param[in]  member_status  this member status

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int after_applier_prepare(
      my_thread_id thread_id,
      Group_member_info::Group_member_status member_status);

  /**
    Call action after this transaction being prepared by other member.

    @param[in]  gcs_member_id  the member id

    @return Operation status
      @retval CONSISTENCY_INFO_OUTCOME_OK      OK
      @retval CONSISTENCY_INFO_OUTCOME_ERROR   error
      @retval CONSISTENCY_INFO_OUTCOME_COMMIT  transaction must proceeded to
    commit
  */
  int handle_remote_prepare(const Gcs_member_identifier &gcs_member_id);

  /**
    Call action after members leave the group.
    If any of these members are on the prepare wait list, they will
    be removed. If the lists becomes empty, the transaction will proceed
    to commit.

    @param[in]  leaving_members  the members that left

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int handle_member_leave(
      const std::vector<Gcs_member_identifier> &leaving_members);

 private:
  my_thread_id m_thread_id;
  const bool m_local_transaction;
  const bool m_sid_specified;
  rpl_sid m_sid;
  const rpl_sidno m_sidno;
  const rpl_gno m_gno;
  const enum_group_replication_consistency_level m_consistency_level;
  Members_list *m_members_that_must_prepare_the_transaction;
  std::unique_ptr<Checkable_rwlock>
      m_members_that_must_prepare_the_transaction_lock;
  bool m_transaction_prepared_locally;
  bool m_transaction_prepared_remotely;
};

typedef std::pair<rpl_sidno, rpl_gno> Transaction_consistency_manager_key;
typedef std::pair<Transaction_consistency_manager_key,
                  Transaction_consistency_info *>
    Transaction_consistency_manager_pair;
typedef std::pair<Pipeline_event *, Transaction_consistency_manager_key>
    Transaction_consistency_manager_pevent_pair;
typedef std::map<
    Transaction_consistency_manager_key, Transaction_consistency_info *,
    std::less<Transaction_consistency_manager_key>,
    Malloc_allocator<std::pair<const Transaction_consistency_manager_key,
                               Transaction_consistency_info *>>>
    Transaction_consistency_manager_map;

/**
  @class Transaction_consistency_manager

  The consistency information of all ongoing transactions which have
  consistency GROUP_REPLICATION_CONSISTENCY_BEFORE,
  GROUP_REPLICATION_CONSISTENCY_AFTER or
  GROUP_REPLICATION_CONSISTENCY_BEFORE_AND_AFTER.
*/
class Transaction_consistency_manager : public Group_transaction_listener {
 public:
  /**
    Constructor.
  */
  Transaction_consistency_manager();

  ~Transaction_consistency_manager() override;

  /**
    Clear all information.
  */
  void clear();

  /**
    Call action after a transaction is certified.
    The transaction coordination among the members will start on
    this point.

    @param[in]  transaction_info  the transaction info

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int after_certification(Transaction_consistency_info *transaction_info);

  /**
    Call action after a transaction being prepared on this member
    applier.

    @param[in]  sidno          the transaction sidno
    @param[in]  gno            the transaction gno
    @param[in]  thread_id      the applier thread id
    @param[in]  member_status  this member status

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int after_applier_prepare(
      rpl_sidno sidno, rpl_gno gno, my_thread_id thread_id,
      Group_member_info::Group_member_status member_status);

  /**
    Call action after a transaction being prepared by other member.

    If this sid is NULL that means this transaction sid is the group
    name.

    @param[in]  sid            the transaction sid
    @param[in]  gno            the transaction gno
    @param[in]  gcs_member_id  the member id

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int handle_remote_prepare(const rpl_sid *sid, rpl_gno gno,
                            const Gcs_member_identifier &gcs_member_id);

  /**
    Call action after members leave the group.
    If any of these members are on the prepare wait lists, they will
    be removed. If any those lists become empty, those transactions
    proceed to commit.

    @param[in]  leaving_members  the members that left

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int handle_member_leave(
      const std::vector<Gcs_member_identifier> &leaving_members);

  /**
    Call action after commit a transaction on this member.
    If new transactions are waiting for this prepared transaction
    to be committed, they will be released.

    @param[in]  thread_id      the transaction thread id
    @param[in]  sidno          the transaction sidno
    @param[in]  gno            the transaction gno

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int after_commit(my_thread_id thread_id, rpl_sidno sidno,
                   rpl_gno gno) override;

  /**
    Call action before a transaction starts.
    It will handle transactions with
    GROUP_REPLICATION_CONSISTENCY_BEFORE consistency and any others
    that need to wait for preceding prepared transactions to
    commit.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  gr_consistency_level the transaction consistency
    @param[in]  timeout           maximum time to wait
    @param[in]  rpl_channel_type  type of the channel that receives the
                                  transaction

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int before_transaction_begin(my_thread_id thread_id,
                               ulong gr_consistency_level, ulong timeout,
                               enum_rpl_channel_type rpl_channel_type) override;

  /**
    Call action once a Sync_before_execution_message is received,
    this will allow fetch the group transactions set ordered with
    the message order.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  gcs_member_id     the member id

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int handle_sync_before_execution_message(
      my_thread_id thread_id, const Gcs_member_identifier &gcs_member_id) const;

  /**
    Are there local prepared transactions waiting for prepare
    acknowledge from other members?

   @return  true   yes
            false  otherwise
  */
  bool has_local_prepared_transactions();

  /**
    Schedule a View_change_log_event log into the relay to after
    the local prepared transactions are complete, since those
    transactions belong to the previous view and as such must be
    logged before this view.

    @param[in]  pevent            the pipeline event that contains
                                  the View_change_log_event

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int schedule_view_change_event(Pipeline_event *pevent);

  /**
    Inform that plugin did start.
  */
  void plugin_started();

  /**
    Inform that plugin is stopping.
    New consistent transactions are not allowed to start.
    On after_applier_prepare the transactions do not wait
    for other prepares.
  */
  void plugin_is_stopping();

  /**
    Register an observer for transactions
  */
  void register_transaction_observer();

  /**
    Unregister the observer for transactions
  */
  void unregister_transaction_observer();

  int before_commit(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin) override;

  int before_rollback(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin) override;

  int after_rollback(my_thread_id thread_id) override;

  /**
    Tells the consistency manager that a primary election is running so it
    shall enable primary election checks
  */
  void enable_primary_election_checks();

  /**
    Tells the consistency manager that a primary election ended so it
    shall disable primary election checks
  */
  void disable_primary_election_checks();

 private:
  /**
    Help method called by transaction begin action that, for
    transactions with consistency GROUP_REPLICATION_CONSISTENCY_BEFORE
    or GROUP_REPLICATION_CONSISTENCY_BEFORE_AND_AFTER will:
      1) send a message to all members;
      2) when that message is received and processed in-order,
         w.r.t. the message stream, will fetch the Group Replication
         applier RECEIVED_TRANSACTION_SET, the set of remote
         transactions that were allowed to commit;
      3) wait until all the transactions on Group Replication applier
         RECEIVED_TRANSACTION_SET are committed.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  consistency_level the transaction consistency
    @param[in]  timeout           maximum time to wait

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int transaction_begin_sync_before_execution(
      my_thread_id thread_id,
      enum_group_replication_consistency_level consistency_level,
      ulong timeout) const;

  /**
    Help method called by transaction begin action that, if there are
    precedent prepared transactions with consistency
    GROUP_REPLICATION_CONSISTENCY_AFTER or
    GROUP_REPLICATION_CONSISTENCY_BEFORE_AND_AFTER, will hold the
    this transaction until the prepared are committed.

    @param[in]  thread_id         the thread that is executing the
                                  transaction
    @param[in]  timeout           maximum time to wait

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int transaction_begin_sync_prepared_transactions(my_thread_id thread_id,
                                                   ulong timeout);

  /**
    Help method that cleans prepared transactions and releases
    transactions waiting on them.

    @param[in]  key               the transaction key

    @return Operation status
      @retval 0      OK
      @retval !=0    error
  */
  int remove_prepared_transaction(Transaction_consistency_manager_key key);

  Checkable_rwlock *m_map_lock;
  Transaction_consistency_manager_map m_map;

  Checkable_rwlock *m_prepared_transactions_on_my_applier_lock;

  std::list<Transaction_consistency_manager_key,
            Malloc_allocator<Transaction_consistency_manager_key>>
      m_prepared_transactions_on_my_applier;
  std::list<my_thread_id, Malloc_allocator<my_thread_id>>
      m_new_transactions_waiting;
  std::list<Transaction_consistency_manager_pevent_pair,
            Malloc_allocator<Transaction_consistency_manager_pevent_pair>>
      m_delayed_view_change_events;
  Transaction_consistency_manager_key m_last_local_transaction;

  std::atomic<bool> m_plugin_stopping;
  std::atomic<bool> m_primary_election_active;

  /** Hold transaction mechanism */
  Hold_transactions m_hold_transactions;
};

#endif /* CONSISTENCY_MANAGER_INCLUDED */
