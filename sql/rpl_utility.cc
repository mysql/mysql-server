/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_utility.h"

#ifndef MYSQL_CLIENT
#include "unireg.h"                      // REQUIRED by later includes
#include "rpl_rli.h"
#include "sql_tmp_table.h"               // tmp tables
#include "rpl_rli.h"
#include "log_event.h"

#include <algorithm>

using std::min;
using std::max;


/**
   Function to compare two size_t integers for their relative
   order. Used below.
 */
static int compare(size_t a, size_t b)
{
  if (a < b)
    return -1;
  if (b < a)
    return 1;
  return 0;
}


/**
   Max value for an unsigned integer of 'bits' bits.

   The somewhat contorted expression is to avoid overflow.
 */
static uint32 uint_max(int bits) {
  return (((1UL << (bits - 1)) - 1) << 1) | 1;
}


/**
   Compute the maximum display length of a field.

   @param sql_type Type of the field
   @param metadata The metadata from the master for the field.
   @return Maximum length of the field in bytes.
 */
static uint32
max_display_length_for_field(enum_field_types sql_type, unsigned int metadata)
{
  DBUG_PRINT("debug", ("sql_type: %d, metadata: 0x%x", sql_type, metadata));
  DBUG_ASSERT(metadata >> 16 == 0);

  switch (sql_type) {
  case MYSQL_TYPE_NEWDECIMAL:
    return metadata >> 8;

  case MYSQL_TYPE_FLOAT:
    return 12;

  case MYSQL_TYPE_DOUBLE:
    return 22;

  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_ENUM:
      return metadata & 0x00ff;

  case MYSQL_TYPE_STRING:
  {
    uchar type= metadata >> 8;
    if (type == MYSQL_TYPE_SET || type == MYSQL_TYPE_ENUM)
      return metadata & 0xff;
    else
      /* This is taken from Field_string::unpack. */
      return (((metadata >> 4) & 0x300) ^ 0x300) + (metadata & 0x00ff);
  }

  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_TINY:
    return 4;

  case MYSQL_TYPE_SHORT:
    return 6;

  case MYSQL_TYPE_INT24:
    return 9;

  case MYSQL_TYPE_LONG:
    return 11;

#ifdef HAVE_LONG_LONG
  case MYSQL_TYPE_LONGLONG:
    return 20;

#endif
  case MYSQL_TYPE_NULL:
    return 0;

  case MYSQL_TYPE_NEWDATE:
    return 3;

  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_TIME2:
    return 3;

  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_TIMESTAMP2:
    return 4;

  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATETIME2:
    return 8;

  case MYSQL_TYPE_BIT:
    /*
      Decode the size of the bit field from the master.
    */
    DBUG_ASSERT((metadata & 0xff) <= 7);
    return 8 * (metadata >> 8U) + (metadata & 0x00ff);

  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    return metadata;

    /*
      The actual length for these types does not really matter since
      they are used to calc_pack_length, which ignores the given
      length for these types.

      Since we want this to be accurate for other uses, we return the
      maximum size in bytes of these BLOBs.
    */

  case MYSQL_TYPE_TINY_BLOB:
    return uint_max(1 * 8);

  case MYSQL_TYPE_MEDIUM_BLOB:
    return uint_max(3 * 8);

  case MYSQL_TYPE_BLOB:
    /*
      For the blob type, Field::real_type() lies and say that all
      blobs are of type MYSQL_TYPE_BLOB. In that case, we have to look
      at the length instead to decide what the max display size is.
     */
    return uint_max(metadata * 8);

  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    return uint_max(4 * 8);

  default:
    return ~(uint32) 0;
  }
}


/*
  Compare the pack lengths of a source field (on the master) and a
  target field (on the slave).

  @param field    Target field.
  @param type     Source field type.
  @param metadata Source field metadata.

  @retval -1 The length of the source field is smaller than the target field.
  @retval  0 The length of the source and target fields are the same.
  @retval  1 The length of the source field is greater than the target field.
 */
int compare_lengths(Field *field, enum_field_types source_type, uint16 metadata)
{
  DBUG_ENTER("compare_lengths");
  size_t const source_length=
    max_display_length_for_field(source_type, metadata);
  size_t const target_length= field->max_display_length();
  DBUG_PRINT("debug", ("source_length: %lu, source_type: %u,"
                       " target_length: %lu, target_type: %u",
                       (unsigned long) source_length, source_type,
                       (unsigned long) target_length, field->real_type()));
  int result= compare(source_length, target_length);
  DBUG_PRINT("result", ("%d", result));
  DBUG_RETURN(result);
}


/*********************************************************************
 *                   table_def member definitions                    *
 *********************************************************************/

