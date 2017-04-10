/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include <mysql/components/service_implementation.h>
#include "english_greeting_service_imp.h"
#include "example_services.h"

/**
  Retrieves an English greeting message.

  @param [out] hello_string A pointer to string data pointer to store result
    in.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(english_greeting_service_imp::say_hello,
  (const char** hello_string))
{
  *hello_string= "Hello, World.";
  return false;
}

/**
  Retrieves a greeting message language.

  @param [out] language_string A pointer to string data pointer to store name
    of the language in.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(english_greeting_service_imp::get_language,
  (const char** language_string))
{
  *language_string= "English";
  return false;
}
