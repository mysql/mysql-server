/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


#ifdef __GNUC__
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
#endif

ulong myisam_sort_buffer_size;

/*****************************************************************************
** MyISAM tables
*****************************************************************************/

// collect errors printed by mi_check routines
static void mi_check_print_msg(MI_CHECK *param,	const char* msg_type,
			       const char *fmt, va_list args)
{
  THD* thd = (THD*)param->thd;
  String* packet = &thd->packet;
  packet->length(0);
  char msgbuf[MI_MAX_MSG_BUF];
  msgbuf[0] = 0;

  my_vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0; // healthy paranoia

  if (thd->net.vio == 0)
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
  net_store_data(packet, param->table_name);
  net_store_data(packet, param->op_name);
  net_store_data(packet, msg_type);
  net_store_data(packet, msgbuf);
  if (my_net_write(&thd->net, (char*)thd->packet.ptr(), thd->packet.length()))
    fprintf(stderr,
            "Failed on my_net_write, writing to stderr instead: %s\n",
            msgbuf);
  return;
}

extern "C" {

void mi_check_print_error(MI_CHECK *param, const char *fmt,...)
{
  param->error_printed|=1;
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
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "warning", fmt, args);
  va_end(args);
}

}

const char **ha_myisam::bas_ext() const
{ static const char *ext[]= { ".MYD",".MYI", NullS }; return ext; }


