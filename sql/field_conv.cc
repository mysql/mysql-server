/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  Functions to copy data to or from fields

    This could be done with a single short function but opencoding this
    gives much more speed.
*/

#include "sql_priv.h"
#include "sql_class.h"                          // THD
#include <m_ctype.h>

static void do_field_eq(Copy_field *copy)
{
  memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
}

static void do_field_1(Copy_field *copy)
{
  copy->to_ptr[0]=copy->from_ptr[0];
}

static void do_field_2(Copy_field *copy)
{
  copy->to_ptr[0]=copy->from_ptr[0];
  copy->to_ptr[1]=copy->from_ptr[1];
}

static void do_field_3(Copy_field *copy)
{
  copy->to_ptr[0]=copy->from_ptr[0];
  copy->to_ptr[1]=copy->from_ptr[1];
  copy->to_ptr[2]=copy->from_ptr[2];
}

static void do_field_4(Copy_field *copy)
{
  copy->to_ptr[0]=copy->from_ptr[0];
  copy->to_ptr[1]=copy->from_ptr[1];
  copy->to_ptr[2]=copy->from_ptr[2];
  copy->to_ptr[3]=copy->from_ptr[3];
}

static void do_field_6(Copy_field *copy)
{						// For blob field
  copy->to_ptr[0]=copy->from_ptr[0];
  copy->to_ptr[1]=copy->from_ptr[1];
  copy->to_ptr[2]=copy->from_ptr[2];
  copy->to_ptr[3]=copy->from_ptr[3];
  copy->to_ptr[4]=copy->from_ptr[4];
  copy->to_ptr[5]=copy->from_ptr[5];
}

static void do_field_8(Copy_field *copy)
{
  copy->to_ptr[0]=copy->from_ptr[0];
  copy->to_ptr[1]=copy->from_ptr[1];
  copy->to_ptr[2]=copy->from_ptr[2];
  copy->to_ptr[3]=copy->from_ptr[3];
  copy->to_ptr[4]=copy->from_ptr[4];
  copy->to_ptr[5]=copy->from_ptr[5];
  copy->to_ptr[6]=copy->from_ptr[6];
  copy->to_ptr[7]=copy->from_ptr[7];
}


static void do_field_to_null_str(Copy_field *copy)
{
  if (*copy->from_null_ptr & copy->from_bit)
  {
    bzero(copy->to_ptr,copy->from_length);
    copy->to_null_ptr[0]=1;			// Always bit 1
  }
  else
  {
    copy->to_null_ptr[0]=0;
    memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  }
}


static void do_outer_field_to_null_str(Copy_field *copy)
{
  if (*copy->null_row ||
      (copy->from_null_ptr && (*copy->from_null_ptr & copy->from_bit)))
  {
    bzero(copy->to_ptr,copy->from_length);
    copy->to_null_ptr[0]=1;			// Always bit 1
  }
  else
  {
    copy->to_null_ptr[0]=0;
    memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  }
}


int
set_field_to_null(Field *field)
{
  if (field->real_maybe_null())
  {
    field->set_null();
    field->reset();
    return 0;
  }
  field->reset();
  switch (field->table->in_use->count_cuted_fields) {
  case CHECK_FIELD_WARN:
    field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
    /* fall through */
  case CHECK_FIELD_IGNORE:
    return 0;
  case CHECK_FIELD_ERROR_FOR_NULL:
    if (!field->table->in_use->no_errors)
      my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
    return -1;
  }
  DBUG_ASSERT(0); // impossible
  return -1;
}


/**
  Set field to NULL or TIMESTAMP or to next auto_increment number.

  @param field           Field to update
  @param no_conversions  Set to 1 if we should return 1 if field can't
                         take null values.
                         If set to 0 we will do store the 'default value'
                         if the field is a special field. If not we will
                         give an error.

  @retval
    0    Field could take 0 or an automatic conversion was used
  @retval
    -1   Field could not take NULL and no conversion was used.
    If no_conversion was not set, an error message is printed
*/

