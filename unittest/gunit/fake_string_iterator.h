#ifndef UNITTEST_GUNIT_FAKE_STRING_ITERATOR_H_
#define UNITTEST_GUNIT_FAKE_STRING_ITERATOR_H_

/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#include <optional>
#include <random>
#include <string>
#include <vector>

#include "m_ctype.h"
#include "sql/field.h"
#include "sql/iterators/row_iterator.h"
#include "sql/sql_class.h"

// An implementation of a RowIterator that contains a user defined set of
// string values, without having to set up the entire SQL machinery. It works
// just like any other iterator; you call Init() before doing any reads, and you
// call Read() until it returns -1 (no more data).
//
// The iterator is expected to only work on a single-column table, and the
// column must be given to the iterator constructor.
class FakeStringIterator final : public TableRowIterator {
 public:
  FakeStringIterator(THD *thd, TABLE *table, Field_varstring *field,
                     std::vector<std::optional<std::string>> dataset)
      : TableRowIterator(thd, table),
        m_field(field),
        m_dataset(std::move(dataset)) {}

  bool Init() override {
    m_current_index = 0;
    return false;
  }

  int Read() override {
    if (m_current_index == m_dataset.size()) {
      return -1;
    }

    const std::optional<std::string> &value = m_dataset[m_current_index++];
    if (value.has_value()) {
      m_field->store(value->c_str(), value->size(),
                     &my_charset_utf8mb4_0900_ai_ci);
      m_field->set_notnull();
    } else {
      m_field->set_null();
    }

    return 0;
  }

 private:
  size_t m_current_index{0};
  Field_varstring *m_field;
  std::vector<std::optional<std::string>> m_dataset;
};

#endif  // UNITTEST_GUNIT_FAKE_STRING_ITERATOR_H_
