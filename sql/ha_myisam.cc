/* Copyright (C) 2000,2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <myisampack.h>
#include "ha_myisam.h"
#include <stdarg.h>
#ifndef MASTER
#include "../srclib/myisam/myisamdef.h"
#else
#include "../myisam/myisamdef.h"
#include "../myisam/rt_index.h"
#endif

ulong myisam_recover_options= HA_RECOVER_NONE;

/* bits in myisam_recover_options */
const char *myisam_recover_names[] =
{ "DEFAULT", "BACKUP", "FORCE", "QUICK", NullS};
TYPELIB myisam_recover_typelib= {array_elements(myisam_recover_names)-1,"",
				 myisam_recover_names, NULL};

const char *myisam_stats_method_names[] = {"nulls_unequal", "nulls_equal",
                                           NullS};
TYPELIB myisam_stats_method_typelib= {
  array_elements(myisam_stats_method_names) - 1, "",
  myisam_stats_method_names, NULL};


/*****************************************************************************
** MyISAM tables
*****************************************************************************/

/* MyISAM handlerton */

handlerton myisam_hton= {
  "MyISAM",
  0,       /* slot */
  0,       /* savepoint size. */
  NULL,    /* close_connection */
  NULL,    /* savepoint */
  NULL,    /* rollback to savepoint */
  NULL,    /* release savepoint */
  NULL,    /* commit */
  NULL,    /* rollback */
  NULL,    /* prepare */
  NULL,    /* recover */
  NULL,    /* commit_by_xid */
  NULL,    /* rollback_by_xid */
  NULL,    /* create_cursor_read_view */
  NULL,    /* set_cursor_read_view */
  NULL,    /* close_cursor_read_view */
  /*
    MyISAM doesn't support transactions and doesn't have
    transaction-dependent context: cursors can survive a commit.
  */
  HTON_NO_FLAGS
};

// collect errors printed by mi_check routines

static void mi_check_print_msg(MI_CHECK *param,	const char* msg_type,
			       const char *fmt, va_list args)
{
  THD* thd = (THD*)param->thd;
  Protocol *protocol= thd->protocol;
  uint length, msg_length;
  char msgbuf[MI_MAX_MSG_BUF];
  char name[NAME_LEN*2+2];

  msg_length= my_vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0; // healthy paranoia

  DBUG_PRINT(msg_type,("message: %s",msgbuf));

  if (!thd->vio_ok())
  {
    sql_print_error(msgbuf);
    return;
  }

  if (param->testflag & (T_CREATE_MISSING_KEYS | T_SAFE_REPAIR |
			 T_AUTO_REPAIR))
  {
    my_message(ER_NOT_KEYFILE,msgbuf,MYF(MY_WME));
    return;
  }
  length=(uint) (strxmov(name, param->db_name,".",param->table_name,NullS) -
		 name);
  protocol->prepare_for_resend();
  protocol->store(name, length, system_charset_info);
  protocol->store(param->op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (protocol->write())
    sql_print_error("Failed on my_net_write, writing to stderr instead: %s\n",
		    msgbuf);
  return;
}

extern "C" {

volatile int *killed_ptr(MI_CHECK *param)
{
  /* In theory Unsafe conversion, but should be ok for now */
  return (int*) &(((THD *)(param->thd))->killed);
}

void mi_check_print_error(MI_CHECK *param, const char *fmt,...)
{
  param->error_printed|=1;
  param->out_flag|= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "error", fmt, args);
  va_end(args);
}

void mi_check_print_info(MI_CHECK *param, const char *fmt,...)
{
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "info", fmt, args);
  va_end(args);
}

void mi_check_print_warning(MI_CHECK *param, const char *fmt,...)
{
  param->warning_printed=1;
  param->out_flag|= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "warning", fmt, args);
  va_end(args);
}

}


ha_myisam::ha_myisam(TABLE *table_arg)
  :handler(&myisam_hton, table_arg), file(0),
  int_table_flags(HA_NULL_IN_KEY | HA_CAN_FULLTEXT | HA_CAN_SQL_HANDLER |
                  HA_DUPP_POS | HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY |
                  HA_FILE_BASED | HA_CAN_GEOMETRY | HA_READ_RND_SAME |
                  HA_CAN_INSERT_DELAYED | HA_CAN_BIT_FIELD),
  can_enable_indexes(1)
{}


static const char *ha_myisam_exts[] = {
  ".MYI",
  ".MYD",
  NullS
};

const char **ha_myisam::bas_ext() const
{
  return ha_myisam_exts;
}


const char *ha_myisam::index_type(uint key_number)
{
  return ((table->key_info[key_number].flags & HA_FULLTEXT) ? 
	  "FULLTEXT" :
	  (table->key_info[key_number].flags & HA_SPATIAL) ?
	  "SPATIAL" :
	  (table->key_info[key_number].algorithm == HA_KEY_ALG_RTREE) ?
	  "RTREE" :
	  "BTREE");
}

#ifdef HAVE_REPLICATION
int ha_myisam::net_read_dump(NET* net)
{
  int data_fd = file->dfile;
  int error = 0;

  my_seek(data_fd, 0L, MY_SEEK_SET, MYF(MY_WME));
  for (;;)
  {
    ulong packet_len = my_net_read(net);
    if (!packet_len)
      break ; // end of file
    if (packet_len == packet_error)
    {
      sql_print_error("ha_myisam::net_read_dump - read error ");
      error= -1;
      goto err;
    }
    if (my_write(data_fd, (byte*)net->read_pos, (uint) packet_len,
		 MYF(MY_WME|MY_FNABP)))
    {
      error = errno;
      goto err;
    }
  }
err:
  return error;
}


