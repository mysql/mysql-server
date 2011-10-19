/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Functions to check if a row is unique */

#include "maria_def.h"
#include <m_ctype.h>

/**
  Check if there exist a row with the same hash

  @notes
  This function is not versioning safe. For the moment this is not a problem
  as it's only used for internal temporary tables in MySQL for which there
  isn't any versioning information.
*/

my_bool _ma_check_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def, uchar *record,
			ha_checksum unique_hash, my_off_t disk_pos)
{
  my_off_t lastpos=info->cur_row.lastpos;
  MARIA_KEYDEF *keyinfo= &info->s->keyinfo[def->key];
  uchar *key_buff= info->lastkey_buff2;
  MARIA_KEY key;
  int error= 0;
  DBUG_ENTER("_ma_check_unique");
  DBUG_PRINT("enter",("unique_hash: %lu", (ulong) unique_hash));

  maria_unique_store(record+keyinfo->seg->start, unique_hash);
  /* Can't be spatial so it's ok to call _ma_make_key directly here */
  _ma_make_key(info, &key, def->key, key_buff, record, 0, 0);

  /* The above changed info->lastkey_buff2. Inform maria_rnext_same(). */
  info->update&= ~HA_STATE_RNEXT_SAME;

  /* Setup that unique key is active key */
  info->last_key.keyinfo= keyinfo;

  /* any key pointer in data is destroyed */
  info->lastinx= ~0;

  DBUG_ASSERT(key.data_length == MARIA_UNIQUE_HASH_LENGTH);
  if (_ma_search(info, &key, SEARCH_FIND | SEARCH_SAVE_BUFF,
                 info->s->state.key_root[def->key]))
  {
    info->page_changed=1;			/* Can't optimize read next */
    info->cur_row.lastpos= lastpos;
    goto end;
  }

  for (;;)
  {
    if (info->cur_row.lastpos != disk_pos &&
	!(*info->s->compare_unique)(info,def,record,info->cur_row.lastpos))
    {
      my_errno=HA_ERR_FOUND_DUPP_UNIQUE;
      info->errkey= (int) def->key;
      info->dup_key_pos= info->cur_row.lastpos;
      info->page_changed= 1;			/* Can't optimize read next */
      info->cur_row.lastpos= lastpos;
      DBUG_PRINT("info",("Found duplicate"));
      error= 1;                                 /* Found identical  */
      goto end;
    }
    DBUG_ASSERT(info->last_key.data_length == MARIA_UNIQUE_HASH_LENGTH);
    if (_ma_search_next(info, &info->last_key, SEARCH_BIGGER,
			info->s->state.key_root[def->key]) ||
	bcmp(info->last_key.data, key_buff, MARIA_UNIQUE_HASH_LENGTH))
    {
      info->page_changed= 1;			/* Can't optimize read next */
      info->cur_row.lastpos= lastpos;
      break;                                    /* end of tree */
    }
  }

end:
  DBUG_RETURN(error);
}


/*
  Calculate a hash for a row

  TODO
    Add support for bit fields
*/

ha_checksum _ma_unique_hash(MARIA_UNIQUEDEF *def, const uchar *record)
{
  const uchar *pos, *end;
  ha_checksum crc= 0;
  ulong seed1=0, seed2= 4;
  HA_KEYSEG *keyseg;

  for (keyseg=def->seg ; keyseg < def->end ; keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint length=keyseg->length;

    if (keyseg->null_bit)
    {
      if (record[keyseg->null_pos] & keyseg->null_bit)
      {
	/*
	  Change crc in a way different from an empty string or 0.
	  (This is an optimisation;  The code will work even if this isn't
	  done)
	*/
	crc=((crc << 8) + 511+
	     (crc >> (8*sizeof(ha_checksum)-8)));
	continue;
      }
    }
    pos= record+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint pack_length=  keyseg->bit_start;
      uint tmp_length= (pack_length == 1 ? (uint) *pos :
                        uint2korr(pos));
      pos+= pack_length;			/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint tmp_length= _ma_calc_blob_length(keyseg->bit_start,pos);
      memcpy(&pos,pos+keyseg->bit_start,sizeof(char*));
      if (!length || length > tmp_length)
	length=tmp_length;			/* The whole blob */
    }
    end= pos+length;
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT1 ||
        type == HA_KEYTYPE_VARTEXT2)
    {
      keyseg->charset->coll->hash_sort(keyseg->charset,
                                       (const uchar*) pos, length, &seed1,
                                       &seed2);
      crc+= seed1;
    }
    else
    {
      my_hash_sort_bin((CHARSET_INFO*) 0, pos, (size_t) (end-pos),
                       &seed1, &seed2);
      crc+= seed1;
    }
  }
  return crc;
}


/*
  compare unique key for two rows

  TODO
    Add support for bit fields

  RETURN
    0   if both rows have equal unique value
    1   Rows are different
*/

my_bool _ma_unique_comp(MARIA_UNIQUEDEF *def, const uchar *a, const uchar *b,
                        my_bool null_are_equal)
{
  const uchar *pos_a, *pos_b, *end;
  HA_KEYSEG *keyseg;

  for (keyseg=def->seg ; keyseg < def->end ; keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint a_length, b_length;
    a_length= b_length= keyseg->length;

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
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint pack_length= keyseg->bit_start;
      if (pack_length == 1)
      {
        a_length= (uint) *pos_a++;
        b_length= (uint) *pos_b++;
      }
      else
      {
        a_length= uint2korr(pos_a);
        b_length= uint2korr(pos_b);
        pos_a+= 2;				/* Skip VARCHAR length */
        pos_b+= 2;
      }
      set_if_smaller(a_length, keyseg->length); /* Safety */
      set_if_smaller(b_length, keyseg->length); /* safety */
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      /* Only compare 'length' characters if length != 0 */
      a_length= _ma_calc_blob_length(keyseg->bit_start,pos_a);
      b_length= _ma_calc_blob_length(keyseg->bit_start,pos_b);
      /* Check that a and b are of equal length */
      if (keyseg->length)
      {
        /*
          This is used in some cases when we are not interested in comparing
          the whole length of the blob.
        */
        set_if_smaller(a_length, keyseg->length);
        set_if_smaller(b_length, keyseg->length);
      }
      memcpy(&pos_a, pos_a+keyseg->bit_start, sizeof(char*));
      memcpy(&pos_b, pos_b+keyseg->bit_start, sizeof(char*));
    }
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT1 ||
        type == HA_KEYTYPE_VARTEXT2)
    {
      if (ha_compare_text(keyseg->charset, pos_a, a_length,
                          pos_b, b_length, 0, 1))
        return 1;
    }
    else
    {
      if (a_length != b_length)
        return 1;
      end= pos_a+a_length;
      while (pos_a != end)
      {
	if (*pos_a++ != *pos_b++)
	  return 1;
      }
    }
  }
  return 0;
}
