/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "rpl_gtid_persist.h"

#include "debug_sync.h"       // debug_sync_set_action
#include "log.h"              // sql_print_error
#include "replication.h"      // THD_ENTER_COND
#include "sql_base.h"         // MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK
#include "sql_parse.h"        // mysql_reset_thd_for_next_command

using std::list;
using std::string;


my_thread_handle compress_thread_id;
static bool terminate_compress_thread= false;
static bool should_compress= false;
const LEX_STRING Gtid_table_access_context::TABLE_NAME= {C_STRING_WITH_LEN("gtid_executed")};
const LEX_STRING Gtid_table_access_context::DB_NAME= {C_STRING_WITH_LEN("mysql")};

/**
  A derived from THD::Attachable_trx class allows updates in
  the attachable transaction. Callers of the class methods must
  make sure the attachable_rw won't cause deadlock with the main transaction.
  The destructor does not invoke ha_commit_{stmt,trans} nor ha_rollback_trans
  on purpose.
  Burden to terminate the read-write instance also lies on the caller!
  In order to use this interface it *MUST* prove that no side effect to
  the global transaction state can be inflicted by a chosen method.
*/

class THD::Attachable_trx_rw : public THD::Attachable_trx
{
public:
  bool is_read_only() const { return false; }
  Attachable_trx_rw(THD *thd) : THD::Attachable_trx(thd)
  {
    m_thd->tx_read_only= false;
    m_thd->lex->sql_command= SQLCOM_END;
    m_xa_state_saved= m_thd->get_transaction()->xid_state()->get_state();
    thd->get_transaction()->xid_state()->set_state(XID_STATE::XA_NOTR);
  }
  ~Attachable_trx_rw()
  {
    /* The attachable transaction has been already committed */
    DBUG_ASSERT(!m_thd->get_transaction()->is_active(Transaction_ctx::STMT)
                && !m_thd->get_transaction()->is_active(Transaction_ctx::SESSION));

    m_thd->get_transaction()->xid_state()->set_state(m_xa_state_saved);
    m_thd->tx_read_only= true;
  }

private:
  XID_STATE::xa_states m_xa_state_saved;
  Attachable_trx_rw(const Attachable_trx_rw &);
  Attachable_trx_rw &operator =(const Attachable_trx_rw &);
};


bool THD::is_attachable_rw_transaction_active() const
{
  return m_attachable_trx != NULL && !m_attachable_trx->is_read_only();
}


void THD::begin_attachable_rw_transaction()
{
  DBUG_ASSERT(!m_attachable_trx);

  m_attachable_trx= new Attachable_trx_rw(this);
}


/**
  Initialize a new THD.

  @param p_thd  Pointer to pointer to thread structure
*/
static void init_thd(THD **p_thd)
{
  DBUG_ENTER("init_thd");
  THD *thd= *p_thd;
  thd->thread_stack= reinterpret_cast<char *>(p_thd);
  thd->set_command(COM_DAEMON);
  thd->security_context()->skip_grants();
  thd->system_thread= SYSTEM_THREAD_COMPRESS_GTID_TABLE;
  thd->store_globals();
  thd->set_time();
  DBUG_VOID_RETURN;
}


/**
  Release resourses for the thread and restores the
  system_thread information.

  @param thd Thread requesting to be destroyed
*/
static void deinit_thd(THD *thd)
{
  DBUG_ENTER("deinit_thd");
  thd->release_resources();
  thd->restore_globals();
  delete thd;
  my_thread_set_THR_THD(NULL);
  DBUG_VOID_RETURN;
}


THD *Gtid_table_access_context::create_thd()
{
  THD *thd= System_table_access::create_thd();
  thd->system_thread= SYSTEM_THREAD_COMPRESS_GTID_TABLE;
  /*
    This is equivalent to a new "statement". For that reason, we call
    both lex_start() and mysql_reset_thd_for_next_command.
  */
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);

  return(thd);
}


void Gtid_table_access_context::before_open(THD* thd)
{
  DBUG_ENTER("Gtid_table_access_context::before_open");
  /*
    Allow to operate the gtid_executed table
    while disconnecting the session.
  */
  m_flags= (MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |
            MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY |
            MYSQL_OPEN_IGNORE_FLUSH |
            MYSQL_LOCK_IGNORE_TIMEOUT |
            MYSQL_OPEN_IGNORE_KILLED);
  DBUG_VOID_RETURN;
}