int ha_myisam::dump(THD* thd, int fd)
{
  MYISAM_SHARE* share = file->s;
  NET* net = &thd->net;
  uint blocksize = share->blocksize;
  my_off_t bytes_to_read = share->state.state.data_file_length;
  int data_fd = file->dfile;
  byte * buf = (byte*) my_malloc(blocksize, MYF(MY_WME));
  if (!buf)
    return ENOMEM;

  int error = 0;
  my_seek(data_fd, 0L, MY_SEEK_SET, MYF(MY_WME));
  for (; bytes_to_read > 0;)
  {
    uint bytes = my_read(data_fd, buf, blocksize, MYF(MY_WME));
    if (bytes == MY_FILE_ERROR)
    {
      error = errno;
      goto err;
    }

    if (fd >= 0)
    {
      if (my_write(fd, buf, bytes, MYF(MY_WME | MY_FNABP)))
      {
	error = errno ? errno : EPIPE;
	goto err;
      }
    }
    else
    {
      if (my_net_write(net, (char*) buf, bytes))
      {
	error = errno ? errno : EPIPE;
	goto err;
      }
    }
    bytes_to_read -= bytes;
  }

  if (fd < 0)
  {
    my_net_write(net, "", 0);
    net_flush(net);
  }

err:
  my_free((gptr) buf, MYF(0));
  return error;
}
#endif /* HAVE_REPLICATION */

	/* Name is here without an extension */

int ha_myisam::open(const char *name, int mode, uint test_if_locked)
{
  if (!(file=mi_open(name, mode, test_if_locked)))
    return (my_errno ? my_errno : -1);
  
  if (test_if_locked & (HA_OPEN_IGNORE_IF_LOCKED | HA_OPEN_TMP_TABLE))
    VOID(mi_extra(file, HA_EXTRA_NO_WAIT_LOCK, 0));
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    VOID(mi_extra(file, HA_EXTRA_WAIT_LOCK, 0));
  if (!table->s->db_record_offset)
    int_table_flags|=HA_REC_NOT_IN_SEQ;
  if (file->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    int_table_flags|=HA_HAS_CHECKSUM;
  return (0);
}

int ha_myisam::close(void)
{
  MI_INFO *tmp=file;
  file=0;
  return mi_close(tmp);
}

int ha_myisam::write_row(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_write_count,&LOCK_status);

  /* If we have a timestamp column, update it to the current time */
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  /*
    If we have an auto_increment column and we are writing a changed row
    or a new row, then update the auto_increment value in the record.
  */
  if (table->next_number_field && buf == table->record[0])
    update_auto_increment();
  return mi_write(file,buf);
}

