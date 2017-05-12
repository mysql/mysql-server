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

#ifndef TEST_LOGGER_INCLUDED
#define TEST_LOGGER_INCLUDED
#include "mysql/gcs/gcs_log_system.h"
#include "gcs_xcom_interface.h"

/*
  It is a logger utility helps unit test to verify functions which will log
  errors correctly.

  Usage:
  #include "test_logger.h"

  test_logger.clear_event(); // clear all logged events before a test.
  ...
  test_logger.assert_error("Expected error message");
*/
class Test_logger : public Ext_logger_interface
{
private:
  std::stringstream m_log_stream;

  std::string get_event()
  {
    return m_log_stream.str();
  }

  void assert_event(gcs_log_level_t level, const std::string &expected)
  {
    std::string complete_log(gcs_log_levels[level]);
    complete_log+= GCS_LOG_PREFIX + expected;
    
    ASSERT_EQ(complete_log, get_event());
  }

public:
  Test_logger()
  {
    Gcs_logger::initialize(this);
  }

  ~Test_logger() {}

  enum_gcs_error initialize()
  {
    return GCS_OK;
  }

  enum_gcs_error finalize()
  {
    return GCS_OK;
  }

  void log_event(gcs_log_level_t level, const char* message)
  {
    m_log_stream << gcs_log_levels[level] << message;
  }

  void clear_event()
  {
    m_log_stream.str("");
  }

  void assert_error(const std::string &expected)
  {
    assert_event(GCS_ERROR, expected);
  }

  void assert_error(const std::stringstream &expected)
  {
    assert_error(expected.str());
  }
};

Test_logger test_logger;

#endif // TEST_LOGGER_INCLUDED