bool Gtid_table_access_context::init(THD **thd, TABLE **table, bool is_write)
{
  DBUG_ENTER("Gtid_table_access_context::init");

  if (!(*thd))
    *thd= m_drop_thd_object= this->create_thd();
  m_is_write= is_write;
  if (m_is_write)
  {
    /* Disable binlog temporarily */
    m_tmp_disable_binlog__save_options= (*thd)->variables.option_bits;
    (*thd)->variables.option_bits&= ~OPTION_BIN_LOG;
  }

  if (!(*thd)->get_transaction()->xid_state()->has_state(XID_STATE::XA_NOTR))
  {
    /*
      This type of caller of Attachable_trx_rw is deadlock-free with
      the main transaction thanks to rejection to update
      'mysql.gtid_executed' by XA main transaction.
    */
    DBUG_ASSERT((*thd)->get_transaction()->xid_state()->
                has_state(XID_STATE::XA_IDLE) ||
                (*thd)->get_transaction()->xid_state()->
                has_state(XID_STATE::XA_PREPARED));

    (*thd)->begin_attachable_rw_transaction();
  }

  (*thd)->is_operating_gtid_table_implicitly= true;
  bool ret= this->open_table(*thd, DB_NAME, TABLE_NAME,
                             Gtid_table_persistor::number_fields,
                             m_is_write ? TL_WRITE : TL_READ,
                             table, &m_backup);

  DBUG_RETURN(ret);
}


bool Gtid_table_access_context::deinit(THD *thd, TABLE *table,
                                       bool error, bool need_commit)
{
  DBUG_ENTER("Gtid_table_access_context::deinit");

  bool err;
  err= this->close_table(thd, table, &m_backup, 0 != error, need_commit);

  /*
    If err is true this means that there was some problem during
    FLUSH LOGS commit phase.
  */
  if (err)
  {
    my_printf_error(ER_ERROR_DURING_FLUSH_LOGS, ER(ER_ERROR_DURING_FLUSH_LOGS),
                    MYF(ME_FATALERROR), err);
    sql_print_error(ER(ER_ERROR_DURING_FLUSH_LOGS), err);
    DBUG_RETURN(err);
  }

  /*
    If Gtid is inserted through Attachable_trx_rw its has been done
    in the above close_table() through ha_commit_trans().
    It does not have any side effect to the global transaction state
    as the only vulnerable part there relates to gtid (and is blocked
    from recursive invocation).
  */
  if (thd->is_attachable_rw_transaction_active())
    thd->end_attachable_transaction();

  thd->is_operating_gtid_table_implicitly= false;
  /* Reenable binlog */
  if (m_is_write)
    thd->variables.option_bits= m_tmp_disable_binlog__save_options;
  if (m_drop_thd_object)
    this->drop_thd(m_drop_thd_object);

  DBUG_RETURN(err);
}


int Gtid_table_persistor::fill_fields(Field **fields, const char *sid,
                                      rpl_gno gno_start, rpl_gno gno_end)
{
  DBUG_ENTER("Gtid_table_persistor::fill_field");

  /* Store SID */
  fields[0]->set_notnull();
  if (fields[0]->store(sid, binary_log::Uuid::TEXT_LENGTH, &my_charset_bin))
  {
    my_error(ER_RPL_INFO_DATA_TOO_LONG, MYF(0), fields[0]->field_name);
    goto err;
  }

  /* Store gno_start */
  fields[1]->set_notnull();
  if (fields[1]->store(gno_start, true /* unsigned = true*/))
  {
    my_error(ER_RPL_INFO_DATA_TOO_LONG, MYF(0), fields[1]->field_name);
    goto err;
  }

  /* Store gno_end */
  fields[2]->set_notnull();
  if (fields[2]->store(gno_end, true /* unsigned = true*/))
  {
    my_error(ER_RPL_INFO_DATA_TOO_LONG, MYF(0), fields[2]->field_name);
    goto err;
  }

  DBUG_RETURN(0);
err:
  DBUG_RETURN(-1);
}


