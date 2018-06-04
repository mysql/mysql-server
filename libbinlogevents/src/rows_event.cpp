/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "rows_event.h"
#include <stdlib.h>
#include <cstring>
#include <string>

namespace binary_log
{
/**
  Get the length of next field.
  Change parameter to point at fieldstart.

  @param  packet pointer to a buffer containing the field in a row.
  @return pos    length of the next field
*/
unsigned long get_field_length(unsigned char **packet)
{
  unsigned char *pos= *packet;
  uint32_t temp= 0;
  if (*pos < 251)
  {
    (*packet)++;
    return  *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return ((unsigned long) ~0);//NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+= 3;
    memcpy(&temp, pos + 1, 2);
    temp= le32toh(temp);
    return (unsigned long)temp;
  }
  if (*pos == 253)
  {
    (*packet)+= 4;
    memcpy(&temp, pos + 1, 3);
    temp= le32toh(temp);
    return (unsigned long)temp;
  }
  (*packet)+= 9;                                 /* Must be 254 when here */
  memcpy(&temp, pos + 1, 4);
  temp= le32toh(temp);
  return (unsigned long)temp;
}


/**
  Constructor used to read the event from the binary log.
*/
Table_map_event::Table_map_event(const char *buf, unsigned int event_len,
                                 const Format_description_event*
                                 description_event)
  : Binary_log_event(&buf, description_event->binlog_version,
                     description_event->server_version),
    m_table_id(0), m_flags(0), m_data_size(0),
    m_dbnam(""), m_dblen(0), m_tblnam(""), m_tbllen(0),
    m_colcnt(0), m_field_metadata_size(0), m_field_metadata(0), m_null_bits(0)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  unsigned int bytes_read= 0;
  uint8_t common_header_len= description_event->common_header_len;
  uint8_t post_header_len=
                      description_event->post_header_len[TABLE_MAP_EVENT - 1];

  /* Set the event data size = post header + body */
  m_data_size= event_len - common_header_len;

  /* Read the post-header */
  const char *post_start= buf;

  post_start+= TM_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    uint64_t table_id = 0;
    memcpy(&table_id, post_start, 4);
    m_table_id= le64toh(table_id);
    post_start+= 4;
  }
  else
  {
    BAPI_ASSERT(post_header_len == TABLE_MAP_HEADER_LEN);
    uint64_t table_id = 0;
    memcpy(&table_id, post_start, 6);
    m_table_id= le64toh(table_id);
    post_start+= TM_FLAGS_OFFSET;
  }

  memcpy(&m_flags, post_start, sizeof(m_flags));
  m_flags= le16toh(m_flags);

  /* Read the variable part of the event */
  const char *const vpart= buf + post_header_len;

  /* Extract the length of the various parts from the buffer */
  unsigned char const *const ptr_dblen= (unsigned char const*)vpart + 0;
  m_dblen= *(unsigned char*) ptr_dblen;

  /* Length of database name + counter + terminating null */
  unsigned char const *const ptr_tbllen= ptr_dblen + m_dblen + 2;
  m_tbllen= *(unsigned char*) ptr_tbllen;

  /* Length of table name + counter + terminating null */
  unsigned char const *const ptr_colcnt= ptr_tbllen + m_tbllen + 2;
  unsigned char *ptr_after_colcnt= (unsigned char*) ptr_colcnt;
  m_colcnt= get_field_length(&ptr_after_colcnt);

  bytes_read= (unsigned int) ((ptr_after_colcnt + common_header_len) -
                              (unsigned char *)buf);
  /* Avoid reading out of buffer */
  if (event_len <= bytes_read || event_len - bytes_read < m_colcnt)
  {
    m_coltype= NULL;
    return;
  }

  m_coltype= static_cast<unsigned char*>(bapi_malloc(m_colcnt, 16));
  m_dbnam= std::string((const char*)ptr_dblen  + 1, m_dblen);
  m_tblnam= std::string((const char*)ptr_tbllen  + 1, m_tbllen + 1);

  memcpy(m_coltype, ptr_after_colcnt, m_colcnt);

  ptr_after_colcnt= ptr_after_colcnt + m_colcnt;
  bytes_read= (unsigned int) (ptr_after_colcnt + common_header_len -
                             (unsigned char *)buf);
  if (bytes_read < event_len)
  {
    m_field_metadata_size= get_field_length(&ptr_after_colcnt);
    if (m_field_metadata_size > (m_colcnt * 2))
      return;
    unsigned int num_null_bytes= (m_colcnt + 7) / 8;

    m_null_bits= static_cast<unsigned char*>(bapi_malloc(num_null_bytes, 0));

    m_field_metadata= static_cast<unsigned char*>(bapi_malloc(m_field_metadata_size,
                                                  0));
    memcpy(m_field_metadata, ptr_after_colcnt, m_field_metadata_size);
    ptr_after_colcnt= (unsigned char*)ptr_after_colcnt + m_field_metadata_size;
    memcpy(m_null_bits, ptr_after_colcnt, num_null_bytes);
  }
}