/*
  This function returns the field size in raw bytes based on the type
  and the encoded field data from the master's raw data.
*/
uint32 table_def::calc_field_size(uint col, uchar *master_data) const
{
  uint32 length;

  switch (type(col)) {
  case MYSQL_TYPE_NEWDECIMAL:
    length= my_decimal_get_binary_size(m_field_metadata[col] >> 8, 
                                       m_field_metadata[col] & 0xff);
    break;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    length= m_field_metadata[col];
    break;
  /*
    The cases for SET and ENUM are include for completeness, however
    both are mapped to type MYSQL_TYPE_STRING and their real types
    are encoded in the field metadata.
  */
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_STRING:
  {
    uchar type= m_field_metadata[col] >> 8U;
    if ((type == MYSQL_TYPE_SET) || (type == MYSQL_TYPE_ENUM))
      length= m_field_metadata[col] & 0x00ff;
    else
    {
      /*
        We are reading the actual size from the master_data record
        because this field has the actual lengh stored in the first
        one or two bytes.
      */
      length= max_display_length_for_field(MYSQL_TYPE_STRING, m_field_metadata[col]) > 255 ? 2 : 1;

      /* As in Field_string::unpack */
      length+= ((length == 1) ? *master_data : uint2korr(master_data));
    }
    break;
  }
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_TINY:
    length= 1;
    break;
  case MYSQL_TYPE_SHORT:
    length= 2;
    break;
  case MYSQL_TYPE_INT24:
    length= 3;
    break;
  case MYSQL_TYPE_LONG:
    length= 4;
    break;
#ifdef HAVE_LONG_LONG
  case MYSQL_TYPE_LONGLONG:
    length= 8;
    break;
#endif
  case MYSQL_TYPE_NULL:
    length= 0;
    break;
  case MYSQL_TYPE_NEWDATE:
    length= 3;
    break;
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
    length= 3;
    break;
  case MYSQL_TYPE_TIME2:
    length= my_time_binary_length(m_field_metadata[col]);
    break;
  case MYSQL_TYPE_TIMESTAMP:
    length= 4;
    break;
  case MYSQL_TYPE_TIMESTAMP2:
    length= my_timestamp_binary_length(m_field_metadata[col]);
    break;
  case MYSQL_TYPE_DATETIME:
    length= 8;
    break;
  case MYSQL_TYPE_DATETIME2:
    length= my_datetime_binary_length(m_field_metadata[col]);
    break;
  case MYSQL_TYPE_BIT:
  {
    /*
      Decode the size of the bit field from the master.
        from_len is the length in bytes from the master
        from_bit_len is the number of extra bits stored in the master record
      If from_bit_len is not 0, add 1 to the length to account for accurate
      number of bytes needed.
    */
    uint from_len= (m_field_metadata[col] >> 8U) & 0x00ff;
    uint from_bit_len= m_field_metadata[col] & 0x00ff;
    DBUG_ASSERT(from_bit_len <= 7);
    length= from_len + ((from_bit_len > 0) ? 1 : 0);
    break;
  }
  case MYSQL_TYPE_VARCHAR:
  {
    length= m_field_metadata[col] > 255 ? 2 : 1; // c&p of Field_varstring::data_length()
    length+= length == 1 ? (uint32) *master_data : uint2korr(master_data);
    break;
  }
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
  {
#if 1
    /*
      BUG#29549: 
      This is currently broken for NDB, which is using big-endian
      order when packing length of BLOB. Once they have decided how to
      fix the issue, we can enable the code below to make sure to
      always read the length in little-endian order.
    */
    Field_blob fb(m_field_metadata[col]);
    length= fb.get_packed_size(master_data, TRUE);
#else
    /*
      Compute the length of the data. We cannot use get_length() here
      since it is dependent on the specific table (and also checks the
      packlength using the internal 'table' pointer) and replication
      is using a fixed format for storing data in the binlog.
    */
    switch (m_field_metadata[col]) {
    case 1:
      length= *master_data;
      break;
    case 2:
      length= uint2korr(master_data);
      break;
    case 3:
      length= uint3korr(master_data);
      break;
    case 4:
      length= uint4korr(master_data);
      break;
    default:
      DBUG_ASSERT(0);		// Should not come here
      break;
    }

    length+= m_field_metadata[col];
#endif
    break;
  }
  default:
    length= ~(uint32) 0;
  }
  return length;
}


/**
 */