int Gtid_table_persistor::write_row(TABLE *table, const char *sid,
                                    rpl_gno gno_start, rpl_gno gno_end)
{
  DBUG_ENTER("Gtid_table_persistor::write_row");
  int error= 0;
  Field **fields= NULL;

  fields= table->field;
  empty_record(table);

  if (fill_fields(fields, sid, gno_start, gno_end))
    DBUG_RETURN(-1);

  /* Inserts a new row into the gtid_executed table. */
  error= table->file->ha_write_row(table->record[0]);
  if (DBUG_EVALUATE_IF("simulate_err_on_write_gtid_into_table",
                       (error= -1), error))
  {
    if (error == HA_ERR_FOUND_DUPP_KEY)
    {
      /* Ignore the duplicate key error, log a warning for it. */
      sql_print_warning("The transaction owned GTID is already in "
                        "the %s table, which is caused by an "
                        "explicit modifying from user client.",
                        Gtid_table_access_context::TABLE_NAME.str);
    }
    else
    {
      table->file->print_error(error, MYF(0));
      /*
        This makes sure that the error is -1 and not the status
        returned by the handler.
      */
      DBUG_RETURN(-1);
    }
  }

  DBUG_RETURN(0);
}


int Gtid_table_persistor::update_row(TABLE *table, const char *sid,
                                     rpl_gno gno_start, rpl_gno new_gno_end)
{
  DBUG_ENTER("Gtid_table_persistor::update_row");
  int error= 0;
  Field **fields= NULL;
  uchar user_key[MAX_KEY_LENGTH];

  fields= table->field;
  empty_record(table);

  /* Store SID */
  fields[0]->set_notnull();
  if (fields[0]->store(sid, binary_log::Uuid::TEXT_LENGTH, &my_charset_bin))
  {
    my_error(ER_RPL_INFO_DATA_TOO_LONG, MYF(0), fields[0]->field_name);
    DBUG_RETURN(-1);
  }

  /* Store gno_start */
  fields[1]->set_notnull();
  if (fields[1]->store(gno_start, true /* unsigned = true*/))
  {
    my_error(ER_RPL_INFO_DATA_TOO_LONG, MYF(0), fields[1]->field_name);
    DBUG_RETURN(-1);
  }

  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if ((error= table->file->ha_index_init(0, 1)))
  {
    table->file->print_error(error, MYF(0));
    DBUG_PRINT("info", ("ha_index_init error"));
    goto end;
  }

  if ((error= table->file->ha_index_read_map(table->record[0], user_key,
                                             HA_WHOLE_KEY,
                                             HA_READ_KEY_EXACT)))
  {
    DBUG_PRINT ("info", ("Row not found"));
    goto end;
  }
  else
  {
    DBUG_PRINT("info", ("Row found"));
    store_record(table, record[1]);
  }

  /* Store new_gno_end */
  fields[2]->set_notnull();
  if ((error= fields[2]->store(new_gno_end, true /* unsigned = true*/)))
  {
    my_error(ER_RPL_INFO_DATA_TOO_LONG, MYF(0), fields[2]->field_name);
    goto end;
  }

  /* Update a row in the gtid_executed table. */
  error= table->file->ha_update_row(table->record[1], table->record[0]);
  if (DBUG_EVALUATE_IF("simulate_error_on_compress_gtid_table",
                       (error= -1), error))
  {
    table->file->print_error(error, MYF(0));
    /*
      This makes sure that the error is -1 and not the status returned
      by the handler.
    */
    goto end;
  }

end:
  table->file->ha_index_end();
  if (error)
    DBUG_RETURN(-1);
  else
    DBUG_RETURN(0);
}


int Gtid_table_persistor::save(THD *thd, const Gtid *gtid)
{
  DBUG_ENTER("Gtid_table_persistor::save(THD *thd, Gtid *gtid)");
  int error= 0;
  TABLE *table= NULL;
  Gtid_table_access_context table_access_ctx;
  char buf[binary_log::Uuid::TEXT_LENGTH + 1];

  /* Get source id */
  global_sid_lock->rdlock();
  rpl_sid sid= global_sid_map->sidno_to_sid(gtid->sidno);
  global_sid_lock->unlock();
  sid.to_string(buf);

  if (table_access_ctx.init(&thd, &table, true))
  {
    error= 1;
    goto end;
  }

  /* Save the gtid info into table. */
  error= write_row(table, buf, gtid->gno, gtid->gno);

end:
  table_access_ctx.deinit(thd, table, 0 != error, false);

  /* Do not protect m_count for improving transactions' concurrency */
  if (error == 0 && gtid_executed_compression_period != 0)
  {
    uint32 count= (uint32)m_count.atomic_add(1);
    if (count == gtid_executed_compression_period ||
        DBUG_EVALUATE_IF("compress_gtid_table", 1, 0))
    {
      mysql_mutex_lock(&LOCK_compress_gtid_table);
      should_compress= true;
      mysql_cond_signal(&COND_compress_gtid_table);
      mysql_mutex_unlock(&LOCK_compress_gtid_table);
    }
  }

  DBUG_RETURN(error);
}


