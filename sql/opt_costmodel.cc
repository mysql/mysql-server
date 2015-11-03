/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "opt_costconstantcache.h"              // Cost_constant_cache
#include "table.h"                              // TABLE

extern Cost_constant_cache *cost_constant_cache;// defined in
                                                // opt_costconstantcache.cc


Cost_model_server::~Cost_model_server()
{
  if (m_cost_constants)
  {
    cost_constant_cache->release_cost_constants(m_cost_constants);
    m_cost_constants= NULL;
  }
}


void Cost_model_server::init()
{
  if (cost_constant_cache && m_server_cost_constants == NULL)
  {
    // Get the current set of cost constants
    m_cost_constants= cost_constant_cache->get_cost_constants();
    DBUG_ASSERT(m_cost_constants != NULL);

    // Get the cost constants for server operations
    m_server_cost_constants= m_cost_constants->get_server_cost_constants();
    DBUG_ASSERT(m_server_cost_constants != NULL);

#if !defined(DBUG_OFF)
    m_initialized= true;
#endif
  }
}


void Cost_model_table::init(const Cost_model_server *cost_model_server,
                            const TABLE *table)
{
  DBUG_ASSERT(cost_model_server != NULL);
  DBUG_ASSERT(table != NULL);

  m_cost_model_server= cost_model_server;
  m_table= table;

  // Find the cost constant object to be used for this table
  m_se_cost_constants=
    m_cost_model_server->get_cost_constants()->get_se_cost_constants(table);
  DBUG_ASSERT(m_se_cost_constants != NULL);

#if !defined(DBUG_OFF)
  m_initialized= true;
#endif
}


double Cost_model_table::page_read_cost(double pages) const
{
  DBUG_ASSERT(m_initialized);
  DBUG_ASSERT(pages >= 0.0);

  const double in_mem= m_table->file->table_in_memory_estimate();

  const double pages_in_mem= pages * in_mem;
  const double pages_on_disk= pages - pages_in_mem;
  DBUG_ASSERT(pages_on_disk >= 0.0);

  const double cost= buffer_block_read_cost(pages_in_mem) +
    io_block_read_cost(pages_on_disk);

  return cost;
}


double Cost_model_table::page_read_cost_index(uint index, double pages) const
{
  DBUG_ASSERT(m_initialized);
  DBUG_ASSERT(pages >= 0.0);

  double in_mem= m_table->file->index_in_memory_estimate(index);

  const double pages_in_mem= pages * in_mem;
  const double pages_on_disk= pages - pages_in_mem;

  const double cost= buffer_block_read_cost(pages_in_mem) +
    io_block_read_cost(pages_on_disk);

  return cost;
}
