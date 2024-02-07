/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  @file sql/histograms/table_histograms.cc
  Table_histograms and Table_histograms_collection implementation.
*/

#include "sql/histograms/table_histograms.h"

#include <array>
#include <cassert>
#include <utility>  // std::move, std::make_pair

#include "map_helpers.h"  // mem_root_unordered_map
#include "my_alloc.h"     // MEM_ROOT
#include "my_compiler.h"

#include "sql/histograms/histogram.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_base.h"  // LOCK_open

// Table_histograms

Table_histograms *Table_histograms::create(PSI_memory_key psi_key) noexcept {
  MEM_ROOT mem_root(psi_key, 512);
  Table_histograms *table_histograms = new (&mem_root) Table_histograms();
  if (table_histograms == nullptr) return nullptr;

  // Move the MEM_ROOT that we used to allocate the object onto the object.
  table_histograms->m_mem_root = std::move(mem_root);

  try {
    // The constructor of mem_root_unordered_map may throw std::bad_alloc.
    table_histograms->m_histograms = new (&table_histograms->m_mem_root)
        mem_root_unordered_map<unsigned int, const histograms::Histogram *>(
            &table_histograms->m_mem_root);
  } catch (const std::bad_alloc &) {
    table_histograms->destroy();
    return nullptr;
  }
  return table_histograms;
}

void Table_histograms::destroy() {
  // Table_histograms is allocated on its own MEM_ROOT m_mem_root. This means
  // that m_mem_root is allocated on itself! The MEM_ROOT destructor calls
  // MEM_ROOT::Clear() which accesses member variables while clearing memory. In
  // order to avoid accessing memory that has been freed, we move-construct the
  // MEM_ROOT in the current function scope before calling Clear() which will
  // free all the memory used by the Table_histograms object. This same pattern
  // is used in TABLE_SHARE::destroy().
  MEM_ROOT own_root = std::move(m_mem_root);
  own_root.Clear();
}

bool Table_histograms::insert_histogram(
    unsigned int field_index, const histograms::Histogram *histogram) {
  const histograms::Histogram *histogram_copy = histogram->clone(&m_mem_root);
  if (histogram_copy == nullptr) return true;
  try {
    bool insertion_happened =
        m_histograms->insert(std::make_pair(field_index, histogram_copy))
            .second;
    if (!insertion_happened) return true;  // Duplicate key.
  } catch (const std::bad_alloc &) {
    return true;  // OOM.
  }
  return false;
}

const histograms::Histogram *Table_histograms::find_histogram(
    unsigned int field_index) const {
  const auto found = m_histograms->find(field_index);
  if (found == m_histograms->end()) return nullptr;
  return found->second;
}

// Table_histograms_collection

Table_histograms_collection::Table_histograms_collection() {
  m_table_histograms.fill(nullptr);
}

Table_histograms_collection::~Table_histograms_collection() {
  for (size_t i = 0; i < kMaxNumberOfTableHistogramsInCollection; ++i) {
    if (m_table_histograms[i]) {
      assert(m_table_histograms[i]->reference_count() == 0);
      free_table_histograms(i);
    }
  }
}

const Table_histograms *Table_histograms_collection::acquire() {
  mysql_mutex_assert_owner(&LOCK_open);
  Table_histograms *current_histograms = m_table_histograms[m_current_index];
  if (current_histograms == nullptr) return nullptr;
  current_histograms->increment_reference_counter();
  return current_histograms;
}

void Table_histograms_collection::release(const Table_histograms *histograms) {
  mysql_mutex_assert_owner(&LOCK_open);
  size_t idx = histograms->get_index();
  assert(m_table_histograms[idx] == histograms);
  m_table_histograms[idx]->decrement_reference_counter();
  if (m_table_histograms[idx]->reference_count() == 0 &&
      idx != m_current_index) {
    free_table_histograms(idx);
  }
}

MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(28020)
bool Table_histograms_collection::insert(Table_histograms *histograms) {
  size_t i = 0;
  while (m_table_histograms[i] != nullptr) {
    ++i;
    if (i == kMaxNumberOfTableHistogramsInCollection) return true;
  }
  // Free the current object if it has a reference count of zero.
  if (m_table_histograms[m_current_index] &&
      m_table_histograms[m_current_index]->reference_count() == 0) {
    free_table_histograms(m_current_index);
  }
  assert(i < kMaxNumberOfTableHistogramsInCollection);
  m_table_histograms[i] = histograms;
  m_table_histograms[i]->set_index(i);
  m_current_index = i;
  return false;
}
MY_COMPILER_DIAGNOSTIC_POP()

size_t Table_histograms_collection::size() const {
  size_t size = 0;
  for (size_t i = 0; i < kMaxNumberOfTableHistogramsInCollection; ++i) {
    if (m_table_histograms[i]) ++size;
  }
  return size;
}

int Table_histograms_collection::total_reference_count() const {
  int count = 0;
  for (size_t i = 0; i < kMaxNumberOfTableHistogramsInCollection; ++i) {
    if (m_table_histograms[i])
      count += m_table_histograms[i]->reference_count();
  }
  return count;
}