Table_map_event::~Table_map_event()
{
    bapi_free(m_null_bits);
    m_null_bits= NULL;
    bapi_free(m_field_metadata);
    m_field_metadata= NULL;
    if (m_coltype != NULL)
      bapi_free(m_coltype);
    m_coltype= NULL;
}



/*****************************************************************************
                      Rows_event Methods
*****************************************************************************/
Rows_event::Rows_event(const char *buf, unsigned int event_len,
                       const Format_description_event *description_event)
  : Binary_log_event(&buf, description_event->binlog_version,
                     description_event->server_version),
    m_table_id(0), m_width(0), m_extra_row_data(0),
    columns_before_image(0), columns_after_image(0), row(0)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  uint8_t const common_header_len= description_event->common_header_len;
  Log_event_type event_type= header()->type_code;
  m_type= event_type;

  uint8_t const post_header_len=
                description_event->post_header_len[event_type - 1];
  const char *post_start= buf;
  post_start+= ROWS_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    uint64_t table_id = 0;
    memcpy(&table_id, post_start, 4);
    m_table_id= le64toh(table_id);
    post_start+= 4;
  }
  else
  {
    uint64_t table_id = 0;
    memcpy(&table_id, post_start, 6);
    m_table_id= le64toh(table_id);
    post_start+= ROWS_FLAGS_OFFSET;
  }

  memcpy(&m_flags, post_start, sizeof(m_flags));
  m_flags= le16toh(m_flags);
  post_start+= 2;

  uint16_t var_header_len= 0;
  if (post_header_len == ROWS_HEADER_LEN_V2)
  {
    /*
      Have variable length header, check length,
      which includes length bytes
    */
    memcpy(&var_header_len, post_start, sizeof(var_header_len));
    var_header_len= le16toh(var_header_len);
    /* Check length and also avoid out of buffer read */
    if (var_header_len < 2 ||
        event_len < static_cast<unsigned int>(var_header_len +
                                              (post_start - buf)))
      return;

    var_header_len-= 2;

    /* Iterate over var-len header, extracting 'chunks' */
    const char* start= post_start + 2;
    const char* end= start + var_header_len;
    for (const char* pos= start; pos < end;)
    {
      switch(*pos++)
      {
      case ROWS_V_EXTRAINFO_TAG:
      {
        /* Have an 'extra info' section, read it in */
        if ((end - pos) < EXTRA_ROW_INFO_HDR_BYTES)
          return;

        uint8_t infoLen= pos[EXTRA_ROW_INFO_LEN_OFFSET];
        if ((end - pos) < infoLen)
          return;

        /* Just store/use the first tag of this type, skip others */
        if (!m_extra_row_data)
        {
          m_extra_row_data= static_cast<unsigned char*>(bapi_malloc(infoLen, 16));
          if (m_extra_row_data != NULL)
          {
            memcpy(m_extra_row_data, pos, infoLen);
          }
        }
        pos+= infoLen;
        break;
      }
      default:
        /* Unknown code, we will not understand anything further here */
        pos= end; /* Break loop */
      }
    }
  }

  unsigned char const *const var_start= (const unsigned char *)buf +
                                        post_header_len + var_header_len;
  unsigned char const *const ptr_width= var_start;
  unsigned char *ptr_after_width= (unsigned char*) ptr_width;
  m_width = get_field_length(&ptr_after_width);
  n_bits_len= (m_width + 7) / 8;
  /* Avoid reading out of buffer */
  if (ptr_after_width + n_bits_len > (const unsigned char *)(buf +
                                                             event_len -
                                                             post_header_len))
    return;
  columns_before_image.reserve((m_width + 7) / 8);
  unsigned char *ch;
  ch= ptr_after_width;
  for(unsigned long i= 0; i < (m_width + 7) / 8; i++)
  {
    columns_before_image.push_back(*ch);
    ch++;
  }

  ptr_after_width+= (m_width + 7) / 8;

  columns_after_image= columns_before_image;
  if ((event_type == UPDATE_ROWS_EVENT) ||
      (event_type == UPDATE_ROWS_EVENT_V1))
  {
    columns_after_image.reserve((m_width + 7) / 8);
    columns_after_image.clear();
    ch= ptr_after_width;
    for(unsigned long i= 0; i < (m_width + 7) / 8; i++)
    {
      columns_after_image.push_back(*ch);
      ch++;
    }
    ptr_after_width+= (m_width + 7) / 8;
  }

  const unsigned char* ptr_rows_data= (unsigned char*) ptr_after_width;

  size_t const read_size= ptr_rows_data + common_header_len -
                          (const unsigned char *) buf;
  if (read_size > event_len)
    return;
  size_t const data_size= event_len - read_size;

  try
  {
    row.assign(ptr_rows_data, ptr_rows_data + data_size + 1);
  }
  catch (const std::bad_alloc &e)
  {
    row.clear();
  }
  BAPI_ASSERT(row.size() == data_size + 1);
}

