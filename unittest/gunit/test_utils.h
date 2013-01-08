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


#ifndef TEST_UTILS_INCLUDED
#define TEST_UTILS_INCLUDED

#include "sql_error.h"
#include "sql_class.h"
#include "set_var.h"

extern pthread_key(MEM_ROOT**,THR_MALLOC);
extern pthread_key(THD*, THR_THD);
extern mysql_mutex_t LOCK_open;
extern uint    opt_debug_sync_timeout;
extern "C" void sql_alloc_error_handler(void);

// A simple helper function to determine array size.
template <class T, int size>
int array_size(const T (&)[size])
{
  return size;
}

namespace my_testing {

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
                                Sql_condition::enum_warning_level level,
                                const char* msg,
                                Sql_condition ** cond_hdl);

  int handle_called() const { return m_handle_called; }
private:
  THD *m_thd;
  uint m_expected_error;
  int  m_handle_called;
};


}  // namespace my_testing

#endif  // TEST_UTILS_INCLUDED
