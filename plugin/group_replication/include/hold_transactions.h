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

#ifndef HOLD_TRANSACTIONS_INCLUDE
#define HOLD_TRANSACTIONS_INCLUDE

#include "my_inttypes.h"
#include "sql/sql_class.h"

/**
  @class Hold_transactions
  Class that contains the logic to hold transactions when
  group_replication_consistency is set to BEFORE_ON_PRIMARY_FAILOVER
*/

class Hold_transactions {
 public:
  /**
    Class constructor for hold transaction logic
  */
  Hold_transactions();

  /**
    Class destructor for hold transaction logic
  */
  ~Hold_transactions();

  /**
    Method to enable the hold of transactions when a primary election is
    occurring
  */
  void enable();

  /**
    Method to resume transactions on hold, primary election ended
    */
  void disable();

  /**
    Method to wait for a primary failover to be complete

      @param hold_timeout seconds to abort wait

      @return the operation status
          @retval 0       if success
          @retval !=0     mysqld error code
  */
  int wait_until_primary_failover_complete(ulong hold_timeout);

 private:
  /**
    Method to verify if thread is killed
  */
  bool is_thread_killed() {
    return current_thd != nullptr && current_thd->is_killed();
  }

  /** is plugin currently applying backlog */
  bool applying_backlog = false;

  /** protect and notify changes on applying_backlog */
  mysql_mutex_t primary_promotion_policy_mutex;
  mysql_cond_t primary_promotion_policy_condition;
};

#endif /* HOLD_TRANSACTIONS_INCLUDE */
