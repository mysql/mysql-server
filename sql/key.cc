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
      *key_length+=key_part->length;
    }
  }
  return(-1);					/* No key is ok */
}


	/* Copy a key from record to some buffer */
	/* if length == 0 then copy hole key */

void key_copy(byte *key,TABLE *table,uint idx,uint key_length)
{
  uint length;
  KEY *key_info=table->key_info+idx;
  KEY_PART_INFO *key_part;

  if (key_length == 0)
    key_length=key_info->key_length;
  for (key_part=key_info->key_part;
       (int) key_length > 0 ;
       key_part++)
  {
    if (key_part->null_bit)
    {
      *key++= test(table->record[0][key_part->null_offset] &
		   key_part->null_bit);
      key_length--;
    }
    if (key_part->key_part_flag & HA_BLOB_PART)
    {
      char *pos;
      ulong blob_length=((Field_blob*) key_part->field)->get_length();
      key_length-=2;
      ((Field_blob*) key_part->field)->get_ptr(&pos);
      length=min(key_length,key_part->length);
      set_if_smaller(blob_length,length);
      int2store(key,(uint) blob_length);
      key+=2;					// Skipp length info
      memcpy(key,pos,blob_length);
    }
    else
    {
      length=min(key_length,key_part->length);
      memcpy(key,table->record[0]+key_part->offset,(size_t) length);
    }
    key+=length;
    key_length-=length;
  }
} /* key_copy */


	/* restore a key from some buffer to record */

void key_restore(TABLE *table,byte *key,uint idx,uint key_length)
{
  uint length;
  KEY *key_info=table->key_info+idx;
  KEY_PART_INFO *key_part;

  if (key_length == 0)
  {
    if (idx == (uint) -1)
      return;
    key_length=key_info->key_length;
  }
  for (key_part=key_info->key_part;
       (int) key_length > 0 ;
       key_part++)
  {
    if (key_part->null_bit)
    {
      if (*key++)
	table->record[0][key_part->null_offset]|= key_part->null_bit;
      else
	table->record[0][key_part->null_offset]&= ~key_part->null_bit;
      key_length--;
    }
    if (key_part->key_part_flag & HA_BLOB_PART)
    {
      uint blob_length=uint2korr(key);
      key+=2;
      key_length-=2;
      ((Field_blob*) key_part->field)->set_ptr((ulong) blob_length,
					       (char*) key);
      length=key_part->length;
    }
    else
    {
      length=min(key_length,key_part->length);
      memcpy(table->record[0]+key_part->offset,key,(size_t) length);
    }
    key+=length;
    key_length-=length;
  }
} /* key_restore */


	/* Compare if a key has changed */

int key_cmp(TABLE *table,const byte *key,uint idx,uint key_length)
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
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH))
    {
      if (key_part->field->key_cmp(key, key_part->length+2))
	return 1;
      length=key_part->length+2;
    }
    else
    {
      length=min(key_length,key_part->length);
      if (!(key_part->key_type & (FIELDFLAG_NUMBER+FIELDFLAG_BINARY+
				  FIELDFLAG_PACK)))
      {
	if (my_sortcmp((char*) key,(char*) table->record[0]+key_part->offset,
		       length))
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
	to->append("NULL");
	continue;
      }
    }
    if ((field=key_part->field))
    {
      field->val_str(&tmp,&tmp);
      if (key_part->length < field->pack_length())
	tmp.length(min(tmp.length(),key_part->length));
      to->append(tmp);
    }
    else
      to->append("???");
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
  return 0;
}
