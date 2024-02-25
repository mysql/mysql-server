/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_LOGGER_INCLUDED
#define GCS_LOGGER_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging.h"

/**
  Group Replication implementation of Logger_interface

  Once a instance of this logger is set at Gcs_interface, all log
  produced by MySQL GCS will be routed by this logger to MySQL
  error log.
*/
class Gcs_gr_logger_impl : public Logger_interface {
 public:
  /**
    Constructor.
  */
  Gcs_gr_logger_impl() = default;

  /**
    Destructor.
  */
  ~Gcs_gr_logger_impl() override = default;

  /**
    Initialize the logger.

    @return the operation status
      @retval GCS_OK   Success
      @retval GCS_NOK  Error
  */
  enum_gcs_error initialize() override;

  /**
    Finalize the logger.

    @return the operation status
      @retval GCS_OK   Success
      @retval GCS_NOK  Error
  */
  enum_gcs_error finalize() override;

  /**
    Log a message using the specified level.

    @param[in] level    logging level of message
    @param[in] message  the message to log
  */
  void log_event(const gcs_log_level_t level,
                 const std::string &message) override;

  /*
    Disabling copy constructor and assignment operator.
  */
 private:
  Gcs_gr_logger_impl(Gcs_gr_logger_impl &l);
  Gcs_gr_logger_impl &operator=(const Gcs_gr_logger_impl &l);
};

#endif /* GCS_LOGGER_INCLUDED */
