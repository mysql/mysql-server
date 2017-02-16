/* Copyright (c) 2011, Monty Program Ab
   Copyright (c) 2011, Oleksandr Byelkin

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

#include "mysys_priv.h"
#include <m_string.h>
#include <ma_dyncol.h>

/*
  Flag byte bits

  2 bits which determinate size of offset in the header -1
*/
/* mask to get above bits */
#define DYNCOL_FLG_OFFSET  3
/* All known flags mask */
#define DYNCOL_FLG_KNOWN  3

/* dynamic column size reserve */
#define DYNCOL_SYZERESERVE 80

/* length of fixed string header 1 byte - flags, 2 bytes - columns counter */
#define FIXED_HEADER_SIZE 3

#define COLUMN_NUMBER_SIZE 2

#define MAX_OFFSET_LENGTH  5

static enum enum_dyncol_func_result
dynamic_column_time_store(DYNAMIC_COLUMN *str,
                          MYSQL_TIME *value);
static enum enum_dyncol_func_result
dynamic_column_date_store(DYNAMIC_COLUMN *str,
                          MYSQL_TIME *value);
static enum enum_dyncol_func_result
dynamic_column_time_read_internal(DYNAMIC_COLUMN_VALUE *store_it_here,
                                  uchar *data, size_t length);
static enum enum_dyncol_func_result
dynamic_column_date_read_internal(DYNAMIC_COLUMN_VALUE *store_it_here,
                                  uchar *data, size_t length);

/**
  Initialize dynamic column string with (make it empty but correct format)

  @param str             The string to initialize
  @param size            Amount of preallocated memory for the string.

  @retval FALSE OK
  @retval TRUE  error
*/

static my_bool dynamic_column_init_str(DYNAMIC_COLUMN *str, size_t size)
{
  DBUG_ASSERT(size != 0);

  /*
    Make string with no fields (empty header)
    - First \0 is flags
    - other 2 \0 is number of fields
  */
  if (init_dynamic_string(str, NULL,
                          size + FIXED_HEADER_SIZE, DYNCOL_SYZERESERVE))
    return TRUE;
  bzero(str->str, FIXED_HEADER_SIZE);
  str->length= FIXED_HEADER_SIZE;
  return FALSE;
}


/**
  Calculate how many bytes needed to store val as variable length integer
  where first bit indicate continuation of the sequence.

  @param val             The value for which we are calculating length

  @return number of bytes
*/

static size_t dynamic_column_var_uint_bytes(ulonglong val)
{
  size_t len= 0;
  do
  {
    len++;
    val>>= 7;
  } while (val);
  return len;
}


/**
   Stores variable length unsigned integer value to a string

  @param str             The string where to append the value
  @param val             The value to put in the string

  @return ER_DYNCOL_* return code

  @notes
  This is used to store a number together with other data in the same
  object.  (Like decimals, length of string etc)
  (As we don't know the length of this object, we can't store 0 in 0 bytes)
*/

static enum enum_dyncol_func_result
dynamic_column_var_uint_store(DYNAMIC_COLUMN *str, ulonglong val)
{
  if (dynstr_realloc(str, 10))                  /* max what we can use */
    return ER_DYNCOL_RESOURCE;

  do
  {
    ulonglong rest= val >> 7;
    str->str[str->length++]= ((val & 0x7f) | (rest ? 0x80 : 0x00));
    val= rest;
  } while (val);
  return ER_DYNCOL_OK;
}


/**
  Reads variable length unsigned integer value from a string

  @param data            The string from which the int should be read
  @param data_length	 Max length of data
  @param len             Where to put length of the string read in bytes

  @return value of the unsigned integer read from the string

  In case of error, *len is set to 0
*/

static ulonglong
dynamic_column_var_uint_get(uchar *data, size_t data_length,
                            size_t *len)
{
  ulonglong val= 0;
  uint length;
  uchar *end= data + data_length;

  for (length=0; data < end ; data++)
  {
    val+= (((ulonglong)((*data) & 0x7f)) << (length * 7));
    length++;
    if (!((*data) & 0x80))
    {
      /* End of data */
      *len= length;
      return val;
    }
  }
  /* Something was wrong with data */
  *len= 0;                                      /* Mark error */
  return 0;
}


/**
  Calculate how many bytes needed to store val as unsigned.

  @param val             The value for which we are calculating length

  @return number of bytes (0-8)
*/

static size_t dynamic_column_uint_bytes(ulonglong val)
{
  size_t len;

  for (len= 0; val ; val>>= 8, len++)
    ;
  return len;
}


/**
  Append the string with given unsigned int value.

  @param str             The string where to put the value
  @param val             The value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_uint_store(DYNAMIC_COLUMN *str, ulonglong val)
{
  if (dynstr_realloc(str, 8)) /* max what we can use */
    return ER_DYNCOL_RESOURCE;

  for (; val; val>>= 8)
    str->str[str->length++]= (char) (val & 0xff);
  return ER_DYNCOL_OK;
}


/**
  Read unsigned int value of given length from the string

  @param store_it_here   The structure to store the value
  @param data            The string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_uint_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                         uchar *data, size_t length)
{
  ulonglong value= 0;
  size_t i;

  for (i= 0; i < length; i++)
    value+= ((ulonglong)data[i]) << (i*8);

  store_it_here->x.ulong_value= value;
  return ER_DYNCOL_OK;
}

/**
  Calculate how many bytes needed to store val as signed in following encoding:
    0 -> 0
   -1 -> 1
    1 -> 2
   -2 -> 3
    2 -> 4
   ...

  @param val             The value for which we are calculating length

  @return number of bytes
*/

static size_t dynamic_column_sint_bytes(longlong val)
{
  return dynamic_column_uint_bytes((val << 1) ^
                                   (val < 0 ? ULL(0xffffffffffffffff) : 0));
}


/**
  Append the string with given signed int value.

  @param str             the string where to put the value
  @param val             the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_sint_store(DYNAMIC_COLUMN *str, longlong val)
{
  return dynamic_column_uint_store(str,
                                 (val << 1) ^
                                 (val < 0 ? ULL(0xffffffffffffffff) : 0));
}


/**
  Read signed int value of given length from the string

  @param store_it_here   The structure to store the value
  @param data            The string which should be read
  @param length          The length (in bytes) of the value in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_sint_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                         uchar *data, size_t length)
{
  ulonglong val;
  dynamic_column_uint_read(store_it_here, data, length);
  val= store_it_here->x.ulong_value;
  if (val & 1)
    val= (val >> 1) ^ ULL(0xffffffffffffffff);
  else
    val>>= 1;
  store_it_here->x.long_value= (longlong) val;
  return ER_DYNCOL_OK;
}


/**
  Calculate how many bytes needed to store the value.

  @param value          The value for which we are calculating length

  @return
  Error:  (size_t) ~0
  ok      number of bytes
*/

