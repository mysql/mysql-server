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

#include "jit_executor_context_pool.h"
#include "jit_executor_common_context.h"
#include "jit_executor_javascript_context.h"

#include <memory>
#include <string>
#include <vector>

#include "include/my_thread.h"
#include "jit_executor_common_context.h"
#include "jit_executor_javascript_context.h"

namespace jit_executor {

ContextPool::ContextPool(CommonContext *common_context)
    : m_common_context{common_context} {
  m_release_thread =
      std::make_unique<std::thread>(&ContextPool::release_thread, this);
}

ContextPool::~ContextPool() { teardown(); }

void ContextPool::teardown() {
  do_teardown();

  // Tear down the releaser thread
  release(nullptr);

  if (m_release_thread) {
    m_release_thread->join();
    m_release_thread.reset();
  }
}

std::shared_ptr<PooledContextHandle> ContextPool::get_context() {
  auto ctx = get();

  if (ctx) {
    return std::make_shared<PooledContextHandle>(this, ctx);
  }

  return {};
}

void ContextPool::release(IContext *ctx) {
  m_running_items--;
  if (ctx != nullptr && ctx->is_idle()) {
    do_release(ctx);
  } else {
    m_queued_items++;
    m_release_queue.push(ctx);
  }
}

void ContextPool::release_thread() {
  my_thread_self_setname("Jit-CtxDispose");
  while (true) {
    auto ctx = m_release_queue.pop();
    m_queued_items--;
    if (ctx) {
      if (ctx->wait_for_idle()) {
        do_release(ctx);
      } else {
        discard(ctx);
      }
    } else {
      // nullptr arrived, meaning we are done
      break;
    }
  }
}

bool ContextPool::can_create() {
  return !m_forbid_context_creation &&
         m_common_context->get_heap_usage_percent() < 95;
}

IContext *ContextPool::create(size_t id) {
  auto context = std::make_unique<JavaScriptContext>(id, m_common_context);
  if (!context->started()) {
    // The factory function should throw runtime exception if fails
    throw std::runtime_error("Failed initializing JavaScriptContext");
  }

  return context.release();
}

void ContextPool::destroy(IContext *ctx) {
  try {
    delete ctx;
  } catch (const std::exception &e) {
    log_error("%s", e.what());
  }
}

void ContextPool::increase_active_items(bool created) {
  {
    std::scoped_lock lock(m_mutex);
    m_running_items++;
    if (created) {
      m_active_items++;
      m_created_items++;
    }
    // NOTE: The following commented code is for debugging purposes
    // std::cout << "===> C/A/R/TR: " << m_created_items << "/" <<
    // m_active_items << "/" << m_running_items << "/" << m_queued_items <<
    // std::endl;
  }
}

void ContextPool::decrease_active_items(bool destroyed) {
  {
    std::scoped_lock lock(m_mutex);
    if (destroyed) {
      m_active_items--;
    }
    // NOTE: The following commented code is for debugging purposes
    // std::cout << "===> C/A/R/TR: " << m_created_items << "/" <<
    // m_active_items << "/" << m_running_items << "/" << m_queued_items <<
    // std::endl;
  }
  m_item_availability.notify_all();
}

IContext *ContextPool::get() {
  IContext *item = nullptr;
  std::optional<bool> got_item;
  mysql_harness::ScopedCallback increase([this, &got_item]() {
    if (got_item.has_value()) {
      increase_active_items(*got_item);
    }
  });
  {
    std::unique_lock lock(m_mutex);

    if (m_teardown) return {};

    // If new context can't be created and there are no contexts in the pool,
    // waits for an existing item to be available...
    if (!can_create() && m_items.empty()) {
      m_item_availability.wait(lock, [this]() {
        return m_active_items == 0 || !m_items.empty() || can_create();
      });

      if (m_active_items == 0) {
        throw std::runtime_error(
            "All the contexts on the pool have been released.");
      }
    }

    if (!m_items.empty()) {
      // Pop a resource from the pool
      item = m_items.front();
      m_items.pop_front();
      got_item = false;  // Got but no created
      return item;
    }
  }

  try {
    item = create(m_created_items);
    got_item = true;  // Got and created
    return item;
  } catch (...) {
    // An initialization failure would raise this exception, not seen in Linux
    // as creation is done only when heap is above 95%, otoh in windows, the
    // heap monitoring is not available and the creation is done in the pool, so
    // we need to handle this case, so when this point is reached, no new
    // context is created and existing ones are reused
    m_forbid_context_creation = true;
    return get();
  }
}

void ContextPool::do_release(IContext *ctx) {
  bool destroyed = false;
  mysql_harness::ScopedCallback decrease(
      [this, &destroyed]() { decrease_active_items(destroyed); });
  {
    std::scoped_lock lock(m_mutex);

    if (!m_teardown) {
      m_items.push_back(ctx);
      m_item_availability.notify_one();
      return;
    }
  }

  destroyed = true;
  destroy(ctx);
}

void ContextPool::discard(IContext *ctx) {
  destroy(ctx);
  decrease_active_items(true);
}

void ContextPool::do_teardown() {
  {
    std::scoped_lock lock(m_mutex);
    m_teardown = true;
  }

  while (!m_items.empty()) {
    auto item = m_items.front();
    m_items.pop_front();

    discard(item);
  }

  // Waits until all the contexts created by the pool get released
  std::unique_lock<std::mutex> lock(m_mutex);
  m_item_availability.wait(lock, [this]() { return m_active_items == 0; });
}
}  // namespace jit_executor