int Gtid_table_persistor::save(const Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_table_persistor::save(Gtid_set *gtid_set)");
  int ret= 0;
  int error= 0;
  TABLE *table= NULL;
  Gtid_table_access_context table_access_ctx;
  THD *thd= current_thd;

  if (table_access_ctx.init(&thd, &table, true))
  {
    error= 1;
    /*
      Gtid table is not ready to be used, so failed to
      open it. Ignore the error.
    */
    thd->clear_error();
    if (!thd->get_stmt_da()->is_set())
      thd->get_stmt_da()->set_ok_status(0, 0, NULL);
    goto end;
  }

  ret= error= save(table, gtid_set);

end:
  const int deinit_ret= table_access_ctx.deinit(thd, table, 0 != error, true);

  if (!ret && deinit_ret)
    ret= -1;

  /* Notify compression thread to compress gtid_executed table. */
  if (error == 0 && DBUG_EVALUATE_IF("dont_compress_gtid_table", 0, 1))
  {
    mysql_mutex_lock(&LOCK_compress_gtid_table);
    should_compress= true;
    mysql_cond_signal(&COND_compress_gtid_table);
    mysql_mutex_unlock(&LOCK_compress_gtid_table);
  }

  DBUG_RETURN(ret);
}


int Gtid_table_persistor::save(TABLE *table, const Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_table_persistor::save(TABLE* table, "
             "Gtid_set *gtid_set)");
  int error= 0;
  list<Gtid_interval> gtid_intervals;
  list<Gtid_interval>::iterator iter;

  /* Get GTID intervals from gtid_set. */
  gtid_set->get_gtid_intervals(&gtid_intervals);
  for (iter= gtid_intervals.begin(); iter != gtid_intervals.end(); iter++)
  {
    /* Get source id. */
    char buf[binary_log::Uuid::TEXT_LENGTH + 1];
    rpl_sid sid= gtid_set->get_sid_map()->sidno_to_sid(iter->sidno);
    sid.to_string(buf);

    /* Save the gtid interval into table. */
    if ((error= write_row(table, buf, iter->gno_start, iter->gno_end)))
      break;
  }

  gtid_intervals.clear();
  DBUG_RETURN(error);
}


/**
  Simulate error and crash in the middle of the transaction
  of compressing gtid_executed table.

  @param  thd Thread requesting to compress the table

  @return
    @retval 0    OK.
    @retval -1   Error.
*/
#ifndef DBUG_OFF
static int dbug_test_on_compress(THD *thd)
{
  DBUG_ENTER("dbug_test_on_compress");
  /*
    Sleep a little, so that notified user thread executed the statement
    completely.
  */
  DBUG_EXECUTE_IF("fetch_compression_thread_stage_info", sleep(5););
  DBUG_EXECUTE_IF("fetch_compression_thread_stage_info",
                  {
                    const char act[]= "now signal fetch_thread_stage";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(thd,
                                                       STRING_WITH_LEN(act)));
                  };);
  /* Sleep a little, so that we can always fetch the correct stage info. */
  DBUG_EXECUTE_IF("fetch_compression_thread_stage_info", sleep(1););

  /*
    Wait until notified user thread executed the statement completely,
    then go to crash.
  */
  DBUG_EXECUTE_IF("simulate_crash_on_compress_gtid_table",
                  {
                    const char act[]= "now wait_for notified_thread_complete";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(thd,
                                                       STRING_WITH_LEN(act)));
                  };);
  DBUG_EXECUTE_IF("simulate_crash_on_compress_gtid_table", DBUG_SUICIDE(););

  DBUG_RETURN(0);
}
#endif