static size_t
dynamic_column_value_len(DYNAMIC_COLUMN_VALUE *value)
{
  switch (value->type) {
  case DYN_COL_NULL:
    return 0;
  case DYN_COL_INT:
    return dynamic_column_sint_bytes(value->x.long_value);
  case DYN_COL_UINT:
    return dynamic_column_uint_bytes(value->x.ulong_value);
  case DYN_COL_DOUBLE:
    return 8;
  case DYN_COL_STRING:
    return (dynamic_column_var_uint_bytes(value->x.string.charset->number) +
            value->x.string.value.length);
  case DYN_COL_DECIMAL:
  {
    int precision= value->x.decimal.value.intg + value->x.decimal.value.frac;
    int scale= value->x.decimal.value.frac;

    if (precision == 0 || decimal_is_zero(&value->x.decimal.value))
    {
      /* This is here to simplify dynamic_column_decimal_store() */
      value->x.decimal.value.intg= value->x.decimal.value.frac= 0;
      return 0;
    }
    /*
      Check if legal decimal;  This is needed to not get an assert in
      decimal_bin_size(). However this should be impossible as all
      decimals entered here should be valid and we have the special check
      above to handle the unlikely but possible case that decimal.value.intg
      and decimal.frac is 0.
    */
    if (scale < 0 || precision <= 0)
    {
      DBUG_ASSERT(0);                           /* Impossible */
      return (size_t) ~0;
    }
    return (dynamic_column_var_uint_bytes(value->x.decimal.value.intg) +
            dynamic_column_var_uint_bytes(value->x.decimal.value.frac) +
            decimal_bin_size(precision, scale));
  }
  case DYN_COL_DATETIME:
    /* date+time in bits: 14 + 4 + 5 + 10 + 6 + 6 + 20 + 1 66bits ~= 9 bytes */
    return 9;
  case DYN_COL_DATE:
    /* date in dits: 14 + 4 + 5 = 23bits ~= 3bytes*/
    return 3;
  case DYN_COL_TIME:
    /* time in bits: 10 + 6 + 6 + 20 + 1 = 43bits ~= 6bytes*/
    return 6;
  }
  DBUG_ASSERT(0);
  return 0;
}


/**
  Append double value to a string

  @param str             the string where to put the value
  @param val             the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_double_store(DYNAMIC_COLUMN *str, double val)
{
   if (dynstr_realloc(str, 8))
     return ER_DYNCOL_RESOURCE;
   float8store(str->str + str->length, val);
   str->length+= 8;
   return ER_DYNCOL_OK;
}


/**
  Read double value of given length from the string

  @param store_it_here   The structure to store the value
  @param data            The string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_double_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                               uchar *data, size_t length)
{
  if (length != 8)
    return ER_DYNCOL_FORMAT;
  float8get(store_it_here->x.double_value, data);
  return ER_DYNCOL_OK;
}


/**
  Append the string with given string value.

  @param str             the string where to put the value
  @param val             the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_string_store(DYNAMIC_COLUMN *str, LEX_STRING *string,
                            CHARSET_INFO *charset)
{
  enum enum_dyncol_func_result rc;
  if ((rc= dynamic_column_var_uint_store(str, charset->number)))
    return rc;
  if (dynstr_append_mem(str, string->str, string->length))
    return ER_DYNCOL_RESOURCE;
  return ER_DYNCOL_OK;
}


/**
  Read string value of given length from the packed string

  @param store_it_here   The structure to store the value
  @param data            The packed string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_string_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                           uchar *data, size_t length)
{
  size_t len;
  uint charset_nr= (uint)dynamic_column_var_uint_get(data, length, &len);
  if (len == 0)                                /* Wrong packed number */
    return ER_DYNCOL_FORMAT;
  store_it_here->x.string.charset= get_charset(charset_nr, MYF(MY_WME));
  if (store_it_here->x.string.charset == NULL)
    return ER_DYNCOL_UNKNOWN_CHARSET;
  data+= len;
  store_it_here->x.string.value.length= (length-= len);
  store_it_here->x.string.value.str= (char*) data;
  return ER_DYNCOL_OK;
}


/**
  Append the string with given decimal value.

  @param str             the string where to put the value
  @param val             the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_decimal_store(DYNAMIC_COLUMN *str,
                             decimal_t *value)
{
  uint bin_size;
  int precision= value->intg + value->frac;
  
  /* Store decimal zero as empty string */
  if (precision == 0)
    return ER_DYNCOL_OK;

  bin_size= decimal_bin_size(precision, value->frac);
  if (dynstr_realloc(str, bin_size + 20))
    return ER_DYNCOL_RESOURCE;

  /* The following can't fail as memory is already allocated */
  (void) dynamic_column_var_uint_store(str, value->intg);
  (void) dynamic_column_var_uint_store(str, value->frac);

  decimal2bin(value, (uchar *) str->str + str->length,
              precision, value->frac);
  str->length+= bin_size;
  return ER_DYNCOL_OK;
}


/**
  Prepare the value to be used as decimal.

  @param value           The value structure which sould be setup.
*/

void dynamic_column_prepare_decimal(DYNAMIC_COLUMN_VALUE *value)
{
  value->x.decimal.value.buf= value->x.decimal.buffer;
  value->x.decimal.value.len= DECIMAL_BUFF_LENGTH;
  /* just to be safe */
  value->type= DYN_COL_DECIMAL;
  decimal_make_zero(&value->x.decimal.value);
}


/**
  Read decimal value of given length from the string

  @param store_it_here   The structure to store the value
  @param data            The string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_decimal_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                            uchar *data, size_t length)
{
  size_t intg_len, frac_len;
  int intg, frac, precision, scale;

  dynamic_column_prepare_decimal(store_it_here);
  /* Decimals 0.0 is stored as a zero length string */
  if (length == 0)
    return ER_DYNCOL_OK;                        /* value contains zero */

  intg= (int)dynamic_column_var_uint_get(data, length, &intg_len);
  data+= intg_len;
  frac= (int)dynamic_column_var_uint_get(data, length - intg_len, &frac_len);
  data+= frac_len;

  /* Check the size of data is correct */
  precision= intg + frac;
  scale=     frac;
  if (scale < 0 || precision <= 0 || scale > precision ||
      (length - intg_len - frac_len) >
      (size_t) (DECIMAL_BUFF_LENGTH*sizeof(decimal_digit_t)) ||
      decimal_bin_size(intg + frac, frac) !=
      (int) (length - intg_len - frac_len))
    return ER_DYNCOL_FORMAT;

  if (bin2decimal(data, &store_it_here->x.decimal.value, precision, scale) !=
      E_DEC_OK)
    return ER_DYNCOL_FORMAT;
  return ER_DYNCOL_OK;
}


/**
  Append the string with given datetime value.

  @param str             the string where to put the value
  @param value           the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_date_time_store(DYNAMIC_COLUMN *str, MYSQL_TIME *value)
{
  enum enum_dyncol_func_result rc;
  /*
    0<----year----><mn><day>00000!<-hours--><min-><sec-><---microseconds--->
     12345678901234123412345     1123456789012345612345612345678901234567890
    <123456><123456><123456><123456><123456><123456><123456><123456><123456>
  */
  if ((rc= dynamic_column_date_store(str, value)) ||
      (rc= dynamic_column_time_store(str, value)))
    return rc;
  return ER_DYNCOL_OK;
}


/**
  Read datetime value of given length from the packed string

  @param store_it_here   The structure to store the value
  @param data            The packed string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_date_time_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                              uchar *data, size_t length)
{
  enum enum_dyncol_func_result rc= ER_DYNCOL_FORMAT;
  /*
    0<----year----><mn><day>00000!<-hours--><min-><sec-><---microseconds--->
     12345678901234123412345     1123456789012345612345612345678901234567890
    <123456><123456><123456><123456><123456><123456><123456><123456><123456>
  */
  if (length != 9)
    goto err;
  store_it_here->x.time_value.time_type= MYSQL_TIMESTAMP_DATETIME;
  if ((rc= dynamic_column_date_read_internal(store_it_here, data, 3)) ||
      (rc= dynamic_column_time_read_internal(store_it_here, data + 3, 6)))
    goto err;
  return ER_DYNCOL_OK;

