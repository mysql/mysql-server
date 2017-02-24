/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

// Avoid warnings from includes of other project and protobuf
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4018 4996)
#endif

#include "ngs_common/protocol_protobuf.h"
#include "mysqlx_resultset.h"
#include "mysqlx_protocol.h"
#include "mysqlx_row.h"
#include "mysqlx_error.h"

#include "my_config.h"

#include "ngs_common/bind.h"
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

#ifdef WIN32
#pragma warning(push, 0)
#endif
#ifdef WIN32
#pragma warning(pop)
#endif
#include <string>
#include <iostream>
#include <limits>
#include "ngs_common/xdecimal.h"

#ifdef WIN32
#  pragma push_macro("ERROR")
#  undef ERROR
#endif


using namespace mysqlx;

static void throw_server_error(const Mysqlx::Error &error)
{
  throw Error(error.code(), error.msg());
}

Result::Result(ngs::shared_ptr<XProtocol>owner, bool expect_data, bool expect_ok)
: current_message(NULL), m_owner(owner), m_last_insert_id(-1), m_affected_rows(-1),
  m_result_index(0), m_state(expect_data ? ReadMetadataI :  expect_ok ? ReadStmtOkI : ReadDone), m_buffered(false), m_buffering(false)
{
}

Result::Result()
: current_message(NULL), m_state(ReadDone), m_buffered(false), m_buffering(false)
{
}

Result::~Result()
{
  // flush the resultset from the pipe
  while (m_state != ReadError && m_state != ReadDone)
    nextDataSet();

  delete current_message;
}

ngs::shared_ptr<std::vector<ColumnMetadata> > Result::columnMetadata()
{
  // If cached, works with the cache data
  if (m_buffered)
    return m_current_result->columnMetadata();
  else
  {
  if (m_state == ReadMetadataI)
    read_metadata();
  }
  return m_columns;
}

bool Result::ready()
{
  // if we've read something (ie not on initial state), then we're ready
  return m_state != ReadMetadataI && m_state != ReadStmtOkI;
}

void Result::wait()
{
  if (m_state == ReadMetadataI)
    read_metadata();
  if (m_state == ReadStmtOkI)
    read_stmt_ok();
}

void Result::mark_error()
{
  m_state = ReadError;
}

bool Result::handle_notice(int32_t type, const std::string &data)
{
  switch (type)
  {
    case 1: // warning
    {
      Mysqlx::Notice::Warning warning;
      warning.ParseFromString(data);
      if (!warning.IsInitialized())
        std::cerr << "Invalid notice received from server " << warning.InitializationErrorString() << "\n";
      else
      {
        Warning w;
        w.code = warning.code();
        w.text = warning.msg();
        w.is_note = warning.level() == Mysqlx::Notice::Warning::NOTE;
        m_warnings.push_back(w);
      }
      return true;
    }

    case 2: // session variable changed
      break;

    case 3: //session state changed
    {
      Mysqlx::Notice::SessionStateChanged change;
      change.ParseFromString(data);
      if (!change.IsInitialized())
        std::cerr << "Invalid notice received from server " << change.InitializationErrorString() << "\n";
      else
      {
        switch (change.param())
        {
          case Mysqlx::Notice::SessionStateChanged::GENERATED_INSERT_ID:
            if (change.value().type() == Mysqlx::Datatypes::Scalar::V_UINT)
              m_last_insert_id = change.value().v_unsigned_int();
            else
              std::cerr << "Invalid notice value received from server: " << data << "\n";
            break;

          case Mysqlx::Notice::SessionStateChanged::ROWS_AFFECTED:
            if (change.value().type() == Mysqlx::Datatypes::Scalar::V_UINT)
              m_affected_rows = change.value().v_unsigned_int();
            else
              std::cerr << "Invalid notice value received from server: " << data << "\n";
            break;

          case Mysqlx::Notice::SessionStateChanged::PRODUCED_MESSAGE:
            if (change.value().type() == Mysqlx::Datatypes::Scalar::V_STRING)
              m_info_message = change.value().v_string().value();
            else
              std::cerr << "Invalid notice value received from server: " << data << "\n";
            break;

          default:
            return false;
        }
      }
      return true;
    }
    default:
      std::cerr << "Unexpected notice type received " << type << "\n";
      return false;
  }
  return false;
}

