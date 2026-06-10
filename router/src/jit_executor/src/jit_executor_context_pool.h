/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CONTEXT_POOL_H_
#define ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CONTEXT_POOL_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/mpsc_queue.h"
#include "mysql/harness/scoped_callback.h"
#include "mysqlrouter/jit_executor_context.h"
#include "mysqlrouter/jit_executor_context_handle.h"
#include "mysqlrouter/jit_executor_value.h"
#include "mysqlrouter/polyglot_file_system.h"

namespace jit_executor {

IMPORT_LOG_FUNCTIONS()

class PooledContextHandle;
class CommonContext;
class ContextPool final {
 public:
  ContextPool(CommonContext *common_context);

  ~ContextPool();

  std::shared_ptr<PooledContextHandle> get_context();
  void teardown();

 private:
  friend class PooledContextHandle;
  void release(IContext *ctx);
  void release_thread();

  void increase_active_items(bool created);
  void decrease_active_items(bool destroyed);

  bool can_create();
  IContext *create();
  void destroy(IContext *ctx);

  void do_release(IContext *ctx);
  void discard(IContext *ctx);
  void do_teardown();
  IContext *create(size_t id);
  IContext *get();

  CommonContext *m_common_context;
  mysql_harness::WaitingMPSCQueue<IContext *> m_release_queue;
  std::unique_ptr<std::thread> m_release_thread;

  std::mutex m_mutex;
  std::condition_variable m_item_availability;
  bool m_teardown = false;
  std::deque<IContext *> m_items;
  size_t m_created_items = 0;               // total created contexts
  size_t m_active_items = 0;                // alive contexts
  std::atomic<size_t> m_running_items = 0;  // contexts in use
  std::atomic<size_t> m_queued_items = 0;   // contexts in the release queue

  std::atomic_bool m_forbid_context_creation = false;
};

/**
 * A wrapper that will return a context to the pool as soon as it is released
 */
class PooledContextHandle : public IContextHandle {
 public:
  PooledContextHandle(ContextPool *pool, IContext *ctx)
      : m_pool{pool}, m_context{ctx} {}
  ~PooledContextHandle() override { m_pool->release(m_context); }

  IContext *get() override { return m_context; }

 private:
  ContextPool *m_pool;
  IContext *m_context;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_CONTEXT_POOL_H_