/* Copyright (c) 2014 Oracle and/or its affiliates. All rights reserved.

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
  @file binary_log_decimal.h

  @brief Contains functions which are used by the server and libbinlogevent
         library both.
*/

#ifndef BINARY_LOG_DECIMAL_INCLUDED
#define BINARY_LOG_DECIMAL_INCLUDED

unsigned int my_time_binary_length(unsigned int dec);
unsigned int my_datetime_binary_length(unsigned int dec);
unsigned int my_timestamp_binary_length(unsigned int dec);

#endif
