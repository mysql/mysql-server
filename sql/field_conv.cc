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
#include "sql_time.h"
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
  if (*copy->null_row ||
      (copy->from_null_ptr && (*copy->from_null_ptr & copy->from_bit)))
  {
    memset(copy->to_ptr, 0, copy->from_length);
    copy->to_null_ptr[0]=1;			// Always bit 1
  }
  else
  {
    copy->to_null_ptr[0]=0;
    memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  }
}


type_conversion_status set_field_to_null(Field *field)
{
  if (field->real_maybe_null())
  {
    field->set_null();
    field->reset();
    return TYPE_OK;
  }
  field->reset();
  switch (field->table->in_use->count_cuted_fields) {
  case CHECK_FIELD_WARN:
    field->set_warning(Sql_condition::SL_WARNING, WARN_DATA_TRUNCATED, 1);
    /* fall through */
  case CHECK_FIELD_IGNORE:
    return TYPE_OK;
  case CHECK_FIELD_ERROR_FOR_NULL:
    if (!field->table->in_use->no_errors)
      my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
    return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
  }
  DBUG_ASSERT(false); // impossible
  return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
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

type_conversion_status
set_field_to_null_with_conversions(Field *field, bool no_conversions)
{
  if (field->real_maybe_null())
  {
    field->set_null();
    field->reset();
    return TYPE_OK;
  }
  if (no_conversions)
    return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;

  /*
    Check if this is a special type, which will get a special walue
    when set to NULL (TIMESTAMP fields which allow setting to NULL
    are handled by first check).

    From the manual:

    TIMESTAMP columns [...] assigning NULL assigns the current timestamp.
  */
  if (field->type() == MYSQL_TYPE_TIMESTAMP)
  {
    Item_func_now_local::store_in(field);
    return TYPE_OK;			// Ok to set time to NULL
  }
  
  // Note: we ignore any potential failure of reset() here.
  field->reset();

  if (field == field->table->next_number_field)
  {
    field->table->auto_increment_field_not_null= FALSE;
    return TYPE_OK;		        // field is set in fill_record()
  }
  switch (field->table->in_use->count_cuted_fields) {
  case CHECK_FIELD_WARN:
    field->set_warning(Sql_condition::SL_WARNING, ER_BAD_NULL_ERROR, 1);
    /* fall through */
  case CHECK_FIELD_IGNORE:
    return TYPE_OK;
  case CHECK_FIELD_ERROR_FOR_NULL:
    if (!field->table->in_use->no_errors)
      my_error(ER_BAD_NULL_ERROR, MYF(0), field->field_name);
    return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
  }
  DBUG_ASSERT(false); // impossible
  return TYPE_ERR_NULL_CONSTRAINT_VIOLATION;
}


static void do_skip(Copy_field *copy __attribute__((unused)))
{
}


static void do_copy_null(Copy_field *copy)
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
  if (*copy->null_row || (*copy->from_null_ptr & copy->from_bit))
  {
    copy->to_field->set_warning(Sql_condition::SL_WARNING,
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
  if (*copy->null_row || (*copy->from_null_ptr & copy->from_bit))
  {
    /* Same as in set_field_to_null_with_conversions() */
    Item_func_now_local::store_in(copy->to_field);
  }
  else
    (copy->do_copy2)(copy);
}


static void do_copy_next_number(Copy_field *copy)
{
  if (*copy->null_row || (*copy->from_null_ptr & copy->from_bit))
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


inline type_conversion_status copy_time_to_time(Field *from, Field *to)
{
  MYSQL_TIME ltime;
  from->get_time(&ltime);
  return to->store_time(&ltime);
}

/**
  Convert between fields using time representation.
*/
static void do_field_time(Copy_field *copy)
{
  (void) copy_time_to_time(copy->from_field, copy->to_field);
}


/**
  string copy for single byte characters set when to string is shorter than
  from string.
*/

static void do_cut_string(Copy_field *copy)
{
  const CHARSET_INFO *cs= copy->from_field->charset();
  memcpy(copy->to_ptr,copy->from_ptr,copy->to_length);

  /* Check if we loosed any important characters */
  if (cs->cset->scan(cs,
                     (char*) copy->from_ptr + copy->to_length,
                     (char*) copy->from_ptr + copy->from_length,
                     MY_SEQ_SPACES) < copy->from_length - copy->to_length)
  {
    copy->to_field->set_warning(Sql_condition::SL_WARNING,
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
  const CHARSET_INFO *cs= copy->from_field->charset();
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
    copy->to_field->set_warning(Sql_condition::SL_WARNING,
                                WARN_DATA_TRUNCATED, 1);
  }

  if (copy_length < copy->to_length)
    cs->cset->fill(cs, (char*) copy->to_ptr + copy_length,
                   copy->to_length - copy_length, ' ');
}




static void do_expand_binary(Copy_field *copy)
{
  const CHARSET_INFO *cs= copy->from_field->charset();
  memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  cs->cset->fill(cs, (char*) copy->to_ptr+copy->from_length,
                     copy->to_length-copy->from_length, '\0');
}



static void do_expand_string(Copy_field *copy)
{
  const CHARSET_INFO *cs= copy->from_field->charset();
  memcpy(copy->to_ptr,copy->from_ptr,copy->from_length);
  cs->cset->fill(cs, (char*) copy->to_ptr+copy->from_length,
                     copy->to_length-copy->from_length, ' ');
}

/**
  Find how many bytes should be copied between Field_varstring fields
  so that only the bytes in use in the 'from' field are copied.
  Handles single and multi-byte charsets. Adds warning if not all
  bytes in 'from' will fit into 'to'.

  @param to   Variable length field we're copying to
  @param from Variable length field we're copying from

  @return Number of bytes that should be copied from 'from' to 'to'.
*/
static uint get_varstring_copy_length(Field_varstring *to,
                                      const Field_varstring *from)
{
  const CHARSET_INFO * const cs= from->charset();
  const bool is_multibyte_charset= (cs->mbmaxlen != 1);
  const uint to_byte_length= to->row_pack_length();

  uint bytes_to_copy;
  if (from->length_bytes == 1)
    bytes_to_copy= *from->ptr;
  else
    bytes_to_copy= uint2korr(from->ptr);

  if (is_multibyte_charset)
  {
    int well_formed_error;
    const char *from_beg= reinterpret_cast<char*>(from->ptr + from->length_bytes);
    const uint to_char_length= (to_byte_length) / cs->mbmaxlen;
    const uint from_byte_length= bytes_to_copy;
    bytes_to_copy=
      cs->cset->well_formed_len(cs, from_beg,
                                from_beg + from_byte_length,
                                to_char_length,
                                &well_formed_error);
    if (bytes_to_copy < from_byte_length)
    {
      if (from->table->in_use->count_cuted_fields)
        to->set_warning(Sql_condition::SL_WARNING,
                        WARN_DATA_TRUNCATED, 1);
    }
  }
  else
  {
    if (bytes_to_copy > (to_byte_length))
    {
      bytes_to_copy= to_byte_length;
      if (from->table->in_use->count_cuted_fields)
        to->set_warning(Sql_condition::SL_WARNING,
                        WARN_DATA_TRUNCATED, 1);
    }
  }
  return bytes_to_copy;
}

/**
  A variable length string field consists of:
   (a) 1 or 2 length bytes, depending on the VARCHAR column definition
   (b) as many relevant character bytes, as defined in the length byte(s)
   (c) unused padding up to the full length of the column

  This function only copies (a) and (b)

  Condition for using this function: to and from must use the same
  number of bytes for length, i.e: to->length_bytes==from->length_bytes

  @param to   Variable length field we're copying to
  @param from Variable length field we're copying from
*/
static void copy_field_varstring(Field_varstring * const to,
                                 const Field_varstring * const from)
{
  const uint length_bytes= from->length_bytes;
  DBUG_ASSERT(length_bytes == to->length_bytes);
  DBUG_ASSERT(length_bytes == 1 || length_bytes == 2);

  const uint bytes_to_copy= get_varstring_copy_length(to, from);
  if (length_bytes == 1)
    *to->ptr= static_cast<uchar>(bytes_to_copy);
  else
    int2store(to->ptr, bytes_to_copy);

  // memcpy should not be used for overlaping memory blocks
  DBUG_ASSERT(to->ptr != from->ptr);
  memcpy(to->ptr + length_bytes, from->ptr + length_bytes, bytes_to_copy);
}

static void do_varstring(Copy_field *copy)
{
  copy_field_varstring(static_cast<Field_varstring*>(copy->to_field),
                       static_cast<Field_varstring*>(copy->from_field));
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
  null_row= &from->table->null_row;
  if (from->maybe_null())
  {
    from_null_ptr=from->get_null_ptr();
    from_bit=	  from->null_bit;
    to_ptr[0]=	  1;				// Null as default value
    to_null_ptr=  (uchar*) to_ptr++;
    to_bit=	  1;
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
  null_row= &from->table->null_row;
  if (from->maybe_null())
  {
    from_null_ptr=	from->get_null_ptr();
    from_bit=		from->null_bit;
    if (to_field->real_maybe_null())
    {
      to_null_ptr=	to->get_null_ptr();
      to_bit=		to->null_bit;
      do_copy=	do_copy_null;
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
    to_null_ptr=	to->get_null_ptr();
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
      to_ptr+=   to_length - portable_sizeof_char_ptr;
      from_ptr+= from_length - portable_sizeof_char_ptr;
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
      if (from->is_temporal())
      {
        if (to->is_temporal())
        {
          return do_field_time;
        }
        else
        {
          if (to->result_type() == INT_RESULT)
            return do_field_int;
          if (to->result_type() == REAL_RESULT)
            return do_field_real;
          /* Note: conversion from any to DECIMAL_RESULT is handled earlier */
        }
      }
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
          to->decimals() != from->decimals() /* e.g. TIME vs TIME(6) */ ||
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
          return do_varstring;
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

type_conversion_status field_conv(Field *to,Field *from)
{
  if (to->real_type() == from->real_type() &&
      !(to->type() == MYSQL_TYPE_BLOB && to->table->copy_blobs) &&
      to->charset() == from->charset())
  {
    if (to->real_type() == MYSQL_TYPE_VARCHAR &&
        from->real_type() == MYSQL_TYPE_VARCHAR)
    {
      Field_varstring *to_vc= static_cast<Field_varstring*>(to);
      const Field_varstring *from_vc= static_cast<Field_varstring*>(from);
      if (to_vc->length_bytes == from_vc->length_bytes)
      {
        copy_field_varstring(to_vc, from_vc);
        return TYPE_OK;
      }
    }
    if (to->pack_length() == from->pack_length() &&
        !(to->flags & UNSIGNED_FLAG && !(from->flags & UNSIGNED_FLAG)) &&
	to->real_type() != MYSQL_TYPE_ENUM &&
	to->real_type() != MYSQL_TYPE_SET &&
        to->real_type() != MYSQL_TYPE_BIT &&
        (!to->is_temporal_with_time() ||
         to->decimals() == from->decimals()) &&
        (to->real_type() != MYSQL_TYPE_NEWDECIMAL ||
         (to->field_length == from->field_length &&
          (((Field_num*)to)->dec == ((Field_num*)from)->dec))) &&
	to->table->s->db_low_byte_first == from->table->s->db_low_byte_first &&
        (!(to->table->in_use->variables.sql_mode &
           (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE | MODE_INVALID_DATES)) ||
         (to->type() != MYSQL_TYPE_DATE &&
          to->type() != MYSQL_TYPE_DATETIME)) &&
        (from->real_type() != MYSQL_TYPE_VARCHAR))
    {						// Identical fields
      // to->ptr==from->ptr may happen if one does 'UPDATE ... SET x=x'
      memmove(to->ptr, from->ptr, to->pack_length());
      return TYPE_OK;
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
    return TYPE_OK;
  }
  else if (from->is_temporal() && to->result_type() == INT_RESULT)
  {
    MYSQL_TIME ltime;
    longlong nr;
    if (from->type() == MYSQL_TYPE_TIME)
    {
      from->get_time(&ltime);
      nr= TIME_to_ulonglong_time_round(&ltime);
    }
    else
    {
      from->get_date(&ltime, TIME_FUZZY_DATE);
      nr= TIME_to_ulonglong_datetime_round(&ltime);
    }
    return to->store(ltime.neg ? -nr : nr, 0);
  }
  else if (from->is_temporal() &&
           (to->result_type() == REAL_RESULT ||
            to->result_type() == DECIMAL_RESULT ||
            to->result_type() == INT_RESULT))
  {
    my_decimal tmp;
    /*
      We prefer DECIMAL as the safest precise type:
      double supports only 15 digits, which is not enough for DATETIME(6).
    */
    return to->store_decimal(from->val_decimal(&tmp));
  }
  else if (from->is_temporal() && to->is_temporal())
  {
    return copy_time_to_time(from, to);
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
