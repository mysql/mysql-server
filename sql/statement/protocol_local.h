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

#ifndef SQL_PROTOCOL_LOCAL_H
#define SQL_PROTOCOL_LOCAL_H

#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/sql_list.h"

class Ed_connection;

/** One result set column. */

struct Ed_column final : public LEX_STRING {
  /** Implementation note: destructor for this class is never called. */
};

/** One result set record. */

class Ed_row final {
 public:
  const Ed_column &operator[](const unsigned int column_index) const {
    return *get_column(column_index);
  }
  const Ed_column *get_column(const unsigned int column_index) const {
    assert(column_index < size());
    return m_column_array + column_index;
  }
  size_t size() const { return m_column_count; }

  Ed_row(Ed_column *column_array_arg, size_t column_count_arg)
      : m_column_array(column_array_arg), m_column_count(column_count_arg) {}

 private:
  Ed_column *m_column_array;
  size_t m_column_count;
};

/**
  Ed_result_set -- a container with result set rows.
  @todo Implement support for result set metadata and
  automatic type conversion.
*/

class Ed_result_set final {
 public:
  operator List<Ed_row> &() { return *m_rows; }
  unsigned int size() const { return m_rows->elements; }
  Ed_row *get_fields() { return m_fields; }

  Ed_result_set(List<Ed_row> *rows_arg, Ed_row *fields, size_t column_count,
                MEM_ROOT *mem_root_arg);

  /** We don't call member destructors, they all are POD types. */
  ~Ed_result_set() = default;

  size_t get_field_count() const { return m_column_count; }

  static void *operator new(size_t size, MEM_ROOT *mem_root,
                            const std::nothrow_t & = std::nothrow) noexcept {
    return mem_root->Alloc(size);
  }

  static void operator delete(void *, size_t) noexcept {
    // Does nothing because m_mem_root is deallocated in the destructor
  }

  static void operator delete(
      void *, MEM_ROOT *, const std::nothrow_t &) noexcept { /* never called */
  }

 private:
  Ed_result_set(const Ed_result_set &);      /* not implemented */
  Ed_result_set &operator=(Ed_result_set &); /* not implemented */
 private:
  MEM_ROOT m_mem_root;
  size_t m_column_count;
  List<Ed_row> *m_rows;
  Ed_row *m_fields;
  Ed_result_set *m_next_rset;
  friend class Ed_connection;
};

/**
  Protocol_local: a helper class to intercept the result
  of the data written to the network.

  At the start of every result set, start_result_metadata allocates m_rset to
  prepare for the results. The metadata is stored on m_current_row which will
  be transferred to m_fields in end_result_metadata. The memory for the
  metadata is allocated on m_rset_root.

  Then, for every row of the result received, each of the fields is stored in
  m_current_row. Then the row is moved to m_rset and m_current_row is cleared
  to receive the next row. The memory for all the results are also stored in
  m_rset_root.

  Finally, at the end of the result set, a new instance of Ed_result_set is
  created on m_rset_root and the result set (m_rset and m_fields) is moved into
  this instance. The ownership of MEM_ROOT m_rset_root is also transferred to
  this instance. So, at the end we have a fresh MEM_ROOT, cleared m_rset and
  m_fields to accept the next result set.
*/

class Protocol_local final : public Protocol {
 public:
  Protocol_local(THD *thd, Ed_connection *ed_connection);
  ~Protocol_local() override { m_rset_root.Clear(); }

  int read_packet() override;

  int get_command(COM_DATA *com_data, enum_server_command *cmd) override;
  ulong get_client_capabilities() override;
  bool has_client_capability(unsigned long client_capability) override;
  void end_partial_result_set() override;
  int shutdown(bool server_shutdown = false) override;
  bool connection_alive() const override;
  void start_row() override;
  bool end_row() override;
  void abort_row() override {}
  uint get_rw_status() override;
  bool get_compression() override;

  char *get_compression_algorithm() override;
  uint get_compression_level() override;

  bool start_result_metadata(uint num_cols, uint flags,
                             const CHARSET_INFO *resultcs) override;
  bool end_result_metadata() override;
  bool send_field_metadata(Send_field *field,
                           const CHARSET_INFO *charset) override;
  bool flush() override { return true; }
  bool send_parameters(List<Item_param> *, bool) override { return false; }
  bool store_ps_status(ulong, uint, uint, ulong) override { return false; }

 protected:
  bool store_null() override;
  bool store_tiny(longlong from, uint32) override;
  bool store_short(longlong from, uint32) override;
  bool store_long(longlong from, uint32) override;
  bool store_longlong(longlong from, bool unsigned_flag, uint32) override;
  bool store_decimal(const my_decimal *, uint, uint) override;
  bool store_string(const char *from, size_t length,
                    const CHARSET_INFO *cs) override;
  bool store_datetime(const MYSQL_TIME &time, uint precision) override;
  bool store_date(const MYSQL_TIME &time) override;
  bool store_time(const MYSQL_TIME &time, uint precision) override;
  bool store_float(float value, uint32 decimals, uint32 zerofill) override;
  bool store_double(double value, uint32 decimals, uint32 zerofill) override;
  bool store_field(const Field *field) override;

  enum enum_protocol_type type() const override { return PROTOCOL_LOCAL; }
  enum enum_vio_type connection_type() const override { return VIO_TYPE_LOCAL; }

  bool send_ok(uint server_status, uint statement_warn_count,
               ulonglong affected_rows, ulonglong last_insert_id,
               const char *message) override;

  bool send_eof(uint server_status, uint statement_warn_count) override;
  bool send_error(uint sql_errno, const char *err_msg,
                  const char *sqlstate) override;

 private:
  bool store_string(const char *str, size_t length, const CHARSET_INFO *src_cs,
                    const CHARSET_INFO *dst_cs);

  bool store_column(const void *data, size_t length);
  void opt_add_row_to_rset();

  Ed_connection *m_connection;
  MEM_ROOT m_rset_root;
  List<Ed_row> *m_rset;
  size_t m_column_count;
  Ed_column *m_current_row;
  Ed_column *m_current_column;
  Ed_row *m_fields;
  bool m_send_metadata;
  THD *m_thd;
};

#endif
