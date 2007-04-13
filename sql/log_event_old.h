/* Copyright 2007 MySQL AB. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef LOG_EVENT_OLD_H
#define LOG_EVENT_OLD_H

/*
  Need to include this file at the proper position of log_event.h
 */


class Write_rows_log_event_old : public Write_rows_log_event
{
public:
  enum
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = PRE_GA_WRITE_ROWS_EVENT
  };

#if !defined(MYSQL_CLIENT)
  Write_rows_log_event_old(THD *thd, TABLE *table, ulong table_id,
                           MY_BITMAP const *cols, bool is_transactional)
    : Write_rows_log_event(thd, table, table_id, cols, is_transactional)
  {
  }
#endif
#if defined(HAVE_REPLICATION)
  Write_rows_log_event_old(const char *buf, uint event_len,
                           const Format_description_log_event *descr)
    : Write_rows_log_event(buf, event_len, descr)
  {
  }
#endif

private:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  virtual int do_prepare_row(THD*, RELAY_LOG_INFO const*, TABLE*,
                             char const *row_start, char const **row_end);
#endif
};


class Update_rows_log_event_old : public Update_rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = PRE_GA_UPDATE_ROWS_EVENT
  };

#if !defined(MYSQL_CLIENT)
  Update_rows_log_event_old(THD *thd, TABLE *table, ulong table_id,
                           MY_BITMAP const *cols, bool is_transactional)
    : Update_rows_log_event(thd, table, table_id, cols, is_transactional)
  {
  }
#endif
#if defined(HAVE_REPLICATION)
  Update_rows_log_event_old(const char *buf, uint event_len, 
                            const Format_description_log_event *descr)
    : Update_rows_log_event(buf, event_len, descr)
  {
  }
#endif

private:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  virtual int do_prepare_row(THD*, RELAY_LOG_INFO const*, TABLE*,
                             char const *row_start, char const **row_end);
#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */
};


class Delete_rows_log_event_old : public Delete_rows_log_event
{
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = PRE_GA_DELETE_ROWS_EVENT
  };

#if !defined(MYSQL_CLIENT)
  Delete_rows_log_event_old(THD *thd, TABLE *table, ulong table_id,
                           MY_BITMAP const *cols, bool is_transactional)
    : Delete_rows_log_event(thd, table, table_id, cols, is_transactional)
  {
  }
#endif
#if defined(HAVE_REPLICATION)
  Delete_rows_log_event_old(const char *buf, uint event_len,
                            const Format_description_log_event *descr)
    : Delete_rows_log_event(buf, event_len, descr)
  {
  }
#endif

private:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  virtual int do_prepare_row(THD*, RELAY_LOG_INFO const*, TABLE*,
                             char const *row_start, char const **row_end);
#endif
};


#endif