int
set_field_to_null_with_conversions(Field *field, bool no_conversions)
{
  if (field->real_maybe_null())
  {
    field->set_null();
    field->reset();
    return 0;
  }
  if (no_conversions)
    return -1;

  /*
    Check if this is a special type, which will get a special walue
    when set to NULL (TIMESTAMP fields which allow setting to NULL
    are handled by first check).
  */
  if (field->type() == MYSQL_TYPE_TIMESTAMP)
  {
    ((Field_timestamp*) field)->set_time();
    return 0;					// Ok to set time to NULL
  }
  
  // Note: we ignore any potential failure of reset() here.
  field->reset();

  if (field == field->table->next_number_field)
  {
    field->table->auto_increment_field_not_null= FALSE;
    return 0;				  // field is set in fill_record()
  }
  switch (field->table->in_use->count_cuted_fields) {
  case CHECK_FIELD_WARN:
    field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_BAD_NULL_ERROR, 1);
    /* fall through */
  case CHECK_FIELD_IGNORE:
    return 0;
  case CHECK_FIELD_ERROR_FOR_NULL:
    if (!field->table->in_use->no_errors)
      my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
    return -1;
  }
  DBUG_ASSERT(0); // impossible
  return -1;
}


static void do_skip(Copy_field *copy __attribute__((unused)))
{
}


static void do_copy_null(Copy_field *copy)
{
  if (*copy->from_null_ptr & copy->from_bit)
  {
    *copy->to_null_ptr|=copy->to_bit;
    copy->to_field->reset();
  }
  else
  {
    *copy->to_null_ptr&= ~copy->to_bit;
    (copy->do_copy2)(copy);
  }
}


static void do_outer_field_null(Copy_field *copy)
{
  if (*copy->null_row ||
      (copy->from_null_ptr && (*copy->from_null_ptr & copy->from_bit)))
  {
    *copy->to_null_ptr|=copy->to_bit;
    copy->to_field->reset();
  }
  else
  {
    *copy->to_null_ptr&= ~copy->to_bit;
    (copy->do_copy2)(copy);
  }
}


static void do_copy_not_null(Copy_field *copy)
{
  if (*copy->from_null_ptr & copy->from_bit)
  {
    copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                WARN_DATA_TRUNCATED, 1);
    copy->to_field->reset();
  }
  else
    (copy->do_copy2)(copy);
}


static void do_copy_maybe_null(Copy_field *copy)
{
  *copy->to_null_ptr&= ~copy->to_bit;
  (copy->do_copy2)(copy);
}

/* timestamp and next_number has special handling in case of NULL values */

static void do_copy_timestamp(Copy_field *copy)
{
  if (*copy->from_null_ptr & copy->from_bit)
  {
    /* Same as in set_field_to_null_with_conversions() */
    ((Field_timestamp*) copy->to_field)->set_time();
  }
  else
    (copy->do_copy2)(copy);
}


static void do_copy_next_number(Copy_field *copy)
{
  if (*copy->from_null_ptr & copy->from_bit)
  {
    /* Same as in set_field_to_null_with_conversions() */
    copy->to_field->table->auto_increment_field_not_null= FALSE;
    copy->to_field->reset();
  }
  else
    (copy->do_copy2)(copy);
}


static void do_copy_blob(Copy_field *copy)
{
  ulong length=((Field_blob*) copy->from_field)->get_length();
  ((Field_blob*) copy->to_field)->store_length(length);
  memcpy(copy->to_ptr, copy->from_ptr, sizeof(char*));
}

static void do_conv_blob(Copy_field *copy)
{
  copy->from_field->val_str(&copy->tmp);
  ((Field_blob *) copy->to_field)->store(copy->tmp.ptr(),
					 copy->tmp.length(),
					 copy->tmp.charset());
}

/** Save blob in copy->tmp for GROUP BY. */

static void do_save_blob(Copy_field *copy)
{
  char buff[MAX_FIELD_WIDTH];
  String res(buff,sizeof(buff),copy->tmp.charset());
  copy->from_field->val_str(&res);
  copy->tmp.copy(res);
  ((Field_blob *) copy->to_field)->store(copy->tmp.ptr(),
					 copy->tmp.length(),
					 copy->tmp.charset());
}


static void do_field_string(Copy_field *copy)
{
  char buff[MAX_FIELD_WIDTH];
  String res(buff, sizeof(buff), copy->from_field->charset());
  res.length(0U);

  copy->from_field->val_str(&res);
  copy->to_field->store(res.c_ptr_quick(), res.length(), res.charset());
}


static void do_field_enum(Copy_field *copy)
{
  if (copy->from_field->val_int() == 0)
    ((Field_enum *) copy->to_field)->store_type((ulonglong) 0);
  else
    do_field_string(copy);
}


