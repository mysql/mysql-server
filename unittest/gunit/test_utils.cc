/* Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"

#include "binlog.h"
#include "conn_handler/connection_handler_manager.h"
#include "debug_sync.h"
#include "derror.h"
#include "item_create.h"
#include "item_func.h"
#include "keycache.h"
#include "keycaches.h"
#include "log.h"                                // query_logger
#include "my_sys.h"
#include "mysqld.h"
#include "mysqld_thd_manager.h"                 // Global_THD_manager
#include "opt_costconstantcache.h"              // optimizer cost constant cache
#include "my_regex.h"
#include "rpl_handler.h"                        // delegates_init()
#include "rpl_filter.h"
#include "sql_db.h"
#include "sql_common.h"
#include "sql_locale.h"
#include "sql_plugin.h"
#include "sql_show.h"

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
  std::string my_name(my_progname);
  char *argv[] = { const_cast<char*>(my_name.c_str()),
                   const_cast<char*>("--secure-file-priv=NULL"),
                   const_cast<char*>("--early_plugin_load=\"\""),
                   const_cast<char*>("--log_syslog=0"),
                   const_cast<char*>("--explicit_defaults_for_timestamp"),
                   const_cast<char*>("--datadir=" DATA_DIR),
                   const_cast<char*>("--lc-messages-dir=" ERRMSG_DIR), 0 };
  set_remaining_args(6, argv);
  system_charset_info= &my_charset_utf8_general_ci;
  sys_var_init();
  init_common_variables();
  my_init_signals();
  randominit(&sql_rand, 0, 0);
  transaction_cache_init();
  delegates_init();
  gtid_server_init();
  error_handler_hook= test_error_handler_hook;
  // Initialize Query_logger last, to avoid spurious warnings to stderr.
  query_logger.init();
  init_optimizer_cost_module(false);
}

void teardown_server_for_unit_tests()
{
  flush_error_log_messages();
  mysql_bin_log.cleanup();
  my_dboptions_cache_free();
  ignore_db_dirs_free();
  sys_var_end();
  delegates_destroy();
  transaction_cache_free();
  gtid_server_cleanup();
  query_logger.cleanup();
  item_func_sleep_free();
  item_create_cleanup();
  delete_optimizer_cost_module();
  key_caches.delete_elements((void (*)(const char*, uchar*)) free_key_cache);
  multi_keycache_free();
  free_tmpdir(&mysql_tmpdir_list);
  bitmap_free(&temp_pool);
  delete binlog_filter;
  delete rpl_filter;
  my_regex_end();
#if defined(ENABLED_DEBUG_SYNC)
  /* End the debug sync facility. See debug_sync.cc. */
  debug_sync_end();
#endif /* defined(ENABLED_DEBUG_SYNC) */
  cleanup_errmsgs();
  Connection_handler_manager::destroy_instance();
  mysql_client_plugin_deinit();
  free_list(opt_early_plugin_load_list_ptr);
  free_list(opt_plugin_load_list_ptr);
  deinit_errmessage(); // finish server errs
  Global_THD_manager::destroy_instance();
  my_end(0);
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

  m_thd->set_new_thread_id();

  m_thd->thread_stack= (char*) &stack_thd;
  m_thd->store_globals();
  lex_start(m_thd);
  m_thd->set_current_time();
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
                                          Sql_condition::enum_severity_level *level,
                                          const char* msg)
{
  EXPECT_EQ(m_expected_error, sql_errno);
  ++m_handle_called;
  return true;
}


}  // namespace my_testing