int Result::get_message_id()
{
  if (NULL != current_message)
  {
    return current_message_id;
  }

  ngs::shared_ptr<XProtocol>owner = m_owner.lock();

  if (owner)
  {
    owner->push_local_notice_handler(ngs::bind(&Result::handle_notice, this, ngs::placeholders::_1, ngs::placeholders::_2));

    try
    {
        current_message = owner->recv_next(current_message_id);
    }
    catch (...)
    {
      m_state = ReadError;
        owner->pop_local_notice_handler();
      throw;
    }

    owner->pop_local_notice_handler();
  }

  // error messages that can be received in any state
  if (current_message_id == Mysqlx::ServerMessages::ERROR)
  {
    m_state = ReadError;
    throw_server_error(static_cast<const Mysqlx::Error&>(*current_message));
  }

  switch (m_state)
  {
    case ReadMetadataI:
    {
      switch (current_message_id)
      {
        case Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK:
          m_state = ReadDone;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA:
          m_state = ReadMetadata;
          return current_message_id;
      }
      break;
    }
    case ReadMetadata:
    {
      // while reading metadata, we can either get more metadata
      // start getting rows (which also signals end of metadata)
      // or EORows, which signals end of metadata AND empty resultset
      switch (current_message_id)
      {
      case Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA:
          m_state = ReadMetadata;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_ROW:
          m_state = ReadRows;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE:
          // empty resultset
          m_state = ReadStmtOk;
          return current_message_id;
      }
      break;
    }
    case ReadRows:
    {
      switch (current_message_id)
      {
        case Mysqlx::ServerMessages::RESULTSET_ROW:
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE:
          m_state = ReadStmtOk;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS:
          m_state = ReadMetadata;
          return current_message_id;
      }
      break;
    }
    case ReadStmtOkI:
    case ReadStmtOk:
    {
      switch (current_message_id)
      {
        case Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK:
          m_state = ReadDone;
          return current_message_id;
      }
      break;
    }
    case ReadError:
    case ReadDone:
      // not supposed to reach here
      throw std::logic_error("attempt to read data at wrong time");
  }

  if (getenv("MYSQLX_DEBUG"))
  {
    std::string out;
    google::protobuf::TextFormat::PrintToString(*current_message, &out);
    std::cout << out << "\n";
  }
  m_state = ReadError;
  throw Error(CR_COMMANDS_OUT_OF_SYNC, "Unexpected message received from server reading results");
}

mysqlx::Message* Result::pop_message()
{
  mysqlx::Message *result = current_message;

  current_message = NULL;

  return result;
}

std::string Result::lastDocumentId()
{
  // Last document id is only available on collection add operations
  // and only if a single document is added (MY-139 Spec, Req 4, 6)
  if (!m_has_doc_ids || m_last_document_ids.size() != 1)
    throw std::logic_error("document id is not available.");

  return m_last_document_ids.at(0);
}

const std::vector<std::string>& Result::lastDocumentIds()
{
  // Last document ids are available on any collection add operation (MY-139 Spec, Req 1-5)
  if (!m_has_doc_ids)
    throw std::logic_error("document ids are not available.");

  return m_last_document_ids;
}

void Result::setLastDocumentIDs(const std::vector<std::string>& document_ids)
{
  m_has_doc_ids = true;
  m_last_document_ids.reserve(document_ids.size());
  std::copy(document_ids.begin(), document_ids.end(), std::back_inserter(m_last_document_ids));
}

static ColumnMetadata unwrap_column_metadata(const Mysqlx::Resultset::ColumnMetaData &column_data)
{
  ColumnMetadata column;

  switch (column_data.type())
  {
    case Mysqlx::Resultset::ColumnMetaData::SINT:
      column.type = mysqlx::SINT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::UINT:
      column.type = mysqlx::UINT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::DOUBLE:
      column.type = mysqlx::DOUBLE;
      break;
    case Mysqlx::Resultset::ColumnMetaData::FLOAT:
      column.type = mysqlx::FLOAT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::BYTES:
      column.type = mysqlx::BYTES;
      break;
    case Mysqlx::Resultset::ColumnMetaData::TIME:
      column.type = mysqlx::TIME;
      break;
    case Mysqlx::Resultset::ColumnMetaData::DATETIME:
      column.type = mysqlx::DATETIME;
      break;
    case Mysqlx::Resultset::ColumnMetaData::SET:
      column.type = mysqlx::SET;
      break;
    case Mysqlx::Resultset::ColumnMetaData::ENUM:
      column.type = mysqlx::ENUM;
      break;
    case Mysqlx::Resultset::ColumnMetaData::BIT:
      column.type = mysqlx::BIT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::DECIMAL:
      column.type = mysqlx::DECIMAL;
      break;
  }
  column.name = column_data.name();
  column.original_name = column_data.original_name();

  column.table = column_data.table();
  column.original_table = column_data.original_table();

  column.schema = column_data.schema();
  column.catalog = column_data.catalog();

  column.collation = column_data.has_collation() ? column_data.collation() : 0;

  column.fractional_digits = column_data.fractional_digits();

  column.length = column_data.length();

  column.flags = column_data.flags();
  column.content_type = column_data.content_type();
  return column;
}

