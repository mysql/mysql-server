/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_LOGGER_INCLUDED
#define GCS_LOGGER_INCLUDED

#include <mysql/gcs/gcs_logging.h>

/**
  Group Replication implementation of @interface Ext_logger_interface

  Once a instance of this logger is set at Gcs_interface, all log
  produced by MySQL GCS will be routed by this logger to MySQL
  error log.
*/
class Gcs_gr_logger_impl : public Ext_logger_interface
{
public:
  /**
    Constructor.
  */
  Gcs_gr_logger_impl() {}

  /**
    Destructor.
  */
  ~Gcs_gr_logger_impl() {}

  /**
    Initialize the logger.

    @return the operation status
      @retval GCS_OK   Success
      @retval GCS_NOK  Error
  */
  enum_gcs_error initialize();

  /**
    Finalize the logger.

    @return the operation status
      @retval GCS_OK   Success
      @retval GCS_NOK  Error
  */
  enum_gcs_error finalize();

  /**
    Log a message using the specified level.

    @param[in] level    logging level of message
    @param[in] message  the message to log
  */
  void log_event(gcs_log_level_t level, const char *message);

  /*
    Disabling copy constructor and assignment operator.
  */
  private:
    Gcs_gr_logger_impl(Gcs_gr_logger_impl &l);
    Gcs_gr_logger_impl& operator=(const Gcs_gr_logger_impl& l);
};

#endif	/* GCS_LOGGER_INCLUDED */