int ha_myisam::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  int error;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;
  const char *old_proc_info=thd->proc_info;

  thd->proc_info="Checking table";
  myisamchk_init(&param);
  param.thd = thd;
  param.op_name =   "check";
  param.db_name=    table->s->db;
  param.table_name= table->alias;
  param.testflag = check_opt->flags | T_CHECK | T_SILENT;
  param.stats_method= (enum_mi_stats_method)thd->variables.myisam_stats_method;

  if (!(table->db_stat & HA_READ_ONLY))
    param.testflag|= T_STATISTICS;
  param.using_global_keycache = 1;

  if (!mi_is_crashed(file) &&
      (((param.testflag & T_CHECK_ONLY_CHANGED) &&
	!(share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				  STATE_CRASHED_ON_REPAIR)) &&
	share->state.open_count == 0) ||
       ((param.testflag & T_FAST) && (share->state.open_count ==
				      (uint) (share->global_changed ? 1 : 0)))))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_status(&param, file);		// Not fatal
  error = chk_size(&param, file);
  if (!error)
    error |= chk_del(&param, file, param.testflag);
  if (!error)
    error = chk_key(&param, file);
  if (!error)
  {
    if ((!(param.testflag & T_QUICK) &&
	 ((share->options &
	   (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ||
	  (param.testflag & (T_EXTEND | T_MEDIUM)))) ||
	mi_is_crashed(file))
    {
      uint old_testflag=param.testflag;
      param.testflag|=T_MEDIUM;
      init_io_cache(&param.read_cache, file->dfile,
		    my_default_record_cache_size, READ_CACHE,
		    share->pack.header_length, 1, MYF(MY_WME));
      error |= chk_data_link(&param, file, param.testflag & T_EXTEND);
      end_io_cache(&(param.read_cache));
      param.testflag=old_testflag;
    }
  }
  if (!error)
  {
    if ((share->state.changed & (STATE_CHANGED |
				 STATE_CRASHED_ON_REPAIR |
				 STATE_CRASHED | STATE_NOT_ANALYZED)) ||
	(param.testflag & T_STATISTICS) ||
	mi_is_crashed(file))
    {
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
      pthread_mutex_lock(&share->intern_lock);
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      if (!(table->db_stat & HA_READ_ONLY))
	error=update_state_info(&param,file,UPDATE_TIME | UPDATE_OPEN_COUNT |
				UPDATE_STAT);
      pthread_mutex_unlock(&share->intern_lock);
      info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	   HA_STATUS_CONST);
    }
  }
  else if (!mi_is_crashed(file) && !thd->killed)
  {
    mi_mark_crashed(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
  }

  thd->proc_info=old_proc_info;
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}


/*
  analyze the key distribution in the table
  As the table may be only locked for read, we have to take into account that
  two threads may do an analyze at the same time!
*/

int ha_myisam::analyze(THD *thd, HA_CHECK_OPT* check_opt)
{
  int error=0;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name=    "analyze";
  param.db_name=    table->s->db;
  param.table_name= table->alias;
  param.testflag= (T_FAST | T_CHECK | T_SILENT | T_STATISTICS |
                   T_DONT_CHECK_CHECKSUM);
  param.using_global_keycache = 1;
  param.stats_method= (enum_mi_stats_method)thd->variables.myisam_stats_method;

  if (!(share->state.changed & STATE_NOT_ANALYZED))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_key(&param, file);
  if (!error)
  {
    pthread_mutex_lock(&share->intern_lock);
    error=update_state_info(&param,file,UPDATE_STAT);
    pthread_mutex_unlock(&share->intern_lock);
  }
  else if (!mi_is_crashed(file) && !thd->killed)
    mi_mark_crashed(file);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}


int ha_myisam::restore(THD* thd, HA_CHECK_OPT *check_opt)
{
  HA_CHECK_OPT tmp_check_opt;
  char *backup_dir= thd->lex->backup_dir;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  const char *table_name= table->s->table_name;
  int error;
  const char* errmsg;
  DBUG_ENTER("restore");

  if (fn_format_relative_to_data_home(src_path, table_name, backup_dir,
				      MI_NAME_DEXT))
    DBUG_RETURN(HA_ADMIN_INVALID);

  if (my_copy(src_path, fn_format(dst_path, table->s->path, "",
				  MI_NAME_DEXT, 4), MYF(MY_WME)))
  {
    error= HA_ADMIN_FAILED;
    errmsg= "Failed in my_copy (Error %d)";
    goto err;
  }

  tmp_check_opt.init();
  tmp_check_opt.flags |= T_VERY_SILENT | T_CALC_CHECKSUM | T_QUICK;
  DBUG_RETURN(repair(thd, &tmp_check_opt));

 err:
  {
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd= thd;
    param.op_name=    "restore";
    param.db_name=    table->s->db;
    param.table_name= table->s->table_name;
    param.testflag= 0;
    mi_check_print_error(&param, errmsg, my_errno);
    DBUG_RETURN(error);
  }
}


int ha_myisam::backup(THD* thd, HA_CHECK_OPT *check_opt)
{
  char *backup_dir= thd->lex->backup_dir;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  const char *table_name= table->s->table_name;
  int error;
  const char *errmsg;
  DBUG_ENTER("ha_myisam::backup");

  if (fn_format_relative_to_data_home(dst_path, table_name, backup_dir,
				      reg_ext))
  {
    errmsg= "Failed in fn_format() for .frm file (errno: %d)";
    error= HA_ADMIN_INVALID;
    goto err;
  }

  if (my_copy(fn_format(src_path, table->s->path, "", reg_ext,
                        MY_UNPACK_FILENAME),
	      dst_path,
	      MYF(MY_WME | MY_HOLD_ORIGINAL_MODES | MY_DONT_OVERWRITE_FILE)))
  {
    error = HA_ADMIN_FAILED;
    errmsg = "Failed copying .frm file (errno: %d)";
    goto err;
  }

  /* Change extension */
  if (!fn_format(dst_path, dst_path, "", MI_NAME_DEXT,
		 MY_REPLACE_EXT | MY_UNPACK_FILENAME | MY_SAFE_PATH))
  {
    errmsg = "Failed in fn_format() for .MYD file (errno: %d)";
    error = HA_ADMIN_INVALID;
    goto err;
  }

  if (my_copy(fn_format(src_path, table->s->path, "", MI_NAME_DEXT,
			MY_UNPACK_FILENAME),
	      dst_path,
	      MYF(MY_WME | MY_HOLD_ORIGINAL_MODES | MY_DONT_OVERWRITE_FILE)))
  {
    errmsg = "Failed copying .MYD file (errno: %d)";
    error= HA_ADMIN_FAILED;
    goto err;
  }
  DBUG_RETURN(HA_ADMIN_OK);

 err:
  {
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd=        thd;
    param.op_name=    "backup";
    param.db_name=    table->s->db;
    param.table_name= table->s->table_name;
    param.testflag =  0;
    mi_check_print_error(&param,errmsg, my_errno);
    DBUG_RETURN(error);
  }
}


int ha_myisam::repair(THD* thd, HA_CHECK_OPT *check_opt)
{
  int error;
  MI_CHECK param;
  ha_rows start_records;

  if (!file) return HA_ADMIN_INTERNAL_ERROR;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name=  "repair";
  param.testflag= ((check_opt->flags & ~(T_EXTEND)) |
                   T_SILENT | T_FORCE_CREATE | T_CALC_CHECKSUM |
                   (check_opt->flags & T_EXTEND ? T_REP : T_REP_BY_SORT));
  param.sort_buffer_length=  check_opt->sort_buffer_size;
  start_records=file->state->records;
  while ((error=repair(thd,param,0)) && param.retry_repair)
  {
    param.retry_repair=0;
    if (test_all_bits(param.testflag,
		      (uint) (T_RETRY_WITHOUT_QUICK | T_QUICK)))
    {
      param.testflag&= ~T_RETRY_WITHOUT_QUICK;
      sql_print_information("Retrying repair of: '%s' without quick",
                            table->s->path);
      continue;
    }
    param.testflag&= ~T_QUICK;
    if ((param.testflag & T_REP_BY_SORT))
    {
      param.testflag= (param.testflag & ~T_REP_BY_SORT) | T_REP;
      sql_print_information("Retrying repair of: '%s' with keycache",
                            table->s->path);
      continue;
    }
    break;
  }
  if (!error && start_records != file->state->records &&
      !(check_opt->flags & T_VERY_SILENT))
  {
    char llbuff[22],llbuff2[22];
    sql_print_information("Found %s of %s rows when repairing '%s'",
                          llstr(file->state->records, llbuff),
                          llstr(start_records, llbuff2),
                          table->s->path);
  }
  return error;
}

int ha_myisam::optimize(THD* thd, HA_CHECK_OPT *check_opt)
{
  int error;
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  MI_CHECK param;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name= "optimize";
  param.testflag= (check_opt->flags | T_SILENT | T_FORCE_CREATE |
                   T_REP_BY_SORT | T_STATISTICS | T_SORT_INDEX);
  param.sort_buffer_length=  check_opt->sort_buffer_size;
  if ((error= repair(thd,param,1)) && param.retry_repair)
  {
    sql_print_warning("Warning: Optimize table got errno %d, retrying",
                      my_errno);
    param.testflag&= ~T_REP_BY_SORT;
    error= repair(thd,param,1);
  }
  return error;
}


int ha_myisam::repair(THD *thd, MI_CHECK &param, bool optimize)
{
  int error=0;
  uint local_testflag=param.testflag;
  bool optimize_done= !optimize, statistics_done=0;
  const char *old_proc_info=thd->proc_info;
  char fixed_name[FN_REFLEN];
  MYISAM_SHARE* share = file->s;
  ha_rows rows= file->state->records;
  DBUG_ENTER("ha_myisam::repair");

  param.db_name=    table->s->db;
  param.table_name= table->alias;
  param.tmpfile_createflag = O_RDWR | O_TRUNC;
  param.using_global_keycache = 1;
  param.thd= thd;
  param.tmpdir= &mysql_tmpdir_list;
  param.out_flag= 0;
  strmov(fixed_name,file->filename);

  // Don't lock tables if we have used LOCK TABLE
  if (!thd->locked_tables && 
      mi_lock_database(file, table->s->tmp_table ? F_EXTRA_LCK : F_WRLCK))
  {
    mi_check_print_error(&param,ER(ER_CANT_LOCK),my_errno);
    DBUG_RETURN(HA_ADMIN_FAILED);
  }

  if (!optimize ||
      ((file->state->del || share->state.split != file->state->records) &&
       (!(param.testflag & T_QUICK) ||
	!(share->state.changed & STATE_NOT_OPTIMIZED_KEYS))))
  {
    ulonglong key_map= ((local_testflag & T_CREATE_MISSING_KEYS) ?
			mi_get_mask_all_keys_active(share->base.keys) :
			share->state.key_map);
    uint testflag=param.testflag;
    if (mi_test_if_sort_rep(file,file->state->records,key_map,0) &&
	(local_testflag & T_REP_BY_SORT))
    {
      local_testflag|= T_STATISTICS;
      param.testflag|= T_STATISTICS;		// We get this for free
      statistics_done=1;
      if (thd->variables.myisam_repair_threads>1)
      {
        char buf[40];
        /* TODO: respect myisam_repair_threads variable */
        my_snprintf(buf, 40, "Repair with %d threads", my_count_bits(key_map));
        thd->proc_info=buf;
        error = mi_repair_parallel(&param, file, fixed_name,
            param.testflag & T_QUICK);
        thd->proc_info="Repair done"; // to reset proc_info, as
                                      // it was pointing to local buffer
      }
      else
      {
        thd->proc_info="Repair by sorting";
        error = mi_repair_by_sort(&param, file, fixed_name,
            param.testflag & T_QUICK);
      }
    }
    else
    {
      thd->proc_info="Repair with keycache";
      param.testflag &= ~T_REP_BY_SORT;
      error=  mi_repair(&param, file, fixed_name,
			param.testflag & T_QUICK);
    }
    param.testflag=testflag;
    optimize_done=1;
  }
  if (!error)
  {
    if ((local_testflag & T_SORT_INDEX) &&
	(share->state.changed & STATE_NOT_SORTED_PAGES))
    {
      optimize_done=1;
      thd->proc_info="Sorting index";
      error=mi_sort_index(&param,file,fixed_name);
    }
    if (!statistics_done && (local_testflag & T_STATISTICS))
    {
      if (share->state.changed & STATE_NOT_ANALYZED)
      {
	optimize_done=1;
	thd->proc_info="Analyzing";
	error = chk_key(&param, file);
      }
      else
	local_testflag&= ~T_STATISTICS;		// Don't update statistics
    }
  }
  thd->proc_info="Saving state";
  if (!error)
  {
    if ((share->state.changed & STATE_CHANGED) || mi_is_crashed(file))
    {
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    }
    /*
      the following 'if', thought conceptually wrong,
      is a useful optimization nevertheless.
    */
    if (file->state != &file->s->state.state)
      file->s->state.state = *file->state;
    if (file->s->base.auto_key)
      update_auto_increment_key(&param, file, 1);
    if (optimize_done)
      error = update_state_info(&param, file,
				UPDATE_TIME | UPDATE_OPEN_COUNT |
				(local_testflag &
				 T_STATISTICS ? UPDATE_STAT : 0));
    info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	 HA_STATUS_CONST);
    if (rows != file->state->records && ! (param.testflag & T_VERY_SILENT))
    {
      char llbuff[22],llbuff2[22];
      mi_check_print_warning(&param,"Number of rows changed from %s to %s",
			     llstr(rows,llbuff),
			     llstr(file->state->records,llbuff2));
    }
  }
  else
  {
    mi_mark_crashed_on_repair(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    update_state_info(&param, file, 0);
  }
  thd->proc_info=old_proc_info;
  if (!thd->locked_tables)
    mi_lock_database(file,F_UNLCK);
  DBUG_RETURN(error ? HA_ADMIN_FAILED :
	      !optimize_done ? HA_ADMIN_ALREADY_DONE : HA_ADMIN_OK);
}


/*
  Assign table indexes to a specific key cache.
*/

int ha_myisam::assign_to_keycache(THD* thd, HA_CHECK_OPT *check_opt)
{
  KEY_CACHE *new_key_cache= check_opt->key_cache;
  const char *errmsg= 0;
  int error= HA_ADMIN_OK;
  ulonglong map= ~(ulonglong) 0;
  TABLE_LIST *table_list= table->pos_in_table_list;
  DBUG_ENTER("ha_myisam::assign_to_keycache");

  /* Check validity of the index references */
  if (table_list->use_index)
  {
    /* We only come here when the user did specify an index map */
    key_map kmap;
    if (get_key_map_from_key_list(&kmap, table, table_list->use_index))
    {
      errmsg= thd->net.last_error;
      error= HA_ADMIN_FAILED;
      goto err;
    }
    map= kmap.to_ulonglong();
  }

  if ((error= mi_assign_to_key_cache(file, map, new_key_cache)))
  { 
    char buf[STRING_BUFFER_USUAL_SIZE];
    my_snprintf(buf, sizeof(buf),
		"Failed to flush to index file (errno: %d)", error);
    errmsg= buf;
    error= HA_ADMIN_CORRUPT;
  }

 err:
  if (error != HA_ADMIN_OK)
  {
    /* Send error to user */
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd= thd;
    param.op_name=    "assign_to_keycache";
    param.db_name=    table->s->db;
    param.table_name= table->s->table_name;
    param.testflag= 0;
    mi_check_print_error(&param, errmsg);
  }
  DBUG_RETURN(error);
}


/*
  Preload pages of the index file for a table into the key cache.
*/

int ha_myisam::preload_keys(THD* thd, HA_CHECK_OPT *check_opt)
{
  int error;
  const char *errmsg;
  ulonglong map= ~(ulonglong) 0;
  TABLE_LIST *table_list= table->pos_in_table_list;
  my_bool ignore_leaves= table_list->ignore_leaves;

  DBUG_ENTER("ha_myisam::preload_keys");

  /* Check validity of the index references */
  if (table_list->use_index)
  {
    key_map kmap;
    get_key_map_from_key_list(&kmap, table, table_list->use_index);
    if (kmap.is_set_all())
    {
      errmsg= thd->net.last_error;
      error= HA_ADMIN_FAILED;
      goto err;
    }
    if (!kmap.is_clear_all())
      map= kmap.to_ulonglong();
  }

  mi_extra(file, HA_EXTRA_PRELOAD_BUFFER_SIZE,
           (void *) &thd->variables.preload_buff_size);

  if ((error= mi_preload(file, map, ignore_leaves)))
  {
    switch (error) {
    case HA_ERR_NON_UNIQUE_BLOCK_SIZE:
      errmsg= "Indexes use different block sizes";
      break;
    case HA_ERR_OUT_OF_MEM:
      errmsg= "Failed to allocate buffer";
      break;
    default:
      char buf[ERRMSGSIZE+20];
      my_snprintf(buf, ERRMSGSIZE,
                  "Failed to read from index file (errno: %d)", my_errno);
      errmsg= buf;
    }
    error= HA_ADMIN_FAILED;
    goto err;
  }

  DBUG_RETURN(HA_ADMIN_OK);

 err:
  {
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd= thd;
    param.op_name=    "preload_keys";
    param.db_name=    table->s->db;
    param.table_name= table->s->table_name;
    param.testflag=   0;
    mi_check_print_error(&param, errmsg);
    DBUG_RETURN(error);
  }
}


/*
  Disable indexes, making it persistent if requested.

  SYNOPSIS
    disable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      disable all non-unique keys
                HA_KEY_SWITCH_ALL          disable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE dis. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     dis. all keys and make persistent

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented.

  RETURN
    0  ok
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_myisam::disable_indexes(uint mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    /* call a storage engine function to switch the key map */
    error= mi_disable_indexes(file);
  }
  else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE)
  {
    mi_extra(file, HA_EXTRA_NO_KEYS, 0);
    info(HA_STATUS_CONST);                        // Read new key info
    error= 0;
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Enable indexes, making it persistent if requested.

  SYNOPSIS
    enable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      enable all non-unique keys
                HA_KEY_SWITCH_ALL          enable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE en. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     en. all keys and make persistent

  DESCRIPTION
    Enable indexes, which might have been disabled by disable_index() before.
    The modes without _SAVE work only if both data and indexes are empty,
    since the MyISAM repair would enable them persistently.
    To be sure in these cases, call handler::delete_all_rows() before.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented.

  RETURN
    0  ok
    !=0  Error, among others:
    HA_ERR_CRASHED  data or index is non-empty. Delete all rows and retry.
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_myisam::enable_indexes(uint mode)
{
  int error;

  if (mi_is_all_keys_active(file->s->state.key_map, file->s->base.keys))
  {
    /* All indexes are enabled already. */
    return 0;
  }

  if (mode == HA_KEY_SWITCH_ALL)
  {
    error= mi_enable_indexes(file);
    /*
       Do not try to repair on error,
       as this could make the enabled state persistent,
       but mode==HA_KEY_SWITCH_ALL forbids it.
    */
  }
  else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE)
  {
    THD *thd=current_thd;
    MI_CHECK param;
    const char *save_proc_info=thd->proc_info;
    thd->proc_info="Creating index";
    myisamchk_init(&param);
    param.op_name= "recreating_index";
    param.testflag= (T_SILENT | T_REP_BY_SORT | T_QUICK |
                     T_CREATE_MISSING_KEYS);
    param.myf_rw&= ~MY_WAIT_IF_FULL;
    param.sort_buffer_length=  thd->variables.myisam_sort_buff_size;
    param.stats_method= (enum_mi_stats_method)thd->variables.myisam_stats_method;
    param.tmpdir=&mysql_tmpdir_list;
    if ((error= (repair(thd,param,0) != HA_ADMIN_OK)) && param.retry_repair)
    {
      sql_print_warning("Warning: Enabling keys got errno %d, retrying",
                        my_errno);
      thd->clear_error();
      param.testflag&= ~(T_REP_BY_SORT | T_QUICK);
      error= (repair(thd,param,0) != HA_ADMIN_OK);
      if (!error && thd->net.report_error)
        error= HA_ERR_CRASHED;
    }
    info(HA_STATUS_CONST);
    thd->proc_info=save_proc_info;
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Test if indexes are disabled.


  SYNOPSIS
    indexes_are_disabled()
      no parameters


  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int ha_myisam::indexes_are_disabled(void)
{
  
  return mi_indexes_are_disabled(file);
}


/*
  prepare for a many-rows insert operation
  e.g. - disable indexes (if they can be recreated fast) or
  activate special bulk-insert optimizations

  SYNOPSIS
    start_bulk_insert(rows)
    rows        Rows to be inserted
                0 if we don't know

  NOTICE
    Do not forget to call end_bulk_insert() later!
*/

void ha_myisam::start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("ha_myisam::start_bulk_insert");
  THD *thd= current_thd;
  ulong size= min(thd->variables.read_buff_size,
                  table->s->avg_row_length*rows);
  DBUG_PRINT("info",("start_bulk_insert: rows %lu size %lu",
                     (ulong) rows, size));

  /* don't enable row cache if too few rows */
  if (! rows || (rows > MI_MIN_ROWS_TO_USE_WRITE_CACHE))
    mi_extra(file, HA_EXTRA_WRITE_CACHE, (void*) &size);

  can_enable_indexes= mi_is_all_keys_active(file->s->state.key_map,
                                            file->s->base.keys);

  if (!(specialflag & SPECIAL_SAFE_MODE))
  {
    /*
      Only disable old index if the table was empty and we are inserting
      a lot of rows.
      We should not do this for only a few rows as this is slower and
      we don't want to update the key statistics based of only a few rows.
    */
    if (file->state->records == 0 && can_enable_indexes &&
        (!rows || rows >= MI_MIN_ROWS_TO_DISABLE_INDEXES))
      mi_disable_non_unique_index(file,rows);
    else
    if (!file->bulk_insert &&
        (!rows || rows >= MI_MIN_ROWS_TO_USE_BULK_INSERT))
    {
      mi_init_bulk_insert(file, thd->variables.bulk_insert_buff_size, rows);
    }
  }
  DBUG_VOID_RETURN;
}

/*
  end special bulk-insert optimizations,
  which have been activated by start_bulk_insert().

  SYNOPSIS
    end_bulk_insert()
    no arguments

  RETURN
    0     OK
    != 0  Error
*/

int ha_myisam::end_bulk_insert()
{
  mi_end_bulk_insert(file);
  int err=mi_extra(file, HA_EXTRA_NO_CACHE, 0);
  return err ? err : can_enable_indexes ?
                     enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE) : 0;
}


