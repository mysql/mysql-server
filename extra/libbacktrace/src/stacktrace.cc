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

#include <backtrace/stacktrace.hpp>

#include <cstdint>

using stacktrace::callback::error_t;
using stacktrace::callback::full_t;
using stacktrace::callback::simple_t;
using stacktrace::callback::syminfo_t;

extern "C" {
struct backtrace_state;
extern backtrace_state *backtrace_create_state(const char *filename,
                                               int threaded,
                                               error_t error_callback,
                                               void *ctx);
extern int backtrace_full(backtrace_state *state, int skip, full_t callback,
                          error_t error_callback, void *ctx);

extern int backtrace_simple(backtrace_state *state, int skip, simple_t callback,
                            error_t error_callback, void *ctx);
extern int backtrace_pcinfo(backtrace_state *state, uintptr_t pc,
                            full_t callback, error_t error_callback, void *ctx);
extern int backtrace_syminfo(backtrace_state *state, uintptr_t addr,
                             syminfo_t callback, error_t error_callback,
                             void *ctx);
}

namespace {
void err_handler(void *, const char *, int) {}
backtrace_state *init() {
  static auto *state = backtrace_create_state(nullptr, 1, err_handler, nullptr);
  return state;
}
}  // namespace

namespace stacktrace {
void full(int skip, full_t callback, error_t error_callback, void *ctx) {
  auto *state = init();
  backtrace_full(state, skip + 1, callback, error_callback, ctx);
}

void simple(int skip, simple_t callback, error_t error_callback, void *ctx) {
  auto *state = init();
  backtrace_simple(state, skip + 1, callback, error_callback, ctx);
}

void pcinfo(uintptr_t pc, full_t callback, error_t error_callback, void *ctx) {
  auto *state = init();
  backtrace_pcinfo(state, pc, callback, error_callback, ctx);
}

void syminfo(uintptr_t addr, syminfo_t callback, error_t error_callback,
             void *ctx) {
  auto *state = init();
  backtrace_syminfo(state, addr, callback, error_callback, ctx);
}

}  // namespace stacktrace