err:
  store_it_here->x.time_value.time_type= MYSQL_TIMESTAMP_ERROR;
  return rc;
}


/**
  Append the string with given time value.

  @param str             the string where to put the value
  @param value           the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_time_store(DYNAMIC_COLUMN *str, MYSQL_TIME *value)
{
  uchar *buf;
  if (dynstr_realloc(str, 6))
    return ER_DYNCOL_RESOURCE;

  buf= ((uchar *)str->str) + str->length;

  if (value->time_type == MYSQL_TIMESTAMP_NONE ||
      value->time_type == MYSQL_TIMESTAMP_ERROR ||
      value->time_type == MYSQL_TIMESTAMP_DATE)
  {
    value->neg= 0;
    value->second_part= 0;
    value->hour= 0;
    value->minute= 0;
    value->second= 0;
  }
  DBUG_ASSERT(value->hour <= 838);
  DBUG_ASSERT(value->minute <= 59);
  DBUG_ASSERT(value->second <= 59);
  DBUG_ASSERT(value->second_part <= 999999);
  /*
    00000!<-hours--><min-><sec-><---microseconds--->
         1123456789012345612345612345678901234567890
    <123456><123456><123456><123456><123456><123456>
  */
  buf[0]= (value->second_part & 0xff);
  buf[1]= ((value->second_part & 0xff00) >> 8);
  buf[2]= (uchar)(((value->second & 0xf) << 4) |
           ((value->second_part & 0xf0000) >> 16));
  buf[3]= ((value->minute << 2) | ((value->second & 0x30) >> 4));
  buf[4]= (value->hour & 0xff);
  buf[5]= ((value->neg ? 0x4 : 0) | (value->hour >> 8));
  str->length+= 6;
  return ER_DYNCOL_OK;
}


/**
  Read time value of given length from the packed string

  @param store_it_here   The structure to store the value
  @param data            The packed string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_time_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                         uchar *data, size_t length)
{
  store_it_here->x.time_value.year= store_it_here->x.time_value.month=
    store_it_here->x.time_value.day= 0;
  store_it_here->x.time_value.time_type= MYSQL_TIMESTAMP_TIME;
  return dynamic_column_time_read_internal(store_it_here, data, length);
}

/**
  Internal function for reading time part from the string.

  @param store_it_here   The structure to store the value
  @param data            The packed string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_time_read_internal(DYNAMIC_COLUMN_VALUE *store_it_here,
                                  uchar *data, size_t length)
{
  if (length != 6)
    goto err;
  /*
    00000!<-hours--><min-><sec-><---microseconds--->
         1123456789012345612345612345678901234567890
    <123456><123456><123456><123456><123456><123456>
  */
  store_it_here->x.time_value.second_part= (data[0] |
                                          (data[1] << 8) |
                                          ((data[2] & 0xf) << 16));
  store_it_here->x.time_value.second= ((data[2] >> 4) |
                                     ((data[3] & 0x3) << 4));
  store_it_here->x.time_value.minute= (data[3] >> 2);
  store_it_here->x.time_value.hour= (((((uint)data[5]) & 0x3 ) << 8) | data[4]);
  store_it_here->x.time_value.neg= ((data[5] & 0x4) ? 1 : 0);
  if (store_it_here->x.time_value.second > 59 ||
      store_it_here->x.time_value.minute > 59 ||
      store_it_here->x.time_value.hour > 838 ||
      store_it_here->x.time_value.second_part > 999999)
    goto err;
  return ER_DYNCOL_OK;

err:
  store_it_here->x.time_value.time_type= MYSQL_TIMESTAMP_ERROR;
  return ER_DYNCOL_FORMAT;
}


/**
  Append the string with given date value.

  @param str             the string where to put the value
  @param value           the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_date_store(DYNAMIC_COLUMN *str, MYSQL_TIME *value)
{
  uchar *buf;
  if (dynstr_realloc(str, 3))
    return ER_DYNCOL_RESOURCE;

  buf= ((uchar *)str->str) + str->length;
  if (value->time_type == MYSQL_TIMESTAMP_NONE ||
      value->time_type == MYSQL_TIMESTAMP_ERROR ||
      value->time_type == MYSQL_TIMESTAMP_TIME)
    value->year= value->month= value->day = 0;
  DBUG_ASSERT(value->year <= 9999);
  DBUG_ASSERT(value->month <= 12);
  DBUG_ASSERT(value->day <= 31);
  /*
    0<----year----><mn><day>
    012345678901234123412345
    <123456><123456><123456>
  */
  buf[0]= (value->day |
           ((value->month & 0x7) << 5));
  buf[1]= ((value->month >> 3) | ((value->year & 0x7F) << 1));
  buf[2]= (value->year >> 7);
  str->length+= 3;
  return ER_DYNCOL_OK;
}



/**
  Read date value of given length from the packed string

  @param store_it_here   The structure to store the value
  @param data            The packed string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_date_read(DYNAMIC_COLUMN_VALUE *store_it_here,
                         uchar *data, size_t length)
{
  store_it_here->x.time_value.neg= 0;
  store_it_here->x.time_value.second_part= 0;
  store_it_here->x.time_value.hour= 0;
  store_it_here->x.time_value.minute= 0;
  store_it_here->x.time_value.second= 0;
  store_it_here->x.time_value.time_type= MYSQL_TIMESTAMP_DATE;
  return dynamic_column_date_read_internal(store_it_here, data, length);
}

/**
  Internal function for reading date part from the string.

  @param store_it_here   The structure to store the value
  @param data            The packed string which should be read
  @param length          The length (in bytes) of the value in nthe string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_date_read_internal(DYNAMIC_COLUMN_VALUE *store_it_here,
                                  uchar *data,
                                  size_t length)
{
  if (length != 3)
    goto err;
  /*
    0<----year----><mn><day>
     12345678901234123412345
    <123456><123456><123456>
  */
  store_it_here->x.time_value.day= (data[0] & 0x1f);
  store_it_here->x.time_value.month= (((data[1] & 0x1) << 3) |
                                    (data[0] >> 5));
  store_it_here->x.time_value.year= ((((uint)data[2]) << 7) |
                                    (data[1] >> 1));
  if (store_it_here->x.time_value.day > 31 ||
      store_it_here->x.time_value.month > 12 ||
      store_it_here->x.time_value.year > 9999)
    goto err;
  return ER_DYNCOL_OK;

err:
  store_it_here->x.time_value.time_type= MYSQL_TIMESTAMP_ERROR;
  return ER_DYNCOL_FORMAT;
}