static void show_sql_type(enum_field_types type, uint16 metadata, String *str,
                          const CHARSET_INFO *field_cs)
{
  DBUG_ENTER("show_sql_type");
  DBUG_PRINT("enter", ("type: %d, metadata: 0x%x", type, metadata));

  switch (type)
  {
  case MYSQL_TYPE_TINY:
    str->set_ascii(STRING_WITH_LEN("tinyint"));
    break;

  case MYSQL_TYPE_SHORT:
    str->set_ascii(STRING_WITH_LEN("smallint"));
    break;

  case MYSQL_TYPE_LONG:
    str->set_ascii(STRING_WITH_LEN("int"));
    break;

  case MYSQL_TYPE_FLOAT:
    str->set_ascii(STRING_WITH_LEN("float"));
    break;

  case MYSQL_TYPE_DOUBLE:
    str->set_ascii(STRING_WITH_LEN("double"));
    break;

  case MYSQL_TYPE_NULL:
    str->set_ascii(STRING_WITH_LEN("null"));
    break;

  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_TIMESTAMP2:
    str->set_ascii(STRING_WITH_LEN("timestamp"));
    break;

  case MYSQL_TYPE_LONGLONG:
    str->set_ascii(STRING_WITH_LEN("bigint"));
    break;

  case MYSQL_TYPE_INT24:
    str->set_ascii(STRING_WITH_LEN("mediumint"));
    break;

  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_DATE:
    str->set_ascii(STRING_WITH_LEN("date"));
    break;

  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_TIME2:
    str->set_ascii(STRING_WITH_LEN("time"));
    break;

  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATETIME2:
    str->set_ascii(STRING_WITH_LEN("datetime"));
    break;

  case MYSQL_TYPE_YEAR:
    str->set_ascii(STRING_WITH_LEN("year"));
    break;

  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    {
      const CHARSET_INFO *cs= str->charset();
      uint32 length=
        cs->cset->snprintf(cs, (char*) str->ptr(), str->alloced_length(),
                           "varchar(%u)", metadata);
      str->length(length);
    }
    break;

  case MYSQL_TYPE_BIT:
    {
      const CHARSET_INFO *cs= str->charset();
      int bit_length= 8 * (metadata >> 8) + (metadata & 0xFF);
      uint32 length=
        cs->cset->snprintf(cs, (char*) str->ptr(), str->alloced_length(),
                           "bit(%d)", bit_length);
      str->length(length);
    }
    break;

  case MYSQL_TYPE_DECIMAL:
    {
      const CHARSET_INFO *cs= str->charset();
      uint32 length=
        cs->cset->snprintf(cs, (char*) str->ptr(), str->alloced_length(),
                           "decimal(%d,?)", metadata);
      str->length(length);
    }
    break;

  case MYSQL_TYPE_NEWDECIMAL:
    {
      const CHARSET_INFO *cs= str->charset();
      uint32 length=
        cs->cset->snprintf(cs, (char*) str->ptr(), str->alloced_length(),
                           "decimal(%d,%d)", metadata >> 8, metadata & 0xff);
      str->length(length);
    }
    break;

  case MYSQL_TYPE_ENUM:
    str->set_ascii(STRING_WITH_LEN("enum"));
    break;

  case MYSQL_TYPE_SET:
    str->set_ascii(STRING_WITH_LEN("set"));
    break;

  case MYSQL_TYPE_BLOB:
    /*
      Field::real_type() lies regarding the actual type of a BLOB, so
      it is necessary to check the pack length to figure out what kind
      of blob it really is.
     */
    switch (get_blob_type_from_length(metadata))
    {
    case MYSQL_TYPE_TINY_BLOB:
      str->set_ascii(STRING_WITH_LEN("tinyblob"));
      break;

    case MYSQL_TYPE_MEDIUM_BLOB:
      str->set_ascii(STRING_WITH_LEN("mediumblob"));
      break;

    case MYSQL_TYPE_LONG_BLOB:
      str->set_ascii(STRING_WITH_LEN("longblob"));
      break;

    case MYSQL_TYPE_BLOB:
      str->set_ascii(STRING_WITH_LEN("blob"));
      break;

    default:
      DBUG_ASSERT(0);
      break;
    }
    break;

  case MYSQL_TYPE_STRING:
    {
      /*
        This is taken from Field_string::unpack.
      */
      const CHARSET_INFO *cs= str->charset();
      uint bytes= (((metadata >> 4) & 0x300) ^ 0x300) + (metadata & 0x00ff);
      uint32 length=
        cs->cset->snprintf(cs, (char*) str->ptr(), str->alloced_length(),
                           "char(%d)", bytes / field_cs->mbmaxlen);
      str->length(length);
    }
    break;

  case MYSQL_TYPE_GEOMETRY:
    str->set_ascii(STRING_WITH_LEN("geometry"));
    break;

  default:
    str->set_ascii(STRING_WITH_LEN("<unknown type>"));
  }
  DBUG_VOID_RETURN;
}


/**
   Check the order variable and print errors if the order is not
   acceptable according to the current settings.

   @param order  The computed order of the conversion needed.
   @param rli    The relay log info data structure: for error reporting.
 */