void Result::read_metadata()
{
  if (m_state != ReadMetadata && m_state != ReadMetadataI)
    throw std::logic_error("read_metadata() called at wrong time");

  // msgs we can get in this state:
  // CURSOR_OK
  // META_DATA

  int msgid = -1;
  m_columns.reset(new std::vector<ColumnMetadata>());
  while (m_state == ReadMetadata || m_state == ReadMetadataI)
  {
    if (-1 != msgid)
    {
      delete pop_message();
    }

    msgid = get_message_id();

    if (msgid == Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA)
    {
      msgid = -1;
      ngs::unique_ptr<Mysqlx::Resultset::ColumnMetaData> column_data(static_cast<Mysqlx::Resultset::ColumnMetaData*>(pop_message()));

      m_columns->push_back(unwrap_column_metadata(*column_data));
    }
  }
}

ngs::shared_ptr<Row> Result::read_row()
{
  ngs::shared_ptr<Row> ret_val;

  if (m_state != ReadRows)
    throw std::logic_error("read_row() called at wrong time");

  // msgs we can get in this state:
  // RESULTSET_ROW
  // RESULTSET_FETCH_DONE
  // RESULTSET_FETCH_DONE_MORE_RESULTSETS
  int mid = get_message_id();

  if (mid == Mysqlx::ServerMessages::RESULTSET_ROW)
  {
    ret_val.reset(new Row(m_columns, static_cast<Mysqlx::Resultset::Row*>(pop_message())));

    // If caching adds it to the cache instead
    if (m_buffering)
      m_current_result->add_row(ret_val);
  }

  return ret_val;
}

void Result::read_stmt_ok()
{
  if (m_state != ReadStmtOk && m_state != ReadStmtOkI)
    throw std::logic_error("read_stmt_ok() called at wrong time");

  // msgs we can get in this state:
  // STMT_EXEC_OK

  if (Mysqlx::ServerMessages::RESULTSET_FETCH_DONE == get_message_id())
    delete pop_message();

  if (Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK != get_message_id())
    throw std::runtime_error("Unexpected message id");

  ngs::unique_ptr<mysqlx::Message> msg(pop_message());
}

bool Result::rewind()
{
  bool ret_val = false;
  if (m_buffered)
  {
    for (m_result_index = 0; m_result_index < m_result_cache.size(); m_result_index++)
      m_result_cache[m_result_index]->rewind();

    m_result_index = 0;
    nextDataSet();

    ret_val = true;
  }

  return ret_val;
}

bool Result::tell(size_t &dataset, size_t&record)
{
  bool ret_val = false;

  if (m_buffered && m_current_result)
  {
    dataset = m_result_index;
    m_current_result->tell(record);
    ret_val = true;
  }

  return ret_val;
}

bool Result::seek(size_t dataset, size_t record)
{
  bool ret_val = false;

  if (m_buffered)
  {
    rewind();

    while (dataset < m_result_index)
      nextDataSet();

    m_current_result->seek(record);

    ret_val = true;
  }

  return ret_val;
}

bool Result::has_data()
{
  bool ret_val = false;

  if (m_buffered)
    ret_val = m_current_result->columnMetadata() && m_current_result->columnMetadata()->size() > 0;
  else
    ret_val = m_columns && m_columns->size() > 0;

  return ret_val;
}

bool Result::nextDataSet()
{
  if (m_buffered)
  {
    if (m_result_index < m_result_cache.size())
      m_current_result = m_result_cache[m_result_index++];
    else
      m_current_result.reset();

    return m_current_result ? true : false;
  }
  else
  {
  // flush left over rows
  while (m_state == ReadRows)
    read_row();

  if (m_state == ReadMetadata)
  {
    read_metadata();
    if (m_state == ReadRows)
      {
        // If caching adds this new resultset to the cache
        if (m_buffering)
        {
          m_current_result.reset(new ResultData(m_columns));
          m_result_cache.push_back(m_current_result);
        }
      return true;
  }
    }
  if (m_state == ReadStmtOk)
    read_stmt_ok();
  }
  return false;
}

ngs::shared_ptr<Row> Result::next()
{
  ngs::shared_ptr<Row> ret_val;

  if (m_buffered)
    ret_val = m_current_result->next();
  else
  {
    if (!ready())
      wait();

    if (m_state == ReadStmtOk)
      read_stmt_ok();

    if (m_state != ReadDone)
    {
      ret_val = read_row();

      if (m_state == ReadStmtOk)
        read_stmt_ok();
    }
  }

  return ret_val;
}

// Flush will read all the messages from the IO
// If caching is enabled the data will be cached, if not
// it will be just discarded
void Result::flush()
{
  // Flushes the leftover data only if it was not previously cached
  wait();
  while (nextDataSet());
}

