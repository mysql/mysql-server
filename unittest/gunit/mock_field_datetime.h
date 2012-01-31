/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MOCK_FIELD_DATETIME_H
#define MOCK_FIELD_DATETIME_H

#include "field.h"

class Mock_field_datetime : public Field_datetime
{
  void initialize()
  {
    ptr= buffer;
    memset(buffer, 0, PACK_LENGTH);
  }

public:
  uchar buffer[PACK_LENGTH];
  Mock_field_datetime() : Field_datetime(false, "") { initialize(); }

};

#endif // MOCK_FIELD_DATETIME_H