/**
  Append the string with given value.

  @param str             the string where to put the value
  @param value           the value to put in the string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
data_store(DYNAMIC_COLUMN *str, DYNAMIC_COLUMN_VALUE *value)
{
  switch (value->type) {
  case DYN_COL_INT:
    return dynamic_column_sint_store(str, value->x.long_value);
  case DYN_COL_UINT:
    return dynamic_column_uint_store(str, value->x.ulong_value);
  case DYN_COL_DOUBLE:
    return dynamic_column_double_store(str, value->x.double_value);
  case DYN_COL_STRING:
    return dynamic_column_string_store(str, &value->x.string.value,
                                     value->x.string.charset);
  case DYN_COL_DECIMAL:
    return dynamic_column_decimal_store(str, &value->x.decimal.value);
  case DYN_COL_DATETIME:
    /* date+time in bits: 14 + 4 + 5 + 5 + 6 + 6 40bits = 5 bytes */
    return dynamic_column_date_time_store(str, &value->x.time_value);
  case DYN_COL_DATE:
    /* date in dits: 14 + 4 + 5 = 23bits ~= 3bytes*/
    return dynamic_column_date_store(str, &value->x.time_value);
  case DYN_COL_TIME:
    /* time in bits: 5 + 6 + 6 = 17bits ~= 3bytes*/
    return dynamic_column_time_store(str, &value->x.time_value);
  case DYN_COL_NULL:
    break;                                      /* Impossible */
  }
  DBUG_ASSERT(0);
  return ER_DYNCOL_OK;                          /* Impossible */
}


/**
  Calculate length of offset field for given data length

  @param data_length     Length of the data segment

  @return number of bytes
*/

static size_t dynamic_column_offset_bytes(size_t data_length)
{
  if (data_length < 0x1f)                /* all 1 value is reserved */
    return 1;
  if (data_length < 0x1fff)              /* all 1 value is reserved */
    return 2;
  if (data_length < 0x1fffff)            /* all 1 value is reserved */
    return 3;
  if (data_length < 0x1fffffff)          /* all 1 value is reserved */
    return 4;
  return MAX_OFFSET_LENGTH;              /* For future */
}

/**
  Store offset and type information in the given place

  @param place           Beginning of the index entry
  @param offset_size     Size of offset field in bytes
  @param type            Type to be written
  @param offset          Offset to be written
*/

static void type_and_offset_store(uchar *place, size_t offset_size,
                                  DYNAMIC_COLUMN_TYPE type,
                                  size_t offset)
{
  ulong val = (((ulong) offset) << 3) | (type - 1);
  DBUG_ASSERT(type != DYN_COL_NULL);
  DBUG_ASSERT(((type - 1) & (~7)) == 0); /* fit in 3 bits */

  /* Index entry starts with column number; Jump over it */
  place+= COLUMN_NUMBER_SIZE;                   

  switch (offset_size) {
  case 1:
    DBUG_ASSERT(offset < 0x1f);          /* all 1 value is reserved */
    place[0]= (uchar)val;
    break;
  case 2:
    DBUG_ASSERT(offset < 0x1fff);        /* all 1 value is reserved */
    int2store(place, val);
    break;
  case 3:
    DBUG_ASSERT(offset < 0x1fffff);      /* all 1 value is reserved */
    int3store(place, val);
    break;
  case 4:
    DBUG_ASSERT(offset < 0x1fffffff);    /* all 1 value is reserved */
    int4store(place, val);
    break;
  default:
    DBUG_ASSERT(0);                             /* impossible */
  }
}


/**
  Read offset and type information from index entry

  @param type            Where to put type info
  @param offset          Where to put offset info
  @param place           Beginning of the index entry
  @param offset_size     Size of offset field in bytes
*/

static void type_and_offset_read(DYNAMIC_COLUMN_TYPE *type,
                                 size_t *offset,
                                 uchar *place, size_t offset_size)
{
  ulong UNINIT_VAR(val);

  place+= COLUMN_NUMBER_SIZE;                 /* skip column number */
  switch (offset_size) {
  case 1:
    val= (ulong)place[0];
    break;
  case 2:
    val= uint2korr(place);
    break;
  case 3:
    val= uint3korr(place);
    break;
  case 4:
    val= uint4korr(place);
    break;
  default:
    DBUG_ASSERT(0);                             /* impossible */
  }
  *type= (val & 0x7) + 1;
  *offset= val >> 3;
}


/**
  Comparator function for references on column numbers for qsort
*/

static int column_sort(const void *a, const void *b)
{
  return **((uint **)a) - **((uint **)b);
}


/**
  Write information to the fixed header

  @param str             String where to write the header
  @param offset_size     Size of offset field in bytes
  @param column_count    Number of columns
*/

static void set_fixed_header(DYNAMIC_COLUMN *str,
                             uint offset_size,
                             uint column_count)
{
  DBUG_ASSERT(column_count <= 0xffff);
  DBUG_ASSERT(offset_size <= 4);
  str->str[0]= ((str->str[0] & ~DYNCOL_FLG_OFFSET) |
                (offset_size - 1));             /* size of offset */
  int2store(str->str + 1, column_count);        /* columns number */
  DBUG_ASSERT((str->str[0] & (~DYNCOL_FLG_KNOWN)) == 0);
}

/*
  Calculate entry size (E) and header size (H) by offset size (O) and column
  count (C).
*/

#define calc_param(E,H,O,C) do { \
  (*(E))= (O) + COLUMN_NUMBER_SIZE;           \
  (*(H))= (*(E)) * (C);                       \
}while(0);


/**
  Adds columns into the empty string

  @param str             String where to write the data
  @param header_size     Size of the header without fixed part
  @param offset_size     Size of offset field in bytes
  @param column_count    Number of columns in the arrays
  @parem not_null_count  Number of non-null columns in the arrays
  @param data_size       Size of the data segment
  @param column_numbers  Array of columns numbers
  @param values          Array of columns values
  @param new_str         True if we need to allocate new string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_new_column_store(DYNAMIC_COLUMN *str,
                         size_t header_size,
                         size_t offset_size,
                         uint column_count,
                         uint not_null_count,
                         size_t data_size,
                         uint *column_numbers,
                         DYNAMIC_COLUMN_VALUE *values,
                         my_bool new_str)
{
  uchar *header_end;
  uint **columns_order;
  uint i;
  uint entry_size= COLUMN_NUMBER_SIZE + offset_size;
  enum enum_dyncol_func_result rc= ER_DYNCOL_RESOURCE;

  if (!(columns_order= malloc(sizeof(uint*)*column_count)))
    return ER_DYNCOL_RESOURCE;
  if (new_str)
  {
    if (dynamic_column_init_str(str,
                                data_size + header_size + DYNCOL_SYZERESERVE))
      goto err;
  }
  else
  {
    str->length= 0;
    if (dynstr_realloc(str, data_size + header_size + DYNCOL_SYZERESERVE))
      goto err;
    bzero(str->str, FIXED_HEADER_SIZE);
    str->length= FIXED_HEADER_SIZE;
  }

  /* sort columns for the header */
  for (i= 0; i < column_count; i++)
    columns_order[i]= column_numbers + i;
  qsort(columns_order, (size_t)column_count, sizeof(uint*), &column_sort);

  /*
    For now we don't allow creating two columns with the same number
    at the time of create.  This can be fixed later to just use the later
    by comparing the pointers.
  */
  for (i= 0; i < column_count - 1; i++)
  {
    if (columns_order[i][0] > UINT_MAX16 ||
        columns_order[i][0] == columns_order[i + 1][0])
    {
      rc= ER_DYNCOL_DATA;
      goto err;
    }
  }
  if (columns_order[i][0] > UINT_MAX16)
  {
    rc= ER_DYNCOL_DATA;
    goto err;
  }

  DBUG_ASSERT(str->max_length >= str->length + header_size);
  set_fixed_header(str, offset_size, not_null_count);
  str->length+= header_size; /* reserve place for header */
  header_end= (uchar *)str->str + FIXED_HEADER_SIZE;
  for (i= 0; i < column_count; i++)
  {
    uint ord= columns_order[i] - column_numbers;
    if (values[ord].type != DYN_COL_NULL)
    {
      /* Store header first in the str */
      int2store(header_end, column_numbers[ord]);
      type_and_offset_store(header_end, offset_size,
                            values[ord].type,
                            str->length - header_size - FIXED_HEADER_SIZE);

      /* Store value in 'str + str->length' and increase str->length */
      if ((rc= data_store(str, values + ord)))
        goto err;
      header_end+= entry_size;
    }
  }
  rc= ER_DYNCOL_OK;
