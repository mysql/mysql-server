/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/service_pfs_notification.cc
  The performance schema implementation of the notification service.
*/

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/plugin.h>
#include <string.h>
#include <atomic>

#include "my_systime.h"  // my_sleep()
#include "pfs_thread_provider.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_server.h"
#include "storage/perfschema/pfs_services.h"
#include "template_utils.h"

int pfs_get_thread_system_attrs_by_id_vc(PSI_thread *thread,
                                         ulonglong thread_id,
                                         PSI_thread_attrs *thread_attrs);

/**
  Bitmap identifiers for PSI_notification callbacks.
  @sa PSI_notification
*/
#define EVENT_THREAD_CREATE 0x0001
#define EVENT_THREAD_DESTROY 0x0002
#define EVENT_SESSION_CONNECT 0x0004
#define EVENT_SESSION_DISCONNECT 0x0008
#define EVENT_SESSION_CHANGE_USER 0x0010

/**
  PFS_notification_node
  Element of the notification registry containing callback functions.
*/
struct PFS_notification_node {
  PFS_notification_node()
      : m_handle(0),
        m_use_ref_count(false),
        m_refs(0),
        m_next(nullptr),
        m_cb_map(0) {}

  explicit PFS_notification_node(const PSI_notification &cb)
      : m_handle(0),
        m_use_ref_count(false),
        m_refs(0),
        m_next(nullptr),
        m_cb_map(0),
        m_cb(cb) {
    m_cb_map = set_callback_map();
  }

  std::uint32_t set_callback_map();

  /** Registration handle. */
  int m_handle;
  /** True if can be unregistered. */
  bool m_use_ref_count;
  /** Reference count with high bit as enabled flag. */
  std::atomic<std::uint32_t> m_refs;
  /** Next registration. */
  std::atomic<PFS_notification_node *> m_next;
  /** Bitmap of registered callbacks. */
  std::atomic<std::uint32_t> m_cb_map;
  /** Registered callback functions. */
  PSI_notification m_cb;
};

/**
  Build a map of the registered callbacks.
*/
std::uint32_t PFS_notification_node::set_callback_map() {
  std::uint32_t map = 0;

  if (m_cb.thread_create != nullptr) {
    map |= EVENT_THREAD_CREATE;
  }

  if (m_cb.thread_destroy != nullptr) {
    map |= EVENT_THREAD_DESTROY;
  }

  if (m_cb.session_connect != nullptr) {
    map |= EVENT_SESSION_CONNECT;
  }

  if (m_cb.session_disconnect != nullptr) {
    map |= EVENT_SESSION_DISCONNECT;
  }

  if (m_cb.session_change_user != nullptr) {
    map |= EVENT_SESSION_CHANGE_USER;
  }

  return map;
}

/**
  PFS_notification_registry
  A singly linked list of callback registrations. Callbacks can be unregistered,
  although the node is disabled and not removed. Plugins must unregister
  callbacks before unloading. A reference count ensures that the
  callback functions remain valid until unregister is complete.
*/
struct PFS_notification_registry {
  PFS_notification_registry() : m_head(nullptr), m_count(0) {}

  ~PFS_notification_registry() {
    auto *node = m_head.load();
    while (node != nullptr) {
      auto *next = node->m_next.load();
      delete node;
      node = next;
    }
  }

  /**
    Add a new registration.
    @param new_node callback registraion node
    @param use_ref_count true if callbacks can be unregistered
    @return handle of the node
  */
  int add(PFS_notification_node *new_node, bool use_ref_count) {
    if (new_node == nullptr) {
      return 0;
    }

    /* At least one callback required. */
    if (new_node->m_cb_map == 0) {
      return 0;
    }

    new_node->m_handle = ++m_count; /* atomic */
    new_node->m_use_ref_count = use_ref_count;
    auto *local_head = m_head.load();

    /* New node becomes the head of the list. */
    if (local_head == nullptr)
      if (m_head.compare_exchange_strong(local_head, new_node)) {
        return new_node->m_handle;
      }

    while (true) {
      local_head = m_head.load();
      new_node->m_next = local_head;

      if (m_head.compare_exchange_strong(local_head, new_node)) {
        return new_node->m_handle;
      }
    }
  }

