/*
   Copyright (c) 2001, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "password_option.h"

using namespace Mysql::Tools::Base::Options;
using Mysql::Nullable;
using std::string;

Password_option::Password_option(Nullable<string>* value, string name, string description)
  : Abstract_string_option<Password_option>(value, GET_PASSWORD, name, description)
{
  this->value_optional()
    ->add_callback(
    new Instance_callback<void, char*, Password_option>(
      this, &Password_option::password_callback));
}

void Password_option::password_callback(char* argument)
{
  if (argument == ::disabled_my_option)
  {
    // This prevents ::disabled_my_option being overriden later in this function.
    argument= (char*) "";
  }

  if (argument != NULL)
  {
    /*
     Destroy argument value, this modifies part of argv passed to main
     routine. This makes command line on linux changed, so no user can see
     password shortly after program starts. This works for example for
     /proc/<pid>/cmdline file and ps tool.
     */
    for (char* pos= argument; *pos != 0; pos++)
    {
      *pos= '*';
    }

    /*
     This cuts argument length to hide password length on linux commandline
     showing tools.
     */
    if (*argument)
      argument[1]= 0;
  }
  else
  {
    char *password= ::get_tty_password(NULL);
    *this->m_destination_value = Nullable<string>(password);
    my_free(password);

  }
}