bool is_conversion_ok(int order, Relay_log_info *rli)
{
  DBUG_ENTER("is_conversion_ok");
  bool allow_non_lossy, allow_lossy;

  allow_non_lossy = slave_type_conversions_options &
                    (ULL(1) << SLAVE_TYPE_CONVERSIONS_ALL_NON_LOSSY);
  allow_lossy= slave_type_conversions_options &
               (ULL(1) << SLAVE_TYPE_CONVERSIONS_ALL_LOSSY);

  DBUG_PRINT("enter", ("order: %d, flags:%s%s", order,
                       allow_non_lossy ? " ALL_NON_LOSSY" : "",
                       allow_lossy ? " ALL_LOSSY" : ""));
  if (order < 0 && !allow_non_lossy)
  {
    /* !!! Add error message saying that non-lossy conversions need to be allowed. */
    DBUG_RETURN(false);
  }

  if (order > 0 && !allow_lossy)
  {
    /* !!! Add error message saying that lossy conversions need to be allowed. */
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}


/**
   Can a type potentially be converted to another type?

   This function check if the types are convertible and what
   conversion is required.

   If conversion is not possible, and error is printed.

   If conversion is possible:

   - *order will be set to -1 if source type is smaller than target
     type and a non-lossy conversion can be required. This includes
     the case where the field types are different but types could
     actually be converted in either direction.

   - *order will be set to 0 if no conversion is required.

   - *order will be set to 1 if the source type is strictly larger
      than the target type and that conversion is potentially lossy.

   @param[in] field    Target field
   @param[in] type     Source field type
   @param[in] metadata Source field metadata
   @param[in] rli      Relay log info (for error reporting)
   @param[in] mflags   Flags from the table map event
   @param[out] order   Order between source field and target field

   @return @c true if conversion is possible according to the current
   settings, @c false if conversion is not possible according to the
   current setting.
 */
static bool
can_convert_field_to(Field *field,
                     enum_field_types source_type, uint16 metadata,
                     Relay_log_info *rli, uint16 mflags,
                     int *order_var)
{
  DBUG_ENTER("can_convert_field_to");
#ifndef DBUG_OFF
  char field_type_buf[MAX_FIELD_WIDTH];
  String field_type(field_type_buf, sizeof(field_type_buf), &my_charset_latin1);
  field->sql_type(field_type);
  DBUG_PRINT("enter", ("field_type: %s, target_type: %d, source_type: %d, source_metadata: 0x%x",
                       field_type.c_ptr_safe(), field->real_type(), source_type, metadata));
#endif
  /*
    If the real type is the same, we need to check the metadata to
    decide if conversions are allowed.
   */
  if (field->real_type() == source_type)
  {
    if (metadata == 0) // Metadata can only be zero if no metadata was provided
    {
      /*
        If there is no metadata, we either have an old event where no
        metadata were supplied, or a type that does not require any
        metadata. In either case, conversion can be done but no
        conversion table is necessary.
       */
      DBUG_PRINT("debug", ("Base types are identical, but there is no metadata"));
      *order_var= 0;
      DBUG_RETURN(true);
    }

    DBUG_PRINT("debug", ("Base types are identical, doing field size comparison"));
    if (field->compatible_field_size(metadata, rli, mflags, order_var))
      DBUG_RETURN(is_conversion_ok(*order_var, rli));
    else
      DBUG_RETURN(false);
  }
  else if (metadata == 0 &&
           ((field->real_type() == MYSQL_TYPE_TIMESTAMP2 &&
             source_type == MYSQL_TYPE_TIMESTAMP) ||
            (field->real_type() == MYSQL_TYPE_TIME2 &&
             source_type == MYSQL_TYPE_TIME) ||
            (field->real_type() == MYSQL_TYPE_DATETIME2 &&
             source_type == MYSQL_TYPE_DATETIME)))
  {
    /*
      TS-TODO: conversion from FSP1>FSP2.
      Can do non-lossy conversion
      from old TIME, TIMESTAMP, DATETIME
      to new TIME(0), TIMESTAMP(0), DATETIME(0).
    */
    *order_var= -1;
    DBUG_RETURN(true);
  }
  else if (!slave_type_conversions_options)
    DBUG_RETURN(false);

  /*
    Here, from and to will always be different. Since the types are
    different, we cannot use the compatible_field_size() function, but
    have to rely on hard-coded max-sizes for fields.
  */

  DBUG_PRINT("debug", ("Base types are different, checking conversion"));
  switch (source_type)                      // Source type (on master)
  {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    switch (field->real_type())
    {
    case MYSQL_TYPE_NEWDECIMAL:
      /*
        Then the other type is either FLOAT, DOUBLE, or old style
        DECIMAL, so we require lossy conversion.
      */
      *order_var= 1;
      DBUG_RETURN(is_conversion_ok(*order_var, rli));
      
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    {
      if (source_type == MYSQL_TYPE_NEWDECIMAL ||
          source_type == MYSQL_TYPE_DECIMAL)
        *order_var = 1;                         // Always require lossy conversions
      else
        *order_var= compare_lengths(field, source_type, metadata);
      DBUG_ASSERT(*order_var != 0);
      DBUG_RETURN(is_conversion_ok(*order_var, rli));
    }

    default:
      DBUG_RETURN(false);
    }
    break;

  /*
    The length comparison check will do the correct job of comparing
    the field lengths (in bytes) of two integer types.
  */
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
    switch (field->real_type())
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      *order_var= compare_lengths(field, source_type, metadata);
      DBUG_ASSERT(*order_var != 0);
      DBUG_RETURN(is_conversion_ok(*order_var, rli));

    default:
      DBUG_RETURN(false);
    }
    break;

  /*
    Since source and target type is different, and it is not possible
    to convert bit types to anything else, this will return false.
   */
  case MYSQL_TYPE_BIT:
    DBUG_RETURN(false);

  /*
    If all conversions are disabled, it is not allowed to convert
    between these types. Since the TEXT vs. BINARY is distinguished by
    the charset, and the charset is not replicated, we cannot
    currently distinguish between , e.g., TEXT and BLOB.
   */
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    switch (field->real_type())
    {
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
      *order_var= compare_lengths(field, source_type, metadata);
      /*
        Here we know that the types are different, so if the order
        gives that they do not require any conversion, we still need
        to have non-lossy conversion enabled to allow conversion
        between different (string) types of the same length.
       */
      if (*order_var == 0)
        *order_var= -1;
      DBUG_RETURN(is_conversion_ok(*order_var, rli));

    default:
      DBUG_RETURN(false);
    }
    break;

  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_TIMESTAMP2:
  case MYSQL_TYPE_DATETIME2:
  case MYSQL_TYPE_TIME2:
    DBUG_RETURN(false);
  }
  DBUG_RETURN(false);                                 // To keep GCC happy
}


