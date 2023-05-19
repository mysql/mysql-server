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

#include <sql/current_thd.h>
#include <sql/mysqld_thd_manager.h>
#include <sql/sql_lex.h>
#include "mutex_lock.h"  // MUTEX_LOCK
#include "mysql/components/services/log_builtins.h"
#include "mysql_ongoing_transaction_query_imp.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_parse.h"  // sql_command_flags

class Get_running_transactions : public Do_THD_Impl {
 public:
  Get_running_transactions() = default;

  /*
   This method relies on the assumption that a thread running a query
   will either have an active query plan, or is in the middle of a
   multi-statement transaction.
  */
  void operator()(THD *thd) override {
    if (thd->is_killed() || thd->is_error()) return;

    MUTEX_LOCK(lock_thd_data, &thd->LOCK_thd_data);
    if (thd->is_being_disposed()) return;

    LEX *l = thd->lex;

    /*
      In an ideal world, we might be able to just look at whether

          sql_command_flags[sql_command] &
            (CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS)

      is true to identify interesting DDL. Unfortunately, sql_command
      is not always set to a valid command. It will be SQLCOM_END
      before parsing, and during e.g. stored routine processing.
      To further muddy the waters, we change sql_command at various
      locations in the code.

      That said, this still wouldn't detect being in the middle of a
      multi-statement transaction, so we also explicitly inspect
      transaction state below.
    */

    // "Command not otherwise specified"
    enum_sql_command sql_command = SQLCOM_END;

    /*
      No flags (blocking or otherwise) found yet.
      We must fail-open as we may never get a usable lex on some threads.
    */
    int blocked_by_sql_command = 0;

    /*
      Get command code set on the lex.
      If we get something valid, we'll inspect the flags for that command
      to see whether the command auto-commits. DDL commands should match
      this pattern.
    */
    if ((l != nullptr) && ((sql_command = l->sql_command) != SQLCOM_END)) {
      /*
        If we got something better than SQLCOM_END from the lex,
        the lex was set up.

        See whether this is a "new-style" command (i.e. it has
        an object derived from Sql_cmd set on thd->lex->m_sql_cmd).
        If so, that's the info we'll use.

        Unfortunately, while there are situations where sql_command
        is SQLCOM_END but there is valid non-NULL value in m_sql_cmd,
        there are others where that value is garbage, so we may not
        deref in such cases. But what we can't identify as a specific
        command here, we still may have identified as TX_STMT_DDL in
        the transaction tracker (see below).
      */
      if (l->m_sql_cmd != nullptr)
        sql_command = l->m_sql_cmd->sql_command_code();

      blocked_by_sql_command =
          sql_command_flags[sql_command] &
          (CF_CHANGES_DATA | CF_IMPLICIT_COMMIT_BEGIN | CF_IMPLICIT_COMMIT_END);
    }

    /*
      Query the transaction tracker for relevant flags.

      TX_EXPLICIT indicates a transaction that was started explicitly,
      e.g. with BEGIN / START TRANSACTION.
      (See also in_active_multi_stmt_transaction().)

      TX_STMT_DML is turned on if the statement "behaves like DML"
      (by passing through run_before_dml_hook()).

      TX_STMT_DDL is turned on if after parsing, the statement
      identifies as DDL (by means of sql_cmd_type()) and
      "behaves like DDL" (by passing through mark_trx_read_write()).

      Due to the different life-cycles,

        ((tst->get_trx_state() & TX_STMT_DDL) > 0)

      may differ from

        (blocked_by_sql_command > 0)

      This works to our advantage in certain corner cases as it
      extends our gaze.
    */
    TX_TRACKER_GET(tst);
    int blocked_by_trx_tracker =
        tst->get_trx_state() & (TX_EXPLICIT | TX_STMT_DML | TX_STMT_DDL);

    /*
      Now add this thread to the list of showstoppers for change-primary
      if we found a reason to.
    */
    if ((blocked_by_sql_command > 0) || (blocked_by_trx_tracker > 0)) {
      thread_ids.push_back(thd->thread_id());
    }
  }

  ulong get_transaction_count() { return thread_ids.size(); }

  void fill_transaction_ids(unsigned long **ids) {
    size_t number_thd = thread_ids.size();
    *ids = (unsigned long *)my_malloc(
        PSI_NOT_INSTRUMENTED, number_thd * sizeof(unsigned long), MYF(MY_WME));
    int index = 0;
    for (std::vector<my_thread_id>::iterator it = thread_ids.begin();
         it != thread_ids.end(); ++it) {
      (*ids)[index] = *it;
      index++;
    }
  }

 private:
  /* Status of all threads are summed into this. */
  std::vector<my_thread_id> thread_ids;
};

DEFINE_BOOL_METHOD(
    mysql_ongoing_transactions_query_imp::get_ongoing_server_transactions,
    (unsigned long **thread_ids, unsigned long *length)) {
  Get_running_transactions trx_counter;
  Global_THD_manager::get_instance()->do_for_all_thd(&trx_counter);
  trx_counter.fill_transaction_ids(thread_ids);
  *length = trx_counter.get_transaction_count();
  return false;
}
