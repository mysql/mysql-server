/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include <string>
#include "sql/dd/impl/tables/tables.h"
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/string_type.h"
#include "unittest/gunit/benchmark.h"

namespace {

/**
  A dummy class that is instantiated with or without PFS instrumentation
  depending on the template parameter. We can also set the number of children
  using a template parameter. The children will be allocated dynamically, and
  get the same PFS instrumentation as their parent. Note that the children
  will not get their own children; there is only one level.

  @tparam with_pfs   Whether to use PFS instrumented memory management or not.
  @tparam n_children Number of children to be allocated.
*/
template <bool with_pfs, size_t n_children>
class Dummy_object : public dd::Weak_object_impl_<with_pfs> {
 private:
  // Some compilers reject zero sized arrays, so make room for at least one.
  Dummy_object<with_pfs, 0> *m_children[n_children ? n_children : 1];

 public:
  Dummy_object() {
    // Skip allocating children if n_children is 0 - in this case, the single
    // element in the array is left uninitialized.
    if (n_children > 0)
      for (auto &child : m_children) child = new Dummy_object<with_pfs, 0>();
  }
  ~Dummy_object() {
    if (n_children > 0)
      for (auto &child : m_children) delete child;
  }

  // Dummy definitions needed to make the class non-abstract.
  virtual void debug_print(dd::String_type &) const {}
  virtual const dd::Object_table &object_table() const {
    return dd::tables::Tables::instance();
  }
  virtual bool validate() const { return false; }
  virtual bool restore_attributes(const dd::Raw_record &) { return false; }
  virtual bool store_attributes(dd::Raw_record *) { return false; }
  virtual bool has_new_primary_key() const { return false; }
  virtual dd::Object_key *create_primary_key() const { return nullptr; }
};

/**
  Do a number of iterations where we allocate and free an instance of the
  dummy class to measure and compare the time used. A sequence of alloc/free
  with PFS instrumentation seems to typically take around 30% more time than
  without such instrumentation.

  @tparam with_pfs   Whether to use PFS instrumented memory management or not.
  @tparam n_children Number of children to be allocated.
*/
template <bool with_pfs, size_t n_children>
void BM_DD_pfs(size_t num_iterations) {
  StopBenchmarkTiming();
  StartBenchmarkTiming();
  for (size_t n = 0; n < num_iterations; n++) {
    Dummy_object<with_pfs, n_children> *dummy =
        new Dummy_object<with_pfs, n_children>();
    delete dummy;
  }
  StopBenchmarkTiming();
}

}  // namespace

/**
  Provide some closures instantiating the template function above to
  associate somewhat meaningful names with the test cases.
*/
auto W_PFS_0 = [](size_t num_iterations) {
  BM_DD_pfs<true, 0>(num_iterations);
};

auto WO_PFS_0 = [](size_t num_iterations) {
  BM_DD_pfs<false, 0>(num_iterations);
};

auto W_PFS_10 = [](size_t num_iterations) {
  BM_DD_pfs<true, 10>(num_iterations);
};

auto WO_PFS_10 = [](size_t num_iterations) {
  BM_DD_pfs<false, 10>(num_iterations);
};

auto W_PFS_100 = [](size_t num_iterations) {
  BM_DD_pfs<true, 100>(num_iterations);
};

auto WO_PFS_100 = [](size_t num_iterations) {
  BM_DD_pfs<false, 100>(num_iterations);
};

BENCHMARK(W_PFS_0)
BENCHMARK(WO_PFS_0)

BENCHMARK(W_PFS_10)
BENCHMARK(WO_PFS_10)

BENCHMARK(W_PFS_100)
BENCHMARK(WO_PFS_100)
