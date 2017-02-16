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

/* Functions to handle keys */

#include "maria_def.h"
#include "m_ctype.h"
#include "ma_sp_defs.h"
#include "ma_blockrec.h"                        /* For ROW_FLAG_TRANSID */
#include "trnman.h"
#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#define CHECK_KEYS                              /* Enable safety checks */

static int _ma_put_key_in_record(MARIA_HA *info, uint keynr,
                                 my_bool unpack_blobs, uchar *record);

#define FIX_LENGTH(cs, pos, length, char_length)                            \
            do {                                                            \
              if (length > char_length)                                     \
                char_length= (uint) my_charpos(cs, pos, pos+length, char_length); \
              set_if_smaller(char_length,length);                           \
            } while(0)


/**
  Store trid in a packed format as part of a key

  @fn    transid_store_packed
  @param info   Maria handler
  @param to     End of key to which we should store a packed transid
  @param trid   Trid to be stored

  @notes

  Keys that have a transid has the lowest bit set for the last byte of the key
  This function sets this bit for the key.

  Trid is max 6 bytes long

  First Trid it's converted to a smaller number by using
  trid= trid - create_trid.
  Then trid is then shifted up one bit so that we can use the
  lowest bit as a marker if it's followed by another trid.

  Trid is then stored as follows:

  if trid < 256-12
    one byte
  else
    one byte prefix length_of_trid_in_bytes + 249 followed by data
    in high-byte-first order

  Prefix bytes 244 to 249 are reserved for negative transid, that can be used
  when we pack transid relative to each other on a key block.

  We have to store transid in high-byte-first order so that we can compare
  them unpacked byte per byte and as soon we find a difference we know
  which is smaller.

  For example, assuming we the following data:

  key_data:               1                (4 byte integer)
  pointer_to_row:         2 << 8 + 3 = 515 (page 2, row 3)
  table_create_transid    1000             Defined at create table time and
                                           stored in table definition
  transid                 1010	           Transaction that created row
  delete_transid          2011             Transaction that deleted row

  In addition we assume the table is created with a data pointer length
  of 4 bytes (this is automatically calculated based on the medium
  length of rows and the given max number of rows)

  The binary data for the key would then look like this in hex:

  00 00 00 01     Key data (1 stored high byte first)
  00 00 00 47	  (515 << 1) + 1         ;  The last 1 is marker that key cont.
  15              ((1010-1000) << 1) + 1 ;  The last 1 is marker that key cont.
  FB 07 E6        Length byte (FE = 249 + 2 means 2 bytes) and 
                  ((2011 - 1000) << 1) = 07 E6
*/

uint transid_store_packed(MARIA_HA *info, uchar *to, ulonglong trid)
{
  uchar *start;
  uint length;
  uchar buff[8];
  DBUG_ASSERT(trid < (LL(1) << (MARIA_MAX_PACK_TRANSID_SIZE*8)));
  DBUG_ASSERT(trid >= info->s->state.create_trid);

  trid= (trid - info->s->state.create_trid) << 1;

  /* Mark that key contains transid */
  to[-1]|= 1;

  if (trid < MARIA_MIN_TRANSID_PACK_OFFSET)
  {
    to[0]= (uchar) trid;
    return 1;
  }
  start= to;

  /* store things in low-byte-first-order in buff */
  to= buff;
  do
  {
    *to++= (uchar) trid;
    trid= trid>>8;
  } while (trid);

  length= (uint) (to - buff);
  /* Store length prefix */
  start[0]= (uchar) (length + MARIA_TRANSID_PACK_OFFSET);
  start++;
  /* Copy things in high-byte-first order to output buffer */
  do
  {
    *start++= *--to;
  } while (to != buff);
  return length+1;
}


/**
   Read packed transid

   @fn    transid_get_packed
   @param info   Maria handler
   @param from	 Transid is stored here

   See transid_store_packed() for how transid is packed

*/

