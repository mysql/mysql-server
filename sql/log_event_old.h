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
  virtual int do_prepare_row(THD*, RELAY_LOG_INFO*, TABLE*,
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
  virtual int do_prepare_row(THD*, RELAY_LOG_INFO*, TABLE*,
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
  virtual int do_prepare_row(THD*, RELAY_LOG_INFO*, TABLE*,
                             char const *row_start, char const **row_end);
#endif
};


#endif

