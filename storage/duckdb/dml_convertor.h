/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/
#pragma once

#include "ddl_convertor.h"

void append_field_value_to_sql(String &target_str, const Field *field);

class DMLConvertor : public BaseConvertor {
 public:
  DMLConvertor(TABLE *table) : m_table(table) {}

  bool check() override { return false; }

  std::string translate() override;

 protected:
  // Implemented in Insert, Update and Delete
  virtual void generate_prefix(String &query [[maybe_unused]]) = 0;

  // Implemented in Insert and Update. Empty function for Delete
  virtual void generate_fields_and_values(String &query [[maybe_unused]]) {}

  // Generate where clause for Update and Delete
  virtual void generate_where_clause(String &query [[maybe_unused]]);

  // Overided in Update and Delete, called when generate where clause
  virtual void append_where_value(String &query [[maybe_unused]],
                                  Field *field [[maybe_unused]]) {}

  TABLE *m_table;

 private:
  void fill_index_fields_for_where(std::vector<Field *> &fields);
};

class InsertConvertor : public DMLConvertor {
 public:
  InsertConvertor(TABLE *table, bool flag)
      : DMLConvertor(table), idempotent_flag(flag) {}

 protected:
  void generate_prefix(String &query) override;

  void generate_fields_and_values(String &query) override;

  void generate_where_clause(String &query [[maybe_unused]]) override {}

 private:
  bool idempotent_flag;
};

class UpdateConvertor : public DMLConvertor {
 public:
  UpdateConvertor(TABLE *table, const uchar *old_row)
      : DMLConvertor(table), m_old_row(old_row) {}

 protected:
  void generate_prefix(String &query) override;

  void generate_fields_and_values(String &query) override;

  void append_where_value(String &query, Field *field) override {
    uchar *saved_ptr = field->field_ptr();
    field->set_field_ptr(
        const_cast<uchar *>(m_old_row + field->offset(m_table->record[0])));
    append_field_value_to_sql(query, field);
    field->set_field_ptr(saved_ptr);
  }

 private:
  const uchar *m_old_row;
};

class DeleteConvertor : public DMLConvertor {
 public:
  DeleteConvertor(TABLE *table, const uchar *old_row = nullptr)
      : DMLConvertor(table), m_old_row(old_row) {}

 protected:
  void generate_prefix(String &query) override;

  void append_where_value(String &query, Field *field) override {
    if (!m_old_row) {
      append_field_value_to_sql(query, field);
    } else {
      uchar *saved_ptr = field->field_ptr();
      field->set_field_ptr(
          const_cast<uchar *>(m_old_row + field->offset(m_table->record[0])));
      append_field_value_to_sql(query, field);
      field->set_field_ptr(saved_ptr);
    }
  }

 private:
  const uchar *m_old_row;
};