ulonglong transid_get_packed(MARIA_SHARE *share, const uchar *from)
{
  ulonglong value;
  uint length;

  if (from[0] < MARIA_MIN_TRANSID_PACK_OFFSET)
    value= (ulonglong) from[0];
  else
  {
    value= 0;
    for (length= (uint) (from[0] - MARIA_TRANSID_PACK_OFFSET),
           value= (ulonglong) from[1], from+=2;
         --length ;
         from++)
      value= (value << 8) + ((ulonglong) *from);
  }
  return (value >> 1) + share->state.create_trid;
}


/*
  Make a normal (not spatial or fulltext) intern key from a record

  SYNOPSIS
    _ma_make_key()
    info		MyiSAM handler
    int_key		Store created key here
    keynr		key number
    key			Buffer used to store key data
    record		Record
    filepos		Position to record in the data file

  NOTES
    This is used to generate keys from the record on insert, update and delete

  RETURN
    key
*/

MARIA_KEY *_ma_make_key(MARIA_HA *info, MARIA_KEY *int_key, uint keynr,
                        uchar *key, const uchar *record,
                        MARIA_RECORD_POS filepos, ulonglong trid)
{
  const uchar *pos;
  reg1 HA_KEYSEG *keyseg;
  my_bool is_ft;
  DBUG_ENTER("_ma_make_key");

  int_key->data= key;
  int_key->flag= 0;                             /* Always return full key */
  int_key->keyinfo= info->s->keyinfo + keynr;

  is_ft= int_key->keyinfo->flag & HA_FULLTEXT;
  for (keyseg= int_key->keyinfo->seg ; keyseg->type ;keyseg++)
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

    pos= record+keyseg->start;
    if (type == HA_KEYTYPE_BIT)
    {
      if (keyseg->bit_length)
      {
        uchar bits= get_rec_bits(record + keyseg->bit_pos,
                                 keyseg->bit_start, keyseg->bit_length);
        *key++= (char) bits;
        length--;
      }
      memcpy(key, pos, length);
      key+= length;
      continue;
    }
    if (keyseg->flag & HA_SPACE_PACK)
    {
      if (type != HA_KEYTYPE_NUM)
      {
        length= (uint) cs->cset->lengthsp(cs, (const char*)pos, length);
      }
      else
      {
        const uchar *end= pos + length;
	while (pos < end && pos[0] == ' ')
	  pos++;
	length= (uint) (end-pos);
      }
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, pos, (size_t) char_length);
      key+=char_length;
      continue;
    }
    if (keyseg->flag & HA_VAR_LENGTH_PART)
    {
      uint pack_length= (keyseg->bit_start == 1 ? 1 : 2);
      uint tmp_length= (pack_length == 1 ? (uint) *pos :
                        uint2korr(pos));
      pos+= pack_length;			/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key,pos,(size_t) char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      uint tmp_length= _ma_calc_blob_length(keyseg->bit_start,pos);
      uchar *blob_pos;
      memcpy(&blob_pos, pos+keyseg->bit_start,sizeof(char*));
      set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, blob_pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, blob_pos, (size_t) char_length);
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
    memcpy(key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char*) key+char_length, length-char_length, ' ');
    key+= length;
  }
  _ma_dpointer(info->s, key, filepos);
  int_key->data_length= (key - int_key->data);
  int_key->ref_length= info->s->rec_reflength;
  int_key->flag= 0;
  if (_ma_have_versioning(info) && trid)
  {
    int_key->ref_length+= transid_store_packed(info,
                                               key + int_key->ref_length,
                                               (TrID) trid);
    int_key->flag|= SEARCH_USER_KEY_HAS_TRANSID;
  }

  DBUG_PRINT("exit",("keynr: %d",keynr));
  DBUG_DUMP_KEY("key", int_key);
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, int_key););
  DBUG_RETURN(int_key);
} /* _ma_make_key */


/*
  Pack a key to intern format from given format (c_rkey)

  SYNOPSIS
    _ma_pack_key()
    info		MARIA handler
    int_key		Store key here
    keynr		key number
    key			Buffer for key data
    old			Original not packed key
    keypart_map         bitmap of used keyparts
    last_used_keyseg	out parameter.  May be NULL

   RETURN
   int_key

     last_use_keyseg    Store pointer to the keyseg after the last used one
*/