Rows_event::~Rows_event()
{
  if (m_extra_row_data)
  {
    bapi_free(m_extra_row_data);
    m_extra_row_data= NULL;
  }
}

/**
  The ctor of Rows_query_event,
  Here we are copying the exact query executed in RBR, to a
  char array m_rows_query
*/
Rows_query_event::
Rows_query_event(const char *buf, unsigned int event_len,
                 const Format_description_event *descr_event)
 : Ignorable_event(buf, descr_event)
{
  uint8_t const common_header_len=
    descr_event->common_header_len;
  uint8_t const post_header_len=
    descr_event->post_header_len[ROWS_QUERY_LOG_EVENT-1];

  m_rows_query= NULL;

  /*
   m_rows_query length is stored using only one byte, but that length is
   ignored and the complete query is read.
  */
  unsigned int offset= common_header_len + post_header_len + 1;
  /* Avoid reading out of buffer */
  if (offset > event_len)
    return;

  unsigned int len= event_len - offset;
  if (!(m_rows_query= static_cast<char*>(bapi_malloc(len + 1, 16))))
    return;

  strncpy(m_rows_query, buf + offset , len);
  // Appending '\0' at the end.
  m_rows_query[len]= '\0';
}

Rows_query_event::~Rows_query_event()
{
  if(m_rows_query)
     bapi_free(m_rows_query);
}

#ifndef HAVE_MYSYS
void Table_map_event::print_event_info(std::ostream& info)
{
  info << "table id: " << m_table_id << " ("
       << m_dbnam.c_str() << "."
        << m_tblnam.c_str() << ")";
}

void Table_map_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\tFlags: " << m_flags;
  info << "\tColumn Type: ";
  /*
    TODO: Column types are stored as integers. To be
    replaced by string representation of types.
  */
  for (unsigned int i= 0; i < m_colcnt; i++)
  {
    info << "\t" << (int)m_coltype[i];
  }
  info << "\n";
  this->print_event_info(info);
}


void Rows_event::print_event_info(std::ostream& info)
{
  info << "table id: " << m_table_id << " flags: ";
  info << get_flag_string(static_cast<enum_flag>(m_flags));
}

void Rows_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\n";

  this->print_event_info(info);

  //TODO: Extract table names and column data.
  if (this->get_event_type() == PRE_GA_WRITE_ROWS_EVENT ||
      this->get_event_type() == WRITE_ROWS_EVENT_V1 ||
      this->get_event_type() == WRITE_ROWS_EVENT)
    info << "\nType: Insert" ;

  if (this->get_event_type() == PRE_GA_DELETE_ROWS_EVENT ||
      this->get_event_type() == DELETE_ROWS_EVENT_V1 ||
      this->get_event_type() == DELETE_ROWS_EVENT)
    info << "\nType: Delete" ;

  if (this->get_event_type() == PRE_GA_UPDATE_ROWS_EVENT ||
      this->get_event_type() == UPDATE_ROWS_EVENT_V1 ||
      this->get_event_type() == UPDATE_ROWS_EVENT)
    info << "\nType: Update" ;
}
#endif
} // end namespace binary_log