  /**
    Disable a node given its handle.
    @param  handle returned by add()
    @return 0 if successful, 1 otherwise
  */
  int disable(int handle) {
    const int max_attempts = 8;
    const time_t timeout = 250000; /* .25s */
    auto *node = m_head.load();

    while (node != nullptr) {
      if (node->m_handle == handle) {
        /* Clear the callback bitmap and mark the node as disabled. */
        node->m_cb_map.store(0);

        /* Permanent registrations can only be disabled. */
        if (!node->m_use_ref_count) {
          return 0;
        }

        /* Get the ref count, mark as FREE. */
        auto refs = node->m_refs.fetch_or(FREE_MASK);

        /* Wait a maximum of 2 seconds for all references to complete. */
        int attempts = 0;
        while ((refs & REFS_MASK)) {
          if (++attempts > max_attempts) {
            return 1;
          }
          my_sleep(timeout);
          refs = node->m_refs.load();
        }
        /* Reset callbacks. */
        memset(&node->m_cb, 0, sizeof(node->m_cb));
        return 0;
      }
      node = node->m_next.load();
    }
    return 1;
  }

  /**
    Get the first active registration.
    @param event_type notification event
    @return callback registration or nullptr
  */
  PFS_notification_node *get_first(int event_type) {
    auto *node = m_head.load();

    while (node != nullptr) {
      /* Is a callback registered for this event? */
      auto cb_map = node->m_cb_map.load();

      if ((cb_map & event_type) != 0) {
        /* No ref count for permanent registrations. */
        if (!node->m_use_ref_count) {
          return node;
        }

        /* Bump ref count, decrement in get_next(). */
        auto refs = node->m_refs.fetch_add(1);

        /* Verify that node is still enabled. */
        if ((refs & FREE_MASK) == 0) {
          return node;
        }

        node->m_refs.fetch_add(-1);
      }
      node = node->m_next.load();
    }
    return nullptr;
  }

  /**
    Get the next active registration.
    @param current node returned from get_first() or get_next()
    @param event_type notification event
    @return callback registration or nullptr
  */
  PFS_notification_node *get_next(PFS_notification_node *current,
                                  int event_type) {
    assert(current != nullptr);

    /* Get the next node, decrement ref count for the current node. */
    auto *next = current->m_next.load();

    if (current->m_use_ref_count) {
      current->m_refs.fetch_add(-1);
    }

    while (next != nullptr) {
      /* Is a callback registered for this event? */
      auto cb_map = next->m_cb_map.load();

      if ((cb_map & event_type) != 0) {
        /* No ref count for permanent registrations. */
        if (!next->m_use_ref_count) {
          return next;
        }

        /* Bump ref count, decrement in next call to get_next(). */
        auto refs = next->m_refs.fetch_add(1);

        /* Verify that node is still enabled. */
        if ((refs & FREE_MASK) == 0) {
          return next;
        }

        next->m_refs.fetch_add(-1);
      }
      next = next->m_next.load();
    }
    return nullptr;
  }

 private:
  static const std::uint32_t REFS_MASK = 0x7FFFFFFF;
  static const std::uint32_t FREE_MASK = 0x80000000;

  std::atomic<PFS_notification_node *> m_head;
  std::atomic<std::uint32_t> m_count;
};

/**
  Notification service registry.
*/
static PFS_notification_registry pfs_notification_registry;

/**
  Register callbacks for the Notification service.
  @param callbacks  block of callback function pointers
  @param with_ref_count true if callbacks can be unregistered
  @return handle unique handle needed to unregister, 0 on failure
  @sa PSI_v1::register_notification
*/
int pfs_register_notification(const PSI_notification *callbacks,
                              bool with_ref_count) {
  if (unlikely(callbacks == nullptr)) {
    return 0;
  }

  return pfs_notification_registry.add(new PFS_notification_node(*callbacks),
                                       with_ref_count);
}

/**
  Unregister callbacks for the Notification service.
  @param handle  unique handle returned by register_notification()
  @return 0 if successful, non-zero otherwise
  @sa PSI_v1::unregister_notification
*/
int pfs_unregister_notification(int handle) {
  return pfs_notification_registry.disable(handle);
}

/**
  Invoke callbacks registered for create thread events.
  This is an internal function, not part of Notification API.
  @param thread  instrumented thread
  @sa pfs_notify_thread_create
*/
void pfs_notify_thread_create(PSI_thread *thread [[maybe_unused]]) {
  auto *node = pfs_notification_registry.get_first(EVENT_THREAD_CREATE);
  if (node == nullptr) {
    return;
  }

  PSI_thread_attrs thread_attrs;

  if (pfs_get_thread_system_attrs_by_id_vc(thread, 0, &thread_attrs) != 0) {
    return;
  }

  while (node != nullptr) {
    auto callback = *node->m_cb.thread_create;
    if (callback != nullptr) {
      callback(&thread_attrs);
    }
    node = pfs_notification_registry.get_next(node, EVENT_THREAD_CREATE);
  }
}

