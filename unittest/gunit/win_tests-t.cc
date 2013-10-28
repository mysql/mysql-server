/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "named_pipe.h"
#include "log.h"
#include "my_pthread.h"


// Mock logger function sql_print_error(), to avoid log.cc linkage
void sql_print_error(const char *format, ...) 
{
}

namespace win_unittest {

class NamedPipeTest : public ::testing::Test
{
protected:

  virtual void SetUp()
  {
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
  }

  SECURITY_ATTRIBUTES m_sec_attr;
  SECURITY_DESCRIPTOR m_sec_descr;
  char                m_pipe_name[256];
  HANDLE              m_pipe_handle;
  std::string         m_name;
};


// Basic test: create a named pipe.
TEST_F(NamedPipeTest, CreatePipe)
{
  char exp_pipe_name[256];

  m_pipe_handle= create_server_named_pipe(&m_sec_attr,
                                          &m_sec_descr,
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
  m_pipe_handle= create_server_named_pipe(&m_sec_attr,
                                          &m_sec_descr,
                                          1024,
                                          m_name.c_str(),
                                          m_pipe_name,
                                          sizeof(m_pipe_name));
  EXPECT_NE(INVALID_HANDLE_VALUE, m_pipe_handle);

  HANDLE handle= create_server_named_pipe(&m_sec_attr,
                                          &m_sec_descr,
                                          1024,
                                          m_name.c_str(),
                                          m_pipe_name,
                                          sizeof(m_pipe_name));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

pthread_handler_t handle_thread(void *arg)
{
  pthread_exit(0);
  return 0; // Avoid compiler warning
}

// Join with running/already finished thread
TEST(Thread, CreateAndJoin)
{
  pthread_t thr;
  pthread_attr_t thr_attr;
  pthread_attr_init(&thr_attr);
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_JOINABLE);
  HANDLE thr_handle= NULL;
  const HANDLE null_thr_handle= NULL;
  int ret, tries=10;
  while (tries)
  {
    ret= pthread_create_get_handle(&thr, &thr_attr,
                                   handle_thread, 0, &thr_handle);
    EXPECT_EQ(0, ret);
    EXPECT_NE(null_thr_handle, thr_handle);
    ret = pthread_join_with_handle(thr_handle);
    EXPECT_EQ(0, ret);
    tries--;
  }
  pthread_attr_destroy(&thr_attr);
}

// Test pthread_create_get_handle and pthread_join_with_handle
// with invalid arguments
TEST(Thread, CreateAndJoinFail)
{
  pthread_t thr;
  pthread_attr_t thr_attr;
  pthread_attr_init(&thr_attr);
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_JOINABLE);
  HANDLE thr_handle= NULL;
  const HANDLE null_thr_handle= NULL;
  int ret;

  // Passing NULL attribute is expected to create thread with default values
  ret= pthread_create_get_handle(&thr, NULL, handle_thread, 0,
                                 &thr_handle);
  EXPECT_EQ(0, ret);
  EXPECT_NE(null_thr_handle, thr_handle);
  ret= pthread_join_with_handle(thr_handle);

  // Passing NULL thread handle is expected to fail
  ret = pthread_join_with_handle(NULL);
  EXPECT_NE(0, ret);

  pthread_attr_destroy(&thr_attr);
}

}
