/*
   Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/opt_costmodel.h"

#include <assert.h>

#include "sql/handler.h"
#include "sql/opt_costconstantcache.h"  // Cost_constant_cache
#include "sql/table.h"                  // TABLE

extern Cost_constant_cache *cost_constant_cache;  // defined in
                                                  // opt_costconstantcache.cc

Cost_model_server::~Cost_model_server() {
  if (m_cost_constants) {
    if (cost_constant_cache) {
      cost_constant_cache->release_cost_constants(m_cost_constants);
    }
    m_cost_constants = nullptr;
  }
}

void Cost_model_server::init(Optimizer optimizer) {
  if (cost_constant_cache && m_server_cost_constants == nullptr) {
    // Get the current set of cost constants
    switch (optimizer) {
      case Optimizer::kOriginal:
        m_cost_constants = cost_constant_cache->get_cost_constants();
        break;
      case Optimizer::kHypergraph:
        m_cost_constants = cost_constant_cache->get_cost_constants_hypergraph();
        break;
    }
    assert(m_cost_constants != nullptr);

    // Get the cost constants for server operations
    m_server_cost_constants = m_cost_constants->get_server_cost_constants();
    assert(m_server_cost_constants != nullptr);

#if !defined(NDEBUG)
    m_initialized = true;
#endif
  }
}

void Cost_model_table::init(const Cost_model_server *cost_model_server,
                            const TABLE *table) {
  assert(cost_model_server != nullptr);
  assert(table != nullptr);

  m_cost_model_server = cost_model_server;
  m_table = table;

  // Find the cost constant object to be used for this table
  m_se_cost_constants =
      m_cost_model_server->get_cost_constants()->get_se_cost_constants(table);
  assert(m_se_cost_constants != nullptr);

#if !defined(NDEBUG)
  m_initialized = true;
#endif
}

double Cost_model_table::page_read_cost(double pages) const {
  assert(m_initialized);
  assert(pages >= 0.0);

  const double in_mem = m_table->file->table_in_memory_estimate();

  const double pages_in_mem = pages * in_mem;
  const double pages_on_disk = pages - pages_in_mem;
  assert(pages_on_disk >= 0.0);

  const double cost =
      buffer_block_read_cost(pages_in_mem) + io_block_read_cost(pages_on_disk);

  return cost;
}

double Cost_model_table::page_read_cost_index(uint index, double pages) const {
  assert(m_initialized);
  assert(pages >= 0.0);

  const double in_mem = m_table->file->index_in_memory_estimate(index);

  const double pages_in_mem = pages * in_mem;
  const double pages_on_disk = pages - pages_in_mem;

  const double cost =
      buffer_block_read_cost(pages_in_mem) + io_block_read_cost(pages_on_disk);

  return cost;
}
