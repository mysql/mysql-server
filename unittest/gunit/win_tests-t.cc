/* Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include "test_utils.h"

#include "my_global.h"
#include "named_pipe.h"
#include "log.h"
#include "my_thread.h"
#include <sddl.h>

namespace win_unittest {
using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class NamedPipeTest : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    m_old_error_handler_hook = error_handler_hook;
    // Make sure my_error() ends up calling my_message_sql so that
    // Mock_error_handler is actually triggered.
    error_handler_hook = my_message_sql;
  }

  static void TearDownTestCase()
  {
    error_handler_hook = m_old_error_handler_hook;
  }

  virtual void SetUp()
  {
    m_initializer.SetUp();

    char pipe_rand_name[256];

    m_pipe_handle= INVALID_HANDLE_VALUE;

    /* 
      Generate a Unique Pipe Name incase multiple instances of the test is run.
    */
    sprintf_s(pipe_rand_name, sizeof(pipe_rand_name), "Pipe-%x", 
              GetTickCount());
    
    const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
    
    m_name.append(pipe_rand_name);
    m_name.append("gunit");
    m_name.append(test_info->name());
  }

  virtual void TearDown()
  {
    if (m_pipe_handle != INVALID_HANDLE_VALUE)
    {
      EXPECT_TRUE(CloseHandle(m_pipe_handle));
    }
    m_initializer.TearDown();
  }

  SECURITY_ATTRIBUTES *mp_sec_attr;
  char                m_pipe_name[256];
  HANDLE              m_pipe_handle;
  std::string         m_name;
  Server_initializer  m_initializer;

  static void(*m_old_error_handler_hook)(uint, const char *, myf);
};
void(*NamedPipeTest::m_old_error_handler_hook)(uint, const char *, myf);


// Basic test: create a named pipe.
TEST_F(NamedPipeTest, CreatePipe)
{
  char exp_pipe_name[256];

  m_pipe_handle= create_server_named_pipe(&mp_sec_attr,
                                          1024,
                                          m_name.c_str(),
                                          m_pipe_name,
                                          sizeof(m_pipe_name));

  strxnmov(exp_pipe_name, sizeof(exp_pipe_name) - 1, "\\\\.\\pipe\\", 
           m_name.c_str(), NullS);

  EXPECT_STREQ(m_pipe_name, exp_pipe_name);
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);
}


// Verify that we fail if we try to create the same named pipe twice.
TEST_F(NamedPipeTest, CreatePipeTwice)
{
  m_pipe_handle= create_server_named_pipe(&mp_sec_attr,
                                          1024,
                                          m_name.c_str(),
                                          m_pipe_name,
                                          sizeof(m_pipe_name));
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);

  Mock_error_handler error_handler(m_initializer.thd(),
                                   ER_CANT_START_SERVER_NAMED_PIPE);
  HANDLE handle= create_server_named_pipe(&mp_sec_attr,
                                          1024,
                                          m_name.c_str(),
                                          m_pipe_name,
                                          sizeof(m_pipe_name));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

// Verify that a warning is written to the error log when using
// "*everyone* as the full access group name.
TEST_F(NamedPipeTest, CreatePipeForEveryone)
{
  Mock_error_handler error_handler(m_initializer.thd(),
                                   WARN_NAMED_PIPE_ACCESS_EVERYONE);
  m_pipe_handle = create_server_named_pipe(&mp_sec_attr,
                                           1024,
                                           m_name.c_str(),
                                           m_pipe_name,
                                           sizeof(m_pipe_name),
                                           "*everyone*");
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);
}

// Verify that a warning is written to the error log when using
// the group name corresponding to the built in Windows group
// with SID S-1-1-0  (i.e. "everyone" on English systems)
TEST_F(NamedPipeTest, CreatePipeForEveryoneSid)
{
  PSID everyone_SID;
  EXPECT_TRUE(ConvertStringSidToSid("S-1-1-0", &everyone_SID));
  const DWORD max_name_len = 256;
  char everyone_name[max_name_len];
  DWORD everyone_name_size = max_name_len;
  char domain_name[max_name_len];
  DWORD domain_name_size = max_name_len;
  SID_NAME_USE name_use;

  EXPECT_TRUE(LookupAccountSid(NULL, everyone_SID, everyone_name, &everyone_name_size,
                               domain_name, &domain_name_size, &name_use));
  // The "S-1-1-0" SID is well known, so we expect the domain_name to empty and
  // the name_use to be SidTypeWellKnownGroup
  EXPECT_EQ(domain_name_size, 0);
  EXPECT_EQ(name_use, SidTypeWellKnownGroup);

  Mock_error_handler error_handler(m_initializer.thd(),
                                   WARN_NAMED_PIPE_ACCESS_EVERYONE);
  m_pipe_handle = create_server_named_pipe(&mp_sec_attr,
                                           1024,
                                           m_name.c_str(),
                                           m_pipe_name,
                                           sizeof(m_pipe_name),
                                           everyone_name);
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);
}

}