err:
  free(columns_order);
  return rc;
}

/**
  Create packed string which contains given columns (internal)

  @param str             String where to write the data
  @param column_count    Number of columns in the arrays
  @param column_numbers  Array of columns numbers
  @param values          Array of columns values
  @param new_str         True if we need allocate new string

  @return ER_DYNCOL_* return code
*/

static enum enum_dyncol_func_result
dynamic_column_create_many_internal(DYNAMIC_COLUMN *str,
                                    uint column_count,
                                    uint *column_numbers,
                                    DYNAMIC_COLUMN_VALUE *values,
                                    my_bool new_str)
{
  size_t data_size= 0;
  size_t header_size, offset_size;
  uint i;
  int not_null_column_count= 0;

  if (new_str)
  {
    /* to make dynstr_free() working in case of errors */
    bzero(str, sizeof(DYNAMIC_COLUMN));
  }

  for (i= 0; i < column_count; i++)
  {
    if (values[i].type != DYN_COL_NULL)
    {
      size_t tmp;
      not_null_column_count++;
      data_size+= (tmp=dynamic_column_value_len(values + i));
      if (tmp == (size_t) ~0)
        return ER_DYNCOL_DATA;
    }
  }

  /* We can handle data up to 1fffffff = 536870911 bytes now */
  if ((offset_size= dynamic_column_offset_bytes(data_size)) >=
      MAX_OFFSET_LENGTH)
    return ER_DYNCOL_LIMIT;

  /* header entry is column number + offset & type */
  header_size= not_null_column_count * (offset_size + 2);

  return dynamic_new_column_store(str,
                                  header_size, offset_size,
                                  column_count,
                                  not_null_column_count,
                                  data_size,
                                  column_numbers, values,
                                  new_str);
}


/**
  Create packed string which contains given columns

  @param str             String where to write the data
  @param column_count    Number of columns in the arrays
  @param column_numbers  Array of columns numbers
  @param values          Array of columns values

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_create_many(DYNAMIC_COLUMN *str,
                           uint column_count,
                           uint *column_numbers,
                           DYNAMIC_COLUMN_VALUE *values)
{
  DBUG_ENTER("dynamic_column_create_many");
  DBUG_RETURN(dynamic_column_create_many_internal(str, column_count,
                                                  column_numbers, values,
                                                  TRUE));
}


/**
  Create packed string which contains given column

  @param str             String where to write the data
  @param column_number   Column number
  @param value           The columns value

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_create(DYNAMIC_COLUMN *str, uint column_nr,
                      DYNAMIC_COLUMN_VALUE *value)
{
  DBUG_ENTER("dynamic_column_create");
  DBUG_RETURN(dynamic_column_create_many(str, 1, &column_nr, value));
}


/**
  Calculate length of data between given two header entries

  @param entry           Pointer to the first entry
  @param entry_next      Pointer to the last entry
  @param header_end      Pointer to the header end
  @param offset_size     Size of offset field in bytes
  @param last_offset     Size of the data segment
  @param error           Set in case of error

  @return number of bytes
*/

static size_t get_length_interval(uchar *entry, uchar *entry_next,
                                  uchar *header_end, size_t offset_size,
                                  size_t last_offset, my_bool *error)
{
  size_t offset, offset_next;
  DYNAMIC_COLUMN_TYPE type, type_next;
  DBUG_ASSERT(entry < entry_next);

  type_and_offset_read(&type, &offset, entry, offset_size);
  if (entry_next >= header_end)
  {
    *error= 0;
    return (last_offset - offset);
  }
  type_and_offset_read(&type_next, &offset_next, entry_next, offset_size);
  *error= (offset_next > last_offset);
  return (offset_next - offset);
}

/*
  Calculate length of data of one column


  @param entry           Pointer to the first entry
  @param header_end      Pointer to the header end
  @param offset_size     Size of offset field in bytes
  @param last_offset     Size of the data segment
  @param error           Set in case of error

  @return number of bytes
*/

static size_t get_length(uchar *entry, uchar *header_end,
                         size_t offset_size,
                         size_t last_offset, my_bool *error)
{
  return get_length_interval(entry,
                             entry + offset_size + COLUMN_NUMBER_SIZE,
                             header_end, offset_size, last_offset, error);
}


/**
  Comparator function for references to header entries for qsort
*/

static int header_compar(const void *a, const void *b)
{
  uint va= uint2korr((uchar*)a), vb= uint2korr((uchar*)b);
  return (va > vb ? 1 : (va < vb ? -1 : 0));
}


/**
  Find column and fill information about it

  @param type            Returns type of the column
  @param data            Returns a pointer to the data
  @param length          Returns length of the data
  @param offset_size     Size of offset field in bytes
  @param column_count    Number of column in the packed string
  @param data_end        Pointer to the data end
  @param num             Number of the column we want to fetch
  @param entry_pos       NULL or place where to put reference to the entry

  @return 0 ok
  @return 1 error in data
*/

static my_bool
find_column(DYNAMIC_COLUMN_TYPE *type, uchar **data, size_t *length,
            uchar *header, size_t offset_size, uint column_count,
            uchar *data_end, uint num, uchar **entry_pos)
{
  uchar *entry;
  size_t offset, total_data, header_size, entry_size;
  uchar key[2+4];
  my_bool error;

  if (!entry_pos)
    entry_pos= &entry;

  calc_param(&entry_size, &header_size, offset_size, column_count);

  if (header + header_size > data_end)
    return 1;

  int2store(key, num);
  entry= bsearch(key, header, (size_t)column_count, entry_size,
                 &header_compar);
  if (!entry)
  {
    /* Column not found */
    *type= DYN_COL_NULL;
    *entry_pos= NULL;
    return 0;
  }
  type_and_offset_read(type, &offset, entry, offset_size);
  total_data= data_end - (header + header_size);
  if (offset > total_data)
    return 1;
  *data= header + header_size + offset;
  *length= get_length(entry, header + header_size, offset_size,
                      total_data, &error);
  /*
    Check that the found data is withing the ranges. This can happen if
    we get data with wrong offsets.
  */
  if (error || (long) *length < 0 || offset + *length > total_data)
    return 1;

  *entry_pos= entry;
  return 0;
}


/**
   Read and check the header of the dynamic string

  @param str             Dynamic string

  @retval FALSE OK
  @retval TRUE  error

  Note
    We don't check for str->length == 0 as all code that calls this
    already have handled this case.
*/

