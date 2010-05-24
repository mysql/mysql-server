/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef RPL_INFO_FIELDS_H
#define RPL_INFO_FIELDS_H

#include <my_global.h>
#include <m_string.h>

struct info_fields
{
  LEX_STRING use;
  LEX_STRING saved;
  int size;
} typedef info_fields;

class Rpl_info_fields
{
public:

  Rpl_info_fields(int param_ninfo): field(0), 
    ninfo(param_ninfo) { };
  virtual ~Rpl_info_fields();

  bool configure();
  bool resize(int needed_size, int pos);

  info_fields *field;

private:
  int ninfo;

  Rpl_info_fields& operator=(const Rpl_info_fields& fields);
  Rpl_info_fields(const Rpl_info_fields& fields);
};
#endif /* RPL_INFO_FIELDS_H */