int ha_myisam::net_read_dump(NET* net)
{
  int data_fd = file->dfile;
  int error = 0;

  my_seek(data_fd, 0L, MY_SEEK_SET, MYF(MY_WME));
  for(;;)
  {
    uint packet_len = my_net_read(net);
    if (!packet_len)
      break ; // end of file
    if (packet_len == packet_error)
    {
      sql_print_error("ha_myisam::net_read_dump - read error ");
      error= -1;
      goto err;
    }
    if (my_write(data_fd, (byte*)net->read_pos, packet_len,
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
  if(!buf)
    return ENOMEM;

  int error = 0;
  my_seek(data_fd, 0L, MY_SEEK_SET, MYF(MY_WME));
  for(; bytes_to_read > 0;)
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

int ha_myisam::open(const char *name, int mode, int test_if_locked)
{
  char name_buff[FN_REFLEN];
  if (!(file=mi_open(fn_format(name_buff,name,"","",2 | 4), mode,
		     test_if_locked)))
    return (my_errno ? my_errno : -1);

  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    VOID(mi_extra(file,HA_EXTRA_NO_WAIT_LOCK));
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    VOID(mi_extra(file,HA_EXTRA_WAIT_LOCK));
  if (!table->db_record_offset)
    int_option_flag|=HA_REC_NOT_IN_SEQ;
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
  statistic_increment(ha_write_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(buf+table->time_stamp-1);
  if (table->next_number_field && buf == table->record[0])
    update_auto_increment();
  return mi_write(file,buf);
}

int ha_myisam::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  int error ;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = (char*)"check";
  param.table_name = table->table_name;
  param.testflag = check_opt->flags | T_CHECK | T_SILENT | T_MEDIUM;

  if (!(table->db_stat & HA_READ_ONLY))
    param.testflag|= T_STATISTICS;
  param.using_global_keycache = 1;

  if (!mi_is_crashed(file) &&
      (((param.testflag & T_CHECK_ONLY_CHANGED) &&
	(share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				 STATE_CRASHED_ON_REPAIR)) &&
	share->state.open_count == 0) ||
       ((param.testflag & T_FAST) && share->state.open_count == 0)))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_size(&param, file);
  if (!error)
    error |= chk_del(&param, file, param.testflag);
  if (!error)
    error = chk_key(&param, file);
  if (!error)
  {
    if (!check_opt->quick &&
	(share->options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
    {
      init_io_cache(&param.read_cache, file->dfile,
		    my_default_record_cache_size, READ_CACHE,
		    share->pack.header_length, 1, MYF(MY_WME));
      error |= chk_data_link(&param, file, param.testflag & T_EXTEND);
      end_io_cache(&(param.read_cache));
    }
  }
  if (!error)
  {
    if ((share->state.changed & (STATE_CHANGED |
				 STATE_CRASHED_ON_REPAIR |
				 STATE_CRASHED | STATE_NOT_ANALYZED)) ||
	(param.testflag & T_STATISTICS))
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
  else if (!mi_is_crashed(file))
  {
    mi_mark_crashed(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
  }

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
  param.op_name = (char*)" analyze";
  param.table_name = table->table_name;
  param.testflag=(T_FAST | T_CHECK | T_SILENT | T_STATISTICS |
		  T_DONT_CHECK_CHECKSUM);
  param.using_global_keycache = 1;

  if (!(share->state.changed & STATE_NOT_ANALYZED))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_key(&param, file);
  if (!error)
  {
    pthread_mutex_lock(&share->intern_lock);
    error=update_state_info(&param,file,UPDATE_STAT);
    pthread_mutex_unlock(&share->intern_lock);
  }
  else if (!mi_is_crashed(file))
    mi_mark_crashed(file);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}

int ha_myisam::restore(THD* thd, HA_CHECK_OPT *check_opt)
{
  HA_CHECK_OPT tmp_check_opt;
  char* backup_dir = thd->lex.backup_dir;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  int backup_dir_len = strlen(backup_dir);
  char* table_name = table->real_name;
  int table_name_len = strlen(table_name);
  if(backup_dir_len + table_name_len + 4 >= FN_REFLEN)
    return HA_ADMIN_INVALID;
  memcpy(src_path, backup_dir, backup_dir_len);
  char* p = src_path + backup_dir_len;
  *p++ = '/';
  memcpy(p, table_name, table_name_len);
  p += table_name_len;
  *p = 0;
  fn_format(src_path, src_path, "", MI_NAME_DEXT, 4);

  MY_STAT stat_area;
  int error = 0;
  char* errmsg = "";
  
  
  if(my_copy(src_path, fn_format(dst_path, table->path, "",
				 MI_NAME_DEXT, 4), MYF(MY_WME)))
    {
      error = HA_ADMIN_FAILED;
      errmsg = "failed in my_copy( Error %d)";
      goto err;
    }
  
  tmp_check_opt.init();
  tmp_check_opt.quick = 1;
  return repair(thd, &tmp_check_opt);
  
 err:
  {
      MI_CHECK param;
      myisamchk_init(&param);
      param.thd = thd;
      param.op_name = (char*)"restore";
      param.table_name = table->table_name;
      param.testflag = 0;
      mi_check_print_error(&param,errmsg, errno );
      return error; 
  }
}

int ha_myisam::backup(THD* thd, HA_CHECK_OPT *check_opt)
{
  char* backup_dir = thd->lex.backup_dir;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  int backup_dir_len = strlen(backup_dir);
  char* table_name = table->real_name;
  int table_name_len = strlen(table_name);
  if(backup_dir_len + table_name_len + 4 >= FN_REFLEN)
    return HA_ADMIN_INVALID;
  memcpy(dst_path, backup_dir, backup_dir_len);
  char* p = dst_path + backup_dir_len;
  *p++ = '/';
  memcpy(p, table_name, table_name_len);
  p += table_name_len;
  *p = 0;
  if(my_copy(fn_format(src_path, table->path,"", reg_ext, 4),
	     fn_format(dst_path, dst_path, "", reg_ext, 4),
	     MYF(MY_WME | MY_HOLD_ORIGINAL_MODES )))
    {
      return HA_ADMIN_FAILED;
    }

  *p = 0;
  *(fn_ext(src_path)) = 0;
  if(my_copy(fn_format(src_path, src_path,"", MI_NAME_DEXT, 4),
	     fn_format(dst_path, dst_path, "", MI_NAME_DEXT, 4),
	     MYF(MY_WME | MY_HOLD_ORIGINAL_MODES ))  )
    return HA_ADMIN_FAILED;

  return HA_ADMIN_OK;
}


int ha_myisam::repair(THD* thd, HA_CHECK_OPT *check_opt)
{
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  MI_CHECK param;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = (char*) "repair";
  param.testflag = (check_opt->flags | T_SILENT | T_FORCE_CREATE |
		    T_REP_BY_SORT);
  if (check_opt->quick)
    param.opt_rep_quick++;
  param.sort_buffer_length=  check_opt->sort_buffer_size;
  return repair(thd,param,0);
}

int ha_myisam::optimize(THD* thd, HA_CHECK_OPT *check_opt)
{
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  MI_CHECK param;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = (char*) "optimize";
  param.testflag = (check_opt->flags | T_SILENT | T_FORCE_CREATE |
		    T_REP_BY_SORT | T_STATISTICS | T_SORT_INDEX);
  param.opt_rep_quick++;
  param.sort_buffer_length=  check_opt->sort_buffer_size;
  return repair(thd,param,1);
}


int ha_myisam::repair(THD *thd, MI_CHECK &param, bool optimize)
{
  int error=0;
  bool optimize_done= !optimize;
  char fixed_name[FN_REFLEN];
  const char *old_proc_info=thd->proc_info;
  MYISAM_SHARE* share = file->s;

  param.table_name = table->table_name;
  param.tmpfile_createflag = O_RDWR | O_TRUNC;
  param.using_global_keycache = 1;
  param.thd=thd;
    
  VOID(fn_format(fixed_name,file->filename,"",MI_NAME_IEXT,
		     4+ (param.opt_follow_links ? 16 : 0)));

  if (!optimize || file->state->del ||
      share->state.split != file->state->records)
  {
    optimize_done=1;
    if (mi_test_if_sort_rep(file,file->state->records))
    {
      param.testflag|= T_STATISTICS;		// We get this for free
      thd->proc_info="Repairing by sorting";
      error = mi_repair_by_sort(&param, file, fixed_name, param.opt_rep_quick);
    }
    else
    {
      thd->proc_info="Repairing";
      error=  mi_repair(&param, file, fixed_name, param.opt_rep_quick);
    }
  }
  if (!error)
  {
    if ((param.testflag & T_SORT_INDEX) &&
	(share->state.changed & STATE_NOT_SORTED_PAGES))
    {
      optimize_done=1;
      thd->proc_info="Sorting index";
      error=mi_sort_index(&param,file,fixed_name);
    }
    if ((param.testflag & T_STATISTICS) &&
	(share->state.changed & STATE_NOT_ANALYZED))
    {
      optimize_done=1;
      thd->proc_info="Analyzing";
      error = chk_key(&param, file);
    }
  }
  thd->proc_info="saving state";
  if (!error)
  {
    if (share->state.changed & STATE_CHANGED)
    {
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    }
    file->save_state=file->s->state.state;
    if (file->s->base.auto_key)
      update_auto_increment_key(&param, file, 1);
    error = update_state_info(&param, file,
			      UPDATE_TIME |
			      (param.testflag & T_STATISTICS ?
			       UPDATE_STAT : 0));
    info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	 HA_STATUS_CONST);
  }
  else if (!mi_is_crashed(file))
  {
    mi_mark_crashed(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
  }
  if (!error)
  {
    if (param.out_flag & (O_NEW_DATA | O_NEW_INDEX))
    {
      /*
	We have to close all instances of this file to ensure that we can
	do the rename safely and that all threads are using the new version.
      */
      thd->proc_info="renaming file";
      VOID(pthread_mutex_lock(&LOCK_open));
      if (close_cached_table(thd,table))
	error=1;

      if (param.out_flag & O_NEW_DATA)
	error|=change_to_newfile(fixed_name,MI_NAME_DEXT,
				 DATA_TMP_EXT, 0);

      if (param.out_flag & O_NEW_INDEX)
	error|=change_to_newfile(fixed_name,MI_NAME_IEXT,
				 INDEX_TMP_EXT,0);
      VOID(pthread_mutex_unlock(&LOCK_open));
    }
  }
  thd->proc_info=old_proc_info;
  return (error ? HA_ADMIN_FAILED :
          !optimize_done ? HA_ADMIN_ALREADY_DONE : HA_ADMIN_OK);
}


/* Deactive all not unique index that can be recreated fast */

void ha_myisam::deactivate_non_unique_index(ha_rows rows)
{
  if (!(specialflag & SPECIAL_SAFE_MODE))
    mi_dectivate_non_unique_index(file,rows);
}


bool ha_myisam::activate_all_index(THD *thd)
{
  int error=0;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;
  DBUG_ENTER("activate_all_index");
  if (share->state.key_map != ((ulonglong) 1L << share->base.keys)-1)
  {
    const char *save_proc_info=thd->proc_info;
    thd->proc_info="creating index";
    myisamchk_init(&param);
    param.op_name = (char*) "recreating_index";
    param.testflag = (T_SILENT | T_REP_BY_SORT |
		      T_STATISTICS | T_CREATE_MISSING_KEYS | T_TRUST_HEADER);
    param.myf_rw&= ~MY_WAIT_IF_FULL;
    param.sort_buffer_length=  myisam_sort_buffer_size;
    param.opt_rep_quick++;
    error=repair(thd,param,0) != HA_ADMIN_OK;
    thd->proc_info=save_proc_info;
  }
  DBUG_RETURN(error);
}

int ha_myisam::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(ha_update_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(new_data+table->time_stamp-1);
  return mi_update(file,old_data,new_data);
}

int ha_myisam::delete_row(const byte * buf)
{
  statistic_increment(ha_delete_count,&LOCK_status);
  return mi_delete(file,buf);
}

int ha_myisam::index_read(byte * buf, const byte * key,
			  uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  int error=mi_rkey(file,buf,active_index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_read_idx(byte * buf, uint index, const byte * key,
			      uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  int error=mi_rkey(file,buf,index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_next(byte * buf)
{
  statistic_increment(ha_read_next_count,&LOCK_status);
  int error=mi_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_prev(byte * buf)
{
  statistic_increment(ha_read_prev_count,&LOCK_status);
  int error=mi_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_first(byte * buf)
{
  statistic_increment(ha_read_first_count,&LOCK_status);
  int error=mi_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_last(byte * buf)
{
  statistic_increment(ha_read_last_count,&LOCK_status);
  int error=mi_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisam::index_next_same(byte * buf,
			       const byte *key __attribute__((unused)),
			       uint length __attribute__((unused)))
{
  statistic_increment(ha_read_next_count,&LOCK_status);
  int error=mi_rnext_same(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


int ha_myisam::rnd_init(bool scan)
{
  if (scan)
    return mi_scan_init(file);
  else
    return mi_extra(file,HA_EXTRA_RESET);
}

int ha_myisam::rnd_next(byte *buf)
{
  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
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
  statistic_increment(ha_read_rnd_count,&LOCK_status);
  int error=mi_rrnd(file, buf, ha_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_myisam::position(const byte* record)
{
  my_off_t position=mi_position(file);
  ha_store_ptr(ref, ref_length, position);
}

void ha_myisam::info(uint flag)
{
  MI_ISAMINFO info;
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
    max_data_file_length=info.max_data_file_length;
    max_index_file_length=info.max_index_file_length;
    create_time = info.create_time;
    sortkey = info.sortkey;
    ref_length=info.reflength;
    table->db_options_in_use    = info.options;
    block_size=myisam_block_size;
    table->keys_in_use &= info.key_map;
    table->db_record_offset=info.record_offset;
    if (table->key_parts)
      memcpy((char*) table->key_info[0].rec_per_key,
	     (char*) info.rec_per_key,
	     sizeof(ulong)*table->key_parts);
    raid_type=info.raid_type;
    raid_chunks=info.raid_chunks;
    raid_chunksize=info.raid_chunksize;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey  = info.errkey;
    ha_store_ptr(dupp_ref, ref_length, info.dupp_key_pos);
  }
  if (flag & HA_STATUS_TIME)
    update_time = info.update_time;
  if (flag & HA_STATUS_AUTO)
    auto_increment_value= info.auto_increment;
}


int ha_myisam::extra(enum ha_extra_function operation)
{
  if (((specialflag & SPECIAL_SAFE_MODE) || (test_flags & TEST_NO_EXTRA)) &&
      (operation == HA_EXTRA_WRITE_CACHE ||
       operation == HA_EXTRA_KEYREAD))
    return 0;
  return mi_extra(file,operation);
}

int ha_myisam::reset(void)
{
  return mi_extra(file,HA_EXTRA_RESET);
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
  return mi_lock_database(file,lock_type);
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
  table->file->info(HA_STATUS_AUTO | HA_STATUS_CONST);
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
}


int ha_myisam::create(const char *name, register TABLE *form,
		      HA_CREATE_INFO *info)
{
  int error;
  uint i,j,recpos,minpos,fieldpos,temp_length,length;
  bool found_auto_increment=0;
  enum ha_base_keytype type;
  char buff[FN_REFLEN];
  KEY *pos;
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo,*recinfo_pos;
  MI_KEYSEG *keyseg;
  uint options=form->db_options_in_use;
  DBUG_ENTER("ha_myisam::create");

  type=HA_KEYTYPE_BINARY;				// Keep compiler happy
  if (!(my_multi_malloc(MYF(MY_WME),
			&recinfo,(form->fields*2+2)*sizeof(MI_COLUMNDEF),
			&keydef, form->keys*sizeof(MI_KEYDEF),
			&keyseg,
			((form->key_parts + form->keys) * sizeof(MI_KEYSEG)),
			0)))
    DBUG_RETURN(1);

  pos=form->key_info;
  for (i=0; i < form->keys ; i++, pos++)
  {
    keydef[i].flag= (pos->flags & (HA_NOSAME | HA_FULLTEXT));
    keydef[i].seg=keyseg;
    keydef[i].keysegs=pos->key_parts;
    for (j=0 ; j < pos->key_parts ; j++)
    {
      keydef[i].seg[j].flag=pos->key_part[j].key_part_flag;
      Field *field=pos->key_part[j].field;
      type=field->key_type();

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
	      (field->type() == FIELD_TYPE_STRING ||
	       field->type() == FIELD_TYPE_VAR_STRING ||
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
      keydef[i].seg[j].bit_start=keydef[i].seg[j].bit_end=0;
      keydef[i].seg[j].language=MY_CHARSET_CURRENT;

      if (field->null_ptr)
      {
	keydef[i].seg[j].null_bit=field->null_bit;
	keydef[i].seg[j].null_pos= (uint) (field->null_ptr-
					   (uchar*) form->record[0]);
      }
      else
      {
	keydef[i].seg[j].null_bit=0;
	keydef[i].seg[j].null_pos=0;
      }
      if (j == 0 && field->flags & AUTO_INCREMENT_FLAG &&
	  !found_auto_increment)
      {
	keydef[i].flag|=HA_AUTO_KEY;
	found_auto_increment=1;
      }
      if (field->type() == FIELD_TYPE_BLOB)
      {
	keydef[i].seg[j].flag|=HA_BLOB_PART;
	/* save number of bytes used to pack length */
	keydef[i].seg[j].bit_start= (uint) (field->pack_length() -
					    form->blob_ptr_size);
      }
    }
    keyseg+=pos->key_parts;
  }

  recpos=0; recinfo_pos=recinfo;
  while (recpos < (uint) form->reclength)
  {
    Field **field,*found=0;
    minpos=form->reclength; length=0;

    for (field=form->field ; *field ; field++)
    {
      if ((fieldpos=(*field)->offset()) >= recpos &&
	  fieldpos <= minpos)
      {
	/* skip null fields */
	if (!(temp_length= (*field)->pack_length()))
	  continue;				/* Skipp null-fields */
	if (! found || fieldpos < minpos ||
	    (fieldpos == minpos && temp_length < length))
	{
	  minpos=fieldpos; found= *field; length=temp_length;
	}
      }
    }
    DBUG_PRINT("loop",("found: %lx  recpos: %d  minpos: %d  length: %d",
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
    {
      recinfo_pos->type= (int) FIELD_BLOB;
    }
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->type= (int) FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->type= (int) FIELD_SKIPP_ZERO;
    else
      recinfo_pos->type= (int) ((length <= 3 ||
				      (found->flags & ZEROFILL_FLAG)) ?
				     FIELD_NORMAL :
				     found->type() == FIELD_TYPE_STRING ||
				     found->type() == FIELD_TYPE_VAR_STRING ?
				     FIELD_SKIPP_ENDSPACE :
				     FIELD_SKIPP_PRESPACE);
    if (found->null_ptr)
    {
      recinfo_pos->null_bit=found->null_bit;
      recinfo_pos->null_pos= (uint) (found->null_ptr-
				  (uchar*) form->record[0]);
    }
    else
    {
      recinfo_pos->null_bit=0;
      recinfo_pos->null_pos=0;
    }
    (recinfo_pos++) ->length=(uint16) length;
    recpos=minpos+length;
    DBUG_PRINT("loop",("length: %d  type: %d",
		       recinfo_pos[-1].length,recinfo_pos[-1].type));

  }
  MI_CREATE_INFO create_info;
  bzero((char*) &create_info,sizeof(create_info));
  create_info.max_rows=form->max_rows;
  create_info.reloc_rows=form->min_rows;
  create_info.auto_increment=(info->auto_increment_value ?
			      info->auto_increment_value -1 :
			      (ulonglong) 0);
  create_info.data_file_length=(ulonglong) form->max_rows*form->avg_row_length;
  create_info.raid_type=info->raid_type;
  create_info.raid_chunks=info->raid_chunks ? info->raid_chunks : RAID_DEFAULT_CHUNKS;
  create_info.raid_chunksize=info->raid_chunksize ? info->raid_chunksize : RAID_DEFAULT_CHUNKSIZE;

  error=mi_create(fn_format(buff,name,"","",2+4+16),
		  form->keys,keydef,
		  (uint) (recinfo_pos-recinfo), recinfo,
		  0, (MI_UNIQUEDEF*) 0,
		  &create_info,
		  (((options & HA_OPTION_PACK_RECORD) ? HA_PACK_RECORD : 0) |
		   ((options & HA_OPTION_CHECKSUM) ? HA_CREATE_CHECKSUM : 0) |
		   ((options & HA_OPTION_DELAY_KEY_WRITE) ?
		    HA_CREATE_DELAY_KEY_WRITE : 0)));


  my_free((gptr) recinfo,MYF(0));
  DBUG_RETURN(error);
}


int ha_myisam::rename_table(const char * from, const char * to)
{
  return mi_rename(from,to);
}


longlong ha_myisam::get_auto_increment()
{
  if (!table->next_number_key_offset)
  {						// Autoincrement at key-start
    ha_myisam::info(HA_STATUS_AUTO);
    return auto_increment_value;
  }

  longlong nr;
  int error;
  byte key[MAX_KEY_LENGTH];
  (void) extra(HA_EXTRA_KEYREAD);
  key_copy(key,table,table->next_number_index,
	   table->next_number_key_offset);
  error=mi_rkey(file,table->record[1],(int) table->next_number_index,
		key,table->next_number_key_offset,HA_READ_PREFIX_LAST);
  if (error)
    nr=1;
  else
    nr=(longlong)
      table->next_number_field->val_int_offset(table->rec_buff_length)+1;
  extra(HA_EXTRA_NO_KEYREAD);
  return nr;
}


ha_rows ha_myisam::records_in_range(int inx,
				    const byte *start_key,uint start_key_len,
				    enum ha_rkey_function start_search_flag,
				    const byte *end_key,uint end_key_len,
				    enum ha_rkey_function end_search_flag)
{
  return (ha_rows) mi_records_in_range(file,
				       inx,
				       start_key,start_key_len,
				       start_search_flag,
				       end_key,end_key_len,
				       end_search_flag);
}

int ha_myisam::ft_init(uint inx, const byte *key, uint keylen, bool presort)
{
  if (ft_handler)
    return -1;

  // Do the search!
  ft_handler=ft_init_search(file,inx,(byte*) key,keylen,presort);

  if (!ft_handler)
    return (my_errno ? my_errno : -1);

  return 0;
}

int ha_myisam::ft_read(byte * buf)
{
  int error;

  if (!ft_handler)
    return -1;

  thread_safe_increment(ha_read_next_count,&LOCK_status); // why ?

  error=ft_read_next((FT_DOCLIST *) ft_handler,(char*) buf);

  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}
