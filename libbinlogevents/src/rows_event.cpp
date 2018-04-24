/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "rows_event.h"

#include <stdlib.h>
#include <cstring>
#include <string>

#include "event_reader_macros.h"

namespace binary_log {
/**
  Get the length of next field.
  Change parameter to point at fieldstart.

  @param  packet pointer to a buffer containing the field in a row.
  @return pos    length of the next field
*/
static unsigned long get_field_length(unsigned char **packet) {
  unsigned char *pos = *packet;
  uint32_t temp = 0;
  if (*pos < 251) {
    (*packet)++;
    return *pos;
  }
  if (*pos == 251) {
    (*packet)++;
    return ((unsigned long)~0);  // NULL_LENGTH;
  }
  if (*pos == 252) {
    (*packet) += 3;
    memcpy(&temp, pos + 1, 2);
    temp = le32toh(temp);
    return (unsigned long)temp;
  }
  if (*pos == 253) {
    (*packet) += 4;
    memcpy(&temp, pos + 1, 3);
    temp = le32toh(temp);
    return (unsigned long)temp;
  }
  (*packet) += 9; /* Must be 254 when here */
  memcpy(&temp, pos + 1, 4);
  temp = le32toh(temp);
  return (unsigned long)temp;
}

Table_map_event::Table_map_event(const char *buf,
                                 const Format_description_event *fde)
    : Binary_log_event(&buf, fde),
      m_table_id(0),
      m_flags(0),
      m_data_size(0),
      m_dbnam(""),
      m_dblen(0),
      m_tblnam(""),
      m_tbllen(0),
      m_colcnt(0),
      m_coltype(nullptr),
      m_field_metadata_size(0),
      m_field_metadata(nullptr),
      m_null_bits(nullptr),
      m_optional_metadata_len(0),
      m_optional_metadata(nullptr) {
  BAPI_ENTER("Table_map_event::Table_map_event(const char*, ...)");
  const char *ptr_dbnam = nullptr;
  const char *ptr_tblnam = nullptr;
  READER_TRY_INITIALIZATION;
  READER_ASSERT_POSITION(fde->common_header_len);

  /* Read the post-header */

  READER_TRY_CALL(forward, TM_MAPID_OFFSET);
  if (fde->post_header_len[TABLE_MAP_EVENT - 1] == 6) {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    READER_TRY_SET(m_table_id, read_and_letoh<uint64_t>, 4);
  } else {
    BAPI_ASSERT(fde->post_header_len[TABLE_MAP_EVENT - 1] ==
                TABLE_MAP_HEADER_LEN);
    READER_TRY_SET(m_table_id, read_and_letoh<uint64_t>, 6);
  }
  READER_TRY_SET(m_flags, read_and_letoh<uint16_t>);

  /* Read the variable part of the event */

  READER_TRY_SET(m_dblen, read<uint8_t>);
  ptr_dbnam = READER_TRY_CALL(ptr, m_dblen + 1);
  m_dbnam = std::string(ptr_dbnam, m_dblen);

  READER_TRY_SET(m_tbllen, read<uint8_t>);
  ptr_tblnam = READER_TRY_CALL(ptr, m_tbllen + 1);
  m_tblnam = std::string(ptr_tblnam, m_tbllen);

  READER_TRY_SET(m_colcnt, net_field_length_ll);
  READER_TRY_CALL(alloc_and_memcpy, &m_coltype, m_colcnt, 16);

  if (READER_CALL(available_to_read) > 0) {
    READER_TRY_SET(m_field_metadata_size, net_field_length_ll);
    if (m_field_metadata_size > (m_colcnt * 2))
      READER_THROW("Invalid m_field_metadata_size");
    unsigned int num_null_bytes = (m_colcnt + 7) / 8;
    READER_TRY_CALL(alloc_and_memcpy, &m_field_metadata, m_field_metadata_size,
                    0);
    READER_TRY_CALL(alloc_and_memcpy, &m_null_bits, num_null_bytes, 0);
  }

  /* After null_bits field, there are some new fields for extra metadata. */
  m_optional_metadata_len = READER_CALL(available_to_read);
  if (m_optional_metadata_len) {
    READER_TRY_CALL(alloc_and_memcpy, &m_optional_metadata,
                    m_optional_metadata_len, 0);
  }

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

Table_map_event::~Table_map_event() {
  bapi_free(m_null_bits);
  m_null_bits = nullptr;
  bapi_free(m_field_metadata);
  m_field_metadata = nullptr;
  bapi_free(m_coltype);
  m_coltype = nullptr;
  bapi_free(m_optional_metadata);
  m_optional_metadata = nullptr;
}

/**
   Parses SIGNEDNESS field.

   @param[out] vec     stores the signedness flags extracted from field.
   @param[in]  field   SIGNEDNESS field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_signedness(std::vector<bool> &vec, unsigned char *field,
                             unsigned int length) {
  for (unsigned int i = 0; i < length; i++) {
    for (unsigned char c = 0x80; c != 0; c >>= 1) vec.push_back(field[i] & c);
  }
}

/**
   Parses DEFAULT_CHARSET field.

   @param[out] default_charset  stores collation numbers extracted from field.
   @param[in]  field   DEFAULT_CHARSET field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_default_charset(
    Table_map_event::Optional_metadata_fields::Default_charset &default_charset,
    unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  default_charset.default_charset = get_field_length(&p);
  while (p < field + length) {
    unsigned int col_index = get_field_length(&p);
    unsigned int col_charset = get_field_length(&p);

    default_charset.charset_pairs.push_back(
        std::make_pair(col_index, col_charset));
  }
}

/**
   Parses COLUMN_CHARSET field.

   @param[out] vec     stores collation numbers extracted from field.
   @param[in]  field   COLUMN_CHARSET field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_column_charset(std::vector<unsigned int> &vec,
                                 unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  while (p < field + length) vec.push_back(get_field_length(&p));
}

/**
   Parses COLUMN_NAME field.

   @param[out] vec     stores column names extracted from field.
   @param[in]  field   COLUMN_NAME field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_column_name(std::vector<std::string> &vec,
                              unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  while (p < field + length) {
    unsigned len = get_field_length(&p);
    vec.push_back(std::string(reinterpret_cast<char *>(p), len));
    p += len;
  }
}

/**
   Parses SET_STR_VALUE/ENUM_STR_VALUE field.

   @param[out] vec     stores SET/ENUM column's string values extracted from
                       field. Each SET/ENUM column's string values are stored
                       into a string separate vector. All of them are stored
                       in 'vec'.
   @param[in]  field   COLUMN_NAME field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_set_str_value(
    std::vector<Table_map_event::Optional_metadata_fields::str_vector> &vec,
    unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  while (p < field + length) {
    unsigned int count = get_field_length(&p);

    vec.push_back(std::vector<std::string>());
    for (unsigned int i = 0; i < count; i++) {
      unsigned len1 = get_field_length(&p);
      vec.back().push_back(std::string(reinterpret_cast<char *>(p), len1));
      p += len1;
    }
  }
}

/**
   Parses GEOMETRY_TYPE field.

   @param[out] vec     stores geometry column's types extracted from field.
   @param[in]  field   GEOMETRY_TYPE field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_geometry_type(std::vector<unsigned int> &vec,
                                unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  while (p < field + length) vec.push_back(get_field_length(&p));
}

/**
   Parses SIMPLE_PRIMARY_KEY field.

   @param[out] vec     stores primary key's column information extracted from
                       field. Each column has an index and a prefix which are
                       stored as a unit_pair. prefix is always 0 for
                       SIMPLE_PRIMARY_KEY field.
   @param[in]  field   SIMPLE_PRIMARY_KEY field in table_map_event.
   @param[in]  length  length of the field
 */
static void parse_simple_pk(
    std::vector<Table_map_event::Optional_metadata_fields::uint_pair> &vec,
    unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  while (p < field + length)
    vec.push_back(std::make_pair(get_field_length(&p), 0));
}

/**
   Parses PRIMARY_KEY_WITH_PREFIX field.

   @param[out] vec     stores primary key's column information extracted from
                       field. Each column has an index and a prefix which are
                       stored as a unit_pair.
   @param[in]  field   PRIMARY_KEY_WITH_PREFIX field in table_map_event.
   @param[in]  length  length of the field
 */

static void parse_pk_with_prefix(
    std::vector<Table_map_event::Optional_metadata_fields::uint_pair> &vec,
    unsigned char *field, unsigned int length) {
  unsigned char *p = field;

  while (p < field + length) {
    unsigned int col_index = get_field_length(&p);
    unsigned int col_prefix = get_field_length(&p);
    vec.push_back(std::make_pair(col_index, col_prefix));
  }
}

Table_map_event::Optional_metadata_fields::Optional_metadata_fields(
    unsigned char *optional_metadata, unsigned int optional_metadata_len) {
  unsigned char *field = optional_metadata;

  if (optional_metadata == NULL) return;

  while (field < optional_metadata + optional_metadata_len) {
    unsigned int len;
    Optional_metadata_field_type type =
        static_cast<Optional_metadata_field_type>(field[0]);

    // Get length and move field to the value.
    field++;
    len = get_field_length(&field);

    switch (type) {
      case SIGNEDNESS:
        parse_signedness(m_signedness, field, len);
        break;
      case DEFAULT_CHARSET:
        parse_default_charset(m_default_charset, field, len);
        break;
      case COLUMN_CHARSET:
        parse_column_charset(m_column_charset, field, len);
        break;
      case COLUMN_NAME:
        parse_column_name(m_column_name, field, len);
        break;
      case SET_STR_VALUE:
        parse_set_str_value(m_set_str_value, field, len);
        break;
      case ENUM_STR_VALUE:
        parse_set_str_value(m_enum_str_value, field, len);
        break;
      case GEOMETRY_TYPE:
        parse_geometry_type(m_geometry_type, field, len);
        break;
      case SIMPLE_PRIMARY_KEY:
        parse_simple_pk(m_primary_key, field, len);
        break;
      case PRIMARY_KEY_WITH_PREFIX:
        parse_pk_with_prefix(m_primary_key, field, len);
        break;
      default:
        BAPI_ASSERT(0);
    }
    // next field
    field += len;
  }
}

Rows_event::Rows_event(const char *buf, const Format_description_event *fde)
    : Binary_log_event(&buf, fde),
      m_table_id(0),
      m_width(0),
      m_extra_row_data(0),
      columns_before_image(0),
      columns_after_image(0),
      row(0) {
  BAPI_ENTER("Rows_event::Rows_event(const char*, ...)");
  READER_TRY_INITIALIZATION;
  READER_ASSERT_POSITION(fde->common_header_len);
  Log_event_type event_type = header()->type_code;
  uint16_t var_header_len = 0;
  size_t data_size = 0;
  uint8_t const post_header_len = fde->post_header_len[event_type - 1];
  m_type = event_type;

  if (post_header_len == 6) {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    READER_TRY_SET(m_table_id, read_and_letoh<uint64_t>, 4);
  } else {
    READER_TRY_SET(m_table_id, read_and_letoh<uint64_t>, 6);
  }
  READER_TRY_SET(m_flags, read_and_letoh<uint16_t>);

  if (post_header_len == ROWS_HEADER_LEN_V2) {
    /*
      Have variable length header, check length,
      which includes length bytes
    */
    READER_TRY_SET(var_header_len, read_and_letoh<uint16_t>);
    var_header_len -= 2;

    /* Iterate over var-len header, extracting 'chunks' */
    uint64_t end = READER_CALL(position) + var_header_len;
    while (READER_CALL(position) < end) {
      uint8_t type;
      READER_TRY_SET(type, read<uint8_t>);
      switch (type) {
        case ROWS_V_EXTRAINFO_TAG: {
          /* Have an 'extra info' section, read it in */
          uint8_t infoLen = 0;
          READER_TRY_SET(infoLen, read<uint8_t>);
          /* infoLen is part of the buffer to be copied below */
          READER_CALL(go_to, READER_CALL(position) - 1);

          /* Just store/use the first tag of this type, skip others */
          if (!m_extra_row_data) {
            READER_TRY_CALL(alloc_and_memcpy, &m_extra_row_data, infoLen, 16);
          } else {
            READER_TRY_CALL(forward, infoLen);
          }
          break;
        }
        default:
          /* Unknown code, we will not understand anything further here */
          READER_CALL(go_to, end); /* Break loop */
      }
      if (READER_CALL(position) > end)
        READER_THROW("Invalid extra rows info header");
    }
  }

  READER_TRY_SET(m_width, net_field_length_ll);
  if (m_width == 0) READER_THROW("Invalid m_width");
  n_bits_len = (m_width + 7) / 8;
  READER_TRY_CALL(assign, &columns_before_image, n_bits_len);

  if (event_type == UPDATE_ROWS_EVENT || event_type == UPDATE_ROWS_EVENT_V1 ||
      event_type == PARTIAL_UPDATE_ROWS_EVENT) {
    READER_TRY_CALL(assign, &columns_after_image, n_bits_len);
  } else
    columns_after_image = columns_before_image;

  data_size = READER_CALL(available_to_read);
  READER_TRY_CALL(assign, &row, data_size);
  // JAG: TODO: Investigate and comment here about the need of this extra byte
  row.push_back(0);

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

Rows_event::~Rows_event() {
  if (m_extra_row_data) {
    bapi_free(m_extra_row_data);
    m_extra_row_data = NULL;
  }
}

Rows_query_event::Rows_query_event(const char *buf,
                                   const Format_description_event *fde)
    : Ignorable_event(buf, fde) {
  BAPI_ENTER("Rows_query_event::Rows_query_event(const char*, ...)");
  READER_TRY_INITIALIZATION;
  READER_ASSERT_POSITION(fde->common_header_len);
  unsigned int len = 0;
  uint8_t const post_header_len =
      fde->post_header_len[ROWS_QUERY_LOG_EVENT - 1];

  m_rows_query = nullptr;

  /*
   m_rows_query length is stored using only one byte (the +1 below), but that
   length is ignored and the complete query is read.
  */
  READER_TRY_CALL(forward, post_header_len + 1);
  len = READER_CALL(available_to_read);
  READER_TRY_CALL(alloc_and_strncpy, &m_rows_query, len, 16);

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

Rows_query_event::~Rows_query_event() {
  if (m_rows_query) bapi_free(m_rows_query);
}

Write_rows_event::Write_rows_event(const char *buf,
                                   const Format_description_event *fde)
    : Rows_event(buf, fde) {
  BAPI_ENTER("Write_rows_event::Write_rows_event(const char*, ...)");
  READER_TRY_INITIALIZATION;
  this->header()->type_code = m_type;
  BAPI_VOID_RETURN;
}

Update_rows_event::Update_rows_event(const char *buf,
                                     const Format_description_event *fde)
    : Rows_event(buf, fde) {
  BAPI_ENTER("Update_rows_event::Update_rows_event(const char*, ...)");
  READER_TRY_INITIALIZATION;
  this->header()->type_code = m_type;
  BAPI_VOID_RETURN;
}

Delete_rows_event::Delete_rows_event(const char *buf,
                                     const Format_description_event *fde)
    : Rows_event(buf, fde) {
  BAPI_ENTER("Delete_rows_event::Delete_rows_event(const char*, ...)");
  READER_TRY_INITIALIZATION;
  this->header()->type_code = m_type;
  BAPI_VOID_RETURN;
}

#ifndef HAVE_MYSYS
void Table_map_event::print_event_info(std::ostream &info) {
  info << "table id: " << m_table_id << " (" << m_dbnam.c_str() << "."
       << m_tblnam.c_str() << ")";
}

void Table_map_event::print_long_info(std::ostream &info) {
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\tFlags: " << m_flags;
  info << "\tColumn Type: ";
  /*
    TODO: Column types are stored as integers. To be
    replaced by string representation of types.
  */
  for (unsigned int i = 0; i < m_colcnt; i++) {
    info << "\t" << (int)m_coltype[i];
  }
  info << "\n";
  this->print_event_info(info);
}

void Rows_event::print_event_info(std::ostream &info) {
  info << "table id: " << m_table_id << " flags: ";
  info << get_flag_string(static_cast<enum_flag>(m_flags));
}

void Rows_event::print_long_info(std::ostream &info) {
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\n";

  this->print_event_info(info);

  // TODO: Extract table names and column data.
  if (this->get_event_type() == WRITE_ROWS_EVENT_V1 ||
      this->get_event_type() == WRITE_ROWS_EVENT)
    info << "\nType: Insert";

  if (this->get_event_type() == DELETE_ROWS_EVENT_V1 ||
      this->get_event_type() == DELETE_ROWS_EVENT)
    info << "\nType: Delete";

  if (this->get_event_type() == UPDATE_ROWS_EVENT_V1 ||
      this->get_event_type() == UPDATE_ROWS_EVENT ||
      this->get_event_type() == PARTIAL_UPDATE_ROWS_EVENT)
    info << "\nType: Update";
}
#endif
}  // end namespace binary_log
