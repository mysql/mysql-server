/* Copyright (C) 2006 MySQL AB

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
#include "rpl_injector.h"

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

  begin_trans(m_thd);

  thd->set_current_stmt_binlog_row_based();
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

  my_free(the_memory, MYF(0));
}

int injector::transaction::commit()
{
   DBUG_ENTER("injector::transaction::commit()");
   m_thd->binlog_flush_pending_rows_event(true);
   end_trans(m_thd, COMMIT);
   DBUG_RETURN(0);
}

int injector::transaction::use_table(server_id_type sid, table tbl)
{
  DBUG_ENTER("injector::transaction::use_table");

  int error;

  if ((error= check_state(TABLE_STATE)))
    DBUG_RETURN(error);

  m_thd->set_server_id(sid);
  error= m_thd->binlog_write_table_map(tbl.get_table(),
                                       tbl.is_transactional());
  DBUG_RETURN(error);
}


int injector::transaction::write_row (server_id_type sid, table tbl, 
				      MY_BITMAP const* cols, size_t colcnt,
				      record_type record)
{
   DBUG_ENTER("injector::transaction::write_row(...)");

   if (int error= check_state(ROW_STATE))
     DBUG_RETURN(error);

   m_thd->set_server_id(sid);
   m_thd->binlog_write_row(tbl.get_table(), tbl.is_transactional(), 
                           cols, colcnt, record);
   DBUG_RETURN(0);
}


int injector::transaction::delete_row(server_id_type sid, table tbl,
				      MY_BITMAP const* cols, size_t colcnt,
				      record_type record)
{
   DBUG_ENTER("injector::transaction::delete_row(...)");

   if (int error= check_state(ROW_STATE))
     DBUG_RETURN(error);

   m_thd->set_server_id(sid);
   m_thd->binlog_delete_row(tbl.get_table(), tbl.is_transactional(), 
                            cols, colcnt, record);
   DBUG_RETURN(0);
}


int injector::transaction::update_row(server_id_type sid, table tbl, 
				      MY_BITMAP const* cols, size_t colcnt,
				      record_type before, record_type after)
{
   DBUG_ENTER("injector::transaction::update_row(...)");

   if (int error= check_state(ROW_STATE))
     DBUG_RETURN(error);

   m_thd->set_server_id(sid);
   m_thd->binlog_update_row(tbl.get_table(), tbl.is_transactional(),
		            cols, colcnt, before, after);
   DBUG_RETURN(0);
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
  mysql_bin_log.rotate_and_purge(RP_FORCE_ROTATE);
  return 0;
}

int injector::record_incident(THD *thd, Incident incident, LEX_STRING message)
{
  Incident_log_event ev(thd, incident, message);
  if (int error= mysql_bin_log.write(&ev))
    return error;
  mysql_bin_log.rotate_and_purge(RP_FORCE_ROTATE);
  return 0;
}
