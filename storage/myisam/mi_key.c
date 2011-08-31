/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Functions to handle keys */

#include "myisamdef.h"
#include "m_ctype.h"
#include "sp_defs.h"
#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#define CHECK_KEYS                              /* Enable safety checks */

#define FIX_LENGTH(cs, pos, length, char_length)                            \
            do {                                                            \
              if (length > char_length)                                     \
                char_length= my_charpos(cs, pos, pos+length, char_length);  \
              set_if_smaller(char_length,length);                           \
            } while(0)

static int _mi_put_key_in_record(MI_INFO *info,uint keynr,uchar *record);

/*
  Make a intern key from a record

  SYNOPSIS
    _mi_make_key()
    info		MyiSAM handler
    keynr		key number
    key			Store created key here
    record		Record
    filepos		Position to record in the data file

  RETURN
    Length of key
*/

uint _mi_make_key(register MI_INFO *info, uint keynr, uchar *key,
		  const uchar *record, my_off_t filepos)
{
  uchar *pos;
  uchar *start;
  reg1 HA_KEYSEG *keyseg;
  my_bool is_ft= info->s->keyinfo[keynr].flag & HA_FULLTEXT;
  DBUG_ENTER("_mi_make_key");

  if (info->s->keyinfo[keynr].flag & HA_SPATIAL)
  {
    /*
      TODO: nulls processing
    */
#ifdef HAVE_SPATIAL
    DBUG_RETURN(sp_make_key(info,keynr,key,record,filepos));
#else
    DBUG_ASSERT(0); /* mi_open should check that this never happens*/
#endif
  }

  start=key;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->type ;keyseg++)
  {
    enum ha_base_keytype type=(enum ha_base_keytype) keyseg->type;
    uint length=keyseg->length;
    uint char_length;
    CHARSET_INFO *cs=keyseg->charset;

    if (keyseg->null_bit)
    {
      if (record[keyseg->null_pos] & keyseg->null_bit)
      {
	*key++= 0;				/* NULL in key */
	continue;
      }
      *key++=1;					/* Not NULL */
    }

    char_length= ((!is_ft && cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen :
                  length);

    pos= (uchar*) record+keyseg->start;
    if (type == HA_KEYTYPE_BIT)
    {
      if (keyseg->bit_length)
      {
        uchar bits= get_rec_bits((uchar*) record + keyseg->bit_pos,
                                 keyseg->bit_start, keyseg->bit_length);
        *key++= bits;
        length--;
      }
      memcpy((uchar*) key, pos, length);
      key+= length;
      continue;
    }
    if (keyseg->flag & HA_SPACE_PACK)
    {
      if (type != HA_KEYTYPE_NUM)
      {
        length= cs->cset->lengthsp(cs, (char*) pos, length);
      }
      else
      {
        uchar *end= pos + length;
	while (pos < end && pos[0] == ' ')
	  pos++;
	length=(uint) (end-pos);
      }
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy((uchar*) key,(uchar*) pos,(size_t) char_length);
      key+=char_length;
      continue;
    }
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint pack_length= (keyseg->bit_start == 1 ? 1 : 2);
      uint tmp_length= (pack_length == 1 ? (uint) *(uchar*) pos :
                        uint2korr(pos));
      pos+= pack_length;			/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy((uchar*) key,(uchar*) pos,(size_t) char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint tmp_length=_mi_calc_blob_length(keyseg->bit_start,pos);
      memcpy(&pos,pos+keyseg->bit_start,sizeof(char*));
      set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy((uchar*) key,(uchar*) pos,(size_t) char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {						/* Numerical column */
#ifdef HAVE_ISNAN
      if (type == HA_KEYTYPE_FLOAT)
      {
	float nr;
	float4get(nr,pos);
	if (isnan(nr))
	{
	  /* Replace NAN with zero */
	  bzero(key,length);
	  key+=length;
	  continue;
	}
      }
      else if (type == HA_KEYTYPE_DOUBLE)
      {
	double nr;
	float8get(nr,pos);
	if (isnan(nr))
	{
	  bzero(key,length);
	  key+=length;
	  continue;
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
    FIX_LENGTH(cs, pos, length, char_length);
    memcpy((uchar*) key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char*) key+char_length, length-char_length, ' ');
    key+= length;
  }
  _mi_dpointer(info,key,filepos);
  DBUG_PRINT("exit",("keynr: %d",keynr));
  DBUG_DUMP("key",(uchar*) start,(uint) (key-start)+keyseg->length);
  DBUG_EXECUTE("key",
	       _mi_print_key(DBUG_FILE,info->s->keyinfo[keynr].seg,start,
			     (uint) (key-start)););
  DBUG_RETURN((uint) (key-start));		/* Return keylength */
} /* _mi_make_key */


/*
  Pack a key to intern format from given format (c_rkey)

  SYNOPSIS
    _mi_pack_key()
    info		MyISAM handler
    uint keynr		key number
    key			Store packed key here
    old			Not packed key
    keypart_map         bitmap of used keyparts
    last_used_keyseg	out parameter.  May be NULL

   RETURN
     length of packed key

     last_use_keyseg    Store pointer to the keyseg after the last used one
*/

uint _mi_pack_key(register MI_INFO *info, uint keynr, uchar *key, uchar *old,
                  key_part_map keypart_map, HA_KEYSEG **last_used_keyseg)
{
  uchar *start_key=key;
  HA_KEYSEG *keyseg;
  my_bool is_ft= info->s->keyinfo[keynr].flag & HA_FULLTEXT;
  DBUG_ENTER("_mi_pack_key");

  /* "one part" rtree key is 2*SPDIMS part key in MyISAM */
  if (info->s->keyinfo[keynr].key_alg == HA_KEY_ALG_RTREE)
    keypart_map= (((key_part_map)1) << (2*SPDIMS)) - 1;

  /* only key prefixes are supported */
  DBUG_ASSERT(((keypart_map+1) & keypart_map) == 0);

  for (keyseg= info->s->keyinfo[keynr].seg ; keyseg->type && keypart_map;
       old+= keyseg->length, keyseg++)
  {
    enum ha_base_keytype type= (enum ha_base_keytype) keyseg->type;
    uint length= keyseg->length;
    uint char_length;
    uchar *pos;
    CHARSET_INFO *cs=keyseg->charset;

    keypart_map>>= 1;
    if (keyseg->null_bit)
    {
      if (!(*key++= (char) 1-*old++))			/* Copy null marker */
      {
        if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
          old+= 2;
	continue;					/* Found NULL */
      }
    }
    char_length= (!is_ft && cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen : length;
    pos=old;
    if (keyseg->flag & HA_SPACE_PACK)
    {
      if (type == HA_KEYTYPE_NUM)
      {
        uchar *end= pos + length;
        while (pos < end && pos[0] == ' ')
          pos++;
        length= (uint) (end - pos);
      }
      else if (type != HA_KEYTYPE_BINARY)
      {
        length= cs->cset->lengthsp(cs, (char*) pos, length);
      }
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy((uchar*) key,pos,(size_t) char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
    {
      /* Length of key-part used with mi_rkey() always 2 */
      uint tmp_length=uint2korr(pos);
      pos+=2;
      set_if_smaller(length,tmp_length);	/* Safety */
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      old+=2;					/* Skip length */
      memcpy((uchar*) key, pos,(size_t) char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {						/* Numerical column */
      pos+=length;
      while (length--)
	*key++ = *--pos;
      continue;
    }
    FIX_LENGTH(cs, pos, length, char_length);
    memcpy((uchar*) key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char*) key+char_length, length-char_length, ' ');
    key+= length;
  }
  if (last_used_keyseg)
    *last_used_keyseg= keyseg;

  DBUG_RETURN((uint) (key-start_key));
} /* _mi_pack_key */



/*
  Store found key in record

  SYNOPSIS
    _mi_put_key_in_record()
    info		MyISAM handler
    keynr		Key number that was used
    record 		Store key here

    Last read key is in info->lastkey

 NOTES
   Used when only-keyread is wanted

 RETURN
   0   ok
   1   error
*/

static int _mi_put_key_in_record(register MI_INFO *info, uint keynr,
				 uchar *record)
{
  reg2 uchar *key;
  uchar *pos,*key_end;
  reg1 HA_KEYSEG *keyseg;
  uchar *blob_ptr;
  DBUG_ENTER("_mi_put_key_in_record");

  blob_ptr= (uchar*) info->lastkey2;             /* Place to put blob parts */
  key=(uchar*) info->lastkey;                    /* KEy that was read */
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
    if (keyseg->type == HA_KEYTYPE_BIT)
    {
      uint length= keyseg->length;

      if (keyseg->bit_length)
      {
        uchar bits= *key++;
        set_rec_bits(bits, record + keyseg->bit_pos, keyseg->bit_start,
                     keyseg->bit_length);
        length--;
      }
      else
      {
        clr_rec_bits(record + keyseg->bit_pos, keyseg->bit_start,
                     keyseg->bit_length);
      }
      memcpy(record + keyseg->start, (uchar*) key, length);
      key+= length;
      continue;
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
        keyseg->charset->cset->fill(keyseg->charset,
                                    (char*) pos + length,
                                    keyseg->length - length,
                                    ' ');
      }
      else
      {
	bfill(pos,keyseg->length-length,' ');
	memcpy(pos+keyseg->length-length,key,(size_t) length);
      }
      key+=length;
      continue;
    }

    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint length;
      get_key_length(length,key);
#ifdef CHECK_KEYS
      if (length > keyseg->length || key+length > key_end)
	goto err;
#endif
      /* Store key length */
      if (keyseg->bit_start == 1)
        *(uchar*) (record+keyseg->start)= (uchar) length;
      else
        int2store(record+keyseg->start, length);
      /* And key data */
      memcpy(record+keyseg->start + keyseg->bit_start, (uchar*) key, length);
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

      /* The above changed info->lastkey2. Inform mi_rnext_same(). */
      info->update&= ~HA_STATE_RNEXT_SAME;

      _my_store_blob_length(record+keyseg->start,
			    (uint) keyseg->bit_start,length);
      key+=length;
    }
    else if (keyseg->flag & HA_SWAP_KEY)
    {
      uchar *to=  record+keyseg->start+keyseg->length;
      uchar *end= key+keyseg->length;
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
      memcpy(record+keyseg->start,(uchar*) key,
	     (size_t) keyseg->length);
      key+= keyseg->length;
    }
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);				/* Crashed row */
} /* _mi_put_key_in_record */


	/* Here when key reads are used */

int _mi_read_key_record(MI_INFO *info, my_off_t filepos, uchar *buf)
{
  fast_mi_writeinfo(info);
  if (filepos != HA_OFFSET_ERROR)
  {
    if (info->lastinx >= 0)
    {				/* Read only key */
      if (_mi_put_key_in_record(info,(uint) info->lastinx,buf))
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
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


/*
  Retrieve auto_increment info

  SYNOPSIS
    retrieve_auto_increment()
    info			MyISAM handler
    record			Row to update

  IMPLEMENTATION
    For signed columns we don't retrieve the auto increment value if it's
    less than zero.
*/

ulonglong retrieve_auto_increment(MI_INFO *info,const uchar *record)
{
  ulonglong value= 0;			/* Store unsigned values here */
  longlong s_value= 0;			/* Store signed values here */
  HA_KEYSEG *keyseg= info->s->keyinfo[info->s->base.auto_key-1].seg;
  const uchar *key= (uchar*) record + keyseg->start;

  switch (keyseg->type) {
  case HA_KEYTYPE_INT8:
    s_value= (longlong) *(char*)key;
    break;
  case HA_KEYTYPE_BINARY:
    value=(ulonglong)  *(uchar*) key;
    break;
  case HA_KEYTYPE_SHORT_INT:
    s_value= (longlong) sint2korr(key);
    break;
  case HA_KEYTYPE_USHORT_INT:
    value=(ulonglong) uint2korr(key);
    break;
  case HA_KEYTYPE_LONG_INT:
    s_value= (longlong) sint4korr(key);
    break;
  case HA_KEYTYPE_ULONG_INT:
    value=(ulonglong) uint4korr(key);
    break;
  case HA_KEYTYPE_INT24:
    s_value= (longlong) sint3korr(key);
    break;
  case HA_KEYTYPE_UINT24:
    value=(ulonglong) uint3korr(key);
    break;
  case HA_KEYTYPE_FLOAT:                        /* This shouldn't be used */
  {
    float f_1;
    float4get(f_1,key);
    /* Ignore negative values */
    value = (f_1 < (float) 0.0) ? 0 : (ulonglong) f_1;
    break;
  }
  case HA_KEYTYPE_DOUBLE:                       /* This shouldn't be used */
  {
    double f_1;
    float8get(f_1,key);
    /* Ignore negative values */
    value = (f_1 < 0.0) ? 0 : (ulonglong) f_1;
    break;
  }
  case HA_KEYTYPE_LONGLONG:
    s_value= sint8korr(key);
    break;
  case HA_KEYTYPE_ULONGLONG:
    value= uint8korr(key);
    break;
  default:
    DBUG_ASSERT(0);
    value=0;                                    /* Error */
    break;
  }

  /*
    The following code works becasue if s_value < 0 then value is 0
    and if s_value == 0 then value will contain either s_value or the
    correct value.
  */
  return (s_value > 0) ? (ulonglong) s_value : value;
}
