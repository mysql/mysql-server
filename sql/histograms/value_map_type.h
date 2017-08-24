#ifndef HISTOGRAMS_VALUE_MAP_TYPE_INCLUDED
#define HISTOGRAMS_VALUE_MAP_TYPE_INCLUDED

/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

namespace histograms
{

/**
  Datatypes that a Value_map and histogram can hold (including the invalid
  type).
*/
enum class Value_map_type
{
  INVALID,
  STRING,
  INT,
  UINT,
  DOUBLE,
  DECIMAL,
  DATE,
  TIME,
  DATETIME,
  ENUM,
  SET
};

}
#endif