static void do_field_varbinary_pre50(Copy_field *copy)
{
  char buff[MAX_FIELD_WIDTH];
  copy->tmp.set_quick(buff,sizeof(buff),copy->tmp.charset());
  copy->from_field->val_str(&copy->tmp);

  /* Use the same function as in 4.1 to trim trailing spaces */
  uint length= my_lengthsp_8bit(&my_charset_bin, copy->tmp.c_ptr_quick(),
                                copy->from_field->field_length);

  copy->to_field->store(copy->tmp.c_ptr_quick(), length,
                        copy->tmp.charset());
}


static void do_field_int(Copy_field *copy)
{
  longlong value= copy->from_field->val_int();
  copy->to_field->store(value,
                        test(copy->from_field->flags & UNSIGNED_FLAG));
}

static void do_field_real(Copy_field *copy)
{
  double value=copy->from_field->val_real();
  copy->to_field->store(value);
}


static void do_field_decimal(Copy_field *copy)
{
  my_decimal value;
  copy->to_field->store_decimal(copy->from_field->val_decimal(&value));
}


/**
  string copy for single byte characters set when to string is shorter than
  from string.
*/

static void do_cut_string(Copy_field *copy)
{
  CHARSET_INFO *cs= copy->from_field->charset();
  memcpy(copy->to_ptr,copy->from_ptr,copy->to_length);

  /* Check if we loosed any important characters */
  if (cs->cset->scan(cs,
                     (char*) copy->from_ptr + copy->to_length,
                     (char*) copy->from_ptr + copy->from_length,
                     MY_SEQ_SPACES) < copy->from_length - copy->to_length)
  {
    copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                WARN_DATA_TRUNCATED, 1);
  }
}


/**
  string copy for multi byte characters set when to string is shorter than
  from string.
*/

static void do_cut_string_complex(Copy_field *copy)
{						// Shorter string field
  int well_formed_error;
  CHARSET_INFO *cs= copy->from_field->charset();
  const uchar *from_end= copy->from_ptr + copy->from_length;
  uint copy_length= cs->cset->well_formed_len(cs,
                                              (char*) copy->from_ptr,
                                              (char*) from_end, 
                                              copy->to_length / cs->mbmaxlen,
                                              &well_formed_error);
  if (copy->to_length < copy_length)
    copy_length= copy->to_length;
  memcpy(copy->to_ptr, copy->from_ptr, copy_length);

  /* Check if we lost any important characters */
  if (well_formed_error ||
      cs->cset->scan(cs, (char*) copy->from_ptr + copy_length,
                     (char*) from_end,
                     MY_SEQ_SPACES) < (copy->from_length - copy_length))
  {
    copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                WARN_DATA_TRUNCATED, 1);
  }

  if (copy_length < copy->to_length)
    cs->cset->fill(cs, (char*) copy->to_ptr + copy_length,
                   copy->to_length - copy_length, ' ');
}




static void do_expand_binary(Copy_field *copy)
{
  CHARSET_INFO *cs= copy->from_field->charset();
  memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  cs->cset->fill(cs, (char*) copy->to_ptr+copy->from_length,
                     copy->to_length-copy->from_length, '\0');
}



static void do_expand_string(Copy_field *copy)
{
  CHARSET_INFO *cs= copy->from_field->charset();
  memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  cs->cset->fill(cs, (char*) copy->to_ptr+copy->from_length,
                     copy->to_length-copy->from_length, ' ');
}


static void do_varstring1(Copy_field *copy)
{
  uint length= (uint) *(uchar*) copy->from_ptr;
  if (length > copy->to_length- 1)
  {
    length=copy->to_length - 1;
    if (copy->from_field->table->in_use->count_cuted_fields)
      copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                  WARN_DATA_TRUNCATED, 1);
  }
  *(uchar*) copy->to_ptr= (uchar) length;
  memcpy(copy->to_ptr+1, copy->from_ptr + 1, length);
}


static void do_varstring1_mb(Copy_field *copy)
{
  int well_formed_error;
  CHARSET_INFO *cs= copy->from_field->charset();
  uint from_length= (uint) *(uchar*) copy->from_ptr;
  const uchar *from_ptr= copy->from_ptr + 1;
  uint to_char_length= (copy->to_length - 1) / cs->mbmaxlen;
  uint length= cs->cset->well_formed_len(cs, (char*) from_ptr,
                                         (char*) from_ptr + from_length,
                                         to_char_length, &well_formed_error);
  if (length < from_length)
  {
    if (current_thd->count_cuted_fields)
      copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                  WARN_DATA_TRUNCATED, 1);
  }
  *copy->to_ptr= (uchar) length;
  memcpy(copy->to_ptr + 1, from_ptr, length);
}


