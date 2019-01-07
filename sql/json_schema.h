/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_JSON_SCHEMA_H_INCLUDED
#define SQL_JSON_SCHEMA_H_INCLUDED

#include <cstddef>
#include <string>

/**
  This function will validate a JSON document against a JSON Schema using the
  validation provided by rapidjson.

  @param document_str A pointer to the JSON document to be validated.
  @param document_length The length of the JSON document to be validated.
  @param json_schema_str A pointer to the JSON Schema.
  @param json_schema_length The length of the JSON Schema.
  @param function_name The name of the SQL function calling this function. Used
                       in error reporting.
  @param[out] is_valid A variable containing the result of the validation. If
                       true, the JSON document is valid according to the given
                       JSON Schema.

  @retval true if anything went wrong (like parsing the JSON inputs). my_error
               has been called with an appopriate error message.
  @retval false if the validation succeeded. The result of the validation can be
                found in the output variable "is_valid".
*/
bool is_valid_json_schema(const char *document_str, size_t document_length,
                          const char *json_schema_str,
                          size_t json_schema_length, const char *function_name,
                          bool *is_valid);

#endif
