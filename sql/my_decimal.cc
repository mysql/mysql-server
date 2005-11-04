/* Copyright (C) 2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "mysql_priv.h"

#ifndef MYSQL_CLIENT
/*
  report result of decimal operation

  SYNOPSIS
    decimal_operation_results()
    result  decimal library return code (E_DEC_* see include/decimal.h)

  TODO
    Fix error messages

  RETURN
    result
*/

int decimal_operation_results(int result)
{
  switch (result) {
  case E_DEC_OK:
    break;
  case E_DEC_TRUNCATED:
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			WARN_DATA_TRUNCATED, ER(WARN_DATA_TRUNCATED),
			"", (long)-1);
    break;
  case E_DEC_OVERFLOW:
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE),
			"DECIMAL", "");
    break;
  case E_DEC_DIV_ZERO:
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_DIVISION_BY_ZERO, ER(ER_DIVISION_BY_ZERO));
    break;
  case E_DEC_BAD_NUM:
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
			ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
			"decimal", "", "", (long)-1);
    break;
  case E_DEC_OOM:
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    break;
  default:
    DBUG_ASSERT(0);
  }
  return result;
}


/*
  Converting decimal to string

  SYNOPSIS
     my_decimal2string()

  return
    E_DEC_OK
    E_DEC_TRUNCATED
    E_DEC_OVERFLOW
    E_DEC_OOM
*/

int my_decimal2string(uint mask, const my_decimal *d,
                      uint fixed_prec, uint fixed_dec,
                      char filler, String *str)
{
  int length= (fixed_prec ? (fixed_prec + 1) : my_decimal_string_length(d));
  int result;
  if (str->alloc(length))
    return check_result(mask, E_DEC_OOM);
  result= decimal2string((decimal_t*) d, (char*) str->ptr(),
                         &length, (int)fixed_prec, fixed_dec,
                         filler);
  str->length(length);
  return check_result(mask, result);
}


/*
  Convert from decimal to binary representation

  SYNOPSIS
    my_decimal2binary()
    mask        error processing mask
    d           number for conversion
    bin         pointer to buffer where to write result
    prec        overall number of decimal digits
    scale       number of decimal digits after decimal point

  NOTE
    Before conversion we round number if it need but produce truncation
    error in this case

  RETURN
    E_DEC_OK
    E_DEC_TRUNCATED
    E_DEC_OVERFLOW
*/

int my_decimal2binary(uint mask, const my_decimal *d, char *bin, int prec,
		      int scale)
{
  int err1= E_DEC_OK, err2;
  my_decimal rounded;
  my_decimal2decimal(d, &rounded);
  rounded.frac= decimal_actual_fraction(&rounded);
  if (scale < rounded.frac)
  {
    err1= E_DEC_TRUNCATED;
    /* decimal_round can return only E_DEC_TRUNCATED */
    decimal_round(&rounded, &rounded, scale, HALF_UP);
  }
  err2= decimal2bin(&rounded, bin, prec, scale);
  if (!err2)
    err2= err1;
  return check_result(mask, err2);
}


/*
  Convert string for decimal when string can be in some multibyte charset

  SYNOPSIS
    str2my_decimal()
    mask            error processing mask
    from            string to process
    length          length of given string
    charset         charset of given string
    decimal_value   buffer for result storing

  RESULT
    E_DEC_OK
    E_DEC_TRUNCATED
    E_DEC_OVERFLOW
    E_DEC_BAD_NUM
    E_DEC_OOM
*/

int str2my_decimal(uint mask, const char *from, uint length,
                   CHARSET_INFO *charset, my_decimal *decimal_value)
{
  char *end, *from_end;
  int err;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp(buff, sizeof(buff), &my_charset_bin);
  if (charset->mbminlen > 1)
  {
    uint dummy_errors;
    tmp.copy(from, length, charset, &my_charset_latin1, &dummy_errors);
    from= tmp.ptr();
    length=  tmp.length();
    charset= &my_charset_bin;
  }
  from_end= end= (char*) from+length;
  err= string2decimal((char *)from, (decimal_t*) decimal_value, &end);
  if (end != from_end && !err)
  {
    /* Give warning if there is something other than end space */
    for ( ; end < from_end; end++)
    {
      if (!my_isspace(&my_charset_latin1, *end))
      {
        err= E_DEC_TRUNCATED;
        break;
      }
    }
  }
  check_result_and_overflow(mask, err, decimal_value);
  return err;
}


#ifndef DBUG_OFF
/* routines for debugging print */

/* print decimal */
void
print_decimal(const my_decimal *dec)
{
  fprintf(DBUG_FILE,
          "\nDecimal: sign: %d intg: %d frac: %d \n\
%09d,%09d,%09d,%09d,%09d,%09d,%09d,%09d\n",
          dec->sign(), dec->intg, dec->frac,
          dec->buf[0], dec->buf[1], dec->buf[2], dec->buf[3],
          dec->buf[4], dec->buf[5], dec->buf[6], dec->buf[7]);
}


/* print decimal with its binary representation */
void
print_decimal_buff(const my_decimal *dec, const byte* ptr, int length)
{
  print_decimal(dec);
  fprintf(DBUG_FILE, "Record: ");
  for (int i= 0; i < length; i++)
  {
    fprintf(DBUG_FILE, "%02X ", (uint)((uchar *)ptr)[i]);
  }
  fprintf(DBUG_FILE, "\n");
}


const char *dbug_decimal_as_string(char *buff, const my_decimal *val)
{
  int length= DECIMAL_MAX_STR_LENGTH;
  if (!val)
    return "NULL";
  (void)decimal2string((decimal_t*) val, buff, &length, 0,0,0);
  return buff;
}

#endif /*DBUG_OFF*/


#endif /*MYSQL_CLIENT*/
