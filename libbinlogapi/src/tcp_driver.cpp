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

#include "tcp_driver.h"
#include "binlog_api.h"
#include "protocol.h"
#include "binlog_event.h"
#include "rowset.h"
#include "field_iterator.h"
#include "transitional_methods.h"

#include <my_global.h>
#include <mysql.h>
#include <m_ctype.h>
#include <sql_common.h>

#include <iostream>
#include <cstring>
#include <fstream>
#include <time.h>
#include <streambuf>
#include <cstdio>
#include <exception>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace mysql { namespace system {

/**
  Copies the first length character from source to destination

  @param destination  pointer to the destination where the content
                      is to be copied
  @param source       C string to be copied

  @param length       length of the characters to be copied from source

  @retval             destination is returned
*/

uchar *net_store_data(uchar *destination, const uchar *source, size_t length)
{
  destination= net_store_length(destination, length);
  memcpy(destination, source, length);
  return destination + length;
}

/**
  Read the header from the istream and populate the attributes
  of the class Log_event_header

  @param is   an input stream pointing to the buffer received from the server
  @param h    an object of Log_event_header class
*/
static void proto_event_packet_header(std::istream &is, Log_event_header *h)
{
  Protocol_chunk<uint8> prot_marker(h->marker);
  Protocol_chunk<uint32_t> prot_timestamp(h->timestamp);
  Protocol_chunk<uint8_t> prot_type_code(h->type_code);
  Protocol_chunk<uint32_t> prot_server_id(h->server_id);
  Protocol_chunk<uint32_t> prot_event_length(h->event_length);
  Protocol_chunk<uint32_t> prot_next_position(h->next_position);
  Protocol_chunk<uint16_t> prot_flags(h->flags);

  is >> prot_marker
          >> prot_timestamp
          >> prot_type_code
          >> prot_server_id
          >> prot_event_length
          >> prot_next_position
          >> prot_flags;
}
/**
  Intializes the MYSQL object and calls sync_connect_and_authenticate()

  @param user             user name
  @param passwd           password for connecting to the mysql-server
  @param host             host name
  @param port             port number
  @binlog_filename        name of the binlog file, when we connect to the
                          server for the first time its empty, but when we
                          reconnect it should have the name of binlog file,
                          the code for this will be added.
  @offset                 the position in the binlog file from where we will
                          start reading, default is 4

  @retval ERR_OK          success
  @retval Other Value     failure
*/

int Binlog_tcp_driver::connect(const std::string& user,
                               const std::string& passwd,
                               const std::string& host, uint port,
                               const std::string& binlog_filename,
                               size_t offset)
{
  m_mysql= mysql_init(NULL);

  if (!m_mysql)
    return ERR_FAIL;

  int err= sync_connect_and_authenticate(m_mysql, user, passwd, host, port, offset);
  if (err != ERR_OK)
    return err;

  const char *binlog_file= "";
  if (binlog_filename != "" || offset > 4)
    start_binlog_dump(binlog_filename.c_str(), offset);
  else
    start_binlog_dump(binlog_file, m_binlog_offset);
  return ERR_OK;
}