static void do_varstring2(Copy_field *copy)
{
  uint length=uint2korr(copy->from_ptr);
  if (length > copy->to_length- HA_KEY_BLOB_LENGTH)
  {
    length=copy->to_length-HA_KEY_BLOB_LENGTH;
    if (copy->from_field->table->in_use->count_cuted_fields)
      copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                  WARN_DATA_TRUNCATED, 1);
  }
  int2store(copy->to_ptr,length);
  memcpy(copy->to_ptr+HA_KEY_BLOB_LENGTH, copy->from_ptr + HA_KEY_BLOB_LENGTH,
         length);
}


static void do_varstring2_mb(Copy_field *copy)
{
  int well_formed_error;
  CHARSET_INFO *cs= copy->from_field->charset();
  uint char_length= (copy->to_length - HA_KEY_BLOB_LENGTH) / cs->mbmaxlen;
  uint from_length= uint2korr(copy->from_ptr);
  const uchar *from_beg= copy->from_ptr + HA_KEY_BLOB_LENGTH;
  uint length= cs->cset->well_formed_len(cs, (char*) from_beg,
                                         (char*) from_beg + from_length,
                                         char_length, &well_formed_error);
  if (length < from_length)
  {
    if (current_thd->count_cuted_fields)
      copy->to_field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                  WARN_DATA_TRUNCATED, 1);
  }  
  int2store(copy->to_ptr, length);
  memcpy(copy->to_ptr+HA_KEY_BLOB_LENGTH, from_beg, length);
}
 

/***************************************************************************
** The different functions that fills in a Copy_field class
***************************************************************************/

/**
  copy of field to maybe null string.
  If field is null then the all bytes are set to 0.
  if field is not null then the first byte is set to 1 and the rest of the
  string is the field value.
  The 'to' buffer should have a size of field->pack_length()+1
*/

void Copy_field::set(uchar *to,Field *from)
{
  from_ptr=from->ptr;
  to_ptr=to;
  from_length=from->pack_length();
  if (from->maybe_null())
  {
    from_null_ptr=from->null_ptr;
    from_bit=	  from->null_bit;
    to_ptr[0]=	  1;				// Null as default value
    to_null_ptr=  (uchar*) to_ptr++;
    to_bit=	  1;
    if (from->table->maybe_null)
    {
      null_row=   &from->table->null_row;
      do_copy=	  do_outer_field_to_null_str;
    }
    else
      do_copy=	  do_field_to_null_str;
  }
  else
  {
    to_null_ptr=  0;				// For easy debugging
    do_copy=	  do_field_eq;
  }
}


/*
  To do: 

  If 'save' is set to true and the 'from' is a blob field, do_copy is set to
  do_save_blob rather than do_conv_blob.  The only differences between them
  appears to be:

  - do_save_blob allocates and uses an intermediate buffer before calling 
    Field_blob::store. Is this in order to trigger the call to 
    well_formed_copy_nchars, by changing the pointer copy->tmp.ptr()?
    That call will take place anyway in all known cases.

  - The above causes a truncation to MAX_FIELD_WIDTH. Is this the intended 
    effect? Truncation is handled by well_formed_copy_nchars anyway.
 */
