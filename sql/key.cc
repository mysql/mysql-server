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


/* Functions to handle keys and fields in forms */

#include "mysql_priv.h"

	/*
	** Search after with key field is. If no key starts with field test
	** if field is part of some key.
	**
	** returns number of key. keylength is set to length of key before
	** (not including) field
	** Used when calculating key for NEXT_NUMBER
	*/

int find_ref_key(TABLE *table,Field *field, uint *key_length)
{
  reg2 int i;
  reg3 KEY *key_info;
  uint fieldpos;

  fieldpos=    field->offset();

	/* Test if some key starts as fieldpos */

  for (i=0, key_info=table->key_info ; i < (int) table->keys ; i++, key_info++)
  {
    if (key_info->key_part[0].offset == fieldpos)
    {						/* Found key. Calc keylength */
      *key_length=0;
      return(i);			/* Use this key */
    }
  }
	/* Test if some key contains fieldpos */

  for (i=0, key_info=table->key_info ; i < (int) table->keys ; i++, key_info++)
  {
    uint j;
    KEY_PART_INFO *key_part;
    *key_length=0;
    for (j=0, key_part=key_info->key_part ;
	 j < key_info->key_parts ;
	 j++, key_part++)
    {
      if (key_part->offset == fieldpos)
	return(i);			/* Use this key */
      *key_length+=key_part->store_length;
    }
  }
  return(-1);					/* No key is ok */
}


/*
  Copy part of a record that forms a key or key prefix to a buffer.

  SYNOPSIS
    key_copy()
    to_key      buffer that will be used as a key
    from_record full record to be copied from
    key_info    descriptor of the index
    key_length  specifies length of all keyparts that will be copied

  DESCRIPTION
    The function takes a complete table record (as e.g. retrieved by
    handler::index_read()), and a description of an index on the same table,
    and extracts the first key_length bytes of the record which are part of a
    key into to_key. If length == 0 then copy all bytes from the record that
    form a key.

  RETURN
    None
*/

void key_copy(byte *to_key, byte *from_record, KEY *key_info, uint key_length)
{
  uint length;
  KEY_PART_INFO *key_part;

  if (key_length == 0)
    key_length= key_info->key_length;
  for (key_part= key_info->key_part; (int) key_length > 0; key_part++)
  {
    if (key_part->null_bit)
    {
      *to_key++= test(from_record[key_part->null_offset] &
		   key_part->null_bit);
      key_length--;
    }
    if (key_part->key_part_flag & HA_BLOB_PART)
    {
      char *pos;
      ulong blob_length= ((Field_blob*) key_part->field)->get_length();
      key_length-= HA_KEY_BLOB_LENGTH;
      ((Field_blob*) key_part->field)->get_ptr(&pos);
      length=min(key_length, key_part->length);
      set_if_smaller(blob_length, length);
      int2store(to_key, (uint) blob_length);
      to_key+= HA_KEY_BLOB_LENGTH;			// Skip length info
      memcpy(to_key, pos, blob_length);
    }
    else if (key_part->key_part_flag & HA_VAR_LENGTH_PART)
    {
      key_length-= HA_KEY_BLOB_LENGTH;
      length= min(key_length, key_part->length);
      key_part->field->get_key_image(to_key, length, Field::itRAW);
      to_key+= HA_KEY_BLOB_LENGTH;
    }
    else
    {
      length= min(key_length, key_part->length);
      memcpy(to_key, from_record + key_part->offset, (size_t) length);
    }
    to_key+= length;
    key_length-= length;
  }
}


/*
  Restore a key from some buffer to record.

  SYNOPSIS
    key_restore()
    to_record   record buffer where the key will be restored to
    from_key    buffer that contains a key
    key_info    descriptor of the index
    key_length  specifies length of all keyparts that will be restored

  DESCRIPTION
    This function converts a key into record format. It can be used in cases
    when we want to return a key as a result row.

  RETURN
    None
*/

void key_restore(byte *to_record, byte *from_key, KEY *key_info,
                 uint key_length)
{
  uint length;
  KEY_PART_INFO *key_part;

  if (key_length == 0)
  {
    key_length= key_info->key_length;
  }
  for (key_part= key_info->key_part ; (int) key_length > 0 ; key_part++)
  {
    if (key_part->null_bit)
    {
      if (*from_key++)
	to_record[key_part->null_offset]|= key_part->null_bit;
      else
	to_record[key_part->null_offset]&= ~key_part->null_bit;
      key_length--;
    }
    if (key_part->key_part_flag & HA_BLOB_PART)
    {
      uint blob_length= uint2korr(from_key);
      from_key+= HA_KEY_BLOB_LENGTH;
      key_length-= HA_KEY_BLOB_LENGTH;
      ((Field_blob*) key_part->field)->set_ptr((ulong) blob_length,
					       (char*) from_key);
      length= key_part->length;
    }
    else if (key_part->key_part_flag & HA_VAR_LENGTH_PART)
    {
      key_length-= HA_KEY_BLOB_LENGTH;
      length= min(key_length, key_part->length);
      key_part->field->set_key_image(from_key, length);
      from_key+= HA_KEY_BLOB_LENGTH;
    }
    else
    {
      length= min(key_length, key_part->length);
      memcpy(to_record + key_part->offset, from_key, (size_t) length);
    }
    from_key+= length;
    key_length-= length;
  }
}


