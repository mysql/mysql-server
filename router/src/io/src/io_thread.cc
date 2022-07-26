/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/io_thread.h"

#include <string>

#include "my_thread.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/thread_affinity.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;

void IoThread::operator()() {
  if (cpu_affinity_.any()) {
    auto res = ThreadAffinity(thr_.native_handle()).affinity(cpu_affinity_);
    if (!res) {
      if (res.error() != make_error_code(std::errc::not_supported)) {
        // in case of failure, log it, but ignore it otherwise
        log_info("failed to set cpu affinity for io-thread 'io-%zu': %s", ndx_,
                 res.error().message().c_str());
      }
    }
  }
  // ensure the .run() doesn't exit by itself as it has no more work
  auto work_guard = net::make_work_guard(io_ctx_);

  my_thread_self_setname(("io-"s + std::to_string(ndx_)).c_str());
  io_ctx_.run();
}