void Copy_field::set(Field *to,Field *from,bool save)
{
  if (to->type() == MYSQL_TYPE_NULL)
  {
    to_null_ptr=0;				// For easy debugging
    to_ptr=0;
    do_copy=do_skip;
    return;
  }
  from_field=from;
  to_field=to;
  from_ptr=from->ptr;
  from_length=from->pack_length();
  to_ptr=  to->ptr;
  to_length=to_field->pack_length();

  // set up null handling
  from_null_ptr=to_null_ptr=0;
  if (from->maybe_null())
  {
    from_null_ptr=	from->null_ptr;
    from_bit=		from->null_bit;
    if (to_field->real_maybe_null())
    {
      to_null_ptr=	to->null_ptr;
      to_bit=		to->null_bit;
      if (from_null_ptr)
	do_copy=	do_copy_null;
      else
      {
	null_row=	&from->table->null_row;
	do_copy=	do_outer_field_null;
      }
    }
    else
    {
      if (to_field->type() == MYSQL_TYPE_TIMESTAMP)
        do_copy= do_copy_timestamp;               // Automatic timestamp
      else if (to_field == to_field->table->next_number_field)
        do_copy= do_copy_next_number;
      else
        do_copy= do_copy_not_null;
    }
  }
  else if (to_field->real_maybe_null())
  {
    to_null_ptr=	to->null_ptr;
    to_bit=		to->null_bit;
    do_copy= do_copy_maybe_null;
  }
  else
   do_copy=0;

  if ((to->flags & BLOB_FLAG) && save)
    do_copy2= do_save_blob;
  else
    do_copy2= get_copy_func(to,from);
  if (!do_copy)					// Not null
    do_copy=do_copy2;
}


Copy_field::Copy_func *
Copy_field::get_copy_func(Field *to,Field *from)
{
  bool compatible_db_low_byte_first= (to->table->s->db_low_byte_first ==
                                     from->table->s->db_low_byte_first);
  if (to->flags & BLOB_FLAG)
  {
    if (!(from->flags & BLOB_FLAG) || from->charset() != to->charset())
      return do_conv_blob;
    if (from_length != to_length || !compatible_db_low_byte_first)
    {
      // Correct pointer to point at char pointer
      to_ptr+=   to_length - to->table->s->blob_ptr_size;
      from_ptr+= from_length- from->table->s->blob_ptr_size;
      return do_copy_blob;
    }
  }
  else
  {
    if (to->real_type() == MYSQL_TYPE_BIT ||
        from->real_type() == MYSQL_TYPE_BIT)
      return do_field_int;
    if (to->result_type() == DECIMAL_RESULT)
      return do_field_decimal;
    // Check if identical fields
    if (from->result_type() == STRING_RESULT)
    {
      /*
        Detect copy from pre 5.0 varbinary to varbinary as of 5.0 and
        use special copy function that removes trailing spaces and thus
        repairs data.
      */
      if (from->type() == MYSQL_TYPE_VAR_STRING && !from->has_charset() &&
          to->type() == MYSQL_TYPE_VARCHAR && !to->has_charset())
        return do_field_varbinary_pre50;

      /*
        If we are copying date or datetime's we have to check the dates
        if we don't allow 'all' dates.
      */
      if (to->real_type() != from->real_type() ||
          !compatible_db_low_byte_first ||
          (((to->table->in_use->variables.sql_mode &
            (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE | MODE_INVALID_DATES)) &&
           to->type() == MYSQL_TYPE_DATE) ||
           to->type() == MYSQL_TYPE_DATETIME))
      {
	if (from->real_type() == MYSQL_TYPE_ENUM ||
	    from->real_type() == MYSQL_TYPE_SET)
	  if (to->result_type() != STRING_RESULT)
	    return do_field_int;		// Convert SET to number
	return do_field_string;
      }
      if (to->real_type() == MYSQL_TYPE_ENUM ||
	  to->real_type() == MYSQL_TYPE_SET)
      {
	if (!to->eq_def(from))
        {
          if (from->real_type() == MYSQL_TYPE_ENUM &&
              to->real_type() == MYSQL_TYPE_ENUM)
            return do_field_enum;
          else
            return do_field_string;
        }
      }
      else if (to->charset() != from->charset())
	return do_field_string;
      else if (to->real_type() == MYSQL_TYPE_VARCHAR)
      {
        if (((Field_varstring*) to)->length_bytes !=
            ((Field_varstring*) from)->length_bytes)
          return do_field_string;
        else
          return (((Field_varstring*) to)->length_bytes == 1 ?
                  (from->charset()->mbmaxlen == 1 ? do_varstring1 :
                                                    do_varstring1_mb) :
                  (from->charset()->mbmaxlen == 1 ? do_varstring2 :
                                                    do_varstring2_mb));
      }
      else if (to_length < from_length)
	return (from->charset()->mbmaxlen == 1 ?
                do_cut_string : do_cut_string_complex);
      else if (to_length > from_length)
      {
        if (to->charset() == &my_charset_bin)
          return do_expand_binary;
        else
          return do_expand_string;
      }

    }
    else if (to->real_type() != from->real_type() ||
	     to_length != from_length ||
             !compatible_db_low_byte_first)
    {
      if (to->real_type() == MYSQL_TYPE_DECIMAL ||
	  to->result_type() == STRING_RESULT)
	return do_field_string;
      if (to->result_type() == INT_RESULT)
	return do_field_int;
      return do_field_real;
    }
    else
    {
      if (!to->eq_def(from) || !compatible_db_low_byte_first)
      {
	if (to->real_type() == MYSQL_TYPE_DECIMAL)
	  return do_field_string;
	if (to->result_type() == INT_RESULT)
	  return do_field_int;
	else
	  return do_field_real;
      }
    }
  }
    /* Eq fields */
  switch (to_length) {
  case 1: return do_field_1;
  case 2: return do_field_2;
  case 3: return do_field_3;
  case 4: return do_field_4;
  case 6: return do_field_6;
  case 8: return do_field_8;
  }
  return do_field_eq;
}