/**
  Is the definition compatible with a table?

  This function will compare the master table with an existing table
  on the slave and see if they are compatible with respect to the
  current settings of @c SLAVE_TYPE_CONVERSIONS.

  If the tables are compatible and conversions are required, @c
  *tmp_table_var will be set to a virtual temporary table with field
  pointers for the fields that require conversions.  This allow simple
  checking of whether a conversion are to be applied or not.

  If tables are compatible, but no conversions are necessary, @c
  *tmp_table_var will be set to NULL.

  @param rli_arg[in]
  Relay log info, for error reporting.

  @param table[in]
  Table to compare with

  @param tmp_table_var[out]
  Virtual temporary table for performing conversions, if necessary.

  @retval true Master table is compatible with slave table.
  @retval false Master table is not compatible with slave table.
*/
bool
table_def::compatible_with(THD *thd, Relay_log_info *rli,
                           TABLE *table, TABLE **conv_table_var)
  const
{
  /*
    We only check the initial columns for the tables.
  */
  uint const cols_to_check= min<ulong>(table->s->fields, size());
  TABLE *tmp_table= NULL;

  for (uint col= 0 ; col < cols_to_check ; ++col)
  {
    Field *const field= table->field[col];
    int order;
    if (can_convert_field_to(field, type(col), field_metadata(col), rli, m_flags, &order))
    {
      DBUG_PRINT("debug", ("Checking column %d -"
                           " field '%s' can be converted - order: %d",
                           col, field->field_name, order));
      DBUG_ASSERT(order >= -1 && order <= 1);

      /*
        If order is not 0, a conversion is required, so we need to set
        up the conversion table.
       */
      if (order != 0 && tmp_table == NULL)
      {
        /*
          This will create the full table with all fields. This is
          necessary to ge the correct field lengths for the record.
        */
        tmp_table= create_conversion_table(thd, rli, table);
        if (tmp_table == NULL)
            return false;
        /*
          Clear all fields up to, but not including, this column.
        */
        for (unsigned int i= 0; i < col; ++i)
          tmp_table->field[i]= NULL;
      }

      if (order == 0 && tmp_table != NULL)
        tmp_table->field[col]= NULL;
    }
    else
    {
      DBUG_PRINT("debug", ("Checking column %d -"
                           " field '%s' can not be converted",
                           col, field->field_name));
      DBUG_ASSERT(col < size() && col < table->s->fields);
      DBUG_ASSERT(table->s->db.str && table->s->table_name.str);
      const char *db_name= table->s->db.str;
      const char *tbl_name= table->s->table_name.str;
      char source_buf[MAX_FIELD_WIDTH];
      char target_buf[MAX_FIELD_WIDTH];
      String source_type(source_buf, sizeof(source_buf), &my_charset_latin1);
      String target_type(target_buf, sizeof(target_buf), &my_charset_latin1);
      show_sql_type(type(col), field_metadata(col), &source_type, field->charset());
      field->sql_type(target_type);
      rli->report(ERROR_LEVEL, ER_SLAVE_CONVERSION_FAILED,
                  ER(ER_SLAVE_CONVERSION_FAILED),
                  col, db_name, tbl_name,
                  source_type.c_ptr_safe(), target_type.c_ptr_safe());
      return false;
    }
  }

#ifndef DBUG_OFF
  if (tmp_table)
  {
    for (unsigned int col= 0; col < tmp_table->s->fields; ++col)
      if (tmp_table->field[col])
      {
        char source_buf[MAX_FIELD_WIDTH];
        char target_buf[MAX_FIELD_WIDTH];
        String source_type(source_buf, sizeof(source_buf), &my_charset_latin1);
        String target_type(target_buf, sizeof(target_buf), &my_charset_latin1);
        tmp_table->field[col]->sql_type(source_type);
        table->field[col]->sql_type(target_type);
        DBUG_PRINT("debug", ("Field %s - conversion required."
                             " Source type: '%s', Target type: '%s'",
                             tmp_table->field[col]->field_name,
                             source_type.c_ptr_safe(), target_type.c_ptr_safe()));
      }
  }
#endif

  *conv_table_var= tmp_table;
  return true;
}

/**
  Create a conversion table.

  If the function is unable to create the conversion table, an error
  will be printed and NULL will be returned.

  @return Pointer to conversion table, or NULL if unable to create
  conversion table.
 */

TABLE *table_def::create_conversion_table(THD *thd, Relay_log_info *rli, TABLE *target_table) const
{
  DBUG_ENTER("table_def::create_conversion_table");

  List<Create_field> field_list;
  /*
    At slave, columns may differ. So we should create
    min(columns@master, columns@slave) columns in the
    conversion table.
  */
  uint const cols_to_create= min<ulong>(target_table->s->fields, size());
  for (uint col= 0 ; col < cols_to_create; ++col)
  {
    Create_field *field_def=
      (Create_field*) alloc_root(thd->mem_root, sizeof(Create_field));
    if (field_list.push_back(field_def))
      DBUG_RETURN(NULL);

    uint decimals= 0;
    TYPELIB* interval= NULL;
    uint pack_length= 0;
    uint32 max_length=
      max_display_length_for_field(type(col), field_metadata(col));

    switch(type(col))
    {
      int precision;
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
      interval= static_cast<Field_enum*>(target_table->field[col])->typelib;
      pack_length= field_metadata(col) & 0x00ff;
      break;

    case MYSQL_TYPE_NEWDECIMAL:
      /*
        The display length of a DECIMAL type is not the same as the
        length that should be supplied to make_field, so we correct
        the length here.
       */
      precision= field_metadata(col) >> 8;
      decimals= field_metadata(col) & 0x00ff;
      max_length=
        my_decimal_precision_to_length(precision, decimals, FALSE);
      break;

    case MYSQL_TYPE_DECIMAL:
      precision= field_metadata(col);
      decimals= static_cast<Field_num*>(target_table->field[col])->dec;
      max_length= field_metadata(col);
      break;

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY:
      pack_length= field_metadata(col) & 0x00ff;
      break;

    default:
      break;
    }

    DBUG_PRINT("debug", ("sql_type: %d, target_field: '%s', max_length: %d, decimals: %d,"
                         " maybe_null: %d, unsigned_flag: %d, pack_length: %u",
                         binlog_type(col), target_table->field[col]->field_name,
                         max_length, decimals, TRUE, FALSE, pack_length));
    field_def->init_for_tmp_table(type(col),
                                  max_length,
                                  decimals,
                                  TRUE,         // maybe_null
                                  FALSE,        // unsigned_flag
                                  pack_length);
    field_def->charset= target_table->field[col]->charset();
    field_def->interval= interval;
  }

  TABLE *conv_table= create_virtual_tmp_table(thd, field_list);
  if (conv_table == NULL)
    rli->report(ERROR_LEVEL, ER_SLAVE_CANT_CREATE_CONVERSION,
                ER(ER_SLAVE_CANT_CREATE_CONVERSION),
                target_table->s->db.str,
                target_table->s->table_name.str);
  DBUG_RETURN(conv_table);
}