/**
  Invoke callbacks registered for destroy thread events.
  This is an internal function, not part of Notification API.
  @param thread  instrumented thread
  @sa pfs_notify_thread_destroy
*/
void pfs_notify_thread_destroy(PSI_thread *thread [[maybe_unused]]) {
  auto *node = pfs_notification_registry.get_first(EVENT_THREAD_DESTROY);
  if (node == nullptr) {
    return;
  }

  PSI_thread_attrs thread_attrs;

  if (pfs_get_thread_system_attrs_by_id_vc(thread, 0, &thread_attrs) != 0) {
    return;
  }

  while (node != nullptr) {
    auto callback = *node->m_cb.thread_destroy;
    if (callback != nullptr) {
      callback(&thread_attrs);
    }
    node = pfs_notification_registry.get_next(node, EVENT_THREAD_DESTROY);
  }
}

/**
  Invoke callbacks registered for session connect events.
  @param thread  instrumented thread
  @sa PSI_v1::notify_session_connect
*/
void pfs_notify_session_connect(PSI_thread *thread [[maybe_unused]]) {
#ifndef NDEBUG
  {
    auto *pfs = reinterpret_cast<PFS_thread *>(thread);

    if (pfs != nullptr) {
      assert(!pfs->m_debug_session_notified);
      pfs->m_debug_session_notified = true;
    }
  }
#endif

  auto *node = pfs_notification_registry.get_first(EVENT_SESSION_CONNECT);
  if (node == nullptr) {
    return;
  }

  PSI_thread_attrs thread_attrs;

  if (pfs_get_thread_system_attrs_by_id_vc(thread, 0, &thread_attrs) != 0) {
    return;
  }

  while (node != nullptr) {
    auto callback = *node->m_cb.session_connect;
    if (callback != nullptr) {
      callback(&thread_attrs);
    }
    node = pfs_notification_registry.get_next(node, EVENT_SESSION_CONNECT);
  }
}

/**
  Invoke callbacks registered for session disconnect events.
  @param thread  instrumented thread
  @sa PSI_v1::notify_session_disconnect
*/
void pfs_notify_session_disconnect(PSI_thread *thread [[maybe_unused]]) {
#ifndef NDEBUG
  {
    auto *pfs = reinterpret_cast<PFS_thread *>(thread);

    if (pfs != nullptr) {
      // TODO: clean all callers, and enforce
      // assert(pfs->m_debug_session_notified);
      pfs->m_debug_session_notified = false;
    }
  }
#endif

  auto *node = pfs_notification_registry.get_first(EVENT_SESSION_DISCONNECT);
  if (node == nullptr) {
    return;
  }

  PSI_thread_attrs thread_attrs;

  if (pfs_get_thread_system_attrs_by_id_vc(thread, 0, &thread_attrs) != 0) {
    return;
  }

  while (node != nullptr) {
    auto callback = *node->m_cb.session_disconnect;
    if (callback != nullptr) {
      callback(&thread_attrs);
    }
    node = pfs_notification_registry.get_next(node, EVENT_SESSION_DISCONNECT);
  }
}

/**
  Invoke callbacks registered for session change user events.
  @param thread  instrumented thread
  @sa PSI_v1::notify_session_change_user
*/
void pfs_notify_session_change_user(PSI_thread *thread [[maybe_unused]]) {
  auto *node = pfs_notification_registry.get_first(EVENT_SESSION_CHANGE_USER);
  if (node == nullptr) {
    return;
  }

  PSI_thread_attrs thread_attrs;

  if (pfs_get_thread_system_attrs_by_id_vc(thread, 0, &thread_attrs) != 0) {
    return;
  }

  while (node != nullptr) {
    auto callback = *node->m_cb.session_change_user;
    if (callback != nullptr) {
      callback(&thread_attrs);
    }
    node = pfs_notification_registry.get_next(node, EVENT_SESSION_CHANGE_USER);
  }
}

/**
  Notification Service implementation.
*/
int impl_register_notification(const PSI_notification *callbacks,
                               bool with_ref_count) {
  return pfs_register_notification(callbacks, with_ref_count);
}

int impl_unregister_notification(int handle) {
  return pfs_unregister_notification(handle);
}

SERVICE_TYPE(pfs_notification_v3)
SERVICE_IMPLEMENTATION(mysql_server, pfs_notification_v3) = {
    impl_register_notification, impl_unregister_notification};