bool ha_myisam::check_and_repair(THD *thd)
{
  int error=0;
  int marked_crashed;
  char *old_query;
  uint old_query_length;
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_myisam::check_and_repair");

  check_opt.init();
  check_opt.flags= T_MEDIUM | T_AUTO_REPAIR;
  // Don't use quick if deleted rows
  if (!file->state->del && (myisam_recover_options & HA_RECOVER_QUICK))
    check_opt.flags|=T_QUICK;
  sql_print_warning("Checking table:   '%s'",table->s->path);

  old_query= thd->query;
  old_query_length= thd->query_length;
  pthread_mutex_lock(&LOCK_thread_count);
  thd->query= (char*) table->s->table_name;
  thd->query_length= (uint32) strlen(table->s->table_name);
  pthread_mutex_unlock(&LOCK_thread_count);

  if ((marked_crashed= mi_is_crashed(file)) || check(thd, &check_opt))
  {
    sql_print_warning("Recovering table: '%s'",table->s->path);
    check_opt.flags=
      ((myisam_recover_options & HA_RECOVER_BACKUP ? T_BACKUP_DATA : 0) |
       (marked_crashed                             ? 0 : T_QUICK) |
       (myisam_recover_options & HA_RECOVER_FORCE  ? 0 : T_SAFE_REPAIR) |
       T_AUTO_REPAIR);
    if (repair(thd, &check_opt))
      error=1;
  }
  pthread_mutex_lock(&LOCK_thread_count);
  thd->query= old_query;
  thd->query_length= old_query_length;
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_RETURN(error);
}

