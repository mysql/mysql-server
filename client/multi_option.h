/*
   Copyright (c) 2012, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql.h"
#include "mysql/service_mysql_alloc.h"
#include "prealloced_array.h"

#ifndef _MULTI_OPTION_H_
#define _MULTI_OPTION_H_

/**
  Class for handling multiple options like e.g. --init-command,
  --init-command-add
*/
class Multi_option {
  /**
    Type of the internal container
  */
  using Multi_option_container = Prealloced_array<char *, 5>;

 public:
  /**
    Constaexpr constructor
  */
  constexpr Multi_option() : option_values(nullptr) {}

  /**
    Adds option value to the container

    @param value [in]: value of the option
    @param clear [in]: if true the container will be cleared before adding the
    command
  */
  void add_value(char *value, bool clear);

  /**
    Sets options to MYSQL structure.

    @param mysql [in, out]: pointer to MYSQL structure to be augmented with the
                            option
    @param option [in]: option to be set
  */
  void set_mysql_options(MYSQL *mysql, mysql_option option);

  /**
    Free the commands
  */
  void free();

 private:
  /**
    The internal container with values
  */
  Multi_option_container *option_values;
};

#endif  //_MULTI_OPTION_H_
