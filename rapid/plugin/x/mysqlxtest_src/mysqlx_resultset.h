/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef _MYSQLX_RESULTSET_H_
#define _MYSQLX_RESULTSET_H_

#include <stdexcept>
#include <vector>
#include <map>
#include <set>

#include "ngs_common/xdatetime.h"
#include "mysqlx_protocol.h"


namespace Mysqlx
{
  namespace Resultset
  {
    class Row;
  }
}

namespace mysqlx
{
  enum FieldType
  {
    SINT,
    UINT,

    DOUBLE,
    FLOAT,

    BYTES,

    TIME,
    DATETIME,
    SET,
    ENUM,
    BIT,
    DECIMAL
  };

  struct ColumnMetadata
  {
    FieldType type;
    std::string name;
    std::string original_name;

    std::string table;
    std::string original_table;

    std::string schema;
    std::string catalog;

    uint64_t collation;

    uint32_t fractional_digits;

    uint32_t length;

    uint32_t flags;
    uint32_t content_type;
  };

  class Row
  {
  public:
    ~Row();

    bool isNullField(int field) const;
    int32_t sIntField(int field) const;
    uint32_t uIntField(int field) const;
    int64_t sInt64Field(int field) const;
    uint64_t uInt64Field(int field) const;
    uint64_t bitField(int field) const;
    std::string stringField(int field) const;
    std::string decimalField(int field) const;
    std::string setFieldStr(int field) const;
    std::set<std::string> setField(int field) const;
    std::string enumField(int field) const;
    const char *stringField(int field, size_t &rlength) const;
    float floatField(int field) const;
    double doubleField(int field) const;
    DateTime dateTimeField(int field) const;
    Time timeField(int field) const;

    int numFields() const;

  private:
    friend class Result;
    Row(ngs::shared_ptr<std::vector<ColumnMetadata> > columns, Mysqlx::Resultset::Row *data);

    void check_field(int field, FieldType type) const;

    ngs::shared_ptr<std::vector<ColumnMetadata> > m_columns;
    Mysqlx::Resultset::Row *m_data;
  };

  class ResultData
  {
  public:
    ResultData(ngs::shared_ptr<std::vector<ColumnMetadata> > columns);
    ngs::shared_ptr<std::vector<ColumnMetadata> > columnMetadata(){ return m_columns; }
    void add_row(ngs::shared_ptr<Row> row);
    void rewind();
    void tell(size_t &record);
    void seek(size_t record);
    ngs::shared_ptr<Row> next();
  private:
    ngs::shared_ptr<std::vector<ColumnMetadata> > m_columns;
    std::vector<ngs::shared_ptr<Row> > m_rows;
    size_t m_row_index;
  };

  class Result
  {
  public:
    ~Result();

    ngs::shared_ptr<std::vector<ColumnMetadata> > columnMetadata();
    int64_t lastInsertId() const { return m_last_insert_id; }
    std::string lastDocumentId();
    const std::vector<std::string>& lastDocumentIds();
    int64_t affectedRows() const { return m_affected_rows; }
    std::string infoMessage() const { return m_info_message; }

    bool ready();
    void wait();

    ngs::shared_ptr<Row> next();
    bool nextDataSet();
    void flush();

    Result& buffer();

    // Return true if the operation was successfully executed
    bool rewind();
    bool tell(size_t &dataset, size_t&record);
    bool seek(size_t dataset, size_t record);

    bool has_data();

    void mark_error();

    struct Warning
    {
      std::string text;
      int code;
      bool is_note;
    };
    const std::vector<Warning> &getWarnings() const { return m_warnings; }
    void setLastDocumentIDs(const std::vector<std::string>& document_ids);
  private:
    Result();
    Result(const Result &o);
    Result(ngs::shared_ptr<XProtocol>owner, bool expect_data, bool expect_ok = true);

    void read_metadata();
    ngs::shared_ptr<Row> read_row();
    void read_stmt_ok();

    bool handle_notice(int32_t type, const std::string &data);

    int get_message_id();
    mysqlx::Message* pop_message();

    mysqlx::Message* current_message;
    int              current_message_id;

    friend class XProtocol;
    ngs::weak_ptr<XProtocol>m_owner;
    ngs::shared_ptr<std::vector<ColumnMetadata> > m_columns;
    int64_t m_last_insert_id;
    std::vector<std::string> m_last_document_ids;
    int64_t m_affected_rows;
    std::string m_info_message;

    std::vector<Warning> m_warnings;

    std::vector<ngs::shared_ptr<ResultData> > m_result_cache;
    ngs::shared_ptr<ResultData> m_current_result;
    size_t m_result_index;

    enum {
      ReadStmtOkI, // initial state
      ReadMetadataI, // initial state
      ReadMetadata,
      ReadRows,
      ReadStmtOk,
      ReadDone, // end
      ReadError // end
    } m_state;

    bool m_buffered;
    bool m_buffering;
    bool m_has_doc_ids;
  };
}

#endif