/**
  Connects to the mysql database, register as a slave,
  and create a network stream using COM_BINLOG_DUMP to read the events.

  @param conn             Pointer to a MYSQL object
  @param passwd           password for connecting to the mysql-server
  @param host             host name
  @param port             port number
  @retval ERR_OK          success
  @retval Other Value     failure
*/
int sync_connect_and_authenticate(MYSQL *conn, const std::string &user,
                                  const std::string &passwd,
                                  const std::string &host, uint port,
                                  long offset)
{

  ushort binlog_flags= 0;
  uchar buf[1024];
  uchar *pos= buf;
  /* So that mysql_real_connect use TCP_IP_PROTOCOL. */
  mysql_unix_port=0;
  int server_id= 1;
  MYSQL_RES* res = 0;
  MYSQL_ROW row;
  const char* checksum;

  uchar version_split[3];


/*
  Attempts to establish a connection to a MySQL database engine
  running on host

  Returns a MYSQL* connection handle if the connection was successful,
  NULL if the connection was unsuccessful.
  For a successful connection, the return value is the same as
  the value of the first parameter.
*/
  if (!mysql_real_connect(conn, host.c_str(), user.c_str(),
      passwd.c_str(), "", port, 0, 0))
    return ERR_FAIL;

  do_server_version_split(conn->server_version, version_split);
  if (version_product(version_split) >= checksum_version_product)
  {

    if (mysql_query(conn, "SHOW GLOBAL VARIABLES LIKE 'BINLOG_CHECKSUM'")
        || !(res= mysql_store_result(conn)))
    {
      return ERR_CHECKSUM_QUERY_FAIL;
    }

    if (!(row= mysql_fetch_row(res)))
    {
      mysql_free_result(res);
      return ERR_CHECKSUM_QUERY_FAIL;
    }

    if (!(checksum= row[0]))
    {
      mysql_free_result(res);
      return ERR_CHECKSUM_QUERY_FAIL;
    }

    checksum= row[1];
    if (strcmp(checksum, "NONE") != 0)
    {
      mysql_free_result(res);
      return ERR_CHECKSUM_ENABLED;
    }
    mysql_free_result(res);
  }//end if version 5.6

  int4store(pos, server_id); pos+= 4;
  pos= net_store_data(pos, (uchar*) host.c_str(), host.size());
  pos= net_store_data(pos, (uchar*) user.c_str(), user.size());
  pos= net_store_data(pos, (uchar*) passwd.c_str(), passwd.size());
  int2store(pos, (uint16) port);
  pos+= 2;

  /*
    Fake rpl_recovery_rank, which was removed in BUG#13963,
    so that this server can register itself on old servers,
    see BUG#49259.
  */
  int4store(pos, /* rpl_recovery_rank */ 0);
  pos+= 4;
  /* The master will fill in master_id */
  int4store(pos, 0);
  pos+= 4;

/*
    It sends a command packet to the mysql-server.

    @retval ERR_OK      if success
    @retval ERR_FAIL    on failure
*/
  if (simple_command(conn, COM_REGISTER_SLAVE, buf, (size_t) (pos - buf), 0))
    return ERR_FAIL;

  return ERR_OK;
}

void Binlog_tcp_driver::start_binlog_dump(const char *binlog_name,
                                          size_t offset)
{
  uchar buf[1024];
  ushort binlog_flags= 0;
  int server_id= 1;
  size_t binlog_name_length;
  m_mysql->status= MYSQL_STATUS_READY;
  int4store(buf, long(offset));
  int2store(buf + 4, binlog_flags);
  int4store(buf + 6, server_id);
  binlog_name_length= strlen(binlog_name);
  memcpy(buf + 10, binlog_name, binlog_name_length);
  simple_command(m_mysql, COM_BINLOG_DUMP, buf, binlog_name_length + 10, 1);
}

unsigned int Binlog_tcp_driver::wait_for_next_event(mysql::Binary_log_event **event_ptr)
{
  // poll for new event until one event is found.
  // return the event
  unsigned long len;
  len= cli_safe_read(m_mysql);
  if (len == packet_error)
  {
     uchar version_split[3];
     do_server_version_split(m_mysql->server_version, version_split);
     if (version_product(version_split) >= checksum_version_product)
       return ERR_PACKET_LENGTH;
     return ERR_FAIL;
  }
  std::istringstream is(std::string((char*)m_mysql->net.buff, len));
  proto_event_packet_header(is, m_waiting_event);
  *event_ptr= parse_event(is, m_waiting_event);
  if (*event_ptr)
  {
    if ((*event_ptr)->header()->type_code == FORMAT_DESCRIPTION_EVENT)
    {
      // Check server version and the checksum value
      int ret= check_checksum_value(event_ptr);
      return ret; // ret is the error code
    }
    return ERR_OK;
  }
  return ERR_FAIL;
}

