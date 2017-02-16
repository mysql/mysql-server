/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "sql_priv.h"
#include "sql_binlog.h"
#include "sql_parse.h"                          // check_global_access
#include "sql_acl.h"                            // *_ACL
#include "rpl_rli.h"
#include "base64.h"
#include "slave.h"                              // apply_event_and_update_pos
#include "log_event.h"                          // Format_description_log_event,
                                                // EVENT_LEN_OFFSET,
                                                // EVENT_TYPE_OFFSET,
                                                // FORMAT_DESCRIPTION_LOG_EVENT,
                                                // START_EVENT_V3,
                                                // Log_event_type,
                                                // Log_event
/**
  Execute a BINLOG statement.

  To execute the BINLOG command properly the server needs to know
  which format the BINLOG command's event is in.  Therefore, the first
  BINLOG statement seen must be a base64 encoding of the
  Format_description_log_event, as outputted by mysqlbinlog.  This
  Format_description_log_event is cached in
  rli->description_event_for_exec.

  @param thd Pointer to THD object for the client thread executing the
  statement.
*/

void mysql_client_binlog_statement(THD* thd)
{
  DBUG_ENTER("mysql_client_binlog_statement");
  DBUG_PRINT("info",("binlog base64: '%*s'",
                     (int) (thd->lex->comment.length < 2048 ?
                            thd->lex->comment.length : 2048),
                     thd->lex->comment.str));

  if (check_global_access(thd, SUPER_ACL))
    DBUG_VOID_RETURN;

  size_t coded_len= thd->lex->comment.length;
  if (!coded_len)
  {
    my_error(ER_SYNTAX_ERROR, MYF(0));
    DBUG_VOID_RETURN;
  }
  size_t decoded_len= base64_needed_decoded_length(coded_len);

  /*
    option_bits will be changed when applying the event. But we don't expect
    it be changed permanently after BINLOG statement, so backup it first.
    It will be restored at the end of this function.
  */
  ulonglong thd_options= thd->variables.option_bits;

  /*
    Allocation
  */

  /*
    If we do not have a Format_description_event, we create a dummy
    one here.  In this case, the first event we read must be a
    Format_description_event.
  */
  my_bool have_fd_event= TRUE;
  int err;
  Relay_log_info *rli;
  rli= thd->rli_fake;
  if (!rli)
  {
    rli= thd->rli_fake= new Relay_log_info(FALSE);
#ifdef HAVE_valgrind
    rli->is_fake= TRUE;
#endif
    have_fd_event= FALSE;
  }
  if (rli && !rli->relay_log.description_event_for_exec)
  {
    rli->relay_log.description_event_for_exec=
      new Format_description_log_event(4);
    have_fd_event= FALSE;
  }

  const char *error= 0;
  char *buf= (char *) my_malloc(decoded_len, MYF(MY_WME));
  Log_event *ev = 0;

  /*
    Out of memory check
  */
  if (!(rli &&
        rli->relay_log.description_event_for_exec &&
        buf))
  {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), 1);  /* needed 1 bytes */
    goto end;
  }

  rli->sql_thd= thd;
  rli->no_storage= TRUE;

  for (char const *strptr= thd->lex->comment.str ;
       strptr < thd->lex->comment.str + thd->lex->comment.length ; )
  {
    char const *endptr= 0;
    int bytes_decoded= base64_decode(strptr, coded_len, buf, &endptr);

#ifndef HAVE_valgrind
      /*
        This debug printout should not be used for valgrind builds
        since it will read from unassigned memory.
      */
    DBUG_PRINT("info",
               ("bytes_decoded: %d  strptr: 0x%lx  endptr: 0x%lx ('%c':%d)",
                bytes_decoded, (long) strptr, (long) endptr, *endptr,
                *endptr));
#endif

    if (bytes_decoded < 0)
    {
      my_error(ER_BASE64_DECODE_ERROR, MYF(0));
      goto end;
    }
    else if (bytes_decoded == 0)
      break; // If no bytes where read, the string contained only whitespace

    DBUG_ASSERT(bytes_decoded > 0);
    DBUG_ASSERT(endptr > strptr);
    coded_len-= endptr - strptr;
    strptr= endptr;

    /*
      Now we have one or more events stored in the buffer. The size of
      the buffer is computed based on how much base64-encoded data
      there were, so there should be ample space for the data (maybe
      even too much, since a statement can consist of a considerable
      number of events).

      TODO: Switch to use a stream-based base64 encoder/decoder in
      order to be able to read exactly what is necessary.
    */

    DBUG_PRINT("info",("binlog base64 decoded_len: %lu  bytes_decoded: %d",
                       (ulong) decoded_len, bytes_decoded));

    /*
      Now we start to read events of the buffer, until there are no
      more.
    */
    for (char *bufptr= buf ; bytes_decoded > 0 ; )
    {
      /*
        Checking that the first event in the buffer is not truncated.
      */
      ulong event_len;
      if (bytes_decoded < EVENT_LEN_OFFSET + 4 || 
          (event_len= uint4korr(bufptr + EVENT_LEN_OFFSET)) > 
           (uint) bytes_decoded)
      {
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }
      DBUG_PRINT("info", ("event_len=%lu, bytes_decoded=%d",
                          event_len, bytes_decoded));

      /*
        If we have not seen any Format_description_event, then we must
        see one; it is the only statement that can be read in base64
        without a prior Format_description_event.
      */
      if (!have_fd_event)
      {
        int type = (uchar)bufptr[EVENT_TYPE_OFFSET];
        if (type == FORMAT_DESCRIPTION_EVENT || type == START_EVENT_V3)
          have_fd_event= TRUE;
        else
        {
          my_error(ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT,
                   MYF(0), Log_event::get_type_str((Log_event_type)type));
          goto end;
        }
      }

      ev= Log_event::read_log_event(bufptr, event_len, &error,
                                    rli->relay_log.description_event_for_exec,
                                    0);

      DBUG_PRINT("info",("binlog base64 err=%s", error));
      if (!ev)
      {
        /*
          This could actually be an out-of-memory, but it is more likely
          causes by a bad statement
        */
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }

      bytes_decoded -= event_len;
      bufptr += event_len;

      DBUG_PRINT("info",("ev->get_type_code()=%d", ev->get_type_code()));
      ev->thd= thd;
      /*
        We go directly to the application phase, since we don't need
        to check if the event shall be skipped or not.

        Neither do we have to update the log positions, since that is
        not used at all: the rli_fake instance is used only for error
        reporting.
      */
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
      ulonglong save_skip_replication=
                        thd->variables.option_bits & OPTION_SKIP_REPLICATION;
      thd->variables.option_bits=
        (thd->variables.option_bits & ~OPTION_SKIP_REPLICATION) |
        (ev->flags & LOG_EVENT_SKIP_REPLICATION_F ?
         OPTION_SKIP_REPLICATION : 0);

      err= ev->apply_event(rli);

      thd->variables.option_bits=
        (thd->variables.option_bits & ~OPTION_SKIP_REPLICATION) |
        save_skip_replication;
#else
      err= 0;
#endif
      /*
        Format_description_log_event should not be deleted because it
        will be used to read info about the relay log's format; it
        will be deleted when the SQL thread does not need it,
        i.e. when this thread terminates.
      */
      if (ev->get_type_code() != FORMAT_DESCRIPTION_EVENT)
        delete ev; 
      ev= 0;
      if (err)
      {
        /*
          TODO: Maybe a better error message since the BINLOG statement
          now contains several events.
        */
        my_error(ER_UNKNOWN_ERROR, MYF(0));
        goto end;
      }
    }
  }


  DBUG_PRINT("info",("binlog base64 execution finished successfully"));
  my_ok(thd);

end:
  thd->variables.option_bits= thd_options;
  rli->slave_close_thread_tables(thd);
  my_free(buf);
  DBUG_VOID_RETURN;
}
