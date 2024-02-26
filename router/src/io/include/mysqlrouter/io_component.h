/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_IO_COMPONENT_INCLUDED
#define ROUTER_IO_COMPONENT_INCLUDED

#include <list>
#include <memory>  // make_unique
#include <string>

#include "mysql/harness/net_ts/executor.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysqlrouter/io_thread.h"

#include "mysqlrouter/io_component_export.h"

enum class IoComponentErrc {
  already_initialized = 1,
  unknown_backend = 2,
};

IO_COMPONENT_EXPORT std::error_code make_error_code(IoComponentErrc ec);

class IO_COMPONENT_EXPORT IoComponent {
 public:
  std::list<IoThread> &io_threads() { return io_threads_; }

  /**
   * get ref to the io_context.
   *
   * behaviour is undefined when called before IoComponent::init() and after
   * IoComponent::reset()
   */
  net::io_context &io_context();

  static IoComponent &get_instance();

  /**
   * initialize io-component.
   *
   * use IoComponent::reset() to reset the io-component into its initial state
   *
   * @param num_worker_threads number of worker-threads to spawn
   * @param backend_name name of the io-backend
   *
   * @see IoComponent::reset()
   */
  stdx::expected<void, std::error_code> init(size_t num_worker_threads,
                                             const std::string &backend_name);

  /**
   * run the main loop of the io-component.
   *
   * runs until all no more work is assigned to the mainloop or stopped.
   *
   * @see IoComponent::stop()
   */
  void run();

  void stop();

  /**
   * reset the io_component into its initial state.
   *
   * when calling reset() no io-thread SHALL run which can be achieved by
   *
   * - calling stop() after run() was called.
   * - not calling run()
   */
  void reset();

  std::string backend_name() const { return backend_name_; }

  class Workguard {
   public:
    Workguard(IoComponent &io_comp)
        : io_comp_{io_comp},
          io_ctx_work_guard_{net::make_work_guard(io_comp.io_context())} {
      ++io_comp_.users_;
    }

    Workguard(const Workguard &) = delete;
    Workguard(Workguard &&) = delete;
    Workguard &operator=(const Workguard &) = delete;
    Workguard &operator=(Workguard &&) = delete;

    ~Workguard() {
      if (--io_comp_.users_ == 0) {
        io_comp_.stop();
      }
    }

   private:
    IoComponent &io_comp_;
    net::executor_work_guard<net::io_context::executor_type> io_ctx_work_guard_;
  };

  Workguard work_guard();

 private:
  IoComponent() = default;

  std::list<IoThread> io_threads_;

  std::unique_ptr<net::io_context> io_ctx_;

  std::atomic<int> users_{};

  std::string backend_name_;
};

#endif