static inline my_bool read_fixed_header(DYNAMIC_COLUMN *str,
                                        size_t *offset_size,
                                        uint *column_count)
{
  DBUG_ASSERT(str != NULL && str->length != 0);
  if ((str->length < FIXED_HEADER_SIZE) ||
      (str->str[0] & (~DYNCOL_FLG_KNOWN)))
    return 1;                                   /* Wrong header */
  *offset_size= (str->str[0] & DYNCOL_FLG_OFFSET) + 1;
  *column_count= uint2korr(str->str + 1);
  return 0;
}


/**
  Get dynamic column value

  @param str             The packed string to extract the column
  @param column_nr       Number of column to fetch
  @param store_it_here   Where to store the extracted value

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_get(DYNAMIC_COLUMN *str, uint column_nr,
                       DYNAMIC_COLUMN_VALUE *store_it_here)
{
  uchar *data;
  size_t offset_size, length;
  uint column_count;
  enum enum_dyncol_func_result rc= ER_DYNCOL_FORMAT;

  if (str->length == 0)
    goto null;

  if (read_fixed_header(str, &offset_size, &column_count))
    goto err;

  if (column_count == 0)
    goto null;

  if (find_column(&store_it_here->type, &data, &length,
                  (uchar*)str->str + FIXED_HEADER_SIZE,
                  offset_size, column_count, (uchar*)str->str + str->length,
                  column_nr, NULL))
    goto err;

  switch (store_it_here->type) {
  case DYN_COL_INT:
    rc= dynamic_column_sint_read(store_it_here, data, length);
    break;
  case DYN_COL_UINT:
    rc= dynamic_column_uint_read(store_it_here, data, length);
    break;
  case DYN_COL_DOUBLE:
    rc= dynamic_column_double_read(store_it_here, data, length);
    break;
  case DYN_COL_STRING:
    rc= dynamic_column_string_read(store_it_here, data, length);
    break;
  case DYN_COL_DECIMAL:
    rc= dynamic_column_decimal_read(store_it_here, data, length);
    break;
  case DYN_COL_DATETIME:
    rc= dynamic_column_date_time_read(store_it_here, data, length);
    break;
  case DYN_COL_DATE:
    rc= dynamic_column_date_read(store_it_here, data, length);
    break;
  case DYN_COL_TIME:
    rc= dynamic_column_time_read(store_it_here, data, length);
    break;
  case DYN_COL_NULL:
    rc= ER_DYNCOL_OK;
    break;
  default:
    goto err;
  }
  return rc;

null:
    rc= ER_DYNCOL_OK;
err:
    store_it_here->type= DYN_COL_NULL;
    return rc;
}

/**
  Delete column with given number from the packed string

  @param str             The packed string to delete the column
  @param column_nr       Number of column to delete

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_delete(DYNAMIC_COLUMN *str, uint column_nr)
{
  uchar *data, *header_entry, *read, *write;
  size_t offset_size, new_offset_size, length, entry_size, new_entry_size,
         header_size, new_header_size, data_size, new_data_size,
         deleted_entry_offset;
  uint column_count, i;
  DYNAMIC_COLUMN_TYPE type;

  if (str->length == 0)
    return ER_DYNCOL_OK;  /* no columns */

  if (read_fixed_header(str, &offset_size, &column_count))
    return ER_DYNCOL_FORMAT;

  if (column_count == 0)
  {
    str->length= 0;
    return ER_DYNCOL_OK;  /* no columns */
  }

  if (find_column(&type, &data, &length, (uchar*)str->str + FIXED_HEADER_SIZE,
                  offset_size, column_count, (uchar*)str->str + str->length,
                  column_nr, &header_entry))
    return ER_DYNCOL_FORMAT;

  if (type == DYN_COL_NULL)
    return ER_DYNCOL_OK;  /* no such column */

  if (column_count == 1)
  {
    /* delete the only column; Return empty string */
    str->length= 0;
    return ER_DYNCOL_OK;
  }

  /* Calculate entry_size and header_size */
  calc_param(&entry_size, &header_size, offset_size, column_count);
  data_size= str->length - FIXED_HEADER_SIZE - header_size;

  new_data_size= data_size - length;
  if ((new_offset_size= dynamic_column_offset_bytes(new_data_size)) >=
      MAX_OFFSET_LENGTH)
    return ER_DYNCOL_LIMIT;
  DBUG_ASSERT(new_offset_size <= offset_size);

  calc_param(&new_entry_size, &new_header_size,
             new_offset_size, column_count - 1);

  deleted_entry_offset= ((data - (uchar*) str->str) -
                         header_size - FIXED_HEADER_SIZE);

  /* rewrite header*/
  set_fixed_header(str, new_offset_size, column_count - 1);
  for (i= 0, write= read= (uchar *)str->str + FIXED_HEADER_SIZE;
       i < column_count;
       i++, read+= entry_size, write+= new_entry_size)
  {
    size_t offs;
    uint nm;
    DYNAMIC_COLUMN_TYPE tp;
    if (read == header_entry)
    {
#ifndef DBUG_OFF
      nm= uint2korr(read);
      type_and_offset_read(&tp, &offs, read,
                           offset_size);
      DBUG_ASSERT(nm == column_nr);
      DBUG_ASSERT(offs == deleted_entry_offset);
#endif
      write-= new_entry_size;                 /* do not move writer */
      continue;                               /* skip removed field */
    }

    nm= uint2korr(read),
    type_and_offset_read(&tp, &offs, read,
                         offset_size);

    if (offs > deleted_entry_offset)
      offs-= length;              /* data stored after removed data */

    int2store(write, nm);
    type_and_offset_store(write, new_offset_size, tp, offs);
  }

  /* move data */
  {
    size_t first_chunk_len= ((data - (uchar *)str->str) -
                             FIXED_HEADER_SIZE - header_size);
    size_t second_chunk_len= new_data_size - first_chunk_len;
    if (first_chunk_len)
      memmove(str->str + FIXED_HEADER_SIZE + new_header_size,
              str->str + FIXED_HEADER_SIZE + header_size,
              first_chunk_len);
    if (second_chunk_len)
      memmove(str->str +
              FIXED_HEADER_SIZE + new_header_size + first_chunk_len,
              str->str +
              FIXED_HEADER_SIZE + header_size + first_chunk_len + length,
              second_chunk_len);
  }

  /* fix str length */
  DBUG_ASSERT(str->length >=
              FIXED_HEADER_SIZE + new_header_size + new_data_size);
  str->length= FIXED_HEADER_SIZE + new_header_size + new_data_size;

  return ER_DYNCOL_OK;
}


/**
  Check existence of the column in the packed string

  @param str             The packed string to check the column
  @param column_nr       Number of column to check

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_exists(DYNAMIC_COLUMN *str, uint column_nr)
{
  uchar *data;
  size_t offset_size, length;
  uint column_count;
  DYNAMIC_COLUMN_TYPE type;

  if (str->length == 0)
    return ER_DYNCOL_NO;                        /* no columns */

  if (read_fixed_header(str, &offset_size, &column_count))
    return ER_DYNCOL_FORMAT;

  if (column_count == 0)
    return ER_DYNCOL_NO;                        /* no columns */

  if (find_column(&type, &data, &length, (uchar*)str->str + FIXED_HEADER_SIZE,
                  offset_size, column_count, (uchar*)str->str + str->length,
                  column_nr, NULL))
    return ER_DYNCOL_FORMAT;

  return (type != DYN_COL_NULL ? ER_DYNCOL_YES : ER_DYNCOL_NO);
}