int Binlog_tcp_driver::connect()
{
  return connect(m_user, m_passwd, m_host, m_port);
}
int Binlog_tcp_driver::connect(const std::string &binlog_filename,
                               ulong offset)
{
  return connect(m_user, m_passwd, m_host, m_port, binlog_filename, offset);
}
/**
 * Make synchronous reconnect.
 */
void Binlog_tcp_driver::reconnect()
{
  disconnect();
  connect(m_user, m_passwd, m_host, m_port);
}

int Binlog_tcp_driver::disconnect()
{
  delete m_waiting_event;
  if (m_mysql)
    mysql_close(m_mysql);
  return ERR_OK;
}


void Binlog_tcp_driver::shutdown(void)
{
  m_shutdown= true;
 //TODO: Add a call to mysql_shutdown
}

int Binlog_tcp_driver::set_position(const std::string &str, ulong position)
{
  //validate the new position before we attempt to set.

  MYSQL *mysql= mysql_init(NULL);
  if (!mysql)
    return ERR_FAIL;
  int err= sync_connect_and_authenticate(mysql, m_user, m_passwd, m_host, m_port);
  if (err != ERR_OK)
    return err;

  std::map<std::string, unsigned long> binlog_map;
  if (fetch_binlog_name_and_size(mysql, &binlog_map))
    return ERR_MYSQL_QUERY_FAIL;

  mysql_close(mysql);

  std::map<std::string, unsigned long>::iterator binlog_itr= binlog_map.find(str);
  if (binlog_itr == binlog_map.end())
    return ERR_FAIL;
  if (position > binlog_itr->second)
    return ERR_FAIL;
  disconnect();
  
  if (connect(m_user, m_passwd, m_host, m_port, str, position))
    return ERR_CONNECT;
  return ERR_OK;
}
int Binlog_tcp_driver::get_position(std::string *filename_ptr,
                                    ulong *position_ptr)
{
  MYSQL *mysql= mysql_init(NULL);
  if (!mysql)
    return ERR_FAIL;  
  int err= sync_connect_and_authenticate(mysql, m_user, m_passwd, m_host, m_port);
  if (err != ERR_OK)
    return err;

  if (fetch_master_status(mysql, &m_binlog_file_name, &m_binlog_offset))
    return ERR_MYSQL_QUERY_FAIL;

  mysql_close(mysql);
   if (filename_ptr)
    *filename_ptr= m_binlog_file_name;
  if (position_ptr)
    *position_ptr= m_binlog_offset;
  return ERR_OK;
}
bool fetch_master_status(MYSQL *mysql, std::string *filename,
                         unsigned long *position)
{
  if (mysql_query(mysql, "show master status"))
    return ERR_MYSQL_QUERY_FAIL;
  MYSQL_RES *res= mysql_use_result(mysql);
  if (!res)
    return ERR_MYSQL_QUERY_FAIL;
  MYSQL_ROW row= mysql_fetch_row(res);
  if (!row)
    return ERR_MYSQL_QUERY_FAIL;
  *filename= row[0];
  *position= strtoul(row[1], NULL, 0);
  return ERR_OK;
}

bool fetch_binlog_name_and_size(MYSQL *mysql, std::map<std::string, unsigned long> *binlog_map)
{
  if (mysql_query(mysql, "show binary logs"))
    return ERR_MYSQL_QUERY_FAIL;
  MYSQL_RES *res= mysql_use_result(mysql);
  if (!res)
    return ERR_MYSQL_QUERY_FAIL;
  while (MYSQL_ROW row= mysql_fetch_row(res))
  {
    unsigned long position;
    std::string filename;
    if (!row)
      return ERR_MYSQL_QUERY_FAIL;
    filename= row[0];
    position= strtoul(row[1], NULL, 0);
    (*binlog_map).insert(std::make_pair<std::string, unsigned long>(filename, position));
  }
  return ERR_OK;
}
}} // end namespace mysql::system
