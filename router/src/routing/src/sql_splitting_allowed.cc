/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "sql_splitting_allowed.h"

#include "mysql/harness/stdx/expected.h"
#include "sql/sql_yacc.h"

stdx::expected<SplittingAllowedParser::Allowed, std::string>
SplittingAllowedParser::parse() {
  if (accept(SHOW)) {
    // https://dev.mysql.com/doc/refman/en/show.html
    //
    if (                          // BINARY: below
                                  // BINLOG: below
        accept(CHAR_SYM) ||       // CHARACTER
        accept(CHARSET) ||        //
        accept(COLLATION_SYM) ||  //
        accept(COLUMNS) ||        //
        accept(CREATE) ||         //
        accept(DATABASES) ||      //
                                  // ENGINE: below
        accept(ENGINES_SYM) ||    //
        accept(ERRORS) ||         //
        accept(EVENTS_SYM) ||     //
        accept(FUNCTION_SYM) ||   //
        accept(GRANTS) ||         //
        accept(INDEX_SYM) ||      //
                                  // MASTER STATUS: below
                                  // OPEN TABLES: below
        accept(PLUGINS_SYM) ||    //
        accept(PRIVILEGES) ||     //
        accept(PROCEDURE_SYM) ||  //
                                  // PROCESSLIST: below
                                  // PROFILE: below
                                  // PROFILES: below
                                  // RELAYLOG: below
                                  // REPLICA: below
                                  // REPLICAS: below
                                  // SLAVE: below
        accept(STATUS_SYM) ||     //
        accept(TABLE_SYM) ||      //
        accept(TABLES) ||         //
        accept(TRIGGERS_SYM) ||   //
        accept(VARIABLES) ||      //
        accept(WARNINGS)) {
      return Allowed::Always;
    }

    // per instance commands
    if (accept(ENGINE_SYM) ||       //
        accept(OPEN_SYM) ||         // OPEN TABLES
        accept(PLUGINS_SYM) ||      //
        accept(PROCESSLIST_SYM) ||  //
        accept(PROFILES_SYM) ||     //
        accept(PROFILE_SYM)) {
      return Allowed::InTransaction;
    }

    if (accept(GLOBAL_SYM)) {
      if (accept(VARIABLES)) {
        return Allowed::Always;
      }
      if (accept(STATUS_SYM)) {
        return Allowed::InTransaction;
      }

      return Allowed::Never;
    }

    // Write-only
    if (accept(BINARY_SYM) ||  //
        accept(MASTER_SYM) ||  //
        accept(REPLICAS_SYM)) {
      return Allowed::OnlyReadWrite;
    }

    // Read-only
    if (accept(BINLOG_SYM) ||    //
        accept(RELAYLOG_SYM) ||  //
        accept(REPLICA_SYM)      //
    ) {
      return Allowed::OnlyReadOnly;
    }

    if (accept(SLAVE)) {
      if (accept(STATUS_SYM)) {
        return Allowed::OnlyReadOnly;
      }

      if (accept(HOSTS_SYM)) {
        return Allowed::OnlyReadWrite;
      }

      return Allowed::Never;
    }

    // SHOW [EXTENDED] [FULL] COLUMNS|FIELDS

    if (accept(EXTENDED_SYM)) {
      accept(FULL);

      if (accept(COLUMNS)) {  // FIELDS and COLUMNS both resolve to COLUMNS
        return Allowed::Always;
      }

      return Allowed::Never;
    } else if (accept(FULL)) {
      if (accept(COLUMNS) || accept(TABLES)) {
        return Allowed::Always;
      } else if (accept(PROCESSLIST_SYM)) {
        return Allowed::InTransaction;
      }

      return Allowed::Never;
    }

    // SHOW [STORAGE] ENGINES
    if (accept(STORAGE_SYM)) {
      if (accept(ENGINES_SYM)) {
        return Allowed::Always;
      }

      return Allowed::Never;
    }

    if (accept(SESSION_SYM)) {
      if (accept(STATUS_SYM) || accept(VARIABLES)) {
        return Allowed::Always;
      }
    }

    return Allowed::Never;
  } else if (accept(CREATE) || accept(ALTER)) {
    if (accept(DATABASE) ||        //
        accept(EVENT_SYM) ||       //
        accept(FUNCTION_SYM) ||    //
        accept(INDEX_SYM) ||       //
                                   // INSTANCE
        accept(PROCEDURE_SYM) ||   //
                                   // SERVER
        accept(SPATIAL_SYM) ||     //
        accept(TABLE_SYM) ||       //
        accept(TABLESPACE_SYM) ||  //
        accept(TRIGGER_SYM) ||     //
        accept(VIEW_SYM) ||        //
        accept(USER) ||            //
        accept(ROLE_SYM)           //
    ) {
      return Allowed::Always;
    }

    if (accept(AGGREGATE_SYM)) {
      // CREATE AGGREGATE FUNCTION
      if (accept(FUNCTION_SYM)) {
        return Allowed::Always;
      }

      return Allowed::Never;
    }

    if (accept(ALGORITHM_SYM)) {
      // CREATE ALGORITHM = ... VIEW
      return Allowed::Always;
    }

    if (accept(DEFINER_SYM)) {
      // CREATE DEFINER = ... PROCEDURE|FUNCTION|EVENT|VIEW
      return Allowed::Always;
    }

    if (accept(OR_SYM)) {
      // CREATE OR REPLACE ... VIEW|SPATIAL REFERENCE SYSTEM
      if (accept(REPLACE_SYM)) {
        return Allowed::Always;
      }

      return Allowed::Never;
    }

    if (accept(SQL_SYM)) {
      // CREATE SQL SECURITY ... VIEW
      return Allowed::Always;
    }

    if (accept(TEMPORARY)) {
      // CREATE TEMPORARY TABLE
      if (accept(TABLE_SYM)) {
        return Allowed::Always;
      }

      return Allowed::Never;
    }

    if (accept(UNDO_SYM)) {
      // CREATE UNDO TABLESPACE
      if (accept(TABLESPACE_SYM)) {
        return Allowed::Always;
      }

      return Allowed::Never;
    }

    if (accept(UNIQUE_SYM) || accept(FULLTEXT_SYM) || accept(SPATIAL_SYM)) {
      // CREATE UNIQUE|FULLTEXT|SPATIAL INDEX
      if (accept(INDEX_SYM)) {
        return Allowed::Always;
      }
      return Allowed::Never;
    }

    // SERVER
    // INSTANCE
    // LOGFILE GROUP

    return Allowed::Never;
  } else if (accept(DROP)) {
    if (accept(DATABASE) ||        //
        accept(EVENT_SYM) ||       //
        accept(FUNCTION_SYM) ||    //
        accept(INDEX_SYM) ||       //
                                   // INSTANCE
        accept(PROCEDURE_SYM) ||   //
                                   // SERVER
        accept(SPATIAL_SYM) ||     //
                                   // TEMPORARY: below
        accept(TABLE_SYM) ||       //
        accept(TABLESPACE_SYM) ||  //
        accept(TRIGGER_SYM) ||     //
        accept(VIEW_SYM) ||        //
        accept(USER) ||            //
        accept(ROLE_SYM)           //
    ) {
      return Allowed::Always;
    }

    if (accept(TEMPORARY)) {
      // CREATE TEMPORARY TABLE
      if (accept(TABLE_SYM)) {
        return Allowed::Always;
      }

      return Allowed::Never;
    }

    // - SERVER
    // - INSTANCE

    return Allowed::Never;
  } else if (                // read-only statements
      accept(SELECT_SYM) ||  //
      accept(WITH) ||        //
      accept(TABLE_SYM) ||   //
      accept(DO_SYM) ||      //
      accept(VALUES) ||      //
      accept(USE_SYM) ||     //
      accept(DESC) ||        //
      accept(DESCRIBE) ||    //
      accept(HELP_SYM) ||

      // DML
      accept(CALL_SYM) ||     //
      accept(INSERT_SYM) ||   //
      accept(UPDATE_SYM) ||   //
      accept(DELETE_SYM) ||   //
      accept(REPLACE_SYM) ||  //
      accept(TRUNCATE_SYM) ||

      // User management
      accept(GRANT) ||   //
      accept(REVOKE) ||  //

      // transaction and locking
      accept(BEGIN_SYM) ||      //
      accept(COMMIT_SYM) ||     //
      accept(RELEASE_SYM) ||    //
      accept(ROLLBACK_SYM) ||   //
      accept(SAVEPOINT_SYM) ||  //
                                // START is below.
      accept(XA_SYM) ||         //

      // import
      accept(IMPORT)) {
    return Allowed::Always;
  } else if (accept(FLUSH_SYM)) {
    // FLUSH flush_option [, flush option]
    //
    // Not replicated:
    //
    // - if LOCAL or NO_WRITE_TO_BINLOG is specified
    // - LOGS
    // - TABLES ... FOR EXPORT
    // - TABLES WITH READ LOCK

    if (accept(NO_WRITE_TO_BINLOG) || accept(LOCAL_SYM)) {
      return Allowed::Never;
    }

    if (accept(TABLES)) {
      while (auto tkn = accept_if_not(END_OF_INPUT)) {
        // FOR EXPORT
        // WITH READ LOCK
        if (tkn.id() == WITH || tkn.id() == FOR_SYM) return Allowed::Never;
      }

      return Allowed::Always;
    }

    TokenText last_tkn;

    // check for LOGS (after FLUSH ... or after ',')
    while (auto tkn = accept_if_not(END_OF_INPUT)) {
      if (tkn.id() == LOGS_SYM) {
        if (last_tkn.id() == ',' || last_tkn.id() == 0) {
          return Allowed::Never;
        }
      }

      last_tkn = tkn;
    }

    return Allowed::Always;
  } else if (accept(LOCK_SYM) || accept(UNLOCK_SYM)) {
    // per instance, not replicated.
    return Allowed::Never;
  } else if (accept(LOAD)) {
    if (accept(XML_SYM) || accept(DATA_SYM)) {
      return Allowed::Always;
    }

    return Allowed::Never;
  } else if (accept(RENAME)) {
    if (accept(USER) || accept(TABLE_SYM)) {
      return Allowed::Always;
    }

    return Allowed::Never;
  } else if (accept(SET_SYM)) {
    // exclude:
    // - SET RESOURCE GROUP: not replicated
    // - SET GLOBAL
    // - SET PERSIST
    if (accept(PASSWORD) ||         // SET PASSWORD = ...
        accept(TRANSACTION_SYM) ||  // SET TRANSACTION READ ONLY
        accept(DEFAULT_SYM) ||      // SET DEFAULT ROLE
        accept(NAMES_SYM) ||        // SET NAMES
        accept(CHAR_SYM)            // SET CHARACTER SET
    ) {
      return Allowed::Always;
    }

    if (accept(RESOURCE_SYM)) {
      return Allowed::Never;
    }

    // forbid SET GLOBAL, but allow SET foo = @@GLOBAL.foo;
    bool is_lhs{true};

    while (auto tkn = accept_if_not(END_OF_INPUT)) {
      if (tkn.id() == SET_VAR || tkn.id() == EQ) {
        // after := or = is the right-hand-side
        is_lhs = false;
      } else if (tkn.id() == ',') {
        // after , back to left-hand-side
        is_lhs = true;
      }

      if (is_lhs && (tkn.id() == GLOBAL_SYM || tkn.id() == PERSIST_ONLY_SYM ||
                     tkn.id() == PERSIST_SYM)) {
        return Allowed::Never;
      }
    }

    return Allowed::Always;
  } else if (accept(START_SYM)) {
    // exclude GROUP_REPLICATION|REPLICAS
    if (accept(TRANSACTION_SYM)) {
      return Allowed::Always;
    }
    return Allowed::Never;
  } else if (accept(CHECKSUM_SYM) || accept(CHECK_SYM)) {
    if (accept(TABLE_SYM)) {
      return Allowed::Always;
    }
    return Allowed::Never;
  } else if (accept(ANALYZE_SYM) || accept(OPTIMIZE) || accept(REPAIR)) {
    if (accept(NO_WRITE_TO_BINLOG) || accept(LOCAL_SYM)) {
      // ignore LOCAL and NO_WRITE_TO_BINLOG
    }

    if (accept(TABLE_SYM)) {
      return Allowed::Always;
    }
    return Allowed::Never;
  } else if (accept('(')) {
    return Allowed::Always;
  } else if (accept(BINLOG_SYM)) {
    return Allowed::Always;
  }

  // - HANDLER
  // - PREPARE,

  return Allowed::Never;
}
