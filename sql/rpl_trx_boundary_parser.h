/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @addtogroup Replication
  @{

  @file

  @brief Transaction boundary parser definitions. This includes code for
  parsing a stream of events identifying the transaction boundaries (like
  if the event is starting a transaction, is in the middle of a transaction
  or if the event is ending a transaction).
*/

#ifndef RPL_TRX_BOUNDARY_PARSER_H
#define RPL_TRX_BOUNDARY_PARSER_H

#include "my_global.h"

class Format_description_log_event;

/**
  @class Transaction_boundary_parser

  This is the base class for verifying transaction boundaries in a replication
  event stream.
*/
class Transaction_boundary_parser
{
public:
  /**
     Constructor.
  */
  Transaction_boundary_parser()
    :current_parser_state(EVENT_PARSER_NONE)
  {
    DBUG_ENTER("Transaction_boundary_parser::Transaction_boundary_parser");
    DBUG_VOID_RETURN;
  }

  /**
     Reset the transaction boundary parser state.
  */
  void reset();

  /*
    In an event stream, an event is considered safe to be separated from the
    next if it is not inside a transaction.
    We need to know this in order to evaluate if we will let the relay log
    to be rotated or not.
  */

  /**
     State if the transaction boundary parser is inside a transaction.
     This "inside a transaction" means that the parser was fed with at least
     one event of a transaction, but the transaction wasn't completely fed yet.
     This also means that the last event fed depends on following event(s) to
     be correctly applied.

     @return  false if the boundary parser is not inside a transaction.
              true if the boundary parser is inside a transaction.
  */
  inline bool is_inside_transaction()
  {
    return (current_parser_state != EVENT_PARSER_ERROR &&
            current_parser_state != EVENT_PARSER_NONE);
  }

  /**
     State if the transaction boundary parser is not inside a transaction.
     This "not inside a transaction" means that the parser was fed with an
     event that doesn't depend on following events.

     @return  false if the boundary parser is inside a transaction.
              true if the boundary parser is not inside a transaction.
  */
  inline bool is_not_inside_transaction()
  {
    return (current_parser_state == EVENT_PARSER_NONE);
  }

  /**
     State if the transaction boundary parser was fed with a sequence of events
     that the parser wasn't able to parse correctly.

     @return  false if the boundary parser is not in the error state.
              true if the boundary parser is in the error state.
  */
  inline bool is_error()
  {
    return (current_parser_state == EVENT_PARSER_ERROR);
  }

  /**
     Feed the transaction boundary parser with a Log_event of any type,
     serialized into a char* buffer.

     @param buf            Pointer to the event buffer.
     @param length         The size of the event buffer.
     @param fd_event       The description event of the master which logged
                           the event.
     @param throw_warnings If the function should throw warning messages while
                           updating the boundary parser state.
                           While initializing the Relay_log_info the
                           relay log is scanned backwards and this could
                           generate false errors. So, in this case, we
                           don't want to throw warnings.

     @return  false if the transaction boundary parser accepted the event.
              true if the transaction boundary parser didn't accepted the event.
  */
  bool feed_event(const char *buf, size_t length,
                  const Format_description_log_event *description_event,
                  bool throw_warnings);

private:
  enum enum_event_boundary_type {
    EVENT_BOUNDARY_TYPE_ERROR= -1,
    /* Gtid_log_event */
    EVENT_BOUNDARY_TYPE_GTID= 0,
    /* Query_log_event(BEGIN), Query_log_event(XA START) */
    EVENT_BOUNDARY_TYPE_BEGIN_TRX= 1,
    /* Xid, Query_log_event(COMMIT), Query_log_event(ROLLBACK), XA_Prepare_log_event */
    EVENT_BOUNDARY_TYPE_END_TRX= 2,
    /* Query_log_event(XA ROLLBACK) */
    EVENT_BOUNDARY_TYPE_END_XA_TRX= 3,
    /* User_var, Intvar and Rand */
    EVENT_BOUNDARY_TYPE_PRE_STATEMENT= 4,
    /*
      All other Query_log_events and all other DML events
      (Rows, Load_data, etc.)
    */
    EVENT_BOUNDARY_TYPE_STATEMENT= 5,
    /* Incident */
    EVENT_BOUNDARY_TYPE_INCIDENT= 6,
    /*
      All non DDL/DML events: Format_desc, Rotate,
      Previous_gtids, Stop, etc.
    */
    EVENT_BOUNDARY_TYPE_IGNORE= 7
  };

  /*
    Internal states for parsing a stream of events.

    DDL has the format:
      DDL-1: [GTID]
      DDL-2: [User] [Intvar] [Rand]
      DDL-3: Query

    DML has the format:
      DML-1: [GTID]
      DML-2: Query(BEGIN)
      DML-3: Statements
      DML-4: (Query(COMMIT) | Query([XA] ROLLBACK) | Xid | Xa_prepare)
  */
  enum enum_event_parser_state {
    /* NONE is set after DDL-3 or DML-4 */
    EVENT_PARSER_NONE,
    /* GTID is set after DDL-1 or DML-1 */
    EVENT_PARSER_GTID,
    /* DDL is set after DDL-2 */
    EVENT_PARSER_DDL,
    /* DML is set after DML-2 */
    EVENT_PARSER_DML,
    /* ERROR is set whenever the above pattern is not followed */
    EVENT_PARSER_ERROR
  };

  /**
     Current internal state of the event parser.
  */
  enum_event_parser_state current_parser_state;

  /**
     Parses an event based on the event parser logic.
  */
  static enum_event_boundary_type
  get_event_boundary_type(
    const char *buf, size_t length,
    const Format_description_log_event *description_event,
    bool throw_warnings);

  /**
     Set the boundary parser state based on the event parser logic.
  */
  bool update_state(enum_event_boundary_type event_boundary_type,
                    bool throw_warnings);
};

/**
  @} (End of group Replication)
*/

#endif /* RPL_TRX_BOUNDARY_PARSER_H */
