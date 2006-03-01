/* Copyright (C) 2005 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include "base64.h"

/*
  Execute a BINLOG statement

  TODO: This currently assumes a MySQL 5.x binlog.
  When we'll have binlog with a different format, to execute the
  BINLOG command properly the server will need to know which format
  the BINLOG command's event is in.  mysqlbinlog should then send
  the Format_description_log_event of the binlog it reads and the
  server thread should cache this format into
  rli->description_event_for_exec.
*/

void mysql_client_binlog_statement(THD* thd)
{
  DBUG_PRINT("info",("binlog base64: '%*s'",
                     (thd->lex->comment.length < 2048 ?
                      thd->lex->comment.length : 2048),
                     thd->lex->comment.str));

  /*
    Temporarily turn off send_ok, since different events handle this
    differently
  */
  my_bool nsok= thd->net.no_send_ok;
  thd->net.no_send_ok= TRUE;

  const my_size_t coded_len= thd->lex->comment.length + 1;
  const my_size_t event_len= base64_needed_decoded_length(coded_len);
  DBUG_ASSERT(coded_len > 0);

  /*
    Allocation
  */
  if (!thd->rli_fake)
    thd->rli_fake= new RELAY_LOG_INFO;

  const Format_description_log_event *desc=
    new Format_description_log_event(4);

  const char *error= 0;
  char *buf= (char *) my_malloc(event_len, MYF(MY_WME));
  Log_event *ev = 0;
  int res;

  /*
    Out of memory check
  */
  if (!(thd->rli_fake && desc && buf))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), 1);  /* needed 1 bytes */
    goto end;
  }

  thd->rli_fake->sql_thd= thd;
  thd->rli_fake->no_storage= TRUE;

  res= base64_decode(thd->lex->comment.str, coded_len, buf);

  DBUG_PRINT("info",("binlog base64 decoded_len=%d, event_len=%d\n",
                     res, uint4korr(buf + EVENT_LEN_OFFSET)));
  /*
    Note that 'res' is the correct event length, 'event_len' was
    calculated based on the base64-string that possibly contained
    extra spaces, so it can be longer than the real event.
  */
  if (res < EVENT_LEN_OFFSET
      || (uint) res != uint4korr(buf+EVENT_LEN_OFFSET))
  {
    my_error(ER_SYNTAX_ERROR, MYF(0));
    goto end;
  }

  ev= Log_event::read_log_event(buf, res, &error, desc);

  DBUG_PRINT("info",("binlog base64 err=%s", error));
  if (!ev)
  {
    /*
      This could actually be an out-of-memory, but it is more
      likely causes by a bad statement
    */
    my_error(ER_SYNTAX_ERROR, MYF(0));
    goto end;
  }

  DBUG_PRINT("info",("ev->get_type_code()=%d", ev->get_type_code()));
  DBUG_PRINT("info",("buf+EVENT_TYPE_OFFSET=%d", buf+EVENT_TYPE_OFFSET));

  ev->thd= thd;
  if (ev->exec_event(thd->rli_fake))
  {
    my_error(ER_UNKNOWN_ERROR, MYF(0), "Error executing BINLOG statement");
    goto end;
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

  if (ev)
    delete ev;
  if (desc)
    delete desc;
  if (buf)
    my_free(buf, MYF(0));
}
