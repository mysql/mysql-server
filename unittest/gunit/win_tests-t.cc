/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include <gtest/gtest.h>
#include "my_config.h"

#include "my_io.h"
#include "my_thread.h"
#include "sql/log.h"
#include "sql/named_pipe.h"

// Mock logger function to avoid breaking the link
int log_message(int log_type, ...) { return -1; }

namespace win_unittest {

class NamedPipeTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    char pipe_rand_name[256];

    m_pipe_handle = INVALID_HANDLE_VALUE;

    /*
      Generate a Unique Pipe Name incase multiple instances of the test is run.
    */
    sprintf_s(pipe_rand_name, sizeof(pipe_rand_name), "Pipe-%x",
              GetTickCount());

    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();

    m_name.append(pipe_rand_name);
    m_name.append("gunit");
    m_name.append(test_info->name());
  }

  virtual void TearDown() {
    if (m_pipe_handle != INVALID_HANDLE_VALUE) {
      EXPECT_TRUE(CloseHandle(m_pipe_handle));
    }
  }

  SECURITY_ATTRIBUTES m_sec_attr;
  SECURITY_DESCRIPTOR m_sec_descr;
  char m_pipe_name[256];
  HANDLE m_pipe_handle;
  std::string m_name;
};

// Basic test: create a named pipe.
TEST_F(NamedPipeTest, CreatePipe) {
  char exp_pipe_name[256];

  m_pipe_handle =
      create_server_named_pipe(&m_sec_attr, &m_sec_descr, 1024, m_name.c_str(),
                               m_pipe_name, sizeof(m_pipe_name));

  strxnmov(exp_pipe_name, sizeof(exp_pipe_name) - 1, "\\\\.\\pipe\\",
           m_name.c_str(), NullS);

  EXPECT_STREQ(m_pipe_name, exp_pipe_name);
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);
}

// Verify that we fail if we try to create the same named pipe twice.
TEST_F(NamedPipeTest, CreatePipeTwice) {
  m_pipe_handle =
      create_server_named_pipe(&m_sec_attr, &m_sec_descr, 1024, m_name.c_str(),
                               m_pipe_name, sizeof(m_pipe_name));
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);

  HANDLE handle =
      create_server_named_pipe(&m_sec_attr, &m_sec_descr, 1024, m_name.c_str(),
                               m_pipe_name, sizeof(m_pipe_name));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

}  // namespace win_unittest