MARIA_KEY *_ma_pack_key(register MARIA_HA *info, MARIA_KEY *int_key,
                        uint keynr, uchar *key,
                        const uchar *old, key_part_map keypart_map,
                        HA_KEYSEG **last_used_keyseg)
{
  HA_KEYSEG *keyseg;
  my_bool is_ft;
  DBUG_ENTER("_ma_pack_key");

  int_key->data= key;
  int_key->keyinfo= info->s->keyinfo + keynr;

  /* "one part" rtree key is 2*SPDIMS part key in Maria */
  if (int_key->keyinfo->key_alg == HA_KEY_ALG_RTREE)
    keypart_map= (((key_part_map)1) << (2*SPDIMS)) - 1;

  /* only key prefixes are supported */
  DBUG_ASSERT(((keypart_map+1) & keypart_map) == 0);

  is_ft= int_key->keyinfo->flag & HA_FULLTEXT;
  for (keyseg=int_key->keyinfo->seg ; keyseg->type && keypart_map;
       old+= keyseg->length, keyseg++)
  {
    enum ha_base_keytype type= (enum ha_base_keytype) keyseg->type;
    uint length= keyseg->length;
    uint char_length;
    const uchar *pos;
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
    char_length= ((!is_ft && cs && cs->mbmaxlen > 1) ? length/cs->mbmaxlen :
                  length);
    pos= old;
    if (keyseg->flag & HA_SPACE_PACK)
    {
      const uchar *end= pos + length;
      if (type == HA_KEYTYPE_NUM)
      {
	while (pos < end && pos[0] == ' ')
	  pos++;
      }
      else if (type != HA_KEYTYPE_BINARY)
      {
	while (end > pos && end[-1] == ' ')
	  end--;
      }
      length=(uint) (end-pos);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key,pos,(size_t) char_length);
      key+= char_length;
      continue;
    }
    else if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
    {
      /* Length of key-part used with maria_rkey() always 2 */
      uint tmp_length=uint2korr(pos);
      pos+=2;
      set_if_smaller(length,tmp_length);	/* Safety */
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      old+=2;					/* Skip length */
      memcpy(key, pos,(size_t) char_length);
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
    memcpy(key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char*) key+char_length, length-char_length, ' ');
    key+= length;
  }
  if (last_used_keyseg)
    *last_used_keyseg= keyseg;

  /* set flag to SEARCH_PART_KEY if we are not using all key parts */
  int_key->flag= keyseg->type ? SEARCH_PART_KEY : 0;
  int_key->ref_length= 0;
  int_key->data_length= (key - int_key->data);

  DBUG_PRINT("exit", ("length: %u", int_key->data_length));
  DBUG_RETURN(int_key);
} /* _ma_pack_key */


/**
   Copy a key
*/

void _ma_copy_key(MARIA_KEY *to, const MARIA_KEY *from)
{
  memcpy(to->data, from->data, from->data_length + from->ref_length);
  to->keyinfo=     from->keyinfo;
  to->data_length= from->data_length;
  to->ref_length=  from->ref_length;
  to->flag=        from->flag;
}


/*
  Store found key in record

  SYNOPSIS
    _ma_put_key_in_record()
    info		MARIA handler
    keynr		Key number that was used
    unpack_blobs        TRUE  <=> Unpack blob columns
                        FALSE <=> Skip them. This is used by index condition 
                                  pushdown check function
    record 		Store key here

    Last read key is in info->lastkey

 NOTES
   Used when only-keyread is wanted

 RETURN
   0   ok
   1   error
*/

