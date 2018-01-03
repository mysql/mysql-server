/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/rpl_trx_boundary_parser.h"

#include <string.h>
#include <sys/types.h>

#include "binlog_event.h"
#include "m_string.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "mysqld_error.h"
#include "sql/log.h"
#include "sql/log_event.h" // Log_event


#ifndef DBUG_OFF
/* Event parser state names */
static const char *event_parser_state_names[]= {
  "None",
  "GTID",
  "DDL",
  "DML",
  "Error"
};
#endif

/*
  -----------------------------------------
  Transaction_boundary_parser class methods
  -----------------------------------------
*/

/**
   Reset the transaction boundary parser.

   This method initialize the boundary parser state.
*/
void Transaction_boundary_parser::reset()
{
  DBUG_ENTER("Transaction_boundary_parser::reset");
  DBUG_PRINT("info", ("transaction boundary parser is changing state "
                      "from '%s' to '%s'",
                      event_parser_state_names[current_parser_state],
                      event_parser_state_names[EVENT_PARSER_NONE]));
  current_parser_state= EVENT_PARSER_NONE;
  last_parser_state= EVENT_PARSER_NONE;
  DBUG_VOID_RETURN;
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
                         generate false warnings. So, in this case, we
                         don't want to throw warnings.

   @return  false if the transaction boundary parser accepted the event.
            true if the transaction boundary parser didn't accepted the event.
*/
bool Transaction_boundary_parser::feed_event(const char *buf, size_t length,
                                             const Format_description_log_event
                                             *fd_event,
                                             bool throw_warnings)
{
  DBUG_ENTER("Transaction_boundary_parser::feed_event");
  enum_event_boundary_type event_boundary_type=
    get_event_boundary_type(buf, length, fd_event, throw_warnings);
  DBUG_RETURN(update_state(event_boundary_type, throw_warnings));
}

/**
   Get the boundary type for a given Log_event of any type,
   serialized into a char* buffer, based on event parser logic.

   @param buf               Pointer to the event buffer.
   @param length            The size of the event buffer.
   @param fd_event          The description event of the master which logged
                            the event.
   @param throw_warnings    If the function should throw warnings getting the
                            event boundary type.
                            Please see comments on this at feed_event().

   @return  the transaction boundary type of the event.
*/
Transaction_boundary_parser::enum_event_boundary_type
Transaction_boundary_parser::get_event_boundary_type(
  const char *buf, size_t length, const Format_description_log_event *fd_event,
  bool throw_warnings)
{
  DBUG_ENTER("Transaction_boundary_parser::get_event_boundary_type");

  Log_event_type event_type;
  enum_event_boundary_type boundary_type= EVENT_BOUNDARY_TYPE_ERROR;
  uint header_size= fd_event->common_header_len;

  /* Error if the event content is smaller than header size for the format */
  if (length < header_size)
    goto end;

  event_type= (Log_event_type)buf[EVENT_TYPE_OFFSET];
  DBUG_PRINT("info",("trx boundary parser was fed with an event of type %s",
                     Log_event::get_type_str(event_type)));

  switch (event_type)
  {
    case binary_log::GTID_LOG_EVENT:
    case binary_log::ANONYMOUS_GTID_LOG_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_GTID;
      break;

    /*
      There are four types of queries that we have to deal with: BEGIN, COMMIT,
      ROLLBACK and the rest.
    */
    case binary_log::QUERY_EVENT:
    {
      char *query= NULL;
      size_t qlen= 0;
      /* Get the query to let us check for BEGIN/COMMIT/ROLLBACK */
      qlen= Query_log_event::get_query(buf, length, fd_event, &query);
      if (qlen == 0)
      {
        DBUG_ASSERT(query == NULL);
        boundary_type= EVENT_BOUNDARY_TYPE_ERROR;
        break;
      }

      /*
        BEGIN is always the begin of a DML transaction.
      */
      if (!strncmp(query, "BEGIN", qlen) ||
          !strncmp(query, STRING_WITH_LEN("XA START")))
        boundary_type= EVENT_BOUNDARY_TYPE_BEGIN_TRX;
      /*
        COMMIT and ROLLBACK are always the end of a transaction.
      */
      else if (!strncmp(query, "COMMIT", qlen) ||
               (!native_strncasecmp(query, STRING_WITH_LEN("ROLLBACK")) &&
                native_strncasecmp(query, STRING_WITH_LEN("ROLLBACK TO "))))
        boundary_type= EVENT_BOUNDARY_TYPE_END_TRX;
      /*
        XA ROLLBACK is always the end of a XA transaction.
      */
      else if (!native_strncasecmp(query, STRING_WITH_LEN("XA ROLLBACK")))
        boundary_type= EVENT_BOUNDARY_TYPE_END_XA_TRX;
      /*
        If the query is not (BEGIN | XA START | COMMIT | [XA] ROLLBACK), it can
        be considered an ordinary statement.
      */
      else
        boundary_type= EVENT_BOUNDARY_TYPE_STATEMENT;

      break;
    }

    /*
      XID events are always the end of a transaction.
    */
    case binary_log::XID_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_END_TRX;
      break;
    /*
      XA_prepare event ends XA-prepared group of events (prepared XA transaction).
    */
    case binary_log::XA_PREPARE_LOG_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_END_TRX;
      break;

    /*
      Intvar, Rand and User_var events are always considered as pre-statements.
    */
    case binary_log::INTVAR_EVENT:
    case binary_log::RAND_EVENT:
    case binary_log::USER_VAR_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_PRE_STATEMENT;
      break;

    /*
      The following event types are always considered as statements
      because they will always be wrapped between BEGIN/COMMIT.
    */
    case binary_log::EXECUTE_LOAD_QUERY_EVENT:
    case binary_log::TABLE_MAP_EVENT:
    case binary_log::APPEND_BLOCK_EVENT:
    case binary_log::BEGIN_LOAD_QUERY_EVENT:
    case binary_log::ROWS_QUERY_LOG_EVENT:
    case binary_log::WRITE_ROWS_EVENT:
    case binary_log::UPDATE_ROWS_EVENT:
    case binary_log::DELETE_ROWS_EVENT:
    case binary_log::WRITE_ROWS_EVENT_V1:
    case binary_log::UPDATE_ROWS_EVENT_V1:
    case binary_log::DELETE_ROWS_EVENT_V1:
    case binary_log::VIEW_CHANGE_EVENT:
    case binary_log::PARTIAL_UPDATE_ROWS_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_STATEMENT;
      break;

    /*
      Incident events have their own boundary type.
    */
    case binary_log::INCIDENT_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_INCIDENT;
      break;

    /*
      Rotate, Format_description and Heartbeat should be ignored.
      Also, any other kind of event not listed in the "cases" above
      will be ignored.
    */
    case binary_log::ROTATE_EVENT:
    case binary_log::FORMAT_DESCRIPTION_EVENT:
    case binary_log::HEARTBEAT_LOG_EVENT:
    case binary_log::PREVIOUS_GTIDS_LOG_EVENT:
    case binary_log::STOP_EVENT:
    case binary_log::SLAVE_EVENT:
    case binary_log::DELETE_FILE_EVENT:
    case binary_log::TRANSACTION_CONTEXT_EVENT:
      boundary_type= EVENT_BOUNDARY_TYPE_IGNORE;
      break;

    /*
      If the event is none of above supported event types, this is probably
      an event type unsupported by this server version. So, we must check if
      this event is ignorable or not.
    */
    default:
      if (uint2korr(buf + FLAGS_OFFSET) & LOG_EVENT_IGNORABLE_F)
        boundary_type= EVENT_BOUNDARY_TYPE_IGNORE;
      else
      {
        boundary_type= EVENT_BOUNDARY_TYPE_ERROR;
        if (throw_warnings)
          LogErr(WARNING_LEVEL,
                 ER_RPL_UNSUPPORTED_UNIGNORABLE_EVENT_IN_STREAM);
      }
  } /* End of switch(event_type) */

end:
  DBUG_RETURN(boundary_type);
}

/**
   Update the boundary parser state based on a given boundary type.

   @param event_boundary_type The event boundary type of the event used to
                              fed the boundary parser.
   @param throw_warnings      If the function should throw warnings while
                              updating the boundary parser state.
                              Please see comments on this at feed_event().

   @return  false State updated successfully.
            true  There was an error updating the state.
*/
bool Transaction_boundary_parser::update_state(
  enum_event_boundary_type event_boundary_type, bool throw_warnings)
{
  DBUG_ENTER("Transaction_boundary_parser::update_state");

  enum_event_parser_state new_parser_state= EVENT_PARSER_NONE;

  bool error= false;

  switch (event_boundary_type)
  {
  /*
    GTIDs are always the start of a transaction stream.
  */
  case EVENT_BOUNDARY_TYPE_GTID:
    /* In any case, we will update the state to GTID */
    new_parser_state= EVENT_PARSER_GTID;
    /* The following switch is mostly to differentiate the warning messages */
    switch(current_parser_state) {
    case EVENT_PARSER_GTID:
    case EVENT_PARSER_DDL:
    case EVENT_PARSER_DML:
      if (throw_warnings)
        LogErr(WARNING_LEVEL, ER_RPL_GTID_LOG_EVENT_IN_STREAM,
          current_parser_state == EVENT_PARSER_GTID ?
            "after a GTID_LOG_EVENT or an ANONYMOUS_GTID_LOG_EVENT" :
            current_parser_state == EVENT_PARSER_DDL ?
              "in the middle of a DDL" :
              "in the middle of a DML"); /* EVENT_PARSER_DML */
      error= true;
      break;
    case EVENT_PARSER_ERROR: /* we probably threw a warning before */
      error= true;
      /* FALL THROUGH */
    case EVENT_PARSER_NONE:
      break;
    }
    break;

  /*
    There are four types of queries that we have to deal with: BEGIN, COMMIT,
    ROLLBACK and the rest.
  */
  case EVENT_BOUNDARY_TYPE_BEGIN_TRX:
    /* In any case, we will update the state to DML */
    new_parser_state= EVENT_PARSER_DML;
    /* The following switch is mostly to differentiate the warning messages */
    switch(current_parser_state) {
    case EVENT_PARSER_DDL:
    case EVENT_PARSER_DML:
      if (throw_warnings)
        LogErr(WARNING_LEVEL, ER_RPL_UNEXPECTED_BEGIN_IN_STREAM,
          current_parser_state == EVENT_PARSER_DDL ? "DDL" : "DML");
      error= true;
      break;
    case EVENT_PARSER_ERROR: /* we probably threw a warning before */
      error= true;
      /* FALL THROUGH */
    case EVENT_PARSER_NONE:
    case EVENT_PARSER_GTID:
      break;
    }
    break;

  case EVENT_BOUNDARY_TYPE_END_TRX:
    /* In any case, we will update the state to NONE */
    new_parser_state= EVENT_PARSER_NONE;
    /* The following switch is mostly to differentiate the warning messages */
    switch(current_parser_state) {
    case EVENT_PARSER_NONE:
    case EVENT_PARSER_GTID:
    case EVENT_PARSER_DDL:
      if (throw_warnings)
        LogErr(WARNING_LEVEL,
               ER_RPL_UNEXPECTED_COMMIT_ROLLBACK_OR_XID_LOG_EVENT_IN_STREAM,
          current_parser_state == EVENT_PARSER_NONE ? "outside a transaction" :
          current_parser_state == EVENT_PARSER_GTID ? "after a GTID_LOG_EVENT" :
          "in the middle of a DDL"); /* EVENT_PARSER_DDL */
      error= true;
      break;
    case EVENT_PARSER_ERROR: /* we probably threw a warning before */
      error= true;
      /* FALL THROUGH */
    case EVENT_PARSER_DML:
      break;
    }
    break;

  case EVENT_BOUNDARY_TYPE_END_XA_TRX:
    /* In any case, we will update the state to NONE */
    new_parser_state= EVENT_PARSER_NONE;
    /* The following switch is mostly to differentiate the warning messages */
    switch(current_parser_state) {
      case EVENT_PARSER_NONE:
      case EVENT_PARSER_DDL:
        if (throw_warnings)
          LogErr(WARNING_LEVEL, ER_RPL_UNEXPECTED_XA_ROLLBACK_IN_STREAM,
              current_parser_state == EVENT_PARSER_NONE ? "outside a transaction" :
              "in the middle of a DDL"); /* EVENT_PARSER_DDL */
        error= true;
        break;
      case EVENT_PARSER_ERROR: /* we probably threw a warning before */
        error= true;
        /* FALL THROUGH */
      case EVENT_PARSER_DML:
      /* XA ROLLBACK can appear after a GTID event */
      case EVENT_PARSER_GTID:
        break;
    }
    break;

  case EVENT_BOUNDARY_TYPE_STATEMENT:
    switch(current_parser_state) {
    case EVENT_PARSER_NONE:
      new_parser_state= EVENT_PARSER_NONE;
      break;
    case EVENT_PARSER_GTID:
    case EVENT_PARSER_DDL:
      new_parser_state= EVENT_PARSER_NONE;
      break;
    case EVENT_PARSER_DML:
      new_parser_state= current_parser_state;
      break;
    case EVENT_PARSER_ERROR: /* we probably threw a warning before */
      error= true;
      break;
    }
    break;

  /*
    Intvar, Rand and User_var events might be inside of a transaction stream if
    any Intvar, Rand and User_var was fed before, if BEGIN was fed before or if
    GTID was fed before.
    In the case of no GTID, no BEGIN and no previous Intvar, Rand or User_var
    it will be considered the start of a transaction stream.
  */
  case EVENT_BOUNDARY_TYPE_PRE_STATEMENT:
    switch(current_parser_state) {
    case EVENT_PARSER_NONE:
    case EVENT_PARSER_GTID:
      new_parser_state= EVENT_PARSER_DDL;
      break;
    case EVENT_PARSER_DDL:
    case EVENT_PARSER_DML:
      new_parser_state= current_parser_state;
      break;
    case EVENT_PARSER_ERROR: /* we probably threw a warning before */
      error= true;
      break;
    }
    break;

  /*
    Incident events can happen without a GTID (before BUG#19594845 fix) or
    with its own GTID in order to be skipped. In any case, it should always
    mark "the end" of a transaction.
  */
  case EVENT_BOUNDARY_TYPE_INCIDENT:
    /* In any case, we will update the state to NONE */
    new_parser_state= EVENT_PARSER_NONE;
    break;

  /*
    Rotate, Format_description and Heartbeat should be ignored.
    The rotate might be fake, like when the IO thread receives from dump thread
    Previous_gtid and Heartbeat events due to reconnection/auto positioning.
  */
  case EVENT_BOUNDARY_TYPE_IGNORE:
    new_parser_state= current_parser_state;
    break;

  case EVENT_BOUNDARY_TYPE_ERROR:
    error= true;
    new_parser_state= EVENT_PARSER_ERROR;
    break;
  }

  DBUG_PRINT("info", ("transaction boundary parser is changing state "
                      "from '%s' to '%s'",
                      event_parser_state_names[current_parser_state],
                      event_parser_state_names[new_parser_state]));

  last_parser_state= current_parser_state;
  current_parser_state= new_parser_state;

  DBUG_RETURN(error);
}
