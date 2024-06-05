/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * decimal_utils.hpp
 */

#ifndef decimal_utils_hpp
#define decimal_utils_hpp

/* return values (redeclared here if to be mapped to Java) */
#define E_DEC_OK 0
#define E_DEC_TRUNCATED 1
#define E_DEC_OVERFLOW 2
#define E_DEC_BAD_NUM 8
#define E_DEC_OOM 16
/* return values below here are unique to ndbjtie --
   not present in MySQL's decimal library */
#define E_DEC_BAD_PREC 32
#define E_DEC_BAD_SCALE 64

/*
 decimal_str2bin: Convert string directly to on-disk binary format.
 str  - string to convert
 str_len - length of string
 prec - precision of column
 scale - scale of column
 bin - buffer for binary representation
 bin_len - length of buffer

 NOTES
   Added so that NDB API programs can convert directly between  the stored
   binary format and a string representation without using decimal_t.

 RETURN VALUE
   E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW/E_DEC_OOM
*/
int decimal_str2bin(const char *str, int str_len, int prec, int scale,
                    void *bin, int bin_len);

/*
 decimal_bin2str():  Convert directly from on-disk binary format to string
 bin  - value to convert
 bin_len - length to convert
 prec - precision of column
 scale - scale of column
 str - buffer for string representation
 str_len - length of buffer

 NOTES
   Added so that NDB API programs can convert directly between  the stored
   binary format and a string representation without using decimal_t.


 RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW/E_DEC_BAD_NUM/E_DEC_OOM
 */
int decimal_bin2str(const void *bin, int bin_len, int prec, int scale,
                    char *str, int str_len);

#endif  // decimal_utils_hpp