/** Simple quick field convert that is called on insert. */

int field_conv(Field *to,Field *from)
{
  if (to->real_type() == from->real_type() &&
      !(to->type() == MYSQL_TYPE_BLOB && to->table->copy_blobs))
  {
    if (to->pack_length() == from->pack_length() &&
        !(to->flags & UNSIGNED_FLAG && !(from->flags & UNSIGNED_FLAG)) &&
	to->real_type() != MYSQL_TYPE_ENUM &&
	to->real_type() != MYSQL_TYPE_SET &&
        to->real_type() != MYSQL_TYPE_BIT &&
        (to->real_type() != MYSQL_TYPE_NEWDECIMAL ||
         (to->field_length == from->field_length &&
          (((Field_num*)to)->dec == ((Field_num*)from)->dec))) &&
        from->charset() == to->charset() &&
	to->table->s->db_low_byte_first == from->table->s->db_low_byte_first &&
        (!(to->table->in_use->variables.sql_mode &
           (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE | MODE_INVALID_DATES)) ||
         (to->type() != MYSQL_TYPE_DATE &&
          to->type() != MYSQL_TYPE_DATETIME)) &&
        (from->real_type() != MYSQL_TYPE_VARCHAR ||
         ((Field_varstring*)from)->length_bytes ==
          ((Field_varstring*)to)->length_bytes))
    {						// Identical fields
      // to->ptr==from->ptr may happen if one does 'UPDATE ... SET x=x'
      memmove(to->ptr, from->ptr, to->pack_length());
      return 0;
    }
  }
  if (to->type() == MYSQL_TYPE_BLOB)
  {						// Be sure the value is stored
    Field_blob *blob=(Field_blob*) to;
    from->val_str(&blob->value);
    /*
      Copy value if copy_blobs is set, or source is not a string and
      we have a pointer to its internal string conversion buffer.
    */
    if (to->table->copy_blobs ||
        (!blob->value.is_alloced() &&
         from->real_type() != MYSQL_TYPE_STRING &&
         from->real_type() != MYSQL_TYPE_VARCHAR))
      blob->value.copy();
    return blob->store(blob->value.ptr(),blob->value.length(),from->charset());
  }
  if (from->real_type() == MYSQL_TYPE_ENUM &&
      to->real_type() == MYSQL_TYPE_ENUM &&
      from->val_int() == 0)
  {
    ((Field_enum *)(to))->store_type(0);
    return 0;
  }
  else if ((from->result_type() == STRING_RESULT &&
            (to->result_type() == STRING_RESULT ||
             (from->real_type() != MYSQL_TYPE_ENUM &&
              from->real_type() != MYSQL_TYPE_SET))) ||
           to->type() == MYSQL_TYPE_DECIMAL)
  {
    char buff[MAX_FIELD_WIDTH];
    String result(buff,sizeof(buff),from->charset());
    from->val_str(&result);
    /*
      We use c_ptr_quick() here to make it easier if to is a float/double
      as the conversion routines will do a copy of the result doesn't
      end with \0. Can be replaced with .ptr() when we have our own
      string->double conversion.
    */
    return to->store(result.c_ptr_quick(),result.length(),from->charset());
  }
  else if (from->result_type() == REAL_RESULT)
    return to->store(from->val_real());
  else if (from->result_type() == DECIMAL_RESULT)
  {
    my_decimal buff;
    return to->store_decimal(from->val_decimal(&buff));
  }
  else
    return to->store(from->val_int(), test(from->flags & UNSIGNED_FLAG));
}
