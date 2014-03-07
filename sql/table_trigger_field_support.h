/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_TRIGGER_FIELD_SUPPORT_H_INCLUDED
#define TABLE_TRIGGER_FIELD_SUPPORT_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

#include "trigger_def.h"  // enum_trigger_variable_type

///////////////////////////////////////////////////////////////////////////

struct TABLE;
class Field;

///////////////////////////////////////////////////////////////////////////

/**
  This is an interface to be used from Item_trigger_field to access information
  about table trigger fields (NEW/OLD rows).
*/

class Table_trigger_field_support
{
public:
  virtual TABLE *get_subject_table()= 0;

  virtual Field *get_trigger_variable_field(enum_trigger_variable_type v,
                                            int field_index)= 0;

  virtual ~Table_trigger_field_support()
  { }
};

///////////////////////////////////////////////////////////////////////////

#endif // TABLE_TRIGGER_FIELD_SUPPORT_H_INCLUDED
