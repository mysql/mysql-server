/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _MYSQLX_CONNECTOR_H_
#define _MYSQLX_CONNECTOR_H_

#include <stdexcept>
#include <vector>
#include <map>
#include <set>

#include "ngs_common/xdatetime.h"
#include "mysqlx_common.h"

#include <boost/enable_shared_from_this.hpp>

namespace Mysqlx
{
  namespace Resultset
  {
    class Row;
  }
}

namespace google { namespace protobuf { class Message; template <typename Element> class RepeatedPtrField; } }

namespace mysqlx
{
  typedef google::protobuf::Message Message;

  bool parse_mysql_connstring(const std::string &connstring,
    std::string &protocol, std::string &user, std::string &password,
    std::string &host, int &port, std::string &sock,
    std::string &db, int &pwd_found);

  class Result;

  class Error : public std::runtime_error
  {
  public:
    Error(int error = 0, const std::string &message = "");
    virtual ~Error() BOOST_NOEXCEPT_OR_NOTHROW;
    int error() const { return _error; }

  private:
    std::string _message;
    int _error;
  };

  class Schema;
  class Connection;
  struct Ssl_config;

  class ArgumentValue
  {
  public:
    enum Type
    {
      TInteger,
      TUInteger,
      TNull,
      TDouble,
      TFloat,
      TBool,
      TString,
      TOctets,
    };

    ArgumentValue(const ArgumentValue &other)
    {
      m_type = other.m_type;
      m_value = other.m_value;
      if (m_type == TString || m_type == TOctets)
        m_value.s = new std::string(*other.m_value.s);
    }

    ArgumentValue &operator = (const ArgumentValue &other)
    {
      m_type = other.m_type;
      m_value = other.m_value;
      if (m_type == TString || m_type == TOctets)
        m_value.s = new std::string(*other.m_value.s);

      return *this;
    }

    explicit ArgumentValue(const std::string &s, Type type = TString)
    {
      m_type = type;
      m_value.s = new std::string(s);
    }

    explicit ArgumentValue(int64_t n)
    {
      m_type = TInteger;
      m_value.i = n;
    }

    explicit ArgumentValue(uint64_t n)
    {
      m_type = TUInteger;
      m_value.ui = n;
    }

    explicit ArgumentValue(double n)
    {
      m_type = TDouble;
      m_value.d = n;
    }

    explicit ArgumentValue(float n)
    {
      m_type = TFloat;
      m_value.f = n;
    }

    explicit ArgumentValue(bool n)
    {
      m_type = TBool;
      m_value.b = n;
    }

    explicit ArgumentValue()
    {
      m_type = TNull;
    }

    ~ArgumentValue()
    {
      if (m_type == TString || m_type == TOctets)
        delete m_value.s;
    }

    inline Type type() const { return m_type; }

    inline operator uint64_t () const
    {
      if (m_type != TUInteger)
        throw std::logic_error("type error");
      return m_value.ui;
    }

    inline operator int64_t () const
    {
      if (m_type != TInteger)
        throw std::logic_error("type error");
      return m_value.i;
    }

    inline operator double() const
    {
      if (m_type != TDouble)
        throw std::logic_error("type error");
      return m_value.d;
    }

    inline operator float() const
    {
      if (m_type != TFloat)
        throw std::logic_error("type error");
      return m_value.f;
    }

    inline operator bool() const
    {
      if (m_type != TBool)
        throw std::logic_error("type error");
      return m_value.b;
    }

    inline operator const std::string & () const
    {
      if (m_type != TString && m_type != TOctets)
        throw std::logic_error("type error");
      return *m_value.s;
    }

  private:
    Type m_type;
    union
    {
      std::string *s;
      int64_t i;
      uint64_t ui;
      double d;
      float f;
      bool b;
    } m_value;
  };

  class Session : public boost::enable_shared_from_this < Session >
  {
  public:
    Session(const mysqlx::Ssl_config &ssl_config, const std::size_t timeout);
    ~Session();
    boost::shared_ptr<Result> executeSql(const std::string &sql);