/**
  List not-null columns in the packed string

  @param str             The packed string
  @param array_of_uint   Where to put reference on created array

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_list(DYNAMIC_COLUMN *str, DYNAMIC_ARRAY *array_of_uint)
{
  uchar *read;
  size_t offset_size, entry_size;
  uint column_count, i;

  bzero(array_of_uint, sizeof(*array_of_uint)); /* In case of errors */
  if (str->length == 0)
    return ER_DYNCOL_OK;                        /* no columns */

  if (read_fixed_header(str, &offset_size, &column_count))
    return ER_DYNCOL_FORMAT;

  entry_size= COLUMN_NUMBER_SIZE + offset_size;

  if (entry_size * column_count + FIXED_HEADER_SIZE > str->length)
    return ER_DYNCOL_FORMAT;

  if (init_dynamic_array(array_of_uint, sizeof(uint), column_count, 0))
    return ER_DYNCOL_RESOURCE;

  for (i= 0, read= (uchar *)str->str + FIXED_HEADER_SIZE;
       i < column_count;
       i++, read+= entry_size)
  {
    uint nm= uint2korr(read);
    /* Insert can't never fail as it's pre-allocated above */
    (void) insert_dynamic(array_of_uint, (uchar *)&nm);
  }
  return ER_DYNCOL_OK;
}


/**
  Find the place of the column in the header or place where it should be put

  @param num             Number of the column
  @param header          Pointer to the header
  @param entry_size      Size of a header entry
  @param column_count    Number of columns in the packed string
  @param entry           Return pointer to the entry or next entry

  @retval TRUE found
  @retval FALSE pointer set to the next row
*/

static my_bool
find_place(uint num, uchar *header, size_t entry_size,
           uint column_count, uchar **entry)
{
  uint mid, start, end, val;
  int flag;
  LINT_INIT(flag);                              /* 100 % safe */

  start= 0;
  end= column_count -1;
  mid= 1;
  while (start != end)
  {
   uint val;
   mid= (start + end) / 2;
   val= uint2korr(header + mid * entry_size);
   if ((flag= CMP_NUM(num, val)) <= 0)
     end= mid;
   else
     start= mid + 1;
  }
  if (start != mid)
  {
    val= uint2korr(header + start * entry_size);
    flag= CMP_NUM(num, val);
  }
  *entry= header + start * entry_size;
  if (flag > 0)
    *entry+= entry_size;        /* Point at next bigger key */
  return flag == 0;
}


/*
  Description of plan of adding/removing/updating a packed string
*/

typedef enum {PLAN_REPLACE, PLAN_ADD, PLAN_DELETE, PLAN_NOP} PLAN_ACT;

struct st_plan {
  DYNAMIC_COLUMN_VALUE *val;
  uint *num;
  uchar *place;
  size_t length;
  int hdelta, ddelta;
  PLAN_ACT act;
};
typedef struct st_plan PLAN;


static int plan_sort(const void *a, const void *b)
{
  return ((PLAN *)a)->num[0] - ((PLAN *)b)->num[0];
}

#define DELTA_CHECK(S, D, C)        \
  if ((S) == 0)                     \
    (S)= (D);                       \
  else if (((S) > 0 && (D) < 0) ||  \
            ((S) < 0 && (D) > 0))   \
  {                                 \
    (C)= TRUE;                      \
    break;                          \
  }                                 \


/**
  Update the packed string with the given columns

  @param str             String where to write the data
  @param add_column_count Number of columns in the arrays
  @param column_numbers  Array of columns numbers
  @param values          Array of columns values

  @return ER_DYNCOL_* return code
*/

