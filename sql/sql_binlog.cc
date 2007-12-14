/* Copyright (C) 2005-2006 MySQL AB

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

#include "mysql_priv.h"
#include "rpl_rli.h"
#include "base64.h"

/**
  Execute a BINLOG statement

  To execute the BINLOG command properly the server needs to know
  which format the BINLOG command's event is in.  Therefore, the first
  BINLOG statement seen must be a base64 encoding of the
  Format_description_log_event, as outputted by mysqlbinlog.  This
  Format_description_log_event is cached in
  rli->description_event_for_exec.
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

  /*
    Temporarily turn off send_ok, since different events handle this
    differently
  */
  my_bool nsok= thd->net.no_send_ok;
  thd->net.no_send_ok= TRUE;

  size_t coded_len= thd->lex->comment.length + 1;
  size_t decoded_len= base64_needed_decoded_length(coded_len);
  DBUG_ASSERT(coded_len > 0);

  /*
    Allocation
  */

  /*
    If we do not have a Format_description_event, we create a dummy
    one here.  In this case, the first event we read must be a
    Format_description_event.
  */
  my_bool have_fd_event= TRUE;
  if (!thd->rli_fake)
  {
    thd->rli_fake= new Relay_log_info;
    have_fd_event= FALSE;
  }
  if (thd->rli_fake && !thd->rli_fake->relay_log.description_event_for_exec)
  {
    thd->rli_fake->relay_log.description_event_for_exec=
      new Format_description_log_event(4);
    have_fd_event= FALSE;
  }

  const char *error= 0;
  char *buf= (char *) my_malloc(decoded_len, MYF(MY_WME));
  Log_event *ev = 0;

  /*
    Out of memory check
  */
  if (!(thd->rli_fake &&
        thd->rli_fake->relay_log.description_event_for_exec &&
        buf))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), 1);  /* needed 1 bytes */
    goto end;
  }

  thd->rli_fake->sql_thd= thd;
  thd->rli_fake->no_storage= TRUE;

  for (char const *strptr= thd->lex->comment.str ;
       strptr < thd->lex->comment.str + thd->lex->comment.length ; )
  {
    char const *endptr= 0;
    int bytes_decoded= base64_decode(strptr, coded_len, buf, &endptr);

#ifndef HAVE_purify
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
      ulong event_len= uint4korr(bufptr + EVENT_LEN_OFFSET);
      DBUG_PRINT("info", ("event_len=%lu, bytes_decoded=%d",
                          event_len, bytes_decoded));
      if (bytes_decoded < EVENT_LEN_OFFSET || (uint) bytes_decoded < event_len)
      {
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }

      /*
        If we have not seen any Format_description_event, then we must
        see one; it is the only statement that can be read in base64
        without a prior Format_description_event.
      */
      if (!have_fd_event)
      {
        if (bufptr[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT)
          have_fd_event= TRUE;
        else
        {
          my_error(ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT,
                   MYF(0),
                   Log_event::get_type_str(
                     (Log_event_type)bufptr[EVENT_TYPE_OFFSET]));
          goto end;
        }
      }

      ev= Log_event::read_log_event(bufptr, event_len, &error,
                                    thd->rli_fake->relay_log.
                                      description_event_for_exec);

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
#ifndef HAVE_purify
      /*
        This debug printout should not be used for valgrind builds
        since it will read from unassigned memory.
      */
      DBUG_PRINT("info",("bufptr+EVENT_TYPE_OFFSET: 0x%lx",
                         (long) (bufptr+EVENT_TYPE_OFFSET)));
      DBUG_PRINT("info", ("bytes_decoded: %d   bufptr: 0x%lx  buf[EVENT_LEN_OFFSET]: %lu",
                          bytes_decoded, (long) bufptr,
                          (ulong) uint4korr(bufptr+EVENT_LEN_OFFSET)));
#endif
      ev->thd= thd;
      /*
        We go directly to the application phase, since we don't need
        to check if the event shall be skipped or not.

        Neither do we have to update the log positions, since that is
        not used at all: the rli_fake instance is used only for error
        reporting.
      */
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
      if (apply_event_and_update_pos(ev, thd, thd->rli_fake, FALSE))
      {
        /*
          TODO: Maybe a better error message since the BINLOG statement
          now contains several events.
        */
        my_error(ER_UNKNOWN_ERROR, MYF(0), "Error executing BINLOG statement");
        goto end;
      }
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
    }
  }

  /*
    Restore setting of no_send_ok
  */
  thd->net.no_send_ok= nsok;

  DBUG_PRINT("info",("binlog base64 execution finished successfully"));
  send_ok(thd);

end:
  /*
    Restore setting of no_send_ok
  */
  thd->net.no_send_ok= nsok;

  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_VOID_RETURN;
}
