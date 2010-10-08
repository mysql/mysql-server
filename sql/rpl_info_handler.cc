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
#include <sql_priv.h>
#include "rpl_info_handler.h"

Rpl_info_handler::Rpl_info_handler(const int nparam)
  :ninfo(nparam), cursor((my_off_t)0),
  prv_error(0), sync_counter(0), sync_period(0)
{  
  /* Nothing to do here. */
}

Rpl_info_handler::~Rpl_info_handler()
{
  /* Nothing to do here. */
}

void Rpl_info_handler::set_sync_period(uint period)
{
  sync_period= period; 
}