static int _ma_put_key_in_record(register MARIA_HA *info, uint keynr,
				 my_bool unpack_blobs, uchar *record)
{
  reg2 uchar *key;
  uchar *pos,*key_end;
  reg1 HA_KEYSEG *keyseg;
  uchar *blob_ptr;
  DBUG_ENTER("_ma_put_key_in_record");

  blob_ptr= info->lastkey_buff2;         /* Place to put blob parts */
  key= info->last_key.data;               /* Key that was read */
  key_end= key + info->last_key.data_length;
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
      memcpy(record + keyseg->start, key, length);
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
      memcpy(record+keyseg->start + keyseg->bit_start, key, length);
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
      if (unpack_blobs)
      {
        memcpy(record+keyseg->start+keyseg->bit_start,
               &blob_ptr, sizeof(char*));
        memcpy(blob_ptr,key,length);
        blob_ptr+=length;

        /* The above changed info->lastkey2. Inform maria_rnext_same(). */
        info->update&= ~HA_STATE_RNEXT_SAME;

        _ma_store_blob_length(record+keyseg->start,
                              (uint) keyseg->bit_start,length);
      }
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
      memcpy(record+keyseg->start, key, (size_t) keyseg->length);
      key+= keyseg->length;
    }
  }
  DBUG_RETURN(0);

err:
  DBUG_PRINT("info",("error"));
  DBUG_RETURN(1);				/* Crashed row */
} /* _ma_put_key_in_record */


	/* Here when key reads are used */

int _ma_read_key_record(MARIA_HA *info, uchar *buf, MARIA_RECORD_POS filepos)
{
  fast_ma_writeinfo(info);
  if (filepos != HA_OFFSET_ERROR)
  {
    if (info->lastinx >= 0)
    {				/* Read only key */
      if (_ma_put_key_in_record(info, (uint)info->lastinx, TRUE, buf))
      {
        _ma_set_fatal_error(info->s, HA_ERR_CRASHED);
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
  Save current key tuple to record and call index condition check function

  SYNOPSIS
    ma_check_index_cond()
      info    MyISAM handler
      keynr   Index we're running a scan on
      record  Record buffer to use (it is assumed that index check function 
              will look for column values there)

  RETURN
    ICP_ERROR         Error ; my_errno set to HA_ERR_CRASHED
    ICP_NO_MATCH      Index condition is not satisfied, continue scanning
    ICP_MATCH         Index condition is satisfied
    ICP_OUT_OF_RANGE  Index condition is not satisfied, end the scan.
                      my_errno set to HA_ERR_END_OF_FILE

    info->cur_row.lastpos is set to HA_OFFSET_ERROR in case of ICP_ERROR or
    ICP_OUT_OF_RANGE to indicate that we don't have any active row.
*/

ICP_RESULT ma_check_index_cond(register MARIA_HA *info, uint keynr,
                               uchar *record)
{
  ICP_RESULT res= ICP_MATCH;
  if (info->index_cond_func)
  {
    if (_ma_put_key_in_record(info, keynr, FALSE, record))
    {
      /* Impossible case; Can only happen if bug in code */
      maria_print_error(info->s, HA_ERR_CRASHED);
      info->cur_row.lastpos= HA_OFFSET_ERROR;   /* No active record */
      my_errno= HA_ERR_CRASHED;
      res= ICP_ERROR;
    }
    else if ((res= info->index_cond_func(info->index_cond_func_arg)) ==
             ICP_OUT_OF_RANGE)
    {
      /* We got beyond the end of scanned range */
      info->cur_row.lastpos= HA_OFFSET_ERROR;   /* No active record */
      my_errno= HA_ERR_END_OF_FILE;
    }
  }
  return res;
}


/*
  Retrieve auto_increment info

  SYNOPSIS
    retrieve_auto_increment()
    key                         Auto-increment key
    key_type                    Key's type

  NOTE
    'key' should in "record" format, that is, how it is packed in a record
    (this matters with HA_SWAP_KEY).

  IMPLEMENTATION
    For signed columns we don't retrieve the auto increment value if it's
    less than zero.
*/

ulonglong ma_retrieve_auto_increment(const uchar *key, uint8 key_type)
{
  ulonglong value= 0;			/* Store unsigned values here */
  longlong s_value= 0;			/* Store signed values here */

  switch (key_type) {
  case HA_KEYTYPE_INT8:
    s_value= (longlong) *(const char*)key;
    break;
  case HA_KEYTYPE_BINARY:
    value=(ulonglong)  *key;
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