#endif /* MYSQL_CLIENT */

table_def::table_def(unsigned char *types, ulong size,
                     uchar *field_metadata, int metadata_size,
                     uchar *null_bitmap, uint16 flags)
  : m_size(size), m_type(0), m_field_metadata_size(metadata_size),
    m_field_metadata(0), m_null_bits(0), m_flags(flags),
    m_memory(NULL)
{
  m_memory= (uchar *)my_multi_malloc(MYF(MY_WME),
                                     &m_type, size,
                                     &m_field_metadata,
                                     size * sizeof(uint16),
                                     &m_null_bits, (size + 7) / 8,
                                     NULL);

  memset(m_field_metadata, 0, size * sizeof(uint16));

  if (m_type)
    memcpy(m_type, types, size);
  else
    m_size= 0;
  /*
    Extract the data from the table map into the field metadata array
    iff there is field metadata. The variable metadata_size will be
    0 if we are replicating from an older version server since no field
    metadata was written to the table map. This can also happen if 
    there were no fields in the master that needed extra metadata.
  */
  if (m_size && metadata_size)
  { 
    int index= 0;
    for (unsigned int i= 0; i < m_size; i++)
    {
      switch (binlog_type(i)) {
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_DOUBLE:
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_GEOMETRY:
      {
        /*
          These types store a single byte.
        */
        m_field_metadata[i]= field_metadata[index];
        index++;
        break;
      }
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_STRING:
      {
        uint16 x= field_metadata[index++] << 8U; // real_type
        x+= field_metadata[index++];            // pack or field length
        m_field_metadata[i]= x;
        break;
      }
      case MYSQL_TYPE_BIT:
      {
        uint16 x= field_metadata[index++];
        x = x + (field_metadata[index++] << 8U);
        m_field_metadata[i]= x;
        break;
      }
      case MYSQL_TYPE_VARCHAR:
      {
        /*
          These types store two bytes.
        */
        char *ptr= (char *)&field_metadata[index];
        m_field_metadata[i]= uint2korr(ptr);
        index= index + 2;
        break;
      }
      case MYSQL_TYPE_NEWDECIMAL:
      {
        uint16 x= field_metadata[index++] << 8U; // precision
        x+= field_metadata[index++];            // decimals
        m_field_metadata[i]= x;
        break;
      }
      case MYSQL_TYPE_TIME2:
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_TIMESTAMP2:
        m_field_metadata[i]= field_metadata[index++];
        break;
      default:
        m_field_metadata[i]= 0;
        break;
      }
    }
  }
  if (m_size && null_bitmap)
    memcpy(m_null_bits, null_bitmap, (m_size + 7) / 8);
}


table_def::~table_def()
{
  my_free(m_memory);
#ifndef DBUG_OFF
  m_type= 0;
  m_size= 0;
#endif
}

/**
   @param   even_buf    point to the buffer containing serialized event
   @param   event_len   length of the event accounting possible checksum alg

   @return  TRUE        if test fails
            FALSE       as success
*/
bool event_checksum_test(uchar *event_buf, ulong event_len, uint8 alg)
{
  bool res= FALSE;
  uint16 flags= 0; // to store in FD's buffer flags orig value

  if (alg != BINLOG_CHECKSUM_ALG_OFF && alg != BINLOG_CHECKSUM_ALG_UNDEF)
  {
    ha_checksum incoming;
    ha_checksum computed;

    if (event_buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT)
    {
#ifndef DBUG_OFF
      int8 fd_alg= event_buf[event_len - BINLOG_CHECKSUM_LEN -
                             BINLOG_CHECKSUM_ALG_DESC_LEN];
#endif
      /*
        FD event is checksummed and therefore verified w/o the binlog-in-use flag
      */
      flags= uint2korr(event_buf + FLAGS_OFFSET);
      if (flags & LOG_EVENT_BINLOG_IN_USE_F)
        event_buf[FLAGS_OFFSET] &= ~LOG_EVENT_BINLOG_IN_USE_F;
      /*
         The only algorithm currently is CRC32. Zero indicates
         the binlog file is checksum-free *except* the FD-event.
      */
      DBUG_ASSERT(fd_alg == BINLOG_CHECKSUM_ALG_CRC32 || fd_alg == 0);
      DBUG_ASSERT(alg == BINLOG_CHECKSUM_ALG_CRC32);
      /*
        Complile time guard to watch over  the max number of alg
      */
      compile_time_assert(BINLOG_CHECKSUM_ALG_ENUM_END <= 0x80);
    }
    incoming= uint4korr(event_buf + event_len - BINLOG_CHECKSUM_LEN);
    computed= my_checksum(0L, NULL, 0);
    /* checksum the event content but the checksum part itself */
    computed= my_checksum(computed, (const uchar*) event_buf,
                          event_len - BINLOG_CHECKSUM_LEN);
    if (flags != 0)
    {
      /* restoring the orig value of flags of FD */
      DBUG_ASSERT(event_buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT);
      event_buf[FLAGS_OFFSET]= flags;
    }
    res= !(computed == incoming);
  }
  return DBUG_EVALUATE_IF("simulate_checksum_test_failure", TRUE, res);
}

