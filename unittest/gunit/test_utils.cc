/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"
#include "rpl_handler.h"                        // delegates_init()

namespace my_testing {

int chars_2_decimal(const char *chars, my_decimal *to)
{
  char *end= strend(chars);
  return string2decimal(chars, to, &end);
}


/*
  A mock error handler for error_handler_hook.
*/
uint expected_error= 0;
extern "C" void test_error_handler_hook(uint err, const char *str, myf MyFlags)
{
  EXPECT_EQ(expected_error, err) << str;
}

void setup_server_for_unit_tests()
{
  static char *my_name= strdup(my_progname);
  char *argv[] = { my_name, 0 };
  set_remaining_args(1, argv);
  mysql_mutex_init(key_LOCK_error_log, &LOCK_error_log, MY_MUTEX_INIT_FAST);
  system_charset_info= &my_charset_utf8_general_ci;
  sys_var_init();
  init_common_variables();
  my_init_signals();
  randominit(&sql_rand, 0, 0);
  xid_cache_init();
  delegates_init();
  gtid_server_init();
  error_handler_hook= test_error_handler_hook;
  // Initialize logger last, to avoid spurious warnings to stderr.
  logger.init_base();
}

void teardown_server_for_unit_tests()
{
  sys_var_end();
  delegates_destroy();
  xid_cache_free();
  gtid_server_cleanup();
  mysql_mutex_destroy(&LOCK_error_log);
  logger.cleanup_base();
  logger.cleanup_end();
}

void Server_initializer::set_expected_error(uint val)
{
  expected_error= val;
}

void Server_initializer::SetUp()
{
  expected_error= 0;
  m_thd= new THD(false);
  THD *stack_thd= m_thd;
  m_thd->thread_stack= (char*) &stack_thd;
  m_thd->store_globals();
  lex_start(m_thd);
}

void Server_initializer::TearDown()
{
  m_thd->cleanup_after_query();
  delete m_thd;
}


Mock_error_handler::Mock_error_handler(THD *thd, uint expected_error)
  : m_thd(thd),
    m_expected_error(expected_error),
    m_handle_called(0)
{
  thd->push_internal_handler(this);
}

Mock_error_handler::~Mock_error_handler()
{
  // Strange Visual Studio bug: have to store 'this' in local variable.
  Internal_error_handler *me= this;
  EXPECT_EQ(me, m_thd->pop_internal_handler());
  if (m_expected_error == 0)
  {
    EXPECT_EQ(0, m_handle_called);
  }
  else
  {
    EXPECT_GT(m_handle_called, 0);
  }
}

bool Mock_error_handler::handle_condition(THD *thd,
                                          uint sql_errno,
                                          const char* sqlstate,
                                          Sql_condition::enum_severity_level level,
                                          const char* msg,
                                          Sql_condition ** cond_hdl)
{
  EXPECT_EQ(m_expected_error, sql_errno);
  ++m_handle_called;
  return true;
}


}  // namespace my_testing
