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

#include "sql/json_schema.h"

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/error/en.h>
#include <rapidjson/error/error.h>
#include <rapidjson/schema.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/json_syntax_check.h"
#include "sql/sql_exception_handler.h"

// This object acts as a handler/callback for the JSON schema validator and it
// called whenever a schema reference is encountered in the JSON document. Since
// MySQL doesn't support schema references, this class is only used to detect
// whether or not we actually found one in the JSON document.
namespace {
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
}  // namespace

bool is_valid_json_schema(const char *documentStr, size_t documentLength,
                          const char *jsonSchemaStr, size_t jsonSchemaLength,
                          const char *function_name, bool *is_valid) {
  // Check if the JSON schema is valid. Invalid JSON would be caught by
  // rapidjson::Document::Parse, but it will not catch documents that are too
  // deeply nested.
  size_t error_offset;
  std::string error_message;
  if (!is_valid_json_syntax(jsonSchemaStr, jsonSchemaLength, &error_offset,
                            &error_message)) {
    my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), 1, function_name,
             error_message.c_str(), error_offset, "");
    return true;
  }

  rapidjson::Document sd;
  if (sd.Parse(jsonSchemaStr, jsonSchemaLength).HasParseError()) {
    // The document should already be valid, since is_valid_json_syntax
    // succeeded.
    DBUG_ASSERT(false);
    return true;
  }

  // We require the JSON Schema to be an object
  if (!sd.IsObject()) {
    my_error(ER_INVALID_JSON_TYPE, MYF(0), 1, function_name, "object");
    return true;
  }

  // Set up the JSON Schema validator using:
  // a) Syntax_check_handler that will catch JSON documents that are too deeply
  //    nested.
  // b) My_remote_schema_document_provider that will catch usage of remote
  //    references.
  Syntax_check_handler syntaxCheckHandler;
  My_remote_schema_document_provider schemaDocumentProvider;
  rapidjson::SchemaDocument schema(sd, &schemaDocumentProvider);
  rapidjson::GenericSchemaValidator<rapidjson::SchemaDocument,
                                    Syntax_check_handler>
      validator(schema, syntaxCheckHandler);

  rapidjson::Reader reader;
  rapidjson::MemoryStream stream(documentStr, documentLength);

  // Wrap this in a try-catch since rapidjson calls std::regex_search
  // (which isn't noexcept).
  try {
    if (!reader.Parse(stream, validator) && validator.IsValid()) {
      // If the parsing was aborted due to deeply nested JSON document, the
      // Syntax_check_handler will already have reported an error. If that's not
      // the case, we will have to report the error here.
      if (!syntaxCheckHandler.too_deep_error_raised()) {
        std::pair<std::string, size_t> error = get_error_from_reader(reader);
        my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), 2, function_name,
                 error.first.c_str(), error.second, "");
      }
      return true;
    }
  } catch (...) {
    handle_std_exception(function_name);
    return true;
  }

  if (schemaDocumentProvider.used()) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "references in JSON Schema");
    return true;
  }

  *is_valid = validator.IsValid();
  return false;
}