int Gtid_table_persistor::compress(THD *thd)
{
  DBUG_ENTER("Gtid_table_persistor::compress");
  int error= 0;
  bool is_complete= false;

  while (!is_complete && !error)
    error= compress_in_single_transaction(thd, is_complete);

  m_count.atomic_set(0);

  DBUG_EXECUTE_IF("compress_gtid_table",
                  {
                    const char act[]= "now signal complete_compression";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  DBUG_RETURN(error);
}


int Gtid_table_persistor::compress_in_single_transaction(THD *thd,
                                                         bool &is_complete)
{
  DBUG_ENTER("Gtid_table_persistor::compress_in_single_transaction");
  int error= 0;
  TABLE *table= NULL;
  Gtid_table_access_context table_access_ctx;

  mysql_mutex_lock(&LOCK_reset_gtid_table);
  if (table_access_ctx.init(&thd, &table, true))
  {
    error= 1;
    goto end;
  }

  /*
    Reset stage_compressing_gtid_table to overwrite
    stage_system_lock set in open_table(...).
  */
  THD_STAGE_INFO(thd, stage_compressing_gtid_table);

  if ((error= compress_first_consecutive_range(table, is_complete)))
    goto end;

#ifndef DBUG_OFF
  error= dbug_test_on_compress(thd);
#endif

end:
  table_access_ctx.deinit(thd, table, 0 != error, true);
  mysql_mutex_unlock(&LOCK_reset_gtid_table);

  DBUG_RETURN(error);
}


int Gtid_table_persistor::compress_first_consecutive_range(TABLE *table,
                                                           bool &is_complete)
{
  DBUG_ENTER("Gtid_table_persistor::compress_first_consecutive_range");
  int ret= 0;
  int err= 0;
  /* Record the source id of the first consecutive gtid. */
  string sid;
  /* Record the first GNO of the first consecutive gtid. */
  rpl_gno gno_start= 0;
  /* Record the last GNO of the last consecutive gtid. */
  rpl_gno gno_end= 0;
  /* Record the gtid interval of the current gtid. */
  string cur_sid;
  rpl_gno cur_gno_start= 0;
  rpl_gno cur_gno_end= 0;
  /*
    Indicate if we have consecutive gtids in the table.
    Set the flag to true if we find the first consecutive gtids.
    The first consecutive range of gtids will be compressed if
    the flag is true.
  */
  bool find_first_consecutive_gtids= false;

  if ((err= table->file->ha_index_init(0, true)))
    DBUG_RETURN(-1);

  /* Read each row by the PK(sid, gno_start) in increasing order. */
  err= table->file->ha_index_first(table->record[0]);
  /* Compress the first consecutive range of gtids. */
  while(!err)
  {
    get_gtid_interval(table, cur_sid, cur_gno_start, cur_gno_end);
    /*
      Check if gtid intervals of previous gtid and current gtid
      are consecutive.
    */
    if (sid == cur_sid && gno_end + 1 == cur_gno_start)
    {
      find_first_consecutive_gtids= true;
      gno_end= cur_gno_end;
      /* Delete the consecutive gtid. We do not delete the first
         consecutive gtid, so that we can update it later. */
      if ((err= table->file->ha_delete_row(table->record[0])))
      {
        table->file->print_error(err, MYF(0));
        break;
      }
    }
    else
    {
      if (find_first_consecutive_gtids)
        break;

      /* Record the gtid interval of the first consecutive gtid. */
      sid= cur_sid;
      gno_start= cur_gno_start;
      gno_end= cur_gno_end;
    }
    err= table->file->ha_index_next(table->record[0]);
  }

  table->file->ha_index_end();
  /* Indicate if the gtid_executed table is compressd completely. */
  is_complete= (err == HA_ERR_END_OF_FILE);

  if (err != HA_ERR_END_OF_FILE && err != 0)
    ret= -1;
  else if (find_first_consecutive_gtids)
    /*
      Update the gno_end of the first consecutive gtid with the gno_end of
      the last consecutive gtid for the first consecutive range of gtids.
    */
    ret= update_row(table, sid.c_str(), gno_start, gno_end);

  DBUG_RETURN(ret);
}


int Gtid_table_persistor::reset(THD *thd)
{
  DBUG_ENTER("Gtid_table_persistor::reset");
  int error= 0;
  TABLE *table= NULL;
  Gtid_table_access_context table_access_ctx;

  mysql_mutex_lock(&LOCK_reset_gtid_table);
  if (table_access_ctx.init(&thd, &table, true))
  {
    error= 1;
    goto end;
  }

  error= delete_all(table);

end:
  table_access_ctx.deinit(thd, table, 0 != error, true);
  mysql_mutex_unlock(&LOCK_reset_gtid_table);

  DBUG_RETURN(error);
}


string Gtid_table_persistor::encode_gtid_text(TABLE *table)
{
  DBUG_ENTER("Gtid_table_persistor::encode_gtid_text");
  char buff[MAX_FIELD_WIDTH];
  String str(buff, sizeof(buff), &my_charset_bin);

  /* Fetch gtid interval from the table */
  table->field[0]->val_str(&str);
  string gtid_text(str.c_ptr_safe());
  gtid_text.append(Gtid_set::default_string_format.sid_gno_separator);
  table->field[1]->val_str(&str);
  gtid_text.append(str.c_ptr_safe());
  gtid_text.append(Gtid_set::default_string_format.gno_start_end_separator);
  table->field[2]->val_str(&str);
  gtid_text.append(str.c_ptr_safe());

  DBUG_RETURN(gtid_text);
}


void Gtid_table_persistor::get_gtid_interval(TABLE *table, string &sid,
                                             rpl_gno &gno_start,
                                             rpl_gno &gno_end)
{
  DBUG_ENTER("Gtid_table_persistor::get_gtid_interval");
  char buff[MAX_FIELD_WIDTH];
  String str(buff, sizeof(buff), &my_charset_bin);

  /* Fetch gtid interval from the table */
  table->field[0]->val_str(&str);
  sid= string(str.c_ptr_safe());
  gno_start= table->field[1]->val_int();
  gno_end= table->field[2]->val_int();
  DBUG_VOID_RETURN;
}


int Gtid_table_persistor::fetch_gtids(Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_table_persistor::fetch_gtids");
  int ret= 0;
  int err= 0;
  TABLE *table= NULL;
  Gtid_table_access_context table_access_ctx;
  THD *thd= current_thd;

  if (table_access_ctx.init(&thd, &table, false))
  {
    ret= 1;
    goto end;
  }

  if ((err= table->file->ha_rnd_init(true)))
  {
    ret= -1;
    goto end;
  }

  while(!(err= table->file->ha_rnd_next(table->record[0])))
  {
    /* Store the gtid into the gtid_set */

    /**
      @todo:
      - take only global_sid_lock->rdlock(), and take
        gtid_state->sid_lock for each iteration.
      - Add wrapper around Gtid_set::add_gno_interval and call that
        instead.
    */
    global_sid_lock->wrlock();
    if (gtid_set->add_gtid_text(encode_gtid_text(table).c_str()) !=
        RETURN_STATUS_OK)
    {
      global_sid_lock->unlock();
      break;
    }
    global_sid_lock->unlock();
  }

  table->file->ha_rnd_end();
  if (err != HA_ERR_END_OF_FILE)
    ret= -1;

end:
  table_access_ctx.deinit(thd, table, 0 != ret, true);

  DBUG_RETURN(ret);
}


int Gtid_table_persistor::delete_all(TABLE *table)
{
  DBUG_ENTER("Gtid_table_persistor::delete_all");
  int err= 0;

  if ((err= table->file->ha_rnd_init(true)))
    DBUG_RETURN(-1);

  /*
    Delete all rows in the gtid_executed table. We cannot use truncate(),
    since it is a non-transactional DDL operation.
  */
  while(!(err= table->file->ha_rnd_next(table->record[0])))
  {
    /* Delete current row. */
    err= table->file->ha_delete_row(table->record[0]);
    if (DBUG_EVALUATE_IF("simulate_error_on_delete_gtid_from_table",
                         (err= -1), err))
    {
      table->file->print_error(err, MYF(0));
      sql_print_error("Failed to delete the row: '%s' from the gtid_executed "
                      "table.", encode_gtid_text(table).c_str());
      break;
    }
  }

  table->file->ha_rnd_end();
  if (err != HA_ERR_END_OF_FILE)
    DBUG_RETURN(-1);

  DBUG_RETURN(0);
}


/**
  The main function of the compression thread.
  - compress the gtid_executed table when get a compression signal.

  @param  p_thd    Thread requesting to compress the table

  @return
      @retval 0    OK. always, the compression thread will swallow any error
                       for going to wait for next compression signal until
                       it is terminated.
*/
extern "C" void *compress_gtid_table(void *p_thd)
{
  THD *thd=(THD*) p_thd;
  mysql_thread_set_psi_id(thd->thread_id());
  my_thread_init();
  DBUG_ENTER("compress_gtid_table");

  init_thd(&thd);
  /*
    Gtid table compression thread should ignore 'read-only' and
    'super_read_only' options so that it can update 'mysql.gtid_executed'
    replication repository tables.
  */
  thd->set_skip_readonly_check();
  for (;;)
  {
    mysql_mutex_lock(&LOCK_compress_gtid_table);
    if (terminate_compress_thread)
      break;
    THD_ENTER_COND(thd, &COND_compress_gtid_table,
                   &LOCK_compress_gtid_table,
                   &stage_suspending, NULL);
    /* Add the check to handle spurious wakeups from system. */
    while(!(should_compress || terminate_compress_thread))
      mysql_cond_wait(&COND_compress_gtid_table, &LOCK_compress_gtid_table);
    should_compress= false;
    if (terminate_compress_thread)
      break;
    mysql_mutex_unlock(&LOCK_compress_gtid_table);
    THD_EXIT_COND(thd, NULL);

    THD_STAGE_INFO(thd, stage_compressing_gtid_table);
    /* Compressing the gtid_executed table. */
    if (gtid_state->compress(thd))
    {
      sql_print_warning("Failed to compress the gtid_executed table.");
      /* Clear the error for going to wait for next compression signal. */
      thd->clear_error();
      DBUG_EXECUTE_IF("simulate_error_on_compress_gtid_table",
                      {
                        const char act[]= "now signal compression_failed";
                        DBUG_ASSERT(opt_debug_sync_timeout > 0);
                        DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                           STRING_WITH_LEN(act)));
                      };);
    }
  }

  mysql_mutex_unlock(&LOCK_compress_gtid_table);
  thd->reset_skip_readonly_check();
  deinit_thd(thd);
  DBUG_LEAVE;
  my_thread_end();
  my_thread_exit(0);
  return 0;
}