enum enum_dyncol_func_result
dynamic_column_update_many(DYNAMIC_COLUMN *str,
                           uint add_column_count,
                           uint *column_numbers,
                           DYNAMIC_COLUMN_VALUE *values)
{
  PLAN *plan;
  uchar *header_end;
  long data_delta= 0;
  uint i, j, k;
  uint new_column_count, column_count, not_null;
  enum enum_dyncol_func_result rc;
  int header_delta;
  size_t offset_size, entry_size, header_size, data_size;
  size_t new_offset_size, new_entry_size, new_header_size, new_data_size;
  size_t max_offset;

  if (add_column_count == 0)
    return ER_DYNCOL_OK;

  /*
    Get columns in column order. As the data in 'str' is already
    in column order this allows to replace all columns in one loop.
  */

  if (!(plan= my_malloc(sizeof(PLAN) * (add_column_count + 1), MYF(0))))
    return ER_DYNCOL_RESOURCE;

  not_null= add_column_count;
  for (i= 0; i < add_column_count; i++)
  {
    if (column_numbers[i] > UINT_MAX16)
    {
      rc= ER_DYNCOL_DATA;
      goto end;
    }

    plan[i].val= values + i;
    plan[i].num= column_numbers + i;
    if (values[i].type == DYN_COL_NULL)
      not_null--;

  }

  if (str->length == 0)
  {
    /*
      Just add new columns. If there was no columns to add we return
      an empty string.
     */
    goto create_new_string;
  }

  /* Check that header is ok */
  if (read_fixed_header(str, &offset_size, &column_count))
  {
    rc= ER_DYNCOL_FORMAT;
    goto end;
  }
  if (column_count == 0)
    goto create_new_string;

  qsort(plan, (size_t)add_column_count, sizeof(PLAN), &plan_sort);

  new_column_count= column_count;
  calc_param(&entry_size, &header_size, offset_size, column_count);
  max_offset= str->length - (FIXED_HEADER_SIZE + header_size);
  header_end= (uchar*) str->str + FIXED_HEADER_SIZE + header_size;

  if (header_size + FIXED_HEADER_SIZE > str->length)
  {
    rc= ER_DYNCOL_FORMAT;
    goto end;
  }

  /*
    Calculate how many columns and data is added/deleted and make a 'plan'
    for each of them.
  */
  header_delta= 0;
  for (i= 0; i < add_column_count; i++)
  {
    uchar *entry;

    /*
      For now we don't allow creating two columns with the same number
      at the time of create.  This can be fixed later to just use the later
      by comparing the pointers.
    */
    if (i < add_column_count - 1 && plan[i].num[0] == plan[i + 1].num[0])
    {
      rc= ER_DYNCOL_DATA;
      goto end;
    }

    /* Set common variables for all plans */
    plan[i].ddelta= data_delta;
    /* get header delta in entries */
    plan[i].hdelta= header_delta;
    plan[i].length= 0;                          /* Length if NULL */

    if (find_place(plan[i].num[0],
                   (uchar *)str->str + FIXED_HEADER_SIZE,
                   entry_size, column_count, &entry))
    {
      size_t entry_data_size;
      my_bool error;

      /* Data existed; We have to replace or delete it */

      entry_data_size= get_length(entry, header_end,
                                  offset_size, max_offset, &error);
      if (error || (long) entry_data_size < 0)
      {
        rc= ER_DYNCOL_FORMAT;
        goto end;
      }

      if (plan[i].val->type == DYN_COL_NULL)
      {
        /* Inserting a NULL means delete the old data */

        plan[i].act= PLAN_DELETE;	        /* Remove old value */
        header_delta--;                         /* One row less in header */
        data_delta-= entry_data_size;           /* Less data to store */
      }
      else
      {
        /* Replace the value */

        plan[i].act= PLAN_REPLACE;
        /* get data delta in bytes */
        if ((plan[i].length= dynamic_column_value_len(plan[i].val)) ==
            (size_t) ~0)
        {
          rc= ER_DYNCOL_DATA;
          goto end;
        }
        data_delta+= plan[i].length - entry_data_size;
      }
    }
    else
    {
      /* Data did not exists. Add if it it's not NULL */

      if (plan[i].val->type == DYN_COL_NULL)
      {
        plan[i].act= PLAN_NOP;                  /* Mark entry to be skiped */
      }
      else
      {
        /* Add new value */

        plan[i].act= PLAN_ADD;
        header_delta++;                         /* One more row in header */
        /* get data delta in bytes */
        if ((plan[i].length= dynamic_column_value_len(plan[i].val)) ==
            (size_t) ~0)
        {
          rc= ER_DYNCOL_DATA;
          goto end;
        }
        data_delta+= plan[i].length;
      }
    }
    plan[i].place= entry;
  }
  plan[add_column_count].hdelta= header_delta;
  plan[add_column_count].ddelta= data_delta;
  new_column_count= column_count + header_delta;

  /*
    Check if it is only "increasing" or only "decreasing" plan for (header
    and data separately).
  */
  data_size= str->length - header_size - FIXED_HEADER_SIZE;
  new_data_size= data_size + data_delta;
  if ((new_offset_size= dynamic_column_offset_bytes(new_data_size)) >=
      MAX_OFFSET_LENGTH)
  {
    rc= ER_DYNCOL_LIMIT;
    goto end;
  }

#ifdef NOT_IMPLEMENTED
  /* if (new_offset_size != offset_size) then we have to rewrite header */
  header_delta_sign= new_offset_size - offset_size;
  data_delta_sign= 0;
  for (i= 0; i < add_column_count; i++)
  {
    /* This is the check for increasing/decreasing */
    DELTA_CHECK(header_delta_sign, plan[i].hdelta, copy);
    DELTA_CHECK(data_delta_sign, plan[i].ddelta, copy);
  }
#endif
  calc_param(&new_entry_size, &new_header_size,
             new_offset_size, new_column_count);

  /*
    The following code always make a copy. In future we can do a more
    optimized version when data is only increasing / decreasing.
  */

  /*if (copy) */
  {
    DYNAMIC_COLUMN tmp;
    uchar *header_base= (uchar *)str->str + FIXED_HEADER_SIZE,
          *write;
    if (dynamic_column_init_str(&tmp,
                                (FIXED_HEADER_SIZE + new_header_size +
                                 new_data_size + DYNCOL_SYZERESERVE)))
    {
      rc= ER_DYNCOL_RESOURCE;
      goto end;
    }
    write= (uchar *)tmp.str + FIXED_HEADER_SIZE;
    /* Adjust tmp to contain whole the future header */
    tmp.length= FIXED_HEADER_SIZE + new_header_size;
    set_fixed_header(&tmp, new_offset_size, new_column_count);
    data_delta= 0;

    /*
      Copy data to the new string
      i= index in array of changes
      j= index in packed string header index
    */

    for (i= 0, j= 0; i < add_column_count || j < column_count; i++)
    {
      size_t first_offset;
      uint start= j, end;
      LINT_INIT(first_offset);

      /*
        Search in i and j for the next column to add from i and where to
        add.
      */

      while (i < add_column_count && plan[i].act == PLAN_NOP)
        i++;                                    /* skip NOP */
      if (i == add_column_count)
        j= end= column_count;
      else
      {
        /*
          old data portion. We don't need to check that j < column_count
          as plan[i].place is guaranteed to have a pointer inside the
          data.
        */
        while (header_base + j * entry_size < plan[i].place)
          j++;
        end= j;
        if ((plan[i].act == PLAN_REPLACE || plan[i].act == PLAN_DELETE))
          j++;                              /* data at 'j' will be removed */
      }

      if (plan[i].ddelta == 0 && offset_size == new_offset_size)
      {
        uchar *read= header_base + start * entry_size;
        DYNAMIC_COLUMN_TYPE tp;
        /*
          It's safe to copy the header unchanged. This is usually the
          case for the first header block before any changed data.
        */
        if (start < end)                        /* Avoid memcpy with 0 */
        {
          size_t length= entry_size * (end - start);
          memcpy(write, read, length);
          write+= length;
        }
        /* Read first_offset */
        type_and_offset_read(&tp, &first_offset, read, offset_size);
      }
      else
      {
        /*
          Adjust all headers since last loop.
          We have to do this as the offset for data has moved
        */
        for (k= start; k < end; k++)
        {
          uchar *read= header_base + k * entry_size;
          size_t offs;
          uint nm;
          DYNAMIC_COLUMN_TYPE tp;

          nm= uint2korr(read);                    /* Column nummber */
          type_and_offset_read(&tp, &offs, read, offset_size);
          if (k == start)
            first_offset= offs;
          else if (offs < first_offset)
          {
            dynamic_column_column_free(&tmp);
            rc= ER_DYNCOL_FORMAT;
            goto end;
          }

          offs+= plan[i].ddelta;
          int2store(write, nm);
          /* write rest of data at write + COLUMN_NUMBER_SIZE */
          type_and_offset_store(write, new_offset_size, tp, offs);
          write+= new_entry_size;
        }
      }

      /* copy first the data that was not replaced in original packed data */
      if (start < end)
      {
        my_bool error;
        /* Add old data last in 'tmp' */
        size_t data_size=
          get_length_interval(header_base + start * entry_size,
                              header_base + end * entry_size,
                              header_end, offset_size, max_offset, &error);
        if (error || (long) data_size < 0 ||
            data_size > max_offset - first_offset)
        {
          dynamic_column_column_free(&tmp);
          rc= ER_DYNCOL_FORMAT;
          goto end;
        }

        memcpy(tmp.str + tmp.length, (char *)header_end + first_offset,
               data_size);
        tmp.length+= data_size;
      }

      /* new data adding */
      if (i < add_column_count)
      {
        if( plan[i].act == PLAN_ADD || plan[i].act == PLAN_REPLACE)
        {
          int2store(write, plan[i].num[0]);
          type_and_offset_store(write, new_offset_size,
                                plan[i].val[0].type,
                                tmp.length -
                                (FIXED_HEADER_SIZE + new_header_size));
          write+= new_entry_size;
          data_store(&tmp, plan[i].val);        /* Append new data */
        }
        data_delta= plan[i].ddelta;
      }
    }
    dynamic_column_column_free(str);
    *str= tmp;
  }

  rc= ER_DYNCOL_OK;

end:
  my_free(plan);
  return rc;

create_new_string:
  /* There is no columns from before, so let's just add the new ones */
  rc= ER_DYNCOL_OK;
  if (not_null != 0)
    rc= dynamic_column_create_many_internal(str, add_column_count,
                                            column_numbers, values,
                                            str->str == NULL);
  goto end;
}


/**
  Update the packed string with the given column

  @param str             String where to write the data
  @param column_number   Array of columns number
  @param values          Array of columns values

  @return ER_DYNCOL_* return code
*/


enum enum_dyncol_func_result
dynamic_column_update(DYNAMIC_COLUMN *str, uint column_nr,
                          DYNAMIC_COLUMN_VALUE *value)
{
  return dynamic_column_update_many(str, 1, &column_nr, value);
}
