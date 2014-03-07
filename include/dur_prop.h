/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _my_dur_prop_h
#define _my_dur_prop_h

enum durability_properties
{
  /*
    Preserves the durability properties defined by the engine
  */
  HA_REGULAR_DURABILITY= 0,
  /*
     Ignore the durability properties defined by the engine and
     write only in-memory entries.
  */
  HA_IGNORE_DURABILITY= 1
};

#endif /* _my_dur_prop_h */