/*
  Compare if a key has changed

  SYNOPSIS
    key_cmp_if_same()
    table		TABLE
    key			key to compare to row
    idx			Index used
    key_length		Length of key

  NOTES
    In theory we could just call field->cmp() for all field types,
    but as we are only interested if a key has changed (not if the key is
    larger or smaller than the previous value) we can do things a bit
    faster by using memcmp() instead.

  RETURN
    0	If key is equal
    1	Key has changed
*/

bool key_cmp_if_same(TABLE *table,const byte *key,uint idx,uint key_length)
{
  uint length;
  KEY_PART_INFO *key_part;

  for (key_part=table->key_info[idx].key_part;
       (int) key_length > 0;
       key_part++, key+=length, key_length-=length)
  {
    if (key_part->null_bit)
    {
      key_length--;
      if (*key != test(table->record[0][key_part->null_offset] & 
		       key_part->null_bit))
	return 1;
      if (*key)
      {
	length=key_part->store_length;
	continue;
      }
      key++;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {
      if (key_part->field->key_cmp(key, key_part->length))
	return 1;
      length=key_part->length+HA_KEY_BLOB_LENGTH;
    }
    else
    {
      length=min(key_length,key_part->length);
      if (!(key_part->key_type & (FIELDFLAG_NUMBER+FIELDFLAG_BINARY+
				  FIELDFLAG_PACK)))
      {
        CHARSET_INFO *cs= key_part->field->charset();
        uint char_length= key_part->length / cs->mbmaxlen;
        const byte *pos= table->record[0] + key_part->offset;
        if (length > char_length)
        {
          char_length= my_charpos(cs, pos, pos + length, char_length);
          set_if_smaller(char_length, length);
        }
	if (cs->coll->strnncollsp(cs,
                                  (const uchar*) key, length,
                                  (const uchar*) pos, char_length, 0))
	  return 1;
      }
      else if (memcmp(key,table->record[0]+key_part->offset,length))
	return 1;
    }
  }
  return 0;
}

	/* unpack key-fields from record to some buffer */
	/* This is used to get a good error message */

void key_unpack(String *to,TABLE *table,uint idx)
{
  KEY_PART_INFO *key_part,*key_part_end;
  Field *field;
  String tmp;
  DBUG_ENTER("key_unpack");

  to->length(0);
  for (key_part=table->key_info[idx].key_part,key_part_end=key_part+
	 table->key_info[idx].key_parts ;
       key_part < key_part_end;
       key_part++)
  {
    if (to->length())
      to->append('-');
    if (key_part->null_bit)
    {
      if (table->record[0][key_part->null_offset] & key_part->null_bit)
      {
	to->append("NULL", 4);
	continue;
      }
    }
    if ((field=key_part->field))
    {
      field->val_str(&tmp);
      if (key_part->length < field->pack_length())
	tmp.length(min(tmp.length(),key_part->length));
      to->append(tmp);
    }
    else
      to->append("???", 3);
  }
  DBUG_VOID_RETURN;
}


/*
  Return 1 if any field in a list is part of key or the key uses a field
  that is automaticly updated (like a timestamp)
*/

bool check_if_key_used(TABLE *table, uint idx, List<Item> &fields)
{
  List_iterator_fast<Item> f(fields);
  KEY_PART_INFO *key_part,*key_part_end;
  for (key_part=table->key_info[idx].key_part,key_part_end=key_part+
	 table->key_info[idx].key_parts ;
       key_part < key_part_end;
       key_part++)
  {
    Item_field *field;

    if (key_part->field == table->timestamp_field)
      return 1;					// Can't be used for update

    f.rewind();
    while ((field=(Item_field*) f++))
    {
      if (key_part->field == field->field)
	return 1;
    }
  }

  /*
    If table handler has primary key as part of the index, check that primary
    key is not updated
  */
  if (idx != table->primary_key && table->primary_key < MAX_KEY &&
      (table->file->table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX))
    return check_if_key_used(table, table->primary_key, fields);
  return 0;
}


/*
  Compare key in row to a given key

  SYNOPSIS
    key_cmp()
    key_part		Key part handler
    key			Key to compare to value in table->record[0]
    key_length		length of 'key'

  RETURN
    The return value is SIGN(key_in_row - range_key):

    0			Key is equal to range or 'range' == 0 (no range)
   -1			Key is less than range
    1			Key is larger than range
*/

int key_cmp(KEY_PART_INFO *key_part, const byte *key, uint key_length)
{
  uint store_length;

  for (const byte *end=key + key_length;
       key < end;
       key+= store_length, key_part++)
  {
    int cmp;
    store_length= key_part->store_length;
    if (key_part->null_bit)
    {
      /* This key part allows null values; NULL is lower than everything */
      register bool field_is_null= key_part->field->is_null();
      if (*key)                                 // If range key is null
      {
	/* the range is expecting a null value */
	if (!field_is_null)
	  return 1;                             // Found key is > range
        /* null -- exact match, go to next key part */
	continue;
      }
      else if (field_is_null)
	return -1;                              // NULL is less than any value
      key++;					// Skip null byte
      store_length--;
    }
    if ((cmp=key_part->field->key_cmp((byte*) key, key_part->length)) < 0)
      return -1;
    if (cmp > 0)
      return 1;
  }
  return 0;                                     // Keys are equal
}