Result& Result::buffer()
{
  if (!ready())
  wait();

  // The buffer makes sense ONLY if there's something else
  // to be buffered
  if (m_state != ReadDone)
  {
    m_buffering = true;

    // This will enable data caching
    m_current_result.reset(new ResultData(m_columns));
    m_result_cache.push_back(m_current_result);

    // This will actually cache the data
    while (nextDataSet())
      ;

    m_buffering = false;
    m_buffered = true;

    m_result_index = 1;
  }

  return *this;
}

ResultData::ResultData(ngs::shared_ptr<std::vector<ColumnMetadata> > columns) :
m_columns(columns), m_row_index(0)
{
}

void ResultData::add_row(ngs::shared_ptr<Row> row)
{
  m_rows.push_back(row);
}

ngs::shared_ptr<Row> ResultData::next()
{
  ngs::shared_ptr<Row> ret_val;

  if (m_row_index < m_rows.size())
    ret_val = m_rows[m_row_index++];

  return ret_val;
}

void ResultData::rewind()
{
  m_row_index = 0;
}

void ResultData::tell(size_t &record)
{
  record = m_row_index;
}

void ResultData::seek(size_t record)
{
  m_row_index = m_rows.size();

  if (record < m_row_index)
    m_row_index = record;
}

Row::Row(ngs::shared_ptr<std::vector<ColumnMetadata> > columns, Mysqlx::Resultset::Row *data)
: m_columns(columns), m_data(data)
{
}

Row::~Row()
{
  delete m_data;
}

void Row::check_field(int field, FieldType type) const
{
  if (field < 0 || field >= (int)m_columns->size())
    throw std::range_error("invalid field index");

  if (m_columns->at(field).type != type)
    throw std::range_error("invalid field type");
}

bool Row::isNullField(int field) const
{
  if (field < 0 || field >= (int)m_columns->size())
    throw std::range_error("invalid field index");

  if (m_data->field(field).empty())
    return true;
  return false;
}

int32_t Row::sIntField(int field) const
{
  int64_t t = sInt64Field(field);
  if (t > std::numeric_limits<int32_t>::max() || t < std::numeric_limits<int32_t>::min())
    throw std::invalid_argument("field of wrong type");

  return (int32_t)t;
}

uint32_t Row::uIntField(int field) const
{
  uint64_t t = uInt64Field(field);
  if (t > std::numeric_limits<uint32_t>::max())
    throw std::invalid_argument("field of wrong type");

  return (uint32_t)t;
}

int64_t Row::sInt64Field(int field) const
{
  check_field(field, SINT);
  const std::string& field_val = m_data->field(field);

  return Row_decoder::s64_from_buffer(field_val);
}

uint64_t Row::uInt64Field(int field) const
{
  check_field(field, UINT);
  const std::string& field_val = m_data->field(field);

  return Row_decoder::u64_from_buffer(field_val);
}

uint64_t Row::bitField(int field) const
{
  check_field(field, BIT);
  const std::string& field_val = m_data->field(field);

  return Row_decoder::u64_from_buffer(field_val);
}

std::string Row::stringField(int field) const
{
  size_t length;
  check_field(field, BYTES);

  const std::string& field_val = m_data->field(field);

  const char* res = Row_decoder::string_from_buffer(field_val, length);
  return std::string(res, length);
}

std::string Row::decimalField(int field) const
{
  check_field(field, DECIMAL);

  const std::string& field_val = m_data->field(field);

  mysqlx::Decimal decimal = Row_decoder::decimal_from_buffer(field_val);

  return std::string(decimal.str());
}

std::string Row::setFieldStr(int field) const
{
  check_field(field, SET);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::set_from_buffer_as_str(field_val);
}

std::set<std::string> Row::setField(int field) const
{
  std::set<std::string> result;
  check_field(field, SET);

  const std::string& field_val = m_data->field(field);
  Row_decoder::set_from_buffer(field_val, result);

  return result;
}

std::string Row::enumField(int field) const
{
  size_t length;
  check_field(field, ENUM);

  const std::string& field_val = m_data->field(field);

  const char* res = Row_decoder::string_from_buffer(field_val, length);
  return std::string(res, length);
}

const char *Row::stringField(int field, size_t &rlength) const
{
  check_field(field, BYTES);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::string_from_buffer(field_val, rlength);
}

float Row::floatField(int field) const
{
  check_field(field, FLOAT);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::float_from_buffer(field_val);
}

double Row::doubleField(int field) const
{
  check_field(field, DOUBLE);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::double_from_buffer(field_val);
}

DateTime Row::dateTimeField(int field) const
{
  check_field(field, DATETIME);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::datetime_from_buffer(field_val);
}

Time Row::timeField(int field) const
{
  check_field(field, TIME);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::time_from_buffer(field_val);
}

int Row::numFields() const
{
  return m_data->field_size();
}

#ifdef WIN32
#  pragma pop_macro("ERROR")
#endif
