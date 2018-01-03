/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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


/* Procedures (functions with changes output of select) */

#include "sql/procedure.h"

#include "decimal.h"

my_decimal *Item_proc_string::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return 0;
  string2my_decimal(E_DEC_FATAL_ERROR, &str_value, decimal_value);
  return (decimal_value);
}


my_decimal *Item_proc_int::val_decimal(my_decimal *decimal_value)
{
  if (null_value)
    return 0;
  int2my_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_value);
  return (decimal_value);
}

