/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

/*
 * Functions exported from this package.
 */

#ifndef BINARY_LOG_FUNCS_INCLUDED
#define BINARY_LOG_FUNCS_INCLUDED

#include "binary_log_types.h"

// We use cstdint if this is 2011 standard (or later)
#if __cplusplus > 201100L
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

unsigned int my_time_binary_length(unsigned int dec);
unsigned int my_datetime_binary_length(unsigned int dec);
unsigned int my_timestamp_binary_length(unsigned int dec);

/**
 This helper function calculates the size in bytes of a particular field in a
 row type event as defined by the field_ptr and metadata_ptr arguments.
 @param column_type Field type code
 @param field_ptr The field data
 @param metadata_ptr The field metadata

 @note We need the actual field data because the string field size is not
 part of the meta data. :(

 @return The size in bytes of a particular field
*/
uint32_t calc_field_size(unsigned char column_type, const unsigned char *field_ptr,
                         unsigned int metadata);


/**
   Compute the maximum display length of a field.

   @param sql_type Type of the field
   @param metadata The metadata from the master for the field.
   @return Maximum length of the field in bytes.
 */
unsigned int max_display_length_for_field(enum_field_types sql_type,
                                          unsigned int metadata);

/**
   Returns the size of array to hold a binary representation of a decimal
   @param  precision  number of significant digits in a particular radix R
                      where R is either 2 or 10.
   @param  scale      to what position to round.
   @return  size in bytes
*/
int decimal_binary_size(int precision, int scale);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* BINARY_LOG_FUNCS_INCLUDED */
