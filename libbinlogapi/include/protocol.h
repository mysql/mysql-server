/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#ifndef PROTOCOL_INCLUDED
#define	PROTOCOL_INCLUDED

#include "binlog_event.h"
#include <my_global.h>
#include <mysql.h>
#include <m_ctype.h>
#include <sql_common.h>
#include <list>
#include <string.h>

namespace binary_log {
namespace system {

//TODO:Replace this by enum declaration in log_event.h
enum enum_binlog_checksum_alg {
  BINLOG_CHECKSUM_ALG_OFF= 0,                  // Events are without checksum
                                               // though its generator is
                                               // checksum-capable New Master (NM).
  BINLOG_CHECKSUM_ALG_CRC32= 1,                // CRC32 of zlib algorithm.
  BINLOG_CHECKSUM_ALG_ENUM_END,                // the cut line: valid alg range
                                               // is [1, 0x7f].
  BINLOG_CHECKSUM_ALG_UNDEF= 255               // special value to tag undetermined
                                               // yet checksum or events from
                                               // checksum-unaware servers
};

/**
  Checks the Format Description event to determine if the master
  has binlog checksums enabled or not.
*/
int check_checksum_value( binary_log::Binary_log_event **event);

/**
  Storage structure for the handshake package sent from the server to
  the client.
*/
struct st_handshake_package
{
  uint8_t     protocol_version;
  std::string server_version_str;
  uint32_t    thread_id;
  uint8_t     scramble_buff[8];
  uint16_t    server_capabilities;
  uint8_t     server_language;
  uint16_t    server_status;
  uint8_t     scramble_buff2[13];
};

/**
  Storage structure for the OK package sent from the server to
  the client.
*/
struct st_ok_package
{
  uint8_t  result_type;
  uint64_t affected_rows;
  uint64_t insert_id;
  uint16_t server_status;
  uint16_t warning_count;
  std::string  message;
};

struct st_eof_package
{
  uint16_t warning_count;
  uint16_t status_flags;
};

/**
  Storage structure for the Error package sent from the server to
  the client.
*/
struct st_error_package
{
  uint16_t error_code;
  uint8_t  sql_state[5];
  std::string  message;
};

#define CLIENT_LONG_PASSWORD	1	/* new more secure passwords */
#define CLIENT_FOUND_ROWS	2	/* Found instead of affected rows */
#define CLIENT_LONG_FLAG	4	/* Get all column flags */
#define CLIENT_CONNECT_WITH_DB	8	/* One can specify db on connect */
#define CLIENT_NO_SCHEMA	16	/* Don't allow database.table.column */
#define CLIENT_COMPRESS		32	/* Can use compression protocol */
#define CLIENT_ODBC		64	/* Odbc client */
#define CLIENT_LOCAL_FILES	128	/* Can use LOAD DATA LOCAL */
#define CLIENT_IGNORE_SPACE	256	/* Ignore spaces before '(' */
#define CLIENT_PROTOCOL_41	512	/* New 4.1 protocol */
#define CLIENT_INTERACTIVE	1024	/* This is an interactive client */
#define CLIENT_SSL              2048	/* Switch to SSL after handshake */
#define CLIENT_IGNORE_SIGPIPE   4096    /* IGNORE sigpipes */
#define CLIENT_TRANSACTIONS	8192	/* Client knows about transactions */
#define CLIENT_RESERVED         16384   /* Old flag for 4.1 protocol  */
#define CLIENT_SECURE_CONNECTION 32768  /* New 4.1 authentication */
#define CLIENT_MULTI_STATEMENTS (1UL << 16) /* Enable/disable multi-stmt support */
#define CLIENT_MULTI_RESULTS    (1UL << 17) /* Enable/disable multi-results */

#define CLIENT_SSL_VERIFY_SERVER_CERT (1UL << 30)
#define CLIENT_REMEMBER_OPTIONS (1UL << 31)


class buffer_source;

/**
 * The Protocol interface is used to describe a grammar consisting of
 * chunks of bytes that are read or written in consequtive order.
 * Example:
 * class Protocol_impl : public Protocol;
 * Protocol_impl chunk1(chunk_datastore1);
 * Protocol_impl chunk2(chunk_datastore2);
 * input_stream >> chunk1
 *              >> chunk2;
 */
class Protocol
{
public:
  Protocol() { m_length_encoded_binary= false; }
  /**
    Return the number of bytes which is read or written by this protocol chunk.
    The default size is equal to the underlying storage data type.
  */
  virtual unsigned int size()=0;

  /** Return a pointer to the first byte in a linear storage buffer */
  virtual const char *data()=0;

  /**
    Change the number of bytes which will be read or written to the storage.
    The default size is euqal to the storage type size. This can change if the
    protocol is reading a length encoded binary.
    The new size must always be less than the size of the underlying storage
    type.
  */
  virtual void collapse_size(unsigned int new_size)=0;

