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
#pragma implementation			// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_ISAM
#include <m_ctype.h>
#include <myisampack.h>
#include "ha_isam.h"
#ifndef MASTER
#include "../srclib/isam/isamdef.h"
#else
#include "../isam/isamdef.h"
#endif

/*****************************************************************************
** isam tables
*****************************************************************************/


const char **ha_isam::bas_ext() const
{ static const char *ext[]= { ".ISM",".ISD", NullS }; return ext; }

int ha_isam::open(const char *name, int mode, uint test_if_locked)
{
  char name_buff[FN_REFLEN];
  if (!(file=nisam_open(fn_format(name_buff,name,"","",2 | 4), mode,
		     test_if_locked)))
    return (my_errno ? my_errno : -1);

  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    (void) nisam_extra(file,HA_EXTRA_NO_WAIT_LOCK);
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    (void) nisam_extra(file,HA_EXTRA_WAIT_LOCK);
  if (!table->db_record_offset)
    int_table_flags|=HA_REC_NOT_IN_SEQ;
  return (0);
}

int ha_isam::close(void)
{
  return !nisam_close(file) ? 0 : my_errno ? my_errno : -1;
}

uint ha_isam::min_record_length(uint options) const
{
  return (options & HA_OPTION_PACK_RECORD) ? 1 : 5;
}


