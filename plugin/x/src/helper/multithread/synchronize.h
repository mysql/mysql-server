/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_HELPER_MULTITHREAD_SYNCHRONIZE_H_
#define PLUGIN_X_SRC_HELPER_MULTITHREAD_SYNCHRONIZE_H_

#include "plugin/x/src/helper/multithread/cond.h"
#include "plugin/x/src/helper/multithread/mutex.h"

namespace xpl {

class Synchronize {
 public:
  Synchronize(PSI_mutex_key mutex_key, PSI_cond_key cond_key)
      : m_mutex(mutex_key), m_cond(cond_key) {}

  class Block {
   public:
    ~Block() {
      if (m_release_lock) mysql_mutex_unlock(m_sync->m_mutex);
    }

    Block(Block &&block)
        : m_release_lock(block.m_release_lock), m_sync(block.m_sync) {
      block.m_release_lock = false;
    }

    void wait() { m_sync->m_cond.wait(m_sync->m_mutex); }
    void notify() { m_sync->m_cond.signal(); }

   private:
    friend class Synchronize;

    Block(const Block &) = delete;
    Block(Synchronize *sync) : m_sync(sync) {
      m_release_lock = 0 == mysql_mutex_lock(m_sync->m_mutex);
    }

    Block &operator=(const Block &) = delete;

    bool m_release_lock{false};
    Synchronize *m_sync{nullptr};
  };

  Block block() { return Block(this); }

 private:
  Mutex m_mutex;
  Cond m_cond;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_MULTITHREAD_SYNCHRONIZE_H_
