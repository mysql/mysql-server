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

/* Functions to handle keys */

#include "myisamdef.h"
#include "m_ctype.h"

#define CHECK_KEYS

static int _mi_put_key_in_record(MI_INFO *info,uint keynr,byte *record);

	/*
	** Make a intern key from a record
	** Ret: Length of key
	*/

uint _mi_make_key(register MI_INFO *info, uint keynr, uchar *key,
		  const byte *record, my_off_t filepos)
{
  byte *pos,*end;
  uchar *start;
  reg1 MI_KEYSEG *keyseg;
  DBUG_ENTER("_mi_make_key");

  start=key;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->type ;keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint length=keyseg->length;

    if (keyseg->null_bit)
    {
      if (record[keyseg->null_pos] & keyseg->null_bit)
      {
	*key++= 0;				/* NULL in key */
	continue;
      }
      *key++=1;					/* Not NULL */
    }

    pos= (byte*) record+keyseg->start;
    if (keyseg->flag & HA_SPACE_PACK)
    {
      end=pos+length;
      if (type != HA_KEYTYPE_NUM)
      {
	while (end > pos && end[-1] == ' ')
	  end--;
      }
      else
      {
	while (pos < end && pos[0] == ' ')
	  pos++;
      }
      length=(uint) (end-pos);
      store_key_length_inc(key,length);
      memcpy((byte*) key,(byte*) pos,(size_t) length);
      key+=length;
      continue;
    }
    if (keyseg->flag & HA_VAR_LENGTH)
    {
      uint tmp_length=uint2korr(pos);
      pos+=2;					/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
      store_key_length_inc(key,length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint tmp_length=_mi_calc_blob_length(keyseg->bit_start,pos);
      memcpy_fixed((byte*) &pos,pos+keyseg->bit_start,sizeof(char*));
      set_if_smaller(length,tmp_length);
      store_key_length_inc(key,length);
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {						/* Numerical column */
#ifdef NAN_TEST
      float float_nr;
      double dbl_nr;
      if (type == HA_KEYTYPE_FLOAT)
      {
	float_nr=float4get(pos);
	if (float_nr == (float) FLT_MAX)
	{
	  float_nr= (float) FLT_MAX;
	  pos= (byte*) &float_nr;
	}
      }
      else if (type == HA_KEYTYPE_DOUBLE)
      {
	dbl_nr=float8get(key);
	if (dbl_nr == DBL_MAX)
	{
	  dbl_nr=DBL_MAX;
	  pos=(byte*) &dbl_nr;
	}
      }
#endif
      pos+=length;
      while (length--)
      {
	*key++ = *--pos;
      }
      continue;
    }
    memcpy((byte*) key, pos, length);
    key+= length;
  }
  _mi_dpointer(info,key,filepos);
  DBUG_PRINT("exit",("keynr: %d",keynr));
  DBUG_DUMP("key",(byte*) start,(uint) (key-start)+keyseg->length);
  DBUG_EXECUTE("key",
	       _mi_print_key(DBUG_FILE,info->s->keyinfo[keynr].seg,start,
			     (uint) (key-start)););
  DBUG_RETURN((uint) (key-start));		/* Return keylength */
} /* _mi_make_key */


	/* Pack a key to intern format from given format (c_rkey) */
	/* returns length of packed key */

uint _mi_pack_key(register MI_INFO *info, uint keynr, uchar *key, uchar *old,
		  uint k_length)
{
  uint length;
  uchar *pos,*end,*start_key=key;
  reg1 MI_KEYSEG *keyseg;
  enum ha_base_keytype type;
  DBUG_ENTER("_mi_pack_key");

  start_key=key;
  for (keyseg=info->s->keyinfo[keynr].seg ;
       keyseg->type && (int) k_length > 0;
       old+=keyseg->length, keyseg++)
  {
    length=min((uint) keyseg->length,(uint) k_length);
    type=(enum ha_base_keytype) keyseg->type;
    if (keyseg->null_bit)
    {
      k_length--;
      if (!(*key++= (char) 1-*old++))			/* Copy null marker */
      {
	k_length-=length;
	continue;					/* Found NULL */
      }
    }
    pos=old;
    if (keyseg->flag & HA_SPACE_PACK)
    {
      end=pos+length;
      if (type != HA_KEYTYPE_NUM)
      {
	while (end > pos && end[-1] == ' ')
	  end--;
      }
      else
      {
	while (pos < end && pos[0] == ' ')
	  pos++;
      }
      k_length-=length;
      length=(uint) (end-pos);
      store_key_length_inc(key,length);
      memcpy((byte*) key,pos,(size_t) length);
      key+= length;
      continue;
    }
    else if (keyseg->flag & (HA_VAR_LENGTH | HA_BLOB_PART))
    {
      /* Length of key-part used with mi_rkey() always 2 */
      uint tmp_length=uint2korr(pos);
      k_length-= 2+length;
      set_if_smaller(length,tmp_length);	/* Safety */
      store_key_length_inc(key,length);
      old+=2;					/* Skipp length */
      memcpy((byte*) key, pos+2,(size_t) length);
      key+= length;
      continue;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {						/* Numerical column */
      pos+=length;
      k_length-=length;
      while (length--)
      {
	*key++ = *--pos;
      }
      continue;
    }
    memcpy((byte*) key,pos,(size_t) length);
    key+= length;
    k_length-=length;
  }

#ifdef NOT_USED
  if (keyseg->type)
  {
    /* Part-key ; fill with ASCII 0 for easier searching */
    length= (uint) -k_length;			/* unused part of last key */
    do
    {
      if (keyseg->flag & HA_NULL_PART)
	length++;
      if (keyseg->flag & HA_SPACE_PACK)
	length+=2;
      else
	length+= keyseg->length;
      keyseg++;
    } while (keyseg->type);
    bzero((byte*) key,length);
    key+=length;
  }
#endif
  DBUG_RETURN((uint) (key-start_key));
} /* _mi_pack_key */


	/* Put a key in record */
	/* Used when only-keyread is wanted */

static int _mi_put_key_in_record(register MI_INFO *info, uint keynr,
				 byte *record)
{
  reg2 byte *key;
  byte *pos,*key_end;
  reg1 MI_KEYSEG *keyseg;
  byte *blob_ptr;
  DBUG_ENTER("_mi_put_key_in_record");

  if (info->blobs && info->s->keyinfo[keynr].flag & HA_VAR_LENGTH_KEY)
  {
    if (!(blob_ptr=
	  mi_fix_rec_buff_for_blob(info, info->s->keyinfo[keynr].keylength)))
      goto err;
  }
  key=(byte*) info->lastkey;
  key_end=key+info->lastkey_length;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->type ;keyseg++)
  {
    if (keyseg->null_bit)
    {
      if (!*key++)
      {
	record[keyseg->null_pos]|= keyseg->null_bit;
	continue;
      }
      record[keyseg->null_pos]&= ~keyseg->null_bit;
    }
    if (keyseg->flag & HA_SPACE_PACK)
    {
      uint length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      pos= record+keyseg->start;
      if (keyseg->type != (int) HA_KEYTYPE_NUM)
      {
	memcpy(pos,key,(size_t) length);
	bfill(pos+length,keyseg->length-length,' ');
      }
      else
      {
	bfill(pos,keyseg->length-length,' ');
	memcpy(pos+keyseg->length-length,key,(size_t) length);
      }
      key+=length;
      continue;
    }

    if (keyseg->flag & HA_VAR_LENGTH)
    {
      uint length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      memcpy(record+keyseg->start,(byte*) key, length);
      key+= length;
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      memcpy(record+keyseg->start+keyseg->bit_start,
	     (char*) &blob_ptr,sizeof(char*));
      memcpy(blob_ptr,key,length);
      blob_ptr+=length;
      _my_store_blob_length(record+keyseg->start,
			    (uint) keyseg->bit_start,length);
      key+=length;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {
      byte *to=  record+keyseg->start+keyseg->length;
      byte *end= key+keyseg->length;
#ifdef CHECK_KEYS
      if (end > key_end)
	goto err;
#endif
      do
      {
	 *--to= *key++;
      } while (key != end);
      continue;
    }
    else
    {
#ifdef CHECK_KEYS
      if (key+keyseg->length > key_end)
	goto err;
#endif
      memcpy(record+keyseg->start,(byte*) key,
	     (size_t) keyseg->length);
      key+= keyseg->length;
    }
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);				/* Crashed row */
} /* _mi_put_key_in_record */


	/* Here when key reads are used */

int _mi_read_key_record(MI_INFO *info, my_off_t filepos, byte *buf)
{
  fast_mi_writeinfo(info);
  if (filepos != HA_OFFSET_ERROR)
  {
    if (info->lastinx >= 0)
    {				/* Read only key */
      if (_mi_put_key_in_record(info,(uint) info->lastinx,buf))
      {
	my_errno=HA_ERR_CRASHED;
	return -1;
      }
      info->update|= HA_STATE_AKTIV; /* We should find a record */
      return 0;
    }
    my_errno=HA_ERR_WRONG_INDEX;
  }
  return(-1);				/* Wrong data to read */
}


	/* Update auto_increment info */

void update_auto_increment(MI_INFO *info,const byte *record)
{
  ulonglong value;
  MI_KEYSEG *keyseg=info->s->keyinfo[info->s->base.auto_key-1].seg;
  const uchar *key=(uchar*) record+keyseg->start;

  switch (keyseg->type) {
  case HA_KEYTYPE_INT8:
  case HA_KEYTYPE_BINARY:
    value=(ulonglong)  *(uchar*) key;
    break;
  case HA_KEYTYPE_SHORT_INT:
  case HA_KEYTYPE_USHORT_INT:
    value=(ulonglong) uint2korr(key);
    break;
  case HA_KEYTYPE_LONG_INT:
  case HA_KEYTYPE_ULONG_INT:
    value=(ulonglong) uint4korr(key);
    break;
  case HA_KEYTYPE_INT24:
  case HA_KEYTYPE_UINT24:
    value=(ulonglong) uint3korr(key);
    break;
  case HA_KEYTYPE_FLOAT:			/* This shouldn't be used */
  {
    float f_1;
    float4get(f_1,key);
    value = (ulonglong) f_1;
    break;
  }
  case HA_KEYTYPE_DOUBLE:			/* This shouldn't be used */
  {
    double f_1;
    float8get(f_1,key);
    value = (ulonglong) f_1;
    break;
  }
  case HA_KEYTYPE_LONGLONG:
  case HA_KEYTYPE_ULONGLONG:
    value= uint8korr(key);
    break;
  default:
    value=0;					/* Error */
    break;
  }
  set_if_bigger(info->s->state.auto_increment,value);
}
