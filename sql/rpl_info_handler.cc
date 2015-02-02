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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "rpl_info_handler.h"

#include "rpl_info_values.h"   // Rpl_info_values


Rpl_info_handler::Rpl_info_handler(const int nparam)
  :field_values(0), ninfo(nparam), cursor((my_off_t)0),
  prv_error(0), sync_counter(0), sync_period(0)
{  
  field_values= new Rpl_info_values(ninfo);
  /*
    Configures fields to temporary hold information. If the configuration
    fails due to memory allocation problems, the object is deleted.
  */
  if (field_values && field_values->init())
  {
    delete field_values;
    field_values= 0;
  }
}

Rpl_info_handler::~Rpl_info_handler()
{
  delete field_values;
}

void Rpl_info_handler::set_sync_period(uint period)
{
  sync_period= period; 
}

const char* Rpl_info_handler::get_rpl_info_type_str()
{
  switch(do_get_rpl_info_type())
  {
  case INFO_REPOSITORY_DUMMY:  return "DUMMY";
  case INFO_REPOSITORY_FILE:   return "FILE";
  case INFO_REPOSITORY_TABLE:  return "TABLE";
  }

  DBUG_ASSERT(0);
  return "";
}

