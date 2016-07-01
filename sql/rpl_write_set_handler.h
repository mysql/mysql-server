/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_WRITE_SET_HANDLER_INCLUDED
#define RPL_WRITE_SET_HANDLER_INCLUDED

extern const char *transaction_write_set_hashing_algorithms[];

class THD;
struct TABLE;

/**
  Function that returns the write set extraction algorithm name.

  @param[in] algorithm  The algorithm value

  @return the algorithm name
*/
const char* get_write_set_algorithm_string(unsigned int algorithm);

/**
  Function to add the hash of the PKE to the transaction context object.

  @param[in] table - TABLE object
  @param[in] thd - THD object pointing to current thread.
*/
void add_pke(TABLE *table, THD *thd);

#endif
