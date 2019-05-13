/*
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_DDL_TRANSACTION_CTX_H
#define NDB_DDL_TRANSACTION_CTX_H

#include <string>
#include <vector>
#include "sql/ndb_thd.h"

/* A class representing a DDL statement. */
class Ndb_DDL_stmt {
 public:
  enum DDL_type { CREATE_TABLE, RENAME_TABLE, DROP_TABLE };

 private:
  const DDL_type m_ddl_type;  // DDL type
  const std::vector<std::string>
      m_info;                      // vector to store all the DDL information
  bool m_stmt_distributed{false};  // Flag that has the distribution status

 public:
  template <typename... Args>
  Ndb_DDL_stmt(DDL_type ddl_type, Args... info)
      : m_ddl_type(ddl_type), m_info{info...} {}

  const std::vector<std::string> &get_info() const { return m_info; }
  DDL_type get_ddl_type() const { return m_ddl_type; }
  void mark_as_distributed() { m_stmt_distributed = true; }
  bool has_been_distributed() const { return m_stmt_distributed; }
};

/* DDL Transaction context class to log the DDLs being executed.

   A DDL can be executed by making a single request or mutliple requests to
   the Storage Engine depending on the nature of the DDL. For example, a
   CREATE TABLE query can be done in a single request to the SE but a ALTER
   TABLE COPY would require more than a single request. These requests are
   the statements sent to the SE for execution. Apart from these
   statements, every DDL would also involve executing statements in InnoDB
   SE, for updating the entries in DD and executing statements in binlog
   handlers. A DDL transaction is a collection of all these statements. To
   make such a transaction Atomic, the SQL Layer uses a 2PC commit protocol
   derived from the Open/XA distributed transaction specifications.

   In ndbcluster, due to the absence of support for temp tables,
   maintaining a DDL transaction is not possible and we have to commit the
   DDL statements then and there. To support Atomic DDLs with such a setup,
   a logger that logs all the DDL statements executed in ndbcluster is
   required. And if the SQL Layer asks for a rollback at the end of the
   transaction, the schema changes can be undone by simply reversing the
   statements.

   This class implements such a logger to log all the statements that got
   executed in ndbcluster as the part of a DDL transaction. It also provides
   various methods that can be used to commit/rollback the changes when
   requested by the SQL Layer at the end of the DDL transaction. */
class Ndb_DDL_transaction_ctx {
 private:
  class THD *const m_thd;

  /* A list to log all the DDL statements executed in ndbcluster. */
  std::vector<Ndb_DDL_stmt> m_executed_ddl_stmts;

  /* If a participating engine in the DDL transaction is not atomic, then
     the SQL Layer requests all the engines involved in the transaction to
     commit immediately after every statement. Due to this, in an event of
     failure, it also takes care of rolling back any statements that have been
     already asked to commit. In such cases, ndbcluster should not rollback the
     statements that have been asked to commit already by the SQL Layer.
     An example of such query is running 'ALTER TABLE .. ENGINE MYISAM' on an
     NDB table.

     This variable tracks the position of the statement in m_executed_ddl_stmts
     vector until which commit has been requested already by the SQL Layer.  */
  unsigned int m_latest_committed_stmt{0};

  /* Status of the ongoing DDL */
  enum DDL_STATUS {
    DDL_EMPTY,
    DDL_IN_PROGRESS,
    DDL_COMMITED,
    DDL_ROLLED_BACK
  } m_ddl_status{DDL_EMPTY};

  /* @brief Create the Ndb_DDL_stmt objects and append them to the
            executed_ddl_stmts list  */
  template <typename... Args>
  void log_ddl_stmt(Ndb_DDL_stmt::DDL_type ddl_op_type, Args... ddl_info) {
    /* This is a new DDL transaction if there are no ddl stmts yet */
    bool first_stmt_in_trx = false;
    if (m_ddl_status == DDL_EMPTY || m_ddl_status == DDL_COMMITED) {
      /* If the DDL status is empty, this is the first stmt in the transaction.

         If the DDL is already committed, it implies that the stmts so far were
         committed and this is a new stmt. This happens when the SQL Layer is
         calling commit on individual stmts rather than at the end of
         transaction. We should treat all such stmts as mini transactions but
         still maintain the log for the overall DDL transaction.

         In both the cases, mark the DDL as in progress and mark this as the
         first stmt.*/
      m_ddl_status = DDL_IN_PROGRESS;
      first_stmt_in_trx = true;
    }

    /* Log them only if DDL is in progress */
    if (m_ddl_status == DDL_IN_PROGRESS) {
      m_executed_ddl_stmts.emplace_back(ddl_op_type, ddl_info...);

      /* Register ndbcluster as a part of the stmt. Additionally register
         it as a part of the transaction if this is the first stmt. */
      ndb_thd_register_trans(m_thd, first_stmt_in_trx);
    }
  }

  /* Methods to handle rollback of individual DDls */
  bool rollback_create_table(const Ndb_DDL_stmt &);

 public:
  Ndb_DDL_transaction_ctx(class THD *thd) : m_thd(thd) {}

  /* @brief Check if the current DDL execution has made any changes
            to the Schema that has not been committed yet.

     @return Returns true if ddl_status is in progress
                     false otherwise. */
  bool has_uncommitted_schema_changes() const {
    return (m_ddl_status == DDL_IN_PROGRESS);
  }

  /* Helper methods to log the DDL. */
  /* @brief Log a create table statement in DDL Context.

     @param path_name       Path name of the table. */
  void log_create_table(const std::string &path_name);
  void log_rename_table(const std::string &old_db_name,
                        const std::string &old_table_name,
                        const std::string &new_db_name,
                        const std::string &new_table_name,
                        const std::string &from, const std::string &to);

  /* @brief Mark the last DDL stmt as distributed */
  void mark_last_stmt_as_distributed() {
    m_executed_ddl_stmts[m_executed_ddl_stmts.size() - 1].mark_as_distributed();
  }

  /* @brief Commit the DDL transaction  */
  void commit();

  /* @brief Rollback any changes done to the Schema during DDL execution.
            Iterate the executed_ddl_stmts vector and rollback
            all the changes in reverse. Also undo any schema change
            distributed through schema distribution */
  bool rollback();

  /* @brief Check if the DDL is being rollbacked */
  bool rollback_in_progress() const {
    return (m_ddl_status == DDL_ROLLED_BACK);
  }
};

#endif /* NDB_DDL_TRANSACTION_CTX_H */
