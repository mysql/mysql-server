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
#include <myisampack.h>
#include "ha_heap.h"

/*****************************************************************************
** HEAP tables
*****************************************************************************/

const char **ha_heap::bas_ext() const
{ static const char *ext[1]= { NullS }; return ext; }


int ha_heap::open(const char *name, int mode, int test_if_locked)
{
  uint key,part,parts;
  HP_KEYDEF *keydef;
  HP_KEYSEG *seg;

  for (key=parts=0 ; key < table->keys ; key++)
    parts+=table->key_info[key].key_parts;

  if (!(keydef=(HP_KEYDEF*) my_malloc(table->keys*sizeof(HP_KEYDEF)+
				      parts*sizeof(HP_KEYSEG),MYF(MY_WME))))
    return my_errno;
  seg=my_reinterpret_cast(HP_KEYSEG*) (keydef+table->keys);
  for (key=0 ; key < table->keys ; key++)
  {
    KEY *pos=table->key_info+key;

    keydef[key].keysegs=(uint) pos->key_parts;
    keydef[key].flag = (pos->flags & HA_NOSAME);
    keydef[key].seg=seg;
    
    for (part=0 ; part < pos->key_parts ; part++)
    {
      uint flag=pos->key_part[part].key_type;
      if (!f_is_packed(flag) &&
	  f_packtype(flag) == (int) FIELD_TYPE_DECIMAL &&
	  !(flag & FIELDFLAG_BINARY))
	seg->type= (int) HA_KEYTYPE_TEXT;
      else
	seg->type= (int) HA_KEYTYPE_BINARY;
      seg->start=(uint) pos->key_part[part].offset;
      seg->length=(uint) pos->key_part[part].length;
      seg++;
    }
  }
  file=heap_open(table->path,mode,
		 table->keys,keydef,
		 table->reclength,table->max_rows,
		 table->min_rows);
  my_free((gptr) keydef,MYF(0));
  info(HA_STATUS_NO_LOCK | HA_STATUS_CONST | HA_STATUS_VARIABLE);
  ref_length=sizeof(HEAP_PTR);
  return (!file ? errno : 0);
}

int ha_heap::close(void)
{
  return heap_close(file);
}

int ha_heap::write_row(byte * buf)
{
  statistic_increment(ha_write_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(buf+table->time_stamp-1);
  return heap_write(file,buf);
}

int ha_heap::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(ha_update_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(new_data+table->time_stamp-1);
  return heap_update(file,old_data,new_data);
}

int ha_heap::delete_row(const byte * buf)
{
  statistic_increment(ha_delete_count,&LOCK_status);
  return heap_delete(file,buf);
}

int ha_heap::index_read(byte * buf, const byte * key,
			uint key_len __attribute__((unused)),
			enum ha_rkey_function find_flag
			__attribute__((unused)))
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  int error=heap_rkey(file,buf,active_index, key);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_read_idx(byte * buf, uint index, const byte * key,
			    uint key_len __attribute__((unused)),
			    enum ha_rkey_function find_flag
			    __attribute__((unused)))
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  int error=heap_rkey(file, buf, index, key);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


int ha_heap::index_next(byte * buf)
{
  statistic_increment(ha_read_next_count,&LOCK_status);
  int error=heap_rnext(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_prev(byte * buf)
{
  statistic_increment(ha_read_prev_count,&LOCK_status);
  int error=heap_rprev(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}
  
int ha_heap::index_first(byte * buf)
{
  statistic_increment(ha_read_first_count,&LOCK_status);
  int error=heap_rfirst(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_last(byte * buf)
{
  statistic_increment(ha_read_last_count,&LOCK_status);
  int error=heap_rlast(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::rnd_init(bool scan)
{
  return scan ? heap_scan_init(file) : 0;
}

int ha_heap::rnd_next(byte *buf)
{
  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  int error=heap_scan(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::rnd_pos(byte * buf, byte *pos)
{
  int error;
  HEAP_PTR position;
  statistic_increment(ha_read_rnd_count,&LOCK_status);
  memcpy_fixed((char*) &position,pos,sizeof(HEAP_PTR));
  error=heap_rrnd(file, buf, position);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_heap::position(const byte *record)
{
  *(HEAP_PTR*) ref= heap_position(file);	// Ref is aligned
}

void ha_heap::info(uint flag)
{
  HEAPINFO info;
  (void) heap_info(file,&info,flag);

  records = info.records;
  deleted = info.deleted;
  errkey  = info.errkey;
  mean_rec_length=info.reclength;
  data_file_length=info.data_length;
  index_file_length=info.index_length;
  max_data_file_length= info.max_records* info.reclength;
  delete_length= info.deleted * info.reclength;
}

int ha_heap::extra(enum ha_extra_function operation)
{
  return heap_extra(file,operation);
}

int ha_heap::reset(void)
{
  return heap_extra(file,HA_EXTRA_RESET);
}

int ha_heap::delete_all_rows()
{
  heap_clear(file);
  return 0;
}

int ha_heap::external_lock(THD *thd, int lock_type)
{
  return 0;					// No external locking
}  

THR_LOCK_DATA **ha_heap::store_lock(THD *thd,
				    THR_LOCK_DATA **to,
				    enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type=lock_type;
  *to++= &file->lock;
  return to;
}


/*
  We have to ignore ENOENT entries as the HEAP table is created on open and
  not when doing a CREATE on the table.
*/

int ha_heap::delete_table(const char *name)
{
  int error=heap_delete_all(name);
  return error == ENOENT ? 0 : error;
}

int ha_heap::rename_table(const char * from, const char * to)
{
  return heap_rename(from,to);
}


ha_rows ha_heap::records_in_range(int inx,
				  const byte *start_key,uint start_key_len,
				  enum ha_rkey_function start_search_flag,
				  const byte *end_key,uint end_key_len,
				  enum ha_rkey_function end_search_flag)
{
  KEY *pos=table->key_info+inx;
  if (start_key_len != end_key_len ||
      start_key_len != pos->key_length ||
      start_search_flag != HA_READ_KEY_EXACT ||
      end_search_flag != HA_READ_KEY_EXACT)
    return HA_POS_ERROR;			// Can't only use exact keys
  return 10;					// Good guess
}

/* We can just delete the heap on creation */

int ha_heap::create(const char *name, TABLE *form, HA_CREATE_INFO *create_info)

{
  char buff[FN_REFLEN];
  return heap_create(fn_format(buff,name,"","",2));
}
