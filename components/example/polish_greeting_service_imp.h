/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef POLISH_GREETING_SERVICE_IMP_H
#define POLISH_GREETING_SERVICE_IMP_H

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>

/**
  An implementation of the example Services to get Polish greeting message and
  its localization information.
*/
class polish_greeting_service_imp {
 public:
  /**
    Retrieves a Polish greeting message.

    @param [out] hello_string A pointer to string data pointer to store result
    in.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(say_hello, (const char **hello_string));

  /**
    Retrieves a greeting message language.

    @param [out] language_string A pointer to string data pointer to store name
    of the language in.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(get_language, (const char **language_string));
};

#endif /* POLISH_GREETING_SERVICE_IMP_H */
