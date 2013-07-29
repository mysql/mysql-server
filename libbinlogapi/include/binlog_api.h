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

#ifndef REPEVENT_INCLUDED
#define	REPEVENT_INCLUDED

#include "binlog_event.h"
#include "binlog_driver.h"
#include "tcp_driver.h"
#include "file_driver.h"
#include "basic_content_handler.h"
#include "basic_transaction_parser.h"
#include "field_iterator.h"
#include "rowset.h"
#include "access_method_factory.h"
#include <iosfwd>
#include <list>
#include <cassert>
#include <algorithm>

#define BAPI_STRERROR_SIZE (256)
namespace mysql
{

/**
 * Error codes.
 */
enum Error_code {
  ERR_OK = 0,                                   /* All OK */
  ERR_EOF,                                      /* End of file */
  ERR_FAIL,                                     /* Unspecified failure */
  ERR_CHECKSUM_ENABLED,
  ERR_CHECKSUM_QUERY_FAIL,
  ERR_CONNECT,
  ERR_BINLOG_VERSION,
  ERR_PACKET_LENGTH,
  ERR_MYSQL_QUERY_FAIL,
  ERROR_CODE_COUNT
};

/**
 *Errors you can get from the API
 */
extern const char *bapi_error_messages[];

extern const char *str_error(int error_no);

/**
 * Returns true if the event is consumed
 */

class Dummy_driver : public system::Binary_log_driver
{
public:
  Dummy_driver() : Binary_log_driver("", 0) {}
  virtual ~Dummy_driver() {}

  virtual int connect()
  {
    return 1;
  }

  virtual unsigned int wait_for_next_event(mysql::Binary_log_event **event)
  {
    return ERR_EOF;
  }

  virtual int set_position(const std::string &str, unsigned long position)
  {
    return ERR_OK;
  }

  virtual int get_position(std::string *str, unsigned long *position)
  {
    return ERR_OK;
  }
  virtual int connect(const std::string &filename, ulong position)
  {
    return ERR_OK;
  }
  virtual int disconnect()
  {
    return ERR_OK;
  }
};

class Content_handler;

typedef std::list<Content_handler *> Content_handler_pipeline;

class Binary_log {
private:
  system::Binary_log_driver *m_driver;
  Dummy_driver m_dummy_driver;
  Content_handler_pipeline m_content_handlers;
  unsigned long m_binlog_position;
  std::string m_binlog_file;
public:
  Binary_log(system::Binary_log_driver *drv);
  ~Binary_log()
  {
    m_driver= NULL;
  }
  int connect();
  int connect(ulong position);

  /**
   * Blocking attempt to get the next binlog event from the stream
   */

  unsigned int wait_for_next_event(Binary_log_event **event);

  /**
   * Inserts/removes content handlers in and out of the chain
   * The Content_handler_pipeline is a derived std::list
   */
  Content_handler_pipeline *content_handler_pipeline();

  /**
   * Set the binlog position (filename, position)
   *
   * @return Error_code
   *  @retval ERR_OK The position is updated.
   *  @retval ERR_EOF The position is out-of-range
   *  @retval >= ERR_CODE_COUNT An unspecified error occurred
   */
  int set_position(const std::string &filename, unsigned long position);

  /**
   * Set the binlog position using current filename
   * @param position Requested position
   *
   * @return Error_code
   *  @retval ERR_OK The position is updated.
   *  @retval ERR_EOF The position is out-of-range
   *  @retval >= ERR_CODE_COUNT An unspecified error occurred
   */
  int set_position(unsigned long position);

  /**
   * Fetch the binlog position for the current file
   */
  unsigned long get_position(void);

  /**
   * Fetch the current active binlog file name.
   * @param[out] filename
   * TODO replace reference with a pointer.
   * @return The current binlog file position
   */
  unsigned long get_position(std::string &filename);
  int disconnect();
};

}

#endif	/* REPEVENT_INCLUDED */
