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

#include <my_global.h>
#include <mysql_priv.h>
#include "rpl_info_handler.h"

Rpl_info_handler::Rpl_info_handler(const int nparam)
  :ninfo(nparam), cursor((my_off_t)0), prv_error(0),
  sync_counter(0), sync_period(0), field_values(0)
{  
  field_values= new Rpl_info_fields(ninfo);
  /*
    Configures fields to temporary hold information. If the configuration
    fails due to memory allocation problems, the object is deleted.
  */
  if (field_values && field_values->configure())
  {
    delete field_values;
    field_values= 0;
  }
}

Rpl_info_handler::~Rpl_info_handler()
{
  if (field_values)
  {
    delete field_values;
  }
}

void Rpl_info_handler::set_sync_period(uint period)
{
  sync_period= period; 
}

bool Rpl_info_handler::set_info(const char *value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_set_info(cursor, value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::set_info(ulong const value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_set_info(cursor, value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::set_info(int const value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_set_info(cursor, value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::set_info(float const value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_set_info(cursor, value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::set_info(my_off_t const value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_set_info(cursor, value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::set_info(const Server_ids *value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_set_info(cursor, value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::get_info(char *value, const size_t size,
                                const char *default_value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_get_info(cursor, value, size, default_value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::get_info(ulong *value,
                                ulong const default_value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_get_info(cursor, value, default_value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::get_info(int *value,
                                int const default_value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_get_info(cursor, value, default_value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::get_info(float *value,
                                float const default_value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_get_info(cursor, value, default_value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::get_info(my_off_t *value,
                                my_off_t const default_value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_get_info(cursor, value, default_value)))
    cursor++;

  return(prv_error);
}

bool Rpl_info_handler::get_info(Server_ids *value,
                                const Server_ids *default_value)
{
  if (cursor >= ninfo || prv_error)
    return TRUE;

  if (!(prv_error= do_get_info(cursor, value, default_value)))
    cursor++;

  return(prv_error);
}
