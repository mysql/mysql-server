/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#ifndef TEST_UTILS_INCLUDED
#define TEST_UTILS_INCLUDED

#include <string.h>
#include <sys/types.h>
#include <memory>
#include <string_view>

#include "gtest/gtest.h"
#include "my_compiler.h"
#include "my_sys.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "sql/error_handler.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"

class THD;
class my_decimal;
struct MEM_ROOT;

namespace my_testing {

inline int native_compare(size_t *length, unsigned char **a,
                          unsigned char **b) {
  return memcmp(*a, *b, *length);
}

inline qsort2_cmp get_ptr_compare(size_t size [[maybe_unused]]) {
  return (qsort2_cmp)native_compare;
}

void setup_server_for_unit_tests();
void teardown_server_for_unit_tests();
int chars_2_decimal(const char *chars, my_decimal *to);

/*
  A class which wraps the necessary setup/teardown logic for
  unit tests which depend on a working THD environment.
 */
class Server_initializer {
 public:
  Server_initializer() : m_thd(nullptr) {}
  ~Server_initializer() { TearDown(); }

  // Invoke these from corresponding functions in test fixture classes.
  void SetUp();
  void TearDown();

  // Sets expected error for error_handler_hook.
  static void set_expected_error(uint val);

  THD *thd() const { return m_thd; }

 private:
  THD *m_thd;
};

/**
   A mock error handler which registers itself with the THD in the CTOR,
   and unregisters in the DTOR. The function handle_condition() will
   verify that it is called with the expected error number.
   The DTOR will verify that handle_condition() has actually been called.
*/
class Mock_error_handler : public Internal_error_handler {
 public:
  Mock_error_handler(THD *thd, uint expected_error);
  ~Mock_error_handler() override;

  bool handle_condition(THD *thd, uint sql_errno, const char *sqlstate,
                        Sql_condition::enum_severity_level *level,
                        const char *msg) override;

  int handle_called() const { return m_handle_called; }

 private:
  THD *m_thd;
  uint m_expected_error;
  int m_handle_called;
};

/*
  A class which wraps the necessary setup/teardown logic for
  Data Dictionary.
*/
class DD_initializer {
 public:
  static void SetUp();
  static void TearDown();
};

/// Enable unstructured trace for the lifetime of this object.
class TraceGuard final {
 public:
  explicit TraceGuard(THD *thd) : m_context{&thd->opt_trace} {
    // Start trace.
    m_context->start(false, true, false, false, 0, 0, 0, 0);
    m_context->set_unstructured_trace(&m_trace);
  }

  // No copying.
  TraceGuard(const TraceGuard &) = delete;
  TraceGuard &operator=(const TraceGuard &) = delete;

  ~TraceGuard() {
    m_context->set_unstructured_trace(nullptr);
    // Stop trace.
    m_context->end();
  }

  const TraceBuffer &contents() {
    return m_context->unstructured_trace()->contents();
  }

 private:
  /// The trace context in which we enable trace.
  Opt_trace_context *m_context;
  /// The trace.
  UnstructuredTrace m_trace;
};

}  // namespace my_testing

/// To allow SCOPED_TRACE(trace_buffer), as this requires
/// "ostream << trace_buffer" to work.
inline std::ostream &operator<<(std::ostream &stream,
                                const TraceBuffer &buffer) {
  buffer.ForEach([&](char ch) { stream << ch; });
  return stream;
}

#endif  // TEST_UTILS_INCLUDED