/**
  Create the compression thread to compress gtid_executed table.
*/
void create_compress_gtid_table_thread()
{
  my_thread_attr_t attr;
  int error;
  THD *thd;
  if (!(thd= new THD))
  {
    sql_print_error("Failed to compress the gtid_executed table, because "
                    "it is failed to allocate the THD.");
    return;
  }

  thd->set_new_thread_id();

  THD_CHECK_SENTRY(thd);

  if (my_thread_attr_init(&attr))
  {
    sql_print_error("Failed to initialize thread attribute "
                    "when creating compression thread.");
    delete thd;
    return;
  }

  if (DBUG_EVALUATE_IF("simulate_create_compress_thread_failure",
                       error= 1, 0) ||
#ifndef _WIN32
      (error= pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) ||
#endif
      (error= mysql_thread_create(key_thread_compress_gtid_table,
                                  &compress_thread_id, &attr,
                                  compress_gtid_table, (void*) thd)))
  {
    sql_print_error("Can not create thread to compress gtid_executed table "
                    "(errno= %d)", error);
    /* Delete the created THD after failed to create a compression thread. */
    delete thd;
  }

  (void) my_thread_attr_destroy(&attr);
}


/**
  Terminate the compression thread.
*/
void terminate_compress_gtid_table_thread()
{
  DBUG_ENTER("terminate_compress_gtid_table_thread");
  int error= 0;

  /* Notify suspended compression thread. */
  mysql_mutex_lock(&LOCK_compress_gtid_table);
  terminate_compress_thread= true;
  mysql_cond_signal(&COND_compress_gtid_table);
  mysql_mutex_unlock(&LOCK_compress_gtid_table);

  if (compress_thread_id.thread != 0)
  {
    error= my_thread_join(&compress_thread_id, NULL);
    compress_thread_id.thread= 0;
  }

  if (error != 0)
    sql_print_warning("Could not join gtid_executed table compression thread. "
                      "error:%d", error);

  DBUG_VOID_RETURN;
}

