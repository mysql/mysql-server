/* Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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


#ifndef TEST_UTILS_INCLUDED
#define TEST_UTILS_INCLUDED

#include <string.h>
#include <sys/types.h>

#include "error_handler.h"
#include "gtest/gtest.h"
#include "my_compiler.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_mutex.h"
#include "set_var.h"
#include "sql_class.h"
#include "sql_error.h"

class THD;
class my_decimal;

extern thread_local_key_t THR_MALLOC;
extern thread_local_key_t THR_THD;
extern bool THR_THD_initialized;
extern bool THR_MALLOC_initialized;
extern mysql_mutex_t LOCK_open;
extern uint    opt_debug_sync_timeout;
extern "C" void sql_alloc_error_handler(void);

namespace my_testing {

inline int native_compare(size_t *length, unsigned char **a, unsigned char **b)
{
  return memcmp(*a, *b, *length);
}

inline qsort2_cmp get_ptr_compare(size_t size MY_ATTRIBUTE((unused)))
{
  return (qsort2_cmp) native_compare;
}

void setup_server_for_unit_tests();
void teardown_server_for_unit_tests();
int chars_2_decimal(const char *chars, my_decimal *to);

/*
  A class which wraps the necessary setup/teardown logic for
  unit tests which depend on a working THD environment.
 */
class Server_initializer
{
public:
  Server_initializer() : m_thd(NULL) {}

  // Invoke these from corresponding functions in test fixture classes.
  void SetUp();
  void TearDown();

  // Sets expected error for error_handler_hook.
  static void set_expected_error(uint val);

  THD *thd() const { return m_thd; }
private:
  THD *m_thd;
};

/**
   A mock error handler which registers itself with the THD in the CTOR,
   and unregisters in the DTOR. The function handle_condition() will
   verify that it is called with the expected error number.
   The DTOR will verify that handle_condition() has actually been called.
*/
class Mock_error_handler : public Internal_error_handler
{
public:
  Mock_error_handler(THD *thd, uint expected_error);
  virtual ~Mock_error_handler();

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg);

  int handle_called() const { return m_handle_called; }
private:
  THD *m_thd;
  uint m_expected_error;
  int  m_handle_called;
};


/*
  Some compilers want to know the type of the NULL when expanding gunit's
  EXPECT_EQ macros.
*/
template <typename T>
void expect_null(T *t)
{
  T *t_null= NULL;
  EXPECT_EQ(t_null, t);
}

/*
  A class which wraps the necessary setup/teardown logic for
  Data Dictionary.
*/
class DD_initializer
{
public:
  static void SetUp();
  static void TearDown();
};

}  // namespace my_testing



#endif  // TEST_UTILS_INCLUDED
