/*
   Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "client_priv.h"
#include "debug_options.h"
#include "abstract_program.h"
#include "instance_callback.h"

using namespace Mysql::Tools::Base::Options;
using Mysql::Tools::Base::Abstract_program;

static Debug_options* primary_debug_options= NULL;
static uint my_end_arg;

static void debug_do_exit()
{
  my_end(::my_end_arg);
}

Debug_options::Debug_options(Abstract_program* program)
  : m_program(program)
{
  if (::primary_debug_options == NULL)
  {
    primary_debug_options= this;
    /*
      We don't want to call this routine in destructor, as we want it being
      called only once, and second we want it to be called as late as possible.
      Possibly when this object is already freed up.
    */
    atexit(&::debug_do_exit);
  }
}

void Debug_options::create_options()
{
#ifdef DBUG_OFF
  this->create_new_disabled_option("debug",
    "This is a non-debug version. Catch this and exit.")
    ->set_short_character('#');
  this->create_new_disabled_option("debug-check",
    "This is a non-debug version. Catch this and exit.");
  this->create_new_disabled_option("debug-info",
    "This is a non-debug version. Catch this and exit.");
#else
  this->create_new_option(&this->m_dbug_option, "debug", "Output debug log.")
    ->set_short_character('#')
    ->value_optional()
    ->set_value("d:t:O,/tmp/" + this->m_program->get_name() + ".trace")
    ->add_callback(new Instance_callback<void, char*, Debug_options>(
      this, &Debug_options::debug_option_callback));
  this->create_new_option(&this->m_debug_check_flag, "debug-check",
      "Check memory and open file usage at exit.");
  this->create_new_option(&this->m_debug_info_flag, "debug-info",
      "Print some debug info at exit.")
    ->set_short_character('T');
#endif
}


void Debug_options::debug_option_callback(char *argument MY_ATTRIBUTE((unused)))
{
  if (this->m_dbug_option.has_value())
  {
    DBUG_PUSH(this->m_dbug_option.value().c_str());
  }
  this->m_debug_check_flag= 1;
}


void Debug_options::options_parsed()
{
  if (this->m_debug_info_flag)
  {
    ::my_end_arg|= MY_CHECK_ERROR | MY_GIVE_INFO;
  }
  else if (this->m_debug_check_flag)
  {
    ::my_end_arg|= MY_CHECK_ERROR;
  }
}
