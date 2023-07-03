/*
   Copyright (c) 2001, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "client/base/password_option.h"
#include "multi_factor_passwordopt-vars.h"

#include <stddef.h>
#include <functional>
#include <optional>

#include "client/client_priv.h"

using namespace Mysql::Tools::Base::Options;
using std::string;
using std::placeholders::_1;

Password_option::Password_option(std::optional<string> *value, string name,
                                 string description)
    : Abstract_string_option<Password_option>(value, GET_PASSWORD, name,
                                              description) {
  this->value_optional()->add_callback(new std::function<void(char *)>(
      std::bind(&Password_option::password_callback, this, _1)));
}

Password_option::~Password_option() { free_passwords(); }

void Password_option::password_callback(char *argument) {
  struct my_option opt = get_my_option();
  parse_command_line_password_option(&opt, argument);
  unsigned int factor = 0;
  if (strcmp(opt.name, "password"))
    factor = opt.name[strlen("password")] - '0' - 1;
  if (opt_password[factor] != nullptr)
    *this->m_destination_value = std::optional<string>(opt_password[factor]);
}
