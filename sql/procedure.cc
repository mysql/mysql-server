/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/* Procedures (functions with changes output of select) */

#include "procedure.h"
#include "sql_analyse.h"			// Includes procedure

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

