/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef WINDOW_LEX_INCLUDED
#define WINDOW_LEX_INCLUDED

/**
  Cf. SQL 2003 7.11 \<window frame units\> and class PT_border
*/
enum enum_window_frame_unit
{
  WFU_ROWS,
  WFU_RANGE,
  WFU_GROUPS
};


/**
  Cf. SQL 2003 7.11 \<window frame extent\> and class PT_border
  The Window::comparators array depends on the order of values in this enum.
*/
enum enum_window_border_type {
  WBT_CURRENT_ROW= 0,
  WBT_VALUE_PRECEDING,
  WBT_VALUE_FOLLOWING,
  WBT_UNBOUNDED_PRECEDING,
  WBT_UNBOUNDED_FOLLOWING,
};

/**
  Cf. SQL 2003 7.11 \<window frame exclusion\> and class PT_exclusion
*/
enum enum_window_frame_exclusion {
  WFX_CURRENT_ROW,
  WFX_GROUP,
  WFX_TIES,
  WFX_NO_OTHERS
};

/**
  Cf. SQL 2011 6.10 null treatment
*/
enum enum_null_treatment
{
  NT_NONE,
  NT_RESPECT_NULLS,
  NT_IGNORE_NULLS
};

/**
  Cf. SQL 2011 6.10 from first or last
*/
enum enum_from_first_last
{
  NFL_NONE,
  NFL_FROM_FIRST,
  NFL_FROM_LAST
};

#endif /* WINDOW_LEX_INCLUDED */
