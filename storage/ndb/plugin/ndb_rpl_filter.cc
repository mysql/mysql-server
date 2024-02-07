/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

// Implements Ndb_rpl_filter related functionality
#include "storage/ndb/plugin/ndb_rpl_filter.h"

#include "sql/rpl_filter.h"
#include "sql/rpl_rli.h"
#include "sql/sql_class.h"

Ndb_rpl_filter_disable::Ndb_rpl_filter_disable(THD *thd) : m_thd(thd) {
  if (!thd->slave_thread) {
    // Not applier
    return;
  }
  if (!thd->rli_slave->rpl_filter) {
    // No filter
    return;
  }

  thread_local std::unique_ptr<Rpl_filter> empty_rpl_filter;
  if (empty_rpl_filter == nullptr) {
    // Create the empty Rpl_filter instance, will live as long as the thread
    empty_rpl_filter = std::make_unique<Rpl_filter>();
  }

  // Install the empty Rpl_filter. Since the replication filter is read/modified
  // by a single thread during server startup and no command can change it while
  // running, there is no need for lock while applier is running.
  m_save_rpl_filter = thd->rli_slave->rpl_filter;
  thd->rli_slave->rpl_filter = empty_rpl_filter.get();
}

Ndb_rpl_filter_disable::~Ndb_rpl_filter_disable() {
  if (m_save_rpl_filter) {
    m_thd->rli_slave->rpl_filter = m_save_rpl_filter;
  }
}