bool ha_myisam::is_crashed() const
{
  return (file->s->state.changed & STATE_CRASHED ||
	  (my_disable_locking && file->s->state.open_count));
}

int ha_myisam::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(table->in_use->status_var.ha_update_count,&LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return mi_update(file,old_data,new_data);
}

int ha_myisam::delete_row(const byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_delete_count,&LOCK_status);
  return mi_delete(file,buf);
}

int ha_myisam::index_read(byte * buf, const byte * key,
			  uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=mi_rkey(file,buf,active_index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_idx(byte * buf, uint index, const byte * key,
			      uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=mi_rkey(file,buf,index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_last(byte * buf, const byte * key, uint key_len)
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=mi_rkey(file,buf,active_index, key, key_len, HA_READ_PREFIX_LAST);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_next(byte * buf)
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  int error=mi_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_prev(byte * buf)
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_prev_count,
		      &LOCK_status);
  int error=mi_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_first(byte * buf)
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_first_count,
		      &LOCK_status);
  int error=mi_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_last(byte * buf)
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_last_count,
		      &LOCK_status);
  int error=mi_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_next_same(byte * buf,
			       const byte *key __attribute__((unused)),
			       uint length __attribute__((unused)))
{
  DBUG_ASSERT(inited==INDEX);
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  int error=mi_rnext_same(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


int ha_myisam::rnd_init(bool scan)
{
  if (scan)
    return mi_scan_init(file);
  return mi_extra(file, HA_EXTRA_RESET, 0);
}

int ha_myisam::rnd_next(byte *buf)
{
  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);
  int error=mi_scan(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::restart_rnd_next(byte *buf, byte *pos)
{
  return rnd_pos(buf,pos);
}

int ha_myisam::rnd_pos(byte * buf, byte *pos)
{
  statistic_increment(table->in_use->status_var.ha_read_rnd_count,
		      &LOCK_status);
  int error=mi_rrnd(file, buf, my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_myisam::position(const byte* record)
{
  my_off_t position=mi_position(file);
  my_store_ptr(ref, ref_length, position);
}

void ha_myisam::info(uint flag)
{
  MI_ISAMINFO info;
  char name_buff[FN_REFLEN];

  (void) mi_status(file,&info,flag);
  if (flag & HA_STATUS_VARIABLE)
  {
    records = info.records;
    deleted = info.deleted;
    data_file_length=info.data_file_length;
    index_file_length=info.index_file_length;
    delete_length = info.delete_length;
    check_time  = info.check_time;
    mean_rec_length=info.mean_reclength;
  }
  if (flag & HA_STATUS_CONST)
  {
    TABLE_SHARE *share= table->s;
    max_data_file_length=  info.max_data_file_length;
    max_index_file_length= info.max_index_file_length;
    create_time= info.create_time;
    sortkey= info.sortkey;
    ref_length= info.reflength;
    share->db_options_in_use= info.options;
    block_size= myisam_block_size;
    share->keys_in_use.set_prefix(share->keys);
    share->keys_in_use.intersect_extended(info.key_map);
    share->keys_for_keyread.intersect(share->keys_in_use);
    share->db_record_offset= info.record_offset;
    if (share->key_parts)
      memcpy((char*) table->key_info[0].rec_per_key,
	     (char*) info.rec_per_key,
	     sizeof(table->key_info[0].rec_per_key)*share->key_parts);
    raid_type= info.raid_type;
    raid_chunks= info.raid_chunks;
    raid_chunksize= info.raid_chunksize;

   /*
     Set data_file_name and index_file_name to point at the symlink value
     if table is symlinked (Ie;  Real name is not same as generated name)
   */
    data_file_name=index_file_name=0;
    fn_format(name_buff, file->filename, "", MI_NAME_DEXT, 2);
    if (strcmp(name_buff, info.data_file_name))
      data_file_name=info.data_file_name;
    strmov(fn_ext(name_buff),MI_NAME_IEXT);
    if (strcmp(name_buff, info.index_file_name))
      index_file_name=info.index_file_name;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey  = info.errkey;
    my_store_ptr(dupp_ref, ref_length, info.dupp_key_pos);
  }
  if (flag & HA_STATUS_TIME)
    update_time = info.update_time;
  if (flag & HA_STATUS_AUTO)
    auto_increment_value= info.auto_increment;
}


int ha_myisam::extra(enum ha_extra_function operation)
{
  if ((specialflag & SPECIAL_SAFE_MODE) && operation == HA_EXTRA_KEYREAD)
    return 0;
  return mi_extra(file, operation, 0);
}


/* To be used with WRITE_CACHE and EXTRA_CACHE */

int ha_myisam::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  if ((specialflag & SPECIAL_SAFE_MODE) && operation == HA_EXTRA_WRITE_CACHE)
    return 0;
  return mi_extra(file, operation, (void*) &cache_size);
}

int ha_myisam::delete_all_rows()
{
  return mi_delete_all_rows(file);
}

int ha_myisam::delete_table(const char *name)
{
  return mi_delete_table(name);
}


int ha_myisam::external_lock(THD *thd, int lock_type)
{
  return mi_lock_database(file, !table->s->tmp_table ?
			  lock_type : ((lock_type == F_UNLCK) ?
				       F_UNLCK : F_EXTRA_LCK));
}

THR_LOCK_DATA **ha_myisam::store_lock(THD *thd,
				      THR_LOCK_DATA **to,
				      enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type=lock_type;
  *to++= &file->lock;
  return to;
}

void ha_myisam::update_create_info(HA_CREATE_INFO *create_info)
{
  ha_myisam::info(HA_STATUS_AUTO | HA_STATUS_CONST);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  {
    create_info->auto_increment_value=auto_increment_value;
  }
  if (!(create_info->used_fields & HA_CREATE_USED_RAID))
  {
    create_info->raid_type= raid_type;
    create_info->raid_chunks= raid_chunks;
    create_info->raid_chunksize= raid_chunksize;
  }
  create_info->data_file_name=data_file_name;
  create_info->index_file_name=index_file_name;
}


int ha_myisam::create(const char *name, register TABLE *table_arg,
		      HA_CREATE_INFO *info)
{
  int error;
  uint i,j,recpos,minpos,fieldpos,temp_length,length, create_flags= 0;
  bool found_real_auto_increment=0;
  enum ha_base_keytype type;
  char buff[FN_REFLEN];
  KEY *pos;
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo,*recinfo_pos;
  HA_KEYSEG *keyseg;
  TABLE_SHARE *share= table->s;
  uint options= share->db_options_in_use;
  DBUG_ENTER("ha_myisam::create");

  type=HA_KEYTYPE_BINARY;				// Keep compiler happy
  if (!(my_multi_malloc(MYF(MY_WME),
			&recinfo,(share->fields*2+2)*
                        sizeof(MI_COLUMNDEF),
			&keydef, share->keys*sizeof(MI_KEYDEF),
			&keyseg,
			((share->key_parts + share->keys) *
			 sizeof(HA_KEYSEG)),
			NullS)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  pos=table_arg->key_info;
  for (i=0; i < share->keys ; i++, pos++)
  {
    keydef[i].flag= (pos->flags & (HA_NOSAME | HA_FULLTEXT | HA_SPATIAL));
    keydef[i].key_alg= pos->algorithm == HA_KEY_ALG_UNDEF ? 
      (pos->flags & HA_SPATIAL ? HA_KEY_ALG_RTREE : HA_KEY_ALG_BTREE) :
      pos->algorithm;
    keydef[i].seg=keyseg;
    keydef[i].keysegs=pos->key_parts;
    for (j=0 ; j < pos->key_parts ; j++)
    {
      Field *field=pos->key_part[j].field;
      type=field->key_type();
      keydef[i].seg[j].flag=pos->key_part[j].key_part_flag;

      if (options & HA_OPTION_PACK_KEYS ||
	  (pos->flags & (HA_PACK_KEY | HA_BINARY_PACK_KEY |
			 HA_SPACE_PACK_USED)))
      {
	if (pos->key_part[j].length > 8 &&
	    (type == HA_KEYTYPE_TEXT ||
	     type == HA_KEYTYPE_NUM ||
	     (type == HA_KEYTYPE_BINARY && !field->zero_pack())))
	{
	  /* No blobs here */
	  if (j == 0)
	    keydef[i].flag|=HA_PACK_KEY;
	  if (!(field->flags & ZEROFILL_FLAG) &&
	      (field->type() == MYSQL_TYPE_STRING ||
	       field->type() == MYSQL_TYPE_VAR_STRING ||
	       ((int) (pos->key_part[j].length - field->decimals()))
	       >= 4))
	    keydef[i].seg[j].flag|=HA_SPACE_PACK;
	}
	else if (j == 0 && (!(pos->flags & HA_NOSAME) || pos->key_length > 16))
	  keydef[i].flag|= HA_BINARY_PACK_KEY;
      }
      keydef[i].seg[j].type=   (int) type;
      keydef[i].seg[j].start=  pos->key_part[j].offset;
      keydef[i].seg[j].length= pos->key_part[j].length;
      keydef[i].seg[j].bit_start= keydef[i].seg[j].bit_end=
        keydef[i].seg[j].bit_length= 0;
      keydef[i].seg[j].bit_pos= 0;
      keydef[i].seg[j].language= field->charset()->number;

      if (field->null_ptr)
      {
	keydef[i].seg[j].null_bit=field->null_bit;
	keydef[i].seg[j].null_pos= (uint) (field->null_ptr-
					   (uchar*) table_arg->record[0]);
      }
      else
      {
	keydef[i].seg[j].null_bit=0;
	keydef[i].seg[j].null_pos=0;
      }
      if (field->type() == FIELD_TYPE_BLOB ||
	  field->type() == FIELD_TYPE_GEOMETRY)
      {
	keydef[i].seg[j].flag|=HA_BLOB_PART;
	/* save number of bytes used to pack length */
	keydef[i].seg[j].bit_start= (uint) (field->pack_length() -
					    share->blob_ptr_size);
      }
      else if (field->type() == FIELD_TYPE_BIT)
      {
        keydef[i].seg[j].bit_length= ((Field_bit *) field)->bit_len;
        keydef[i].seg[j].bit_start= ((Field_bit *) field)->bit_ofs;
        keydef[i].seg[j].bit_pos= (uint) (((Field_bit *) field)->bit_ptr -
                                          (uchar*) table_arg->record[0]);
      }
    }
    keyseg+=pos->key_parts;
  }

  if (table_arg->found_next_number_field)
  {
    keydef[share->next_number_index].flag|= HA_AUTO_KEY;
    found_real_auto_increment= share->next_number_key_offset == 0;
  }

  recpos=0; recinfo_pos=recinfo;
  while (recpos < (uint) share->reclength)
  {
    Field **field,*found=0;
    minpos= share->reclength;
    length=0;

    for (field=table_arg->field ; *field ; field++)
    {
      if ((fieldpos=(*field)->offset()) >= recpos &&
	  fieldpos <= minpos)
      {
	/* skip null fields */
	if (!(temp_length= (*field)->pack_length_in_rec()))
	  continue;				/* Skip null-fields */
	if (! found || fieldpos < minpos ||
	    (fieldpos == minpos && temp_length < length))
	{
	  minpos=fieldpos; found= *field; length=temp_length;
	}
      }
    }
    DBUG_PRINT("loop",("found: 0x%lx  recpos: %d  minpos: %d  length: %d",
		       found,recpos,minpos,length));
    if (recpos != minpos)
    {						// Reserved space (Null bits?)
      bzero((char*) recinfo_pos,sizeof(*recinfo_pos));
      recinfo_pos->type=(int) FIELD_NORMAL;
      recinfo_pos++->length= (uint16) (minpos-recpos);
    }
    if (! found)
      break;

    if (found->flags & BLOB_FLAG)
      recinfo_pos->type= (int) FIELD_BLOB;
    else if (found->type() == MYSQL_TYPE_VARCHAR)
      recinfo_pos->type= FIELD_VARCHAR;
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->type= (int) FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->type= (int) FIELD_SKIP_ZERO;
    else
      recinfo_pos->type= (int) ((length <= 3 ||
                                 (found->flags & ZEROFILL_FLAG)) ?
                                FIELD_NORMAL :
                                found->type() == MYSQL_TYPE_STRING ||
                                found->type() == MYSQL_TYPE_VAR_STRING ?
                                FIELD_SKIP_ENDSPACE :
                                FIELD_SKIP_PRESPACE);
    if (found->null_ptr)
    {
      recinfo_pos->null_bit=found->null_bit;
      recinfo_pos->null_pos= (uint) (found->null_ptr-
                                     (uchar*) table_arg->record[0]);
    }
    else
    {
      recinfo_pos->null_bit=0;
      recinfo_pos->null_pos=0;
    }
    (recinfo_pos++)->length= (uint16) length;
    recpos=minpos+length;
    DBUG_PRINT("loop",("length: %d  type: %d",
		       recinfo_pos[-1].length,recinfo_pos[-1].type));

  }
  MI_CREATE_INFO create_info;
  bzero((char*) &create_info,sizeof(create_info));
  create_info.max_rows= share->max_rows;
  create_info.reloc_rows= share->min_rows;
  create_info.with_auto_increment=found_real_auto_increment;
  create_info.auto_increment=(info->auto_increment_value ?
			      info->auto_increment_value -1 :
			      (ulonglong) 0);
  create_info.data_file_length= ((ulonglong) share->max_rows *
				 share->avg_row_length);
  create_info.raid_type=info->raid_type;
  create_info.raid_chunks= (info->raid_chunks ? info->raid_chunks :
			    RAID_DEFAULT_CHUNKS);
  create_info.raid_chunksize= (info->raid_chunksize ? info->raid_chunksize :
                               RAID_DEFAULT_CHUNKSIZE);
  create_info.data_file_name=  info->data_file_name;
  create_info.index_file_name= info->index_file_name;

  if (info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags|= HA_CREATE_TMP_TABLE;
  if (options & HA_OPTION_PACK_RECORD)
    create_flags|= HA_PACK_RECORD;
  if (options & HA_OPTION_CHECKSUM)
    create_flags|= HA_CREATE_CHECKSUM;
  if (options & HA_OPTION_DELAY_KEY_WRITE)
    create_flags|= HA_CREATE_DELAY_KEY_WRITE;

  /* TODO: Check that the following fn_format is really needed */
  error=mi_create(fn_format(buff,name,"","",2+4),
		  share->keys,keydef,
		  (uint) (recinfo_pos-recinfo), recinfo,
		  0, (MI_UNIQUEDEF*) 0,
		  &create_info, create_flags);

  my_free((gptr) recinfo,MYF(0));
  DBUG_RETURN(error);
}


int ha_myisam::rename_table(const char * from, const char * to)
{
  return mi_rename(from,to);
}


ulonglong ha_myisam::get_auto_increment()
{
  ulonglong nr;
  int error;
  byte key[MI_MAX_KEY_LENGTH];

  if (!table->s->next_number_key_offset)
  {						// Autoincrement at key-start
    ha_myisam::info(HA_STATUS_AUTO);
    return auto_increment_value;
  }

  /* it's safe to call the following if bulk_insert isn't on */
  mi_flush_bulk_insert(file, table->s->next_number_index);

  (void) extra(HA_EXTRA_KEYREAD);
  key_copy(key, table->record[0],
           table->key_info + table->s->next_number_index,
           table->s->next_number_key_offset);
  error= mi_rkey(file,table->record[1],(int) table->s->next_number_index,
                 key,table->s->next_number_key_offset,HA_READ_PREFIX_LAST);
  if (error)
    nr= 1;
  else
  {
    /* Get data from record[1] */
    nr= ((ulonglong) table->next_number_field->
         val_int_offset(table->s->rec_buff_length)+1);
  }
  extra(HA_EXTRA_NO_KEYREAD);
  return nr;
}


/*
  Find out how many rows there is in the given range

  SYNOPSIS
    records_in_range()
    inx			Index to use
    min_key		Start of range.  Null pointer if from first key
    max_key		End of range. Null pointer if to last key

  NOTES
    min_key.flag can have one of the following values:
      HA_READ_KEY_EXACT		Include the key in the range
      HA_READ_AFTER_KEY		Don't include key in range

    max_key.flag can have one of the following values:  
      HA_READ_BEFORE_KEY	Don't include key in range
      HA_READ_AFTER_KEY		Include all 'end_key' values in the range

  RETURN
   HA_POS_ERROR		Something is wrong with the index tree.
   0			There is no matching keys in the given range
   number > 0		There is approximately 'number' matching rows in
			the range.
*/

ha_rows ha_myisam::records_in_range(uint inx, key_range *min_key,
                                    key_range *max_key)
{
  return (ha_rows) mi_records_in_range(file, (int) inx, min_key, max_key);
}


int ha_myisam::ft_read(byte * buf)
{
  int error;

  if (!ft_handler)
    return -1;

  thread_safe_increment(table->in_use->status_var.ha_read_next_count,
			&LOCK_status); // why ?

  error=ft_handler->please->read_next(ft_handler,(char*) buf);

  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

uint ha_myisam::checksum() const
{
  return (uint)file->s->state.checksum;
}

