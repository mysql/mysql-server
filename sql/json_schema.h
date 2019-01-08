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

/**
  @file json_schema.h

  Functions for validating a string against a JSON Schema

  A JSON Schema is a way to describe the structure of a JSON document. The JSON
  Schema is a JSON document in itself, and allows you to define required
  names/attributes, data types etc. As an example, here is a minimal example of
  a JSON Schema describing that the JSON document MUST be an object:

    {
      "type": "object"
    }

  If the JSON document to be validated is anything else than an object (array,
  scalar), the validation will fail.

  This file contains one class for validation JSON documents against a cached
  JSON Schema, and free functions for validation any string input against a
  (unparsed) JSON Schema. We use the rapidjson library to do the actual
  validation with the following notable behaviors:

  1) Remote references are not supported. If the user provides a JSON Schema
     with a remote reference, an error will be raised.
  2) JSON Schema supports regex patterns, and we use std::regex as the regex
     engine. If an invalid regex pattern is provided in the JSON Schema, the
     regex pattern will be silently ignored.
  3) rapidjson currently supports JSON Schema draft-v4, while there are newer
     versions available (as of writing, draft-v7 is the latest version).
 */

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/schema.h>
#include <cstddef>

#include "my_alloc.h"

struct MEM_ROOT;

/**
  Json_schema_validator is an object that contains a JSON Schema that can
  be re-used multiple times. This is useful in the cases where we have a JSON
  Schema that doesn't change (which should be quite often).
*/
class Json_schema_validator {
 public:
  /**
    Construct the cached JSON Schema with the provided JSON document

    @param schema_document A JSON document that contains the JSON Schema
                           definition
  */
  Json_schema_validator(const rapidjson::Document &schema_document);

  /**
    Validate a JSON input against the cached JSON Schema

    @param document_str A pointer to the JSON input
    @param document_length The length of the JSON input
    @param function_name The function name of the caller (to be used in error
                         reporting)
    @param[out] is_valid The result of the validation

    @retval true on error (my_error has been called)
    @retval false on success (validation result can be found in the output
            parameter is_valid)
  */
  bool is_valid_json_schema(const char *document_str, size_t document_length,
                            const char *function_name, bool *is_valid) const;

 private:
  /**
   This object acts as a handler/callback for the JSON schema validator and is
   called whenever a schema reference is encountered in the JSON document. Since
   MySQL doesn't support schema references, this class is only used to detect
   whether or not we actually found one in the JSON document.
 */
  class My_remote_schema_document_provider
      : public rapidjson::IRemoteSchemaDocumentProvider {
   public:
    const rapidjson::SchemaDocument *GetRemoteDocument(
        const char *, rapidjson::SizeType) override {
      m_used = true;
      return nullptr;
    }

    bool used() const { return m_used; }

   private:
    bool m_used{false};
  };

  My_remote_schema_document_provider m_remote_document_provider;
  rapidjson::SchemaDocument m_cached_schema;
};

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

/**
  Create a Json_schema_validator, allocated on a given MEM_ROOT

  @param mem_root The MEM_ROOT to allocate the validator on
  @param json_schema_str A pointer to the JSON Schema
  @param json_schema_length The length of the JSON Schema input
  @param function_name The function name of the caller (to be used in error
                        reporting)

  @retval nullptr on error (my_error has been called)
*/
unique_ptr_destroy_only<Json_schema_validator> create_json_schema_validator(
    MEM_ROOT *mem_root, const char *json_schema_str, size_t json_schema_length,
    const char *function_name);

#endif
