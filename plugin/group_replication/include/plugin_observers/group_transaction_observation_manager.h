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

#ifndef GROUP_TRANSACTION_OBSERVATION_MANAGER_INCLUDED
#define GROUP_TRANSACTION_OBSERVATION_MANAGER_INCLUDED

#include <mysql/group_replication_priv.h>
#include <list>

#include "my_inttypes.h"

/** Listener for transaction life cycle events */
class Group_transaction_listener {
 public:
  /** Enum for transaction origins */
  enum enum_transaction_origin {
    GROUP_APPLIER_TRANSACTION = 0,   // Group applier transaction
    GROUP_RECOVERY_TRANSACTION = 1,  // Distributed recovery transaction
    GROUP_LOCAL_TRANSACTION = 2      // Local transaction
  };

  /** Class destructor */
  virtual ~Group_transaction_listener();

  /**
    Executed before a transaction begins
    @param thread_id   the transaction thread id
    @param gr_consistency_level  the current consistency level for this session
    @param hold_timeout the max time to execute an action on this transaction
    @param channel_type type channel that receives transaction
  */
  virtual int before_transaction_begin(my_thread_id thread_id,
                                       ulong gr_consistency_level,
                                       ulong hold_timeout,
                                       enum_rpl_channel_type channel_type) = 0;

  /**
    Executed before commit
    @param thread_id   the transaction thread id
    @param origin      who applied it
  */
  virtual int before_commit(my_thread_id thread_id,
                            enum_transaction_origin origin) = 0;

  /**
    Executed before rollback
    @param thread_id   the transaction thread id
    @param origin      who applied it
  */
  virtual int before_rollback(my_thread_id thread_id,
                              enum_transaction_origin origin) = 0;
  /**
    Executed after commit
    @param thread_id   the transaction thread id
    @param sidno       the transaction sidno
    @param gno         the transaction gno
  */
  virtual int after_commit(my_thread_id thread_id, rpl_sidno sidno,
                           rpl_gno gno) = 0;
  /**
    Executed after rollback
    @param thread_id   the transaction thread id
  */
  virtual int after_rollback(my_thread_id thread_id) = 0;
};

class Group_transaction_observation_manager {
 public:
  /*
    Initialize the class and register an observer.
  */
  Group_transaction_observation_manager();

  /*
    Destructor.
    Deletes all observers
  */
  ~Group_transaction_observation_manager();

  /*
    The method to register new observers
    @param observer  An observer class to register
  */
  void register_transaction_observer(Group_transaction_listener *observer);

  /*
    The method to unregister new observers
    @param observer  An observer class to unregister
  */
  void unregister_transaction_observer(Group_transaction_listener *observer);

  /**
    Executed before a transaction begins
    @param thread_id   the transaction thread id
    @param gr_consistency_level  the current consistency level for this session
    @param hold_timeout the max time to execute an action on this transaction
    @param rpl_channel_type type channel that receives transaction
  */
  int before_transaction_begin(my_thread_id thread_id,
                               ulong gr_consistency_level, ulong hold_timeout,
                               enum_rpl_channel_type rpl_channel_type);

  /*
    Executed before commit
    @param thread_id   the transaction thread id
    @param origin      who applied it
  */
  int before_commit(my_thread_id thread_id,
                    Group_transaction_listener::enum_transaction_origin origin);
  /*
    Executed before rollback
    @param thread_id   the transaction thread id
 */
  int before_rollback(my_thread_id thread_id);

  /*
    Executed after commit
    @param thread_id   the transaction thread id
    @param sidno       the transaction sidno
    @param gno         the transaction gno
  */
  int after_commit(my_thread_id thread_id, rpl_sidno sidno, rpl_gno gno);

  /*
    Executed after rollback
    @param thread_id   the transaction thread id
  */
  int after_rollback(
      my_thread_id thread_id,
      Group_transaction_listener::enum_transaction_origin origin);

  /**
    Are there any observers present
    @return true if at least one observer is present, false otherwise
  */
  bool is_any_observer_present();

  /**
    Get all registered observers

    @note to get the list and while using it, you should take a read lock from
    transaction_observer_list_lock (you can use read_lock_observer_list())

    @return The list of all registered observers
  */
  std::list<Group_transaction_listener *> *get_all_observers();

  /** Locks the observer list for reads */
  void read_lock_observer_list();
  /** Locks the observer list for writes */
  void write_lock_observer_list();
  /** Unlocks the observer list */
  void unlock_observer_list();

 private:
  /** List of observers */
  std::list<Group_transaction_listener *> group_transaction_listeners;

  /** The lock to protect the list */
  Checkable_rwlock *transaction_observer_list_lock;

  /** Flag that indicates that there are observers (for performance)*/
  std::atomic<bool> registered_observers;
};

#endif /* GROUP_TRANSACTION_OBSERVATION_MANAGER_INCLUDED */
