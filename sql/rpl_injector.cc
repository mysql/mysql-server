/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

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
#include "unireg.h"                             // REQUIRED by later includes
#include "rpl_injector.h"
#include "transaction.h"
#include "sql_parse.h"                          // begin_trans, end_trans, COMMIT
#include "sql_base.h"                           // close_thread_tables
#include "log_event.h"                          // Incident_log_event

/*
  injector::transaction - member definitions
*/

/* inline since it's called below */
inline
injector::transaction::transaction(MYSQL_BIN_LOG *log, THD *thd)
  : m_state(START_STATE), m_thd(thd)
{
  /* 
     Default initialization of m_start_pos (which initializes it to garbage).
     We need to fill it in using the code below.
  */
  LOG_INFO log_info;
  log->get_current_log(&log_info);
  /* !!! binlog_pos does not follow RAII !!! */
  m_start_pos.m_file_name= my_strdup(log_info.log_file_name, MYF(0));
  m_start_pos.m_file_pos= log_info.pos;

  trans_begin(m_thd);
}

injector::transaction::~transaction()
{
  if (!good())
    return;

  /* Needed since my_free expects a 'char*' (instead of 'void*'). */
  char* const the_memory= const_cast<char*>(m_start_pos.m_file_name);

  /*
    We set the first character to null just to give all the copies of the
    start position a (minimal) chance of seening that the memory is lost.
    All assuming the my_free does not step over the memory, of course.
  */
  *the_memory= '\0';

  my_free(the_memory);
}

/**
   @retval 0 transaction committed
   @retval 1 transaction rolled back
 */
int injector::transaction::commit()
{
   DBUG_ENTER("injector::transaction::commit()");
   int error= m_thd->binlog_flush_pending_rows_event(true);
   /*
     Cluster replication does not preserve statement or
     transaction boundaries of the master.  Instead, a new
     transaction on replication slave is started when a new GCI
     (global checkpoint identifier) is issued, and is committed
     when the last event of the check point has been received and
     processed. This ensures consistency of each cluster in
     cluster replication, and there is no requirement for stronger
     consistency: MySQL replication is asynchronous with other
     engines as well.

     A practical consequence of that is that row level replication
     stream passed through the injector thread never contains
     COMMIT events.
     Here we should preserve the server invariant that there is no
     outstanding statement transaction when the normal transaction
     is committed by committing the statement transaction
     explicitly.
   */
   trans_commit_stmt(m_thd);
   if (!trans_commit(m_thd))
   {
     close_thread_tables(m_thd);
     m_thd->mdl_context.release_transactional_locks();
   }
   DBUG_RETURN(error);
}


int injector::transaction::use_table(server_id_type sid, table tbl)
{
  DBUG_ENTER("injector::transaction::use_table");

  int error;

  if ((error= check_state(TABLE_STATE)))
    DBUG_RETURN(error);

  server_id_type save_id= m_thd->server_id;
  m_thd->set_server_id(sid);
  error= m_thd->binlog_write_table_map(tbl.get_table(),
                                       tbl.is_transactional());
  m_thd->set_server_id(save_id);
  DBUG_RETURN(error);
}


int injector::transaction::write_row (server_id_type sid, table tbl, 
				      MY_BITMAP const* cols, size_t colcnt,
				      record_type record)
{
   DBUG_ENTER("injector::transaction::write_row(...)");

   int error= check_state(ROW_STATE);
   if (error)
     DBUG_RETURN(error);

   server_id_type save_id= m_thd->server_id;
   m_thd->set_server_id(sid);
   error= m_thd->binlog_write_row(tbl.get_table(), tbl.is_transactional(), 
                                  cols, colcnt, record);
   m_thd->set_server_id(save_id);
   DBUG_RETURN(error);
}


int injector::transaction::delete_row(server_id_type sid, table tbl,
				      MY_BITMAP const* cols, size_t colcnt,
				      record_type record)
{
   DBUG_ENTER("injector::transaction::delete_row(...)");

   int error= check_state(ROW_STATE);
   if (error)
     DBUG_RETURN(error);

   server_id_type save_id= m_thd->server_id;
   m_thd->set_server_id(sid);
   error= m_thd->binlog_delete_row(tbl.get_table(), tbl.is_transactional(), 
                                   cols, colcnt, record);
   m_thd->set_server_id(save_id);
   DBUG_RETURN(error);
}


int injector::transaction::update_row(server_id_type sid, table tbl, 
				      MY_BITMAP const* cols, size_t colcnt,
				      record_type before, record_type after)
{
   DBUG_ENTER("injector::transaction::update_row(...)");

   int error= check_state(ROW_STATE);
   if (error)
     DBUG_RETURN(error);

   server_id_type save_id= m_thd->server_id;
   m_thd->set_server_id(sid);
   error= m_thd->binlog_update_row(tbl.get_table(), tbl.is_transactional(),
                                   cols, colcnt, before, after);
   m_thd->set_server_id(save_id);
   DBUG_RETURN(error);
}


injector::transaction::binlog_pos injector::transaction::start_pos() const
{
   return m_start_pos;			
}


/*
  injector - member definitions
*/

/* This constructor is called below */
inline injector::injector()
{
}

static injector *s_injector= 0;
injector *injector::instance()
{
  if (s_injector == 0)
    s_injector= new injector;
  /* "There can be only one [instance]" */
  return s_injector;
}

void injector::free_instance()
{
  injector *inj = s_injector;

  if (inj != 0)
  {
    s_injector= 0;
    delete inj;
  }
}


injector::transaction injector::new_trans(THD *thd)
{
   DBUG_ENTER("injector::new_trans(THD*)");
   /*
     Currently, there is no alternative to using 'mysql_bin_log' since that
     is hardcoded into the way the handler is using the binary log.
   */
   DBUG_RETURN(transaction(&mysql_bin_log, thd));
}

void injector::new_trans(THD *thd, injector::transaction *ptr)
{
   DBUG_ENTER("injector::new_trans(THD *, transaction *)");
   /*
     Currently, there is no alternative to using 'mysql_bin_log' since that
     is hardcoded into the way the handler is using the binary log. 
   */
   transaction trans(&mysql_bin_log, thd);
   ptr->swap(trans);

   DBUG_VOID_RETURN;
}

int injector::record_incident(THD *thd, Incident incident)
{
  Incident_log_event ev(thd, incident);
  if (int error= mysql_bin_log.write(&ev))
    return error;
  return mysql_bin_log.rotate_and_purge(true);
}

int injector::record_incident(THD *thd, Incident incident, LEX_STRING const message)
{
  Incident_log_event ev(thd, incident, message);
  if (int error= mysql_bin_log.write(&ev))
    return error;
  return mysql_bin_log.rotate_and_purge(true);
}
