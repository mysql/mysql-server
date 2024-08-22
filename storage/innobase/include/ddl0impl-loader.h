/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ddl0impl-loader.h
 DDL index loader interface.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_loader_h
#define ddl0impl_loader_h

#include "ddl0fts.h"
#include "ddl0impl-buffer.h"
#include "ut0mpmcbq.h"

namespace ddl {

/** Build indexes on a table by reading a clustered index, creating a temporary
file containing index entries, merge sorting these index entries and inserting
sorted index entries to indexes. */
class Loader {
 public:
  /** Builder task. */
  struct Task {
    /** Constructor. */
    Task() = default;

    /** Constructor.
    @param[in,out] builder        Builder that performs the operation. */
    explicit Task(Builder *builder) : m_builder(builder) {}

    /** Constructor.
    @param[in,out] builder        Builder that performs the operation.
    @param[in] thread_id          Index value of the thread_state to work on. */
    explicit Task(Builder *builder, size_t thread_id)
        : m_builder(builder), m_thread_id(thread_id) {}

    /** Do the operation.
    @return DB_SUCCESS or error code. */
    [[nodiscard]] dberr_t operator()() noexcept;

   private:
    /** Builder instance. */
    Builder *m_builder{};

    /** Thread state index. */
    size_t m_thread_id{std::numeric_limits<size_t>::max()};

    friend class Loader;
  };

  // Forward declaration
  class Task_queue;

  /** Constructor.
  @param[in,out] ctx            DDL context. */
  explicit Loader(ddl::Context &ctx) noexcept;

  /** Destructor. */
  ~Loader() noexcept;

  /** Build the read instance.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build_all() noexcept;

  /** Add a task to the task queue.
  @param[in] task               Task to add. */
  void add_task(Task task) noexcept;

  /** Validate the indexes (except FTS).
  @return true on success. */
  [[nodiscard]] bool validate_indexes() const noexcept;

 private:
  /** Prepare to build and load the indexes.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t prepare() noexcept;

  /** Load the indexes.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t load() noexcept;

  /** Scan and build the indexes.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t scan_and_build_indexes() noexcept;

 private:
  /** DDL context, shared by the loader threads. */
  ddl::Context &m_ctx;

  /** Index builders. */
  Builders m_builders{};

  /** Task queue. */
  Task_queue *m_taskq{};
};

}  // namespace ddl

#endif /* !ddl0impl_loader_h */
