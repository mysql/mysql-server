/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef RPL_INFO_VALUES_H
#define RPL_INFO_VALUES_H

#include "my_global.h"

class String;


class Rpl_info_values
{
public:
  Rpl_info_values(int param_ninfo);
  virtual ~Rpl_info_values();

  bool init();

  /** 
    Sequence of values to be read from or stored into a repository.
  */
  String *value;

private:
  /* This property represents the number of fields. */
  int ninfo;

  Rpl_info_values& operator=(const Rpl_info_values& values);
  Rpl_info_values(const Rpl_info_values& values);
};
#endif /* RPL_INFO_VALUES_H */