    boost::shared_ptr<Result> executeStmt(const std::string &ns, const std::string &stmt,
                                          const std::vector<ArgumentValue> &args);

    boost::shared_ptr<Schema> getSchema(const std::string &name);

    boost::shared_ptr<Connection> connection() { return m_connection; }

    void close();
  private:
    boost::shared_ptr<Connection> m_connection;
    std::map<std::string, boost::shared_ptr<Schema> > m_schemas;
  };
  typedef boost::shared_ptr<Session> SessionRef;

  SessionRef openSession(const std::string &uri, const std::string &pass, const mysqlx::Ssl_config &ssl_config,
                         const bool cap_expired_password, const std::size_t timeout, const bool get_caps = false);
  SessionRef openSession(const std::string &host, int port, const std::string &schema,
                         const std::string &user, const std::string &pass,
                         const mysqlx::Ssl_config &ssl_config, const std::size_t timeout,
                         const std::string &auth_method = "", const bool get_caps = false);

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

  struct MYSQLXTEST_PUBLIC ColumnMetadata
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

  class Document
  {
  public:
    explicit Document();
    explicit Document(const std::string &doc, bool expression = false, const std::string& id = "");
    Document(const Document &doc);

    std::string &str() const { return *m_data; }
    std::string id() const { return m_id; }
    bool is_expression() const { return m_expression; }
    void reset(const std::string &doc, bool expression = false, const std::string &id = "");

  private:
    boost::shared_ptr<std::string> m_data;
    bool m_expression;
    std::string m_id;
  };

  class MYSQLXTEST_PUBLIC Row
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
    Row(boost::shared_ptr<std::vector<ColumnMetadata> > columns, Mysqlx::Resultset::Row *data);

    void check_field(int field, FieldType type) const;

    boost::shared_ptr<std::vector<ColumnMetadata> > m_columns;
    Mysqlx::Resultset::Row *m_data;
  };

  class MYSQLXTEST_PUBLIC ResultData
  {
  public:
    ResultData(boost::shared_ptr<std::vector<ColumnMetadata> > columns);
    boost::shared_ptr<std::vector<ColumnMetadata> > columnMetadata(){ return m_columns; }
    void add_row(boost::shared_ptr<Row> row);
    void rewind();
    void tell(size_t &record);
    void seek(size_t record);
    boost::shared_ptr<Row> next();
  private:
    boost::shared_ptr<std::vector<ColumnMetadata> > m_columns;
    std::vector<boost::shared_ptr<Row> > m_rows;
    size_t m_row_index;
  };

  class MYSQLXTEST_PUBLIC Result
  {
  public:
    ~Result();

    boost::shared_ptr<std::vector<ColumnMetadata> > columnMetadata();
    int64_t lastInsertId() const { return m_last_insert_id; }
    std::string lastDocumentId();
    const std::vector<std::string>& lastDocumentIds();
    int64_t affectedRows() const { return m_affected_rows; }
    std::string infoMessage() const { return m_info_message; }

    bool ready();
    void wait();

    boost::shared_ptr<Row> next();
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
    Result(boost::shared_ptr<Connection>owner, bool expect_data, bool expect_ok = true);

    void read_metadata();
    boost::shared_ptr<Row> read_row();
    void read_stmt_ok();

    bool handle_notice(int32_t type, const std::string &data);

    int get_message_id();
    mysqlx::Message* pop_message();

    mysqlx::Message* current_message;
    int              current_message_id;

    friend class Connection;
    boost::weak_ptr<Connection>m_owner;
    boost::shared_ptr<std::vector<ColumnMetadata> > m_columns;
    int64_t m_last_insert_id;
    std::vector<std::string> m_last_document_ids;
    int64_t m_affected_rows;
    std::string m_info_message;

    std::vector<Warning> m_warnings;

    std::vector<boost::shared_ptr<ResultData> > m_result_cache;
    boost::shared_ptr<ResultData> m_current_result;
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
};

#endif
