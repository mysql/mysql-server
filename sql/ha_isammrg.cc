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
#ifdef HAVE_ISAM
#include <m_ctype.h>
#ifndef MASTER
#include "../srclib/merge/mrg_def.h"
#else
#include "../merge/mrg_def.h"
#endif
#include "ha_isammrg.h"

/*****************************************************************************
** ISAM MERGE tables
*****************************************************************************/

const char **ha_isammrg::bas_ext() const
{ static const char *ext[]= { ".MRG", NullS }; return ext; }

int ha_isammrg::open(const char *name, int mode, uint test_if_locked)
{
  char name_buff[FN_REFLEN];
  if (!(file=mrg_open(fn_format(name_buff,name,"","",2 | 4), mode,
		       test_if_locked)))
    return (my_errno ? my_errno : -1);

  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    mrg_extra(file,HA_EXTRA_NO_WAIT_LOCK);
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    mrg_extra(file,HA_EXTRA_WAIT_LOCK);
  if (table->reclength != mean_rec_length)
  {
    DBUG_PRINT("error",("reclength: %d  mean_rec_length: %d",
			table->reclength, mean_rec_length));
    mrg_close(file);
    file=0;
    return ER_WRONG_MRG_TABLE;
  }
  return (0);
}

int ha_isammrg::close(void)
{
  return !mrg_close(file) ? 0 : my_errno ? my_errno : -1;
}

uint ha_isammrg::min_record_length(uint options) const
{
  return (options & HA_OPTION_PACK_RECORD) ? 1 : 5;
}

int ha_isammrg::write_row(byte * buf)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(current_thd->status_var.ha_update_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return !mrg_update(file,old_data,new_data) ? 0 : my_errno ? my_errno : -1;
}

int ha_isammrg::delete_row(const byte * buf)
{
  statistic_increment(current_thd->status_var.ha_delete_count, &LOCK_status);
  return !mrg_delete(file,buf) ? 0 : my_errno ? my_errno : -1;
}

int ha_isammrg::index_read(byte * buf, const byte * key,
			   uint key_len, enum ha_rkey_function find_flag)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::index_read_idx(byte * buf, uint index, const byte * key,
			       uint key_len, enum ha_rkey_function find_flag)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::index_next(byte * buf)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::index_prev(byte * buf)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::index_first(byte * buf)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::index_last(byte * buf)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_isammrg::rnd_init(bool scan)
{
  return !mrg_extra(file,HA_EXTRA_RESET) ? 0 : my_errno ? my_errno : -1;
}

int ha_isammrg::rnd_next(byte *buf)
{
  statistic_increment(current_thd->status_var.ha_read_rnd_next_count,
		      &LOCK_status);
  int error=mrg_rrnd(file, buf, ~(mrg_off_t) 0);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

int ha_isammrg::rnd_pos(byte * buf, byte *pos)
{
  statistic_increment(current_thd->status_var.ha_read_rnd_count, &LOCK_status);
  int error=mrg_rrnd(file, buf, (ulong) my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

void ha_isammrg::position(const byte *record)
{
  ulong position= mrg_position(file);
  my_store_ptr(ref, ref_length, (my_off_t) position);
}


void ha_isammrg::info(uint flag)
{
  MERGE_INFO info;
  (void) mrg_info(file,&info,flag);
  records = (ha_rows) info.records;
  deleted = (ha_rows) info.deleted;
  data_file_length=info.data_file_length;
  errkey  = info.errkey;
  table->keys_in_use.clear_all();               // No keys yet
  table->db_options_in_use    = info.options;
  mean_rec_length=info.reclength;
  block_size=0;
  update_time=0;
  ref_length=4;					// Should be big enough
}


int ha_isammrg::extra(enum ha_extra_function operation)
{
  return !mrg_extra(file,operation) ? 0 : my_errno ? my_errno : -1;
}

int ha_isammrg::external_lock(THD *thd, int lock_type)
{
  return !mrg_lock_database(file,lock_type) ? 0 : my_errno ? my_errno : -1;
}

uint ha_isammrg::lock_count(void) const
{
  return file->tables;
}

THR_LOCK_DATA **ha_isammrg::store_lock(THD *thd,
				       THR_LOCK_DATA **to,
				       enum thr_lock_type lock_type)
{
  MRG_TABLE *open_table;

  for (open_table=file->open_tables ;
       open_table != file->end_table ;
       open_table++)
  {
    *(to++)= &open_table->table->lock;
    if (lock_type != TL_IGNORE && open_table->table->lock.type == TL_UNLOCK)
      open_table->table->lock.type=lock_type;
  }
  return to;
}


int ha_isammrg::create(const char *name, register TABLE *form,
		       HA_CREATE_INFO *create_info)

{
  char buff[FN_REFLEN];
  return mrg_create(fn_format(buff,name,"","",2+4+16),0);
}
#endif /* HAVE_ISAM */