#ifndef MYSQL_CLIENT

#define HASH_ROWS_POS_SEARCH_INVALID -1

/**
  Utility methods for handling row based operations.
 */

static uchar*
hash_slave_rows_get_key(const uchar *record,
                        size_t *length,
                        my_bool not_used __attribute__((unused)))
{
  DBUG_ENTER("get_key");

  HASH_ROW_ENTRY *entry=(HASH_ROW_ENTRY *) record;
  HASH_ROW_PREAMBLE *preamble= entry->preamble;
  *length= preamble->length;

  DBUG_RETURN((uchar*) &preamble->hash_value);
}

static void
hash_slave_rows_free_entry(HASH_ROW_ENTRY *entry)
{
  DBUG_ENTER("free_entry");
  if (entry)
  {
    if (entry->preamble)
      my_free(entry->preamble);
    if (entry->positions)
      my_free(entry->positions);
    my_free(entry);
  }
  DBUG_VOID_RETURN;
}

bool Hash_slave_rows::is_empty(void)
{
  return (m_hash.records == 0);
}

/**
   Hashing commodity structures and functions.
 */

bool Hash_slave_rows::init(void)
{
  if (my_hash_init(&m_hash,
                   &my_charset_bin,                /* the charater set information */
                   16 /* TODO */,                  /* growth size */
                   0,                              /* key offset */
                   0,                              /* key length */
                   hash_slave_rows_get_key,                        /* get function pointer */
                   (my_hash_free_key) hash_slave_rows_free_entry,  /* freefunction pointer */
                   MYF(0)))                        /* flags */
    return true;
  return false;
}

bool Hash_slave_rows::deinit(void)
{
  if (my_hash_inited(&m_hash))
    my_hash_free(&m_hash);

  return 0;
}

int Hash_slave_rows::size()
{
  return m_hash.records;
}

HASH_ROW_ENTRY* Hash_slave_rows::make_entry(const uchar* bi_start, const uchar* bi_ends,
                                            const uchar* ai_start, const uchar* ai_ends)
{
  DBUG_ENTER("Hash_slave_rows::make_entry");

  HASH_ROW_ENTRY *entry= (HASH_ROW_ENTRY*) my_malloc(sizeof(HASH_ROW_ENTRY), MYF(0));
  HASH_ROW_PREAMBLE *preamble= (HASH_ROW_PREAMBLE *) my_malloc(sizeof(HASH_ROW_PREAMBLE), MYF(0));
  HASH_ROW_POS *pos= (HASH_ROW_POS *) my_malloc(sizeof(HASH_ROW_POS), MYF(0));

  if (!entry || !preamble || !pos)
    goto err;

  /**
     Filling in the preamble.
   */
  preamble->hash_value= 0;
  preamble->length= sizeof(my_hash_value_type);
  preamble->search_state= HASH_ROWS_POS_SEARCH_INVALID;
  preamble->is_search_state_inited= false;

  /**
     Filling in the positions.
   */
  pos->bi_start= (const uchar *) bi_start;
  pos->bi_ends= (const uchar *) bi_ends;
  pos->ai_start= (const uchar *) ai_start;
  pos->ai_ends= (const uchar *) ai_ends;

  /**
    Filling in the entry
   */
  entry->preamble= preamble;
  entry->positions= pos;

  DBUG_RETURN(entry);

err:
  if (entry)
    my_free(entry);
  if (preamble)
    my_free(entry);
  if (pos)
    my_free(pos);
  DBUG_RETURN(NULL);
}

bool
Hash_slave_rows::put(TABLE *table,
                     MY_BITMAP *cols,
                     HASH_ROW_ENTRY* entry)
{

  DBUG_ENTER("Hash_slave_rows::put");

  HASH_ROW_PREAMBLE* preamble= entry->preamble;

  /**
     Skip blobs and BIT fields from key calculation.
     Handle X bits.
     Handle nulled fields.
     Handled fields not signaled.
  */
  preamble->hash_value= make_hash_key(table, cols);

  my_hash_insert(&m_hash, (uchar *) entry);
  DBUG_PRINT("debug", ("Added record to hash with key=%u", preamble->hash_value));
  DBUG_RETURN(false);
}

HASH_ROW_ENTRY*
Hash_slave_rows::get(TABLE *table, MY_BITMAP *cols)
{
  DBUG_ENTER("Hash_slave_rows::get");
  HASH_SEARCH_STATE state;
  my_hash_value_type key;
  HASH_ROW_ENTRY *entry= NULL;

  key= make_hash_key(table, cols);

  DBUG_PRINT("debug", ("Looking for record with key=%u in the hash.", key));

  entry= (HASH_ROW_ENTRY*) my_hash_first(&m_hash,
                                         (const uchar*) &key,
                                         sizeof(my_hash_value_type),
                                         &state);
  if (entry)
  {
    DBUG_PRINT("debug", ("Found record with key=%u in the hash.", key));

    /**
       Save the search state in case we need to go through entries for
       the given key.
    */
    entry->preamble->search_state= state;
    entry->preamble->is_search_state_inited= true;
  }

  DBUG_RETURN(entry);
}

