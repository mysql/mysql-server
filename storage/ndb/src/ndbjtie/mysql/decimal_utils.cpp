/*
 Copyright 2010 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * decimal_utils.cpp
 */

#include "my_global.h"
#include "m_string.h"
C_MODE_START
#include "decimal.h"
C_MODE_END

#include "decimal_utils.hpp"

int decimal_str2bin(const char *str, int str_len,
                    int prec, int scale,
                    void *bin, int bin_len)
{
    register int retval;                      /* return value from str2dec() */
    decimal_t dec;                            /* intermediate representation */
    decimal_digit_t digits[9];                /* for dec->buf */
    char *end = (char *) str + str_len;       
    
    assert(str != 0);   
    assert(bin != 0);
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
    register int retval;                      /* return from bin2decimal() */
    decimal_t dec;                            /* intermediate representation */
    decimal_digit_t digits[9];                /* for dec->buf */
    int to_len;
    
    assert(bin != 0);
    assert(str != 0);
    if(prec < 1) return E_DEC_BAD_PREC;
    if((scale < 0) || (scale > prec)) return E_DEC_BAD_SCALE;
    
    dec.len = 9;                              /* big enough for any decimal */
    dec.buf = digits;
    
    // Note: bin_len is unused -- bin2decimal() does not take a length
    retval = bin2decimal((const uchar *) bin, &dec, prec, scale);
    if(retval != E_DEC_OK) return retval;
    
    to_len = decimal_string_size(&dec);
    if(to_len > str_len) return E_DEC_OOM;   
    
    return decimal2string(&dec, str, &to_len, 0, 0, 0);
}
