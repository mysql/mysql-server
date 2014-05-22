/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include "opt_costmodel.h"


void Cost_model_server::init()
{
#if !defined(DBUG_OFF)
  m_initialized= true;
#endif
}


void Cost_model_table::init(const Cost_model_server *cost_model_server)
{
  DBUG_ASSERT(cost_model_server != NULL);

  m_cost_model_server= cost_model_server;
#if !defined(DBUG_OFF)
  m_initialized= true;
#endif
}