bool Hash_slave_rows::next(HASH_ROW_ENTRY** entry)
{
  DBUG_ENTER("Hash_slave_rows::next");
  DBUG_ASSERT(*entry);

  if (*entry == NULL)
    DBUG_RETURN(true);

  HASH_ROW_PREAMBLE *preamble= (*entry)->preamble;

  if (!preamble->is_search_state_inited)
    DBUG_RETURN(true);

  my_hash_value_type key= preamble->hash_value;
  HASH_SEARCH_STATE state= preamble->search_state;

  /*
    Invalidate search for current preamble, because it is going to be
    used in the search below (and search state is used in a
    one-time-only basis).
   */
  preamble->search_state= HASH_ROWS_POS_SEARCH_INVALID;
  preamble->is_search_state_inited= false;

  DBUG_PRINT("debug", ("Looking for record with key=%u in the hash (next).", key));

  /**
     Do the actual search in the hash table.
   */
  *entry= (HASH_ROW_ENTRY*) my_hash_next(&m_hash,
                                         (const uchar*) &key,
                                         sizeof(my_hash_value_type),
                                         &state);
  if (*entry)
  {
    DBUG_PRINT("debug", ("Found record with key=%u in the hash (next).", key));
    preamble= (*entry)->preamble;

    /**
       Save the search state for next iteration (if any).
     */
    preamble->search_state= state;
    preamble->is_search_state_inited= true;
  }

  DBUG_RETURN(false);
}

bool
Hash_slave_rows::del(HASH_ROW_ENTRY *entry)
{
  DBUG_ENTER("Hash_slave_rows::del");
  DBUG_ASSERT(entry);

  if (my_hash_delete(&m_hash, (uchar *) entry))
    DBUG_RETURN(true);
  DBUG_RETURN(false);
}

my_hash_value_type
Hash_slave_rows::make_hash_key(TABLE *table, MY_BITMAP *cols)
{
  DBUG_ENTER("Hash_slave_rows::make_hash_key");
  ha_checksum crc= 0L;

  uchar *record= table->record[0];
  uchar saved_x= 0, saved_filler= 0;

  if (table->s->null_bytes > 0)
  {
    /*
      If we have an X bit then we need to take care of it.
    */
    if (!(table->s->db_options_in_use & HA_OPTION_PACK_RECORD))
    {
      saved_x= record[0];
      record[0]|= 1U;
    }

    /*
      If (last_null_bit_pos == 0 && null_bytes > 1), then:
      X bit (if any) + N nullable fields + M Field_bit fields = 8 bits
      Ie, the entire byte is used.
    */
    if (table->s->last_null_bit_pos > 0)
    {
      saved_filler= record[table->s->null_bytes - 1];
      record[table->s->null_bytes - 1]|=
        256U - (1U << table->s->last_null_bit_pos);
    }
  }

  /*
    We can only checksum the bytes if all fields have been signaled
    in the before image. Otherwise, unpack_row will not have set the
    null_flags correctly (because it only unpacks those fields and
    their flags that were actually in the before image).

    @c record_compare, as it also skips null_flags if the read_set
    was not marked completely.
   */
  if (bitmap_is_set_all(cols))
    crc= my_checksum(crc, table->null_flags, table->s->null_bytes);

  for (Field **ptr=table->field ;
       *ptr && ((*ptr)->field_index < cols->n_bits);
       ptr++)
  {
    Field *f= (*ptr);

    /* field is set in the read_set and is not a blob or a BIT */
    if (bitmap_is_set(cols, f->field_index) &&
        (f->type() != MYSQL_TYPE_BLOB) && (f->type() != MYSQL_TYPE_BIT))
      crc= my_checksum(crc, f->ptr, f->data_length());
  }

  /*
    Restore the saved bytes.

    TODO[record format ndb]: Remove this code once NDB returns the
    correct record format.
  */
  if (table->s->null_bytes > 0)
  {
    if (!(table->s->db_options_in_use & HA_OPTION_PACK_RECORD))
      record[0]= saved_x;

    if (table->s->last_null_bit_pos)
      record[table->s->null_bytes - 1]= saved_filler;
  }

  DBUG_PRINT("debug", ("Created key=%u", crc));
  DBUG_RETURN(crc);
}


#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)

Deferred_log_events::Deferred_log_events(Relay_log_info *rli) : last_added(NULL)
{
  my_init_dynamic_array(&array, sizeof(Log_event *), 32, 16);
}

Deferred_log_events::~Deferred_log_events() 
{
  delete_dynamic(&array);
}

int Deferred_log_events::add(Log_event *ev)
{
  last_added= ev;
  insert_dynamic(&array, (uchar*) &ev);
  ev->worker= NULL; // to mark event busy avoiding deletion
  return 0;
}

bool Deferred_log_events::is_empty()
{  
  return array.elements == 0;
}

bool Deferred_log_events::execute(Relay_log_info *rli)
{
  bool res= false;

  DBUG_ASSERT(rli->deferred_events_collecting);

  rli->deferred_events_collecting= false;
  for (uint i=  0; !res && i < array.elements; i++)
  {
    Log_event *ev= (* (Log_event **)
                    dynamic_array_ptr(&array, i));
    res= ev->apply_event(rli);
  }
  rli->deferred_events_collecting= true;
  return res;
}

void Deferred_log_events::rewind()
{
  /*
    Reset preceeding Query log event events which execution was
    deferred because of slave side filtering.
  */
  if (!is_empty())
  {
    for (uint i=  0; i < array.elements; i++)
    {
      Log_event *ev= *(Log_event **) dynamic_array_ptr(&array, i);
      delete ev;
    }
    if (array.elements > array.max_element)
      freeze_size(&array);
    reset_dynamic(&array);
  }
}

#endif
