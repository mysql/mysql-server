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

/* Functions to check if a row is unique */

#include "myisamdef.h"
#include <m_ctype.h>

my_bool mi_check_unique(MI_INFO *info, MI_UNIQUEDEF *def, byte *record,
			ha_checksum unique_hash, my_off_t disk_pos)
{
  my_off_t lastpos=info->lastpos;
  MI_KEYDEF *key= &info->s->keyinfo[def->key];
  uchar *key_buff=info->lastkey+info->s->base.max_key_length;
  DBUG_ENTER("mi_check_unique");

  mi_unique_store(record+key->seg->start, unique_hash);
  _mi_make_key(info,def->key,key_buff,record,0);

  if (_mi_search(info,info->s->keyinfo+def->key,key_buff,MI_UNIQUE_HASH_LENGTH,
		 SEARCH_FIND,info->s->state.key_root[def->key]))
  {
    info->page_changed=1;			/* Can't optimize read next */
    info->lastpos= lastpos;
    DBUG_RETURN(0);				/* No matching rows */
  }

  for (;;)
  {
    if (info->lastpos != disk_pos &&
	!(*info->s->compare_unique)(info,def,record,info->lastpos))
    {
      my_errno=HA_ERR_FOUND_DUPP_UNIQUE;
      info->errkey= (int) def->key;
      info->dupp_key_pos= info->lastpos;
      info->page_changed=1;			/* Can't optimize read next */
      info->lastpos=lastpos;
      DBUG_PRINT("info",("Found duplicate"));
      DBUG_RETURN(1);				/* Found identical  */
    }
    if (_mi_search_next(info,info->s->keyinfo+def->key, info->lastkey,
			MI_UNIQUE_HASH_LENGTH, SEARCH_BIGGER,
			info->s->state.key_root[def->key]) ||
	bcmp(info->lastkey, key_buff, MI_UNIQUE_HASH_LENGTH))
    {
      info->page_changed=1;			/* Can't optimize read next */
      info->lastpos=lastpos;
      DBUG_RETURN(0);				/* end of tree */
    }
  }
}


/* Calculate a hash for a row */

ha_checksum mi_unique_hash(MI_UNIQUEDEF *def, const byte *record)
{
  const byte *pos, *end;
  ha_checksum crc=0;
  MI_KEYSEG *keyseg;

  for (keyseg=def->seg ; keyseg < def->end ; keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint length=keyseg->length;

    if (keyseg->null_bit)
    {
      if (record[keyseg->null_pos] & keyseg->null_bit)
	continue;
    }
    pos= record+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH)
    {
      uint tmp_length=uint2korr(pos);
      pos+=2;					/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint tmp_length=_mi_calc_blob_length(keyseg->bit_start,pos);
      memcpy_fixed((byte*) &pos,pos+keyseg->bit_start,sizeof(char*));
      if (!length || length > tmp_length)
	length=tmp_length;			/* The whole blob */
    }
    end= pos+length;
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT)
    {
      uchar *sort_order=keyseg->charset->sort_order;
      while (pos != end)
	crc=((crc << 8) +
	     (((uchar)  sort_order[*(uchar*) pos++]))) +
	  (crc >> (8*sizeof(ha_checksum)-8));
    }
    else
      while (pos != end)
	crc=((crc << 8) +
	     (((uchar)  *(uchar*) pos++))) +
	  (crc >> (8*sizeof(ha_checksum)-8));
  }
  return crc;
}

	/*
	  Returns 0 if both rows have equal unique value
	 */

int mi_unique_comp(MI_UNIQUEDEF *def, const byte *a, const byte *b,
		   my_bool null_are_equal)
{
  const byte *pos_a, *pos_b, *end;
  MI_KEYSEG *keyseg;

  for (keyseg=def->seg ; keyseg < def->end ; keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint length=keyseg->length;

    /* If part is NULL it's regarded as different */
    if (keyseg->null_bit)
    {
      uint tmp;
      if ((tmp=(a[keyseg->null_pos] & keyseg->null_bit)) !=
	  (uint) (b[keyseg->null_pos] & keyseg->null_bit))
	return 1;
      if (tmp)
      {
	if (!null_are_equal)
	  return 1;
	continue;
      }
    }
    pos_a= a+keyseg->start;
    pos_b= b+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH)
    {
      uint tmp_length=uint2korr(pos_a);
      if (tmp_length != uint2korr(pos_b))
	return 1;
      pos_a+=2;					/* Skip VARCHAR length */
      pos_b+=2;
      set_if_smaller(length,tmp_length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      /* Only compare 'length' characters if length<> 0 */
      uint a_length= _mi_calc_blob_length(keyseg->bit_start,pos_a);
      uint b_length= _mi_calc_blob_length(keyseg->bit_start,pos_b);
      /* Check that a and b are of equal length */
      if (length && a_length > length)
	a_length=length;
      if (!length || length > b_length)
	length=b_length;
      if (length != a_length)
	return 1;
      /* Both strings are at least 'length' long */
      memcpy_fixed((byte*) &pos_a,pos_a+keyseg->bit_start,sizeof(char*));
      memcpy_fixed((byte*) &pos_b,pos_b+keyseg->bit_start,sizeof(char*));
    }
    end= pos_a+length;
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT)
    {
      uchar *sort_order=keyseg->charset->sort_order;
      while (pos_a != end)
	if (sort_order[*(uchar*) pos_a++] !=
	    sort_order[*(uchar*) pos_b++])
	  return 1;
    }
    else
      while (pos_a != end)
	if (*pos_a++ != *pos_b++)
	  return 1;
  }
  return 0;
}