int ha_isam::write_row(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  if (table->next_number_field && buf == table->record[0])
    update_auto_increment();
  return !nisam_write(file,buf) ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(table->in_use->status_var.ha_update_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return !nisam_update(file,old_data,new_data) ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::delete_row(const byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_delete_count, &LOCK_status);
  return !nisam_delete(file,buf) ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::index_read(byte * buf, const byte * key,
			uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=nisam_rkey(file, buf, active_index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::index_read_idx(byte * buf, uint index, const byte * key,
			    uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=nisam_rkey(file, buf, index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::index_read_last(byte * buf, const byte * key, uint key_len)
{
  statistic_increment(table->in_use->status_var.ha_read_key_count,
		      &LOCK_status);
  int error=nisam_rkey(file, buf, active_index, key, key_len,
		       HA_READ_PREFIX_LAST);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::index_next(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  int error=nisam_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : HA_ERR_END_OF_FILE;
}

int ha_isam::index_prev(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_prev_count,
		      &LOCK_status);
  int error=nisam_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : HA_ERR_END_OF_FILE;
}

int ha_isam::index_first(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_first_count,
		      &LOCK_status);
  int error=nisam_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : HA_ERR_END_OF_FILE;
}

int ha_isam::index_last(byte * buf)
{
  statistic_increment(table->in_use->status_var.ha_read_last_count,
		      &LOCK_status);
  int error=nisam_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : HA_ERR_END_OF_FILE;
}

int ha_isam::rnd_init(bool scan)
{
  return nisam_extra(file,HA_EXTRA_RESET) ? 0 : my_errno ? my_errno : -1;;
}

int ha_isam::rnd_next(byte *buf)
{
  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);
  int error=nisam_rrnd(file, buf, NI_POS_ERROR);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

int ha_isam::rnd_pos(byte * buf, byte *pos)
{
  statistic_increment(table->in_use->status_var.ha_read_rnd_count,
		      &LOCK_status);
  int error=nisam_rrnd(file, buf, (ulong) my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return !error ? 0 : my_errno ? my_errno : -1;
}

void ha_isam::position(const byte *record)
{
  my_off_t position=nisam_position(file);
  if (position == (my_off_t) ~ (ulong) 0)
    position=HA_OFFSET_ERROR;
  my_store_ptr(ref, ref_length, position);
}

void ha_isam::info(uint flag)
{
  N_ISAMINFO info;
  (void) nisam_info(file,&info,flag);
  if (flag & HA_STATUS_VARIABLE)
  {
    records = info.records;
    deleted = info.deleted;
    data_file_length=info.data_file_length;
    index_file_length=info.index_file_length;
    delete_length = info.delete_length;
    check_time  = info.isamchk_time;
    mean_rec_length=info.mean_reclength;
  }
  if (flag & HA_STATUS_CONST)
  {
    max_data_file_length=info.max_data_file_length;
    max_index_file_length=info.max_index_file_length;
    create_time = info.create_time;
    sortkey = info.sortkey;
    block_size=nisam_block_size;
    table->keys	   = min(table->keys,info.keys);
    table->keys_in_use.set_prefix(table->keys);
    table->db_options_in_use= info.options;
    table->db_record_offset=
      (table->db_options_in_use &
       (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ? 0 :
      table->reclength;
    if (!table->tmp_table)
    {
      ulong *rec_per_key=info.rec_per_key;
      for (uint i=0 ; i < table->keys ; i++)
      {
	table->key_info[i].rec_per_key[table->key_info[i].key_parts-1]=
	  *(rec_per_key++);
      }
    }
    ref_length=4;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey  = info.errkey;
    my_store_ptr(dupp_ref, ref_length, info.dupp_key_pos);
  }
  if (flag & HA_STATUS_TIME)
    update_time = info.update_time;
}


int ha_isam::extra(enum ha_extra_function operation)
{
  if ((specialflag & SPECIAL_SAFE_MODE || test_flags & TEST_NO_EXTRA) &&
      (operation == HA_EXTRA_WRITE_CACHE ||
       operation == HA_EXTRA_KEYREAD))
    return 0;
  return nisam_extra(file,operation);
}

int ha_isam::external_lock(THD *thd, int lock_type)
{
  if (!table->tmp_table)
    return nisam_lock_database(file,lock_type);
  return 0;
}


THR_LOCK_DATA **ha_isam::store_lock(THD *thd,
				    THR_LOCK_DATA **to,
				    enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type=lock_type;
  *to++= &file->lock;
  return to;
}


int ha_isam::create(const char *name, register TABLE *form,
		    HA_CREATE_INFO *create_info)

{
  uint options=form->db_options_in_use;
  int error;
  uint i,j,recpos,minpos,fieldpos,temp_length,length;
  enum ha_base_keytype type;
  char buff[FN_REFLEN];
  KEY *pos;
  N_KEYDEF keydef[MAX_KEY];
  N_RECINFO *recinfo,*recinfo_pos;
  DBUG_ENTER("ha_isam::create");

  type=HA_KEYTYPE_BINARY;				// Keep compiler happy
  if (!(recinfo= (N_RECINFO*) my_malloc((form->fields*2+2)*sizeof(N_RECINFO),
					MYF(MY_WME))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  pos=form->key_info;
  for (i=0; i < form->keys ; i++, pos++)
  {
    keydef[i].base.flag= (pos->flags & HA_NOSAME);
    for (j=0 ; (int7) j < pos->key_parts ; j++)
    {
      keydef[i].seg[j].base.flag=pos->key_part[j].key_part_flag;
      Field *field=pos->key_part[j].field;
      type=field->key_type();

      if ((options & HA_OPTION_PACK_KEYS ||
	   (pos->flags & (HA_PACK_KEY | HA_BINARY_PACK_KEY |
			  HA_SPACE_PACK_USED))) &&
	  pos->key_part[j].length > 8 &&
	  (type == HA_KEYTYPE_TEXT ||
	   type == HA_KEYTYPE_NUM ||
	   (type == HA_KEYTYPE_BINARY && !field->zero_pack())))
      {
	if (j == 0)
	  keydef[i].base.flag|=HA_PACK_KEY;
	if (!(field->flags & ZEROFILL_FLAG) &&
	    (field->type() == FIELD_TYPE_STRING ||
	     field->type() == FIELD_TYPE_VAR_STRING ||
	     ((int) (pos->key_part[j].length - field->decimals()))
	     >= 4))
	  keydef[i].seg[j].base.flag|=HA_SPACE_PACK;
      }
      keydef[i].seg[j].base.type=(int) type;
      keydef[i].seg[j].base.start=  pos->key_part[j].offset;
      keydef[i].seg[j].base.length= pos->key_part[j].length;
    }
    keydef[i].seg[j].base.type=(int) HA_KEYTYPE_END;	/* End of key-parts */
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
      recinfo_pos->base.type=(int) FIELD_NORMAL;
      recinfo_pos++->base.length= (uint16) (minpos-recpos);
    }
    if (! found)
      break;

    if (found->flags & BLOB_FLAG)
    {
      /* ISAM can only handle blob pointers of sizeof(char(*)) */
      recinfo_pos->base.type= (int) FIELD_BLOB;
      if (options & HA_OPTION_LONG_BLOB_PTR)
	length= length-portable_sizeof_char_ptr+sizeof(char*);
    }
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->base.type= (int) FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->base.type= (int) FIELD_SKIP_ZERO;
    else
      recinfo_pos->base.type= (int) ((length <= 3 ||
				      (found->flags & ZEROFILL_FLAG)) ?
				     FIELD_NORMAL :
				     found->type() == FIELD_TYPE_STRING ||
				     found->type() == FIELD_TYPE_VAR_STRING ?
				     FIELD_SKIP_ENDSPACE :
				     FIELD_SKIP_PRESPACE);
    recinfo_pos++ ->base.length=(uint16) length;
    recpos=minpos+length;
    DBUG_PRINT("loop",("length: %d  type: %d",
		       recinfo_pos[-1].base.length,recinfo_pos[-1].base.type));

    if ((found->flags & BLOB_FLAG) && (options & HA_OPTION_LONG_BLOB_PTR) &&
	sizeof(char*) != portable_sizeof_char_ptr)
    {						// Not used space
      recinfo_pos->base.type=(int) FIELD_ZERO;
      recinfo_pos++->base.length=
	(uint16) (portable_sizeof_char_ptr-sizeof(char*));
      recpos+= (portable_sizeof_char_ptr-sizeof(char*));
    }
  }
  recinfo_pos->base.type= (int) FIELD_LAST;	/* End of fieldinfo */
  error=nisam_create(fn_format(buff,name,"","",2+4+16),form->keys,keydef,
		     recinfo,(ulong) form->max_rows, (ulong) form->min_rows,
		     0, 0, 0L);
  my_free((gptr) recinfo,MYF(0));
  DBUG_RETURN(error);

}

static key_range no_range= { (byte*) 0, 0, HA_READ_KEY_EXACT };

ha_rows ha_isam::records_in_range(uint inx, key_range *min_key,
                                  key_range *max_key)
{
  /* ISAM checks if 'key' pointer <> 0 to know if there is no range */
  if (!min_key)
    min_key= &no_range;
  if (!max_key)
    max_key= &no_range;
  return (ha_rows) nisam_records_in_range(file,
                                          (int) inx,
                                          min_key->key, min_key->length,
                                          min_key->flag,
                                          max_key->key, max_key->length,
                                          max_key->flag);
}
#endif /* HAVE_ISAM */
