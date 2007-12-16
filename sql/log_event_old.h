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

  
class Old_rows_log_event
{
 public:
 
  virtual ~Old_rows_log_event() {}

 protected:
  
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

  int do_apply_event(Rows_log_event*,const Relay_log_info*);

  /*
    Primitive to prepare for a sequence of row executions.

    DESCRIPTION

      Before doing a sequence of do_prepare_row() and do_exec_row()
      calls, this member function should be called to prepare for the
      entire sequence. Typically, this member function will allocate
      space for any buffers that are needed for the two member
      functions mentioned above.

    RETURN VALUE

      The member function will return 0 if all went OK, or a non-zero
      error code otherwise.
  */
  virtual int do_before_row_operations(TABLE *table) = 0;

  /*
    Primitive to clean up after a sequence of row executions.

    DESCRIPTION
    
      After doing a sequence of do_prepare_row() and do_exec_row(),
      this member function should be called to clean up and release
      any allocated buffers.
  */
  virtual int do_after_row_operations(TABLE *table, int error) = 0;

  /*
    Primitive to prepare for handling one row in a row-level event.
    
    DESCRIPTION 

      The member function prepares for execution of operations needed for one
      row in a row-level event by reading up data from the buffer containing
      the row. No specific interpretation of the data is normally done here,
      since SQL thread specific data is not available: that data is made
      available for the do_exec function.

      A pointer to the start of the next row, or NULL if the preparation
      failed. Currently, preparation cannot fail, but don't rely on this
      behavior. 

    RETURN VALUE
      Error code, if something went wrong, 0 otherwise.
   */
  virtual int do_prepare_row(THD*, Relay_log_info const*, TABLE*,
                             uchar const *row_start,
                             uchar const **row_end) = 0;

  /*
    Primitive to do the actual execution necessary for a row.

    DESCRIPTION
      The member function will do the actual execution needed to handle a row.

    RETURN VALUE
      0 if execution succeeded, 1 if execution failed.
      
  */
  virtual int do_exec_row(TABLE *table) = 0;

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */
};


class Write_rows_log_event_old 
 : public Write_rows_log_event, public Old_rows_log_event
{

public:
  enum
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = PRE_GA_WRITE_ROWS_EVENT
  };

#if !defined(MYSQL_CLIENT)
  Write_rows_log_event_old(THD *thd_arg, TABLE *table, ulong table_id,
                           MY_BITMAP const *cols, bool is_transactional)
    : Write_rows_log_event(thd_arg, table, table_id, cols, is_transactional)
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
  // use old definition of do_apply_event()
  virtual int do_apply_event(const Relay_log_info *rli)
  { return Old_rows_log_event::do_apply_event(this,rli); }

  // primitives for old version of do_apply_event()
  virtual int do_before_row_operations(TABLE *table);
  virtual int do_after_row_operations(TABLE *table, int error);
  virtual int do_prepare_row(THD*, Relay_log_info const*, TABLE*,
                             uchar const *row_start, uchar const **row_end);
  virtual int do_exec_row(TABLE *table);

#endif
};


class Update_rows_log_event_old 
  : public Update_rows_log_event, public Old_rows_log_event
{
  uchar *m_after_image, *m_memory;
  
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = PRE_GA_UPDATE_ROWS_EVENT
  };

#if !defined(MYSQL_CLIENT)
  Update_rows_log_event_old(THD *thd_arg, TABLE *table, ulong table_id,
                           MY_BITMAP const *cols, bool is_transactional)
    : Update_rows_log_event(thd_arg, table, table_id, cols, is_transactional),
      m_after_image(NULL), m_memory(NULL)
  {
  }
#endif
#if defined(HAVE_REPLICATION)
  Update_rows_log_event_old(const char *buf, uint event_len, 
                            const Format_description_log_event *descr)
    : Update_rows_log_event(buf, event_len, descr),
      m_after_image(NULL), m_memory(NULL)
  {
  }
#endif

private:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  // use old definition of do_apply_event()
  virtual int do_apply_event(const Relay_log_info *rli)
  { return Old_rows_log_event::do_apply_event(this,rli); }

  // primitives for old version of do_apply_event()
  virtual int do_before_row_operations(TABLE *table);
  virtual int do_after_row_operations(TABLE *table, int error);
  virtual int do_prepare_row(THD*, Relay_log_info const*, TABLE*,
                             uchar const *row_start, uchar const **row_end);
  virtual int do_exec_row(TABLE *table);
#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */
};


class Delete_rows_log_event_old 
  : public Delete_rows_log_event, public Old_rows_log_event
{
  uchar *m_after_image, *m_memory;
 
public:
  enum 
  {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = PRE_GA_DELETE_ROWS_EVENT
  };

#if !defined(MYSQL_CLIENT)
  Delete_rows_log_event_old(THD *thd_arg, TABLE *table, ulong table_id,
                           MY_BITMAP const *cols, bool is_transactional)
    : Delete_rows_log_event(thd_arg, table, table_id, cols, is_transactional),
      m_after_image(NULL), m_memory(NULL)
  {
  }
#endif
#if defined(HAVE_REPLICATION)
  Delete_rows_log_event_old(const char *buf, uint event_len,
                            const Format_description_log_event *descr)
    : Delete_rows_log_event(buf, event_len, descr),
      m_after_image(NULL), m_memory(NULL)
  {
  }
#endif

private:
  virtual Log_event_type get_type_code() { return (Log_event_type)TYPE_CODE; }

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  // use old definition of do_apply_event()
  virtual int do_apply_event(const Relay_log_info *rli)
  { return Old_rows_log_event::do_apply_event(this,rli); }

  // primitives for old version of do_apply_event()
  virtual int do_before_row_operations(TABLE *table);
  virtual int do_after_row_operations(TABLE *table, int error);
  virtual int do_prepare_row(THD*, Relay_log_info const*, TABLE*,
                             uchar const *row_start, uchar const **row_end);
  virtual int do_exec_row(TABLE *table);
#endif
};


#endif