  /**
    The first byte will have a special interpretation which will indicate
    how many bytes should be read next.
  */
  void set_length_encoded_binary(bool bswitch)
  {
    m_length_encoded_binary= bswitch;
  }
  bool is_length_encoded_binary(void) { return m_length_encoded_binary; }

private:
    bool m_length_encoded_binary;

    friend std::istream &operator<<(std::istream &is, Protocol &chunk);
    friend std::istream &operator>>(std::istream &is, Protocol &chunk);
    friend buffer_source &operator>>(buffer_source &src, Protocol &chunk);
    friend std::istream &operator>>(std::istream &is, std::string &str);
};

template<typename T>
class Protocol_chunk : public Protocol
{
public:

  Protocol_chunk() : Protocol()
  {
    m_size= 0;
    m_data= 0;
  }

  Protocol_chunk(T &chunk) : Protocol()
  {
    m_data= (const char *)&chunk;
    m_size= sizeof(T);
  }

  Protocol_chunk(const T &chunk) : Protocol ()
  {
     m_data= (const char *) &chunk;
     m_size= sizeof(T);
  }

  /**
   * @param buffer A pointer to the storage
   * @param size The size of the storage
   *
   * @note If size == 0 then the chunk is a
   * length coded binary.
   */
  Protocol_chunk(T *buffer, unsigned long size) : Protocol ()
  {
      m_data= (const char *)buffer;
      m_size= size;
  }

  virtual unsigned int size() { return m_size; }
  virtual const char *data() { return m_data; }
  virtual void collapse_size(unsigned int new_size)
  {
    //assert(new_size <= m_size);
    memset((char *)m_data+new_size,'\0', m_size-new_size);
    m_size= new_size;
  }
private:
  const char *m_data;
  unsigned long m_size;
};

std::ostream &operator<<(std::ostream &os, Protocol &chunk);

typedef Protocol_chunk<uint8_t> Protocol_chunk_uint8;

class Protocol_chunk_string //: public Protocol_chunk_uint8
{
public:
    Protocol_chunk_string(std::string &chunk, unsigned long size)
    {
        m_str= &chunk;
        m_str->assign(size,'*');
    }

    virtual unsigned int size() const { return m_str->size(); }
    virtual const char *data() const { return m_str->data(); }
    virtual void collapse_size(unsigned int new_size)
    {
        m_str->resize(new_size);
    }
private:
    friend std::istream &operator>>(std::istream &is, Protocol_chunk_string &str);
    std::string *m_str;
};

class Protocol_chunk_vector : public Protocol_chunk_uint8
{
public:
    Protocol_chunk_vector(std::vector<uint8_t> &chunk, unsigned long size)
      : Protocol_chunk_uint8()
    {
        m_vec= &chunk;
        m_vec->reserve(size);
        m_size= size;
    }

    virtual unsigned int size() { return m_vec->size(); }
    virtual const char *data()
    {
      return reinterpret_cast<const char *>(&*m_vec->begin());
    }
    virtual void collapse_size(unsigned int new_size)
    {
        m_vec->resize(new_size);
    }
private:
    friend std::istream &operator>>(std::istream &is,
                                    Protocol_chunk_vector &chunk);
    std::vector<uint8_t> *m_vec;
    unsigned long m_size;
};


std::istream &operator>>(std::istream &is, Protocol_chunk_vector &chunk);

class buffer_source
{
public:

    buffer_source(const char *src, int sz)
    {
        m_src= src;
        m_size= sz;
        m_ptr= 0;
    }

    friend buffer_source &operator>>(buffer_source &src, Protocol &chunk);
private:
    const char *m_src;
    int m_size;
    int m_ptr;

};

class Protocol_chunk_string_len
{
public:
    Protocol_chunk_string_len(std::string &str)
    {
        m_storage= &str;
    }

private:
    friend std::istream &operator>>(std::istream &is,
                                    Protocol_chunk_string_len &lenstr);
    std::string *m_storage;
};

buffer_source &operator>>(buffer_source &src, Protocol &chunk);
/** TODO assert that the correct endianess is used */
std::istream &operator>>(std::istream &is, Protocol &chunk);
std::istream &operator>>(std::istream &is, std::string &str);
std::istream &operator>>(std::istream &is, Protocol_chunk_string_len &lenstr);
std::istream &operator>>(std::istream &is, Protocol_chunk_string &str);

/**
  Allocates a new event and copy the header. The caller must be responsible for
  releasing the allocated memory.
*/
Incident_event *proto_incident_event(std::istream &is,
                                     Log_event_header *header);
User_var_event *proto_uservar_event(std::istream &is, Log_event_header *header);

} // end namespace system
} // end namespace binary_log

#endif	/* PROTOCOL_INCLUDED */
