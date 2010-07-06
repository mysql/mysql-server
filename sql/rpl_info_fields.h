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

/*
  Structure that represents a field as a LEX_STRING plus a size.

  The "use" and "saved" points to the same space in memory but as the
  structure can be used to read and store information from a variaty
  of repositories, it is possible that the reference to the allocated
  space is lost. In such cases, we use the reference in the saved ptr
  to restore the original values.
*/
struct info_field
{
  LEX_STRING use;
  LEX_STRING saved;
  int size;
} typedef info_field;

class Rpl_info_fields
{
public:
  Rpl_info_fields(int param_ninfo): field(0), 
    ninfo(param_ninfo) { };
  virtual ~Rpl_info_fields();

  bool init();
  bool resize(int needed_size, int pos);
  void restore();

  /* Sequence of fields to be read or stored from a repository. */
  info_field *field;

private:
  /* This property represents the number of fields. */
  int ninfo;

  Rpl_info_fields& operator=(const Rpl_info_fields& fields);
  Rpl_info_fields(const Rpl_info_fields& fields);
};
#endif /* RPL_INFO_FIELDS_H */
