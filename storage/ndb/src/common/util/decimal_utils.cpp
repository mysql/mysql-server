/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
 * decimal_utils.cpp
 */

#include <assert.h>

#include "m_string.h"
#include "decimal.h"

#include "decimal_utils.hpp"

int decimal_str2bin(const char *str, int str_len,
                    int prec, int scale,
                    void *bin, int bin_len)
{
    int retval;                               /* return value from str2dec() */
    decimal_t dec;                            /* intermediate representation */
    decimal_digit_t digits[9];                /* for dec->buf */
    const char *end = str + str_len;
    
    assert(str != nullptr);   
    assert(bin != nullptr);
    if(prec < 1) return E_DEC_BAD_PREC;
    if((scale < 0) || (scale > prec)) return E_DEC_BAD_SCALE;
    
    if(decimal_bin_size(prec, scale) > bin_len)
        return E_DEC_OOM;
    
    dec.len = 9;                              /* big enough for any decimal */
    dec.buf = digits;
    
    retval = string2decimal(str, &dec, &end);
    if(retval != E_DEC_OK) return retval;
    
    return decimal2bin(&dec, (unsigned char *) bin, prec, scale);
}

int decimal_bin2str(const void *bin, int bin_len,
                    int prec, int scale, 
                    char *str, int str_len)
{
  int retval;                /* return from bin2decimal() */
  decimal_t dec;             /* intermediate representation */
  decimal_digit_t digits[9]; /* for dec->buf */
  int to_len;

  assert(bin != nullptr);
  assert(str != nullptr);
  if (prec < 1) return E_DEC_BAD_PREC;
  if ((scale < 0) || (scale > prec)) return E_DEC_BAD_SCALE;

  dec.len = 9; /* big enough for any decimal */
  dec.buf = digits;

  /*
   * Check of bin_len should be exact, but ndbjtie have no way to expose the
   * correct size, and use in MySqlUtilsTest.java depends on having an
   * oversized buffer.
   */
  if (bin_len < decimal_bin_size(prec, scale)) return E_DEC_BAD_NUM;

  retval = bin2decimal((const uchar *)bin, &dec, prec, scale);
  if (retval != E_DEC_OK) return retval;

  to_len = decimal_string_size(&dec);
  if (to_len > str_len) return E_DEC_OOM;

  return decimal2string(&dec, str, &to_len);
}
