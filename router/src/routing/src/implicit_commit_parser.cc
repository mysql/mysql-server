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

#include "implicit_commit_parser.h"

#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"
#include "sql/sql_yacc.h"

stdx::expected<bool, std::string> ImplicitCommitParser::parse(
    std::optional<classic_protocol::session_track::TransactionState>
        trx_state) {
  if (!trx_state) return stdx::unexpected("Expected trx-state to be set.");

  // no transaction, nothing to commit.
  if (trx_state->trx_type() == '_') return false;

  if (accept(ALTER)) {
    if (accept(EVENT_SYM) ||       //
        accept(FUNCTION_SYM) ||    //
        accept(PROCEDURE_SYM) ||   //
        accept(SERVER_SYM) ||      //
        accept(TABLE_SYM) ||       //
        accept(TABLESPACE_SYM) ||  //
        accept(VIEW_SYM) ||        //
        accept(USER) ||            //
        false) {
      return true;
    }

    return false;
  }

  if (accept(CREATE) || accept(DROP)) {
    if (accept(DATABASE) ||        //
        accept(EVENT_SYM) ||       //
        accept(FUNCTION_SYM) ||    //
        accept(INDEX_SYM) ||       //
        accept(PROCEDURE_SYM) ||   //
        accept(ROLE_SYM) ||        //
        accept(SERVER_SYM) ||      //
        accept(SPATIAL_SYM) ||     //
        accept(TABLE_SYM) ||       //
        accept(TABLESPACE_SYM) ||  //
        accept(TRIGGER_SYM) ||     //
        accept(VIEW_SYM) ||        //
        accept(USER) ||            //
        false) {
      return true;
    }

    return false;
  }

  if (accept(GRANT) || accept(REVOKE) || accept(TRUNCATE_SYM)) {
    return true;
  }

  if (accept(RENAME)) {
    if (accept(USER) || accept(TABLE_SYM)) {
      return true;
    }
    return false;
  }

  if (accept(INSTALL_SYM) || accept(UNINSTALL_SYM)) {
    if (accept(PLUGIN_SYM)) {
      return true;
    }
    return false;
  }

  if (accept(SET_SYM)) {
    if (accept(PASSWORD)) {
      return true;
    }
    return false;
  }

  if (accept(BEGIN_SYM)) {
    return true;
  }

  if (accept(START_SYM)) {
    if (accept(TRANSACTION_SYM) || accept(REPLICA_SYM) || accept(SLAVE)) {
      return true;
    }
    return false;
  }

  if (accept(STOP_SYM)) {
    if (accept(REPLICA_SYM) || accept(SLAVE)) {
      return true;
    }
    return false;
  }

  if (accept(CHANGE)) {
    if (accept(MASTER_SYM) || accept(REPLICATION)) {
      return true;
    }
    return false;
  }

  if (accept(LOCK_SYM)) {
    if (accept(TABLES)) {
      return true;
    }

    return false;
  }

  if (accept(UNLOCK_SYM)) {
    if (accept(TABLES)) {
      // UNLOCK TABLES only commits if there is a table locked and a transaction
      // open.
      return trx_state->locked_tables() != '_';
    }

    return false;
  }

  if (accept(ANALYZE_SYM)) {
    if (accept(TABLE_SYM)) {
      return true;
    }

    return false;
  }

  if (accept(CACHE_SYM)) {
    if (accept(INDEX_SYM)) {
      return true;
    }

    return false;
  }

  if (accept(CHECK_SYM) || accept(OPTIMIZE) || accept(REPAIR)) {
    if (accept(TABLE_SYM)) {
      return true;
    }

    return false;
  }

  if (accept(FLUSH_SYM)) {
    return true;
  }

  // LOAD INDEX INTO CACHE
  if (accept(LOAD)) {
    return (accept(INDEX_SYM) && accept(INTO) && accept(CACHE_SYM));
  }

  // RESET, except RESET PERSIST
  if (accept(RESET_SYM)) {
    if (accept(PERSIST_SYM)) {
      return false;
    }

    return true;
  }

  return false;
}
