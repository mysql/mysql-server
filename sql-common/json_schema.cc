/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql-common/json_schema.h"

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <assert.h>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <string>
#include <utility>

#include "my_alloc.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "sql-common/json_syntax_check.h"

/**
  Json_schema_validator_impl is an object that contains a JSON Schema that can
  be re-used multiple times. This is useful in the cases where we have a JSON
  Schema that doesn't change (which should be quite often).
*/
class Json_schema_validator_impl {
 public:
  /**
    Construct the cached JSON Schema with the provided JSON document

    @param schema_document A JSON document that contains the JSON Schema
                           definition
  */
  Json_schema_validator_impl(const rapidjson::Document &schema_document);

  /**
    Validate a JSON input against the cached JSON Schema

    @param document_str A pointer to the JSON input
    @param document_length The length of the JSON input
    @param error_handler Error handlers to be called when parsing errors occur.
    @param depth_handler Pointer to a function that should handle error
                         occurred when depth is exceeded.
    @param[out] is_valid The result of the validation
    @param[out] report A structure containing a detailed report from the
                       validation. Is only populated if is_valid is set to
                       "false" Can be nullptr if a detailed report isn't needed.

    @retval true on error (my_error has been called)
    @retval false on success (validation result can be found in the output
            parameter is_valid)
  */
  bool is_valid_json_schema(const char *document_str, size_t document_length,
                            const JsonSchemaErrorHandler &error_handler,
                            const JsonErrorHandler &depth_handler,
                            bool *is_valid,
                            Json_schema_validation_report *report) const;

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
    using rapidjson::IRemoteSchemaDocumentProvider::GetRemoteDocument;

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
  parse_json_schema will parse a JSON input into a JSON Schema. If the input
  isn't a valid JSON, or if the JSON is too deeply nested, an error will be
  returned to the user.

  @param json_schema_str A pointer to the JSON Schema input
  @param json_schema_length The length of the JSON Schema input
  @param error_handler Error handlers to be called when parsing errors occur.
  @param depth_handler Pointer to a function that should handle error
                       occurred when depth is exceeded.
  @param[out] schema_document An object where the JSON Schema will be put. This
              variable MUST be initialized.

  @retval true on error (my_error has been called)
  @retval false on success. The JSON Schema can be found in the output
          parameter schema_document.
*/
static bool parse_json_schema(const char *json_schema_str,
                              size_t json_schema_length,
                              const JsonSchemaErrorHandler &error_handler,
                              const JsonErrorHandler &depth_handler,
                              rapidjson::Document *schema_document) {
  assert(schema_document != nullptr);

  // Check if the JSON schema is valid. Invalid JSON would be caught by
  // rapidjson::Document::Parse, but it will not catch documents that are too
  // deeply nested.
  size_t error_offset;
  std::string error_message;
  if (!is_valid_json_syntax(json_schema_str, json_schema_length, &error_offset,
                            &error_message, depth_handler)) {
    error_handler.InvalidJsonText(1, error_message.c_str(), error_offset);
    return true;
  }

  if (schema_document->Parse(json_schema_str, json_schema_length)
          .HasParseError()) {
    // The document should already be valid, since is_valid_json_syntax
    // succeeded.
    assert(false);
    return true;
  }

  // We require the JSON Schema to be an object
  if (!schema_document->IsObject()) {
    error_handler.InvalidJsonType();
    return true;
  }

  return false;
}

bool is_valid_json_schema(const char *document_str, size_t document_length,
                          const char *json_schema_str,
                          size_t json_schema_length,
                          const JsonSchemaErrorHandler &error_handler,
                          const JsonErrorHandler &depth_handler, bool *is_valid,
                          Json_schema_validation_report *validation_report) {
  rapidjson::Document schema_document;
  if (parse_json_schema(json_schema_str, json_schema_length, error_handler,
                        depth_handler, &schema_document)) {
    return true;
  }

  return Json_schema_validator_impl(schema_document)
      .is_valid_json_schema(document_str, document_length, error_handler,
                            depth_handler, is_valid, validation_report);
}

Json_schema_validator_impl::Json_schema_validator_impl(
    const rapidjson::Document &schema_document)
    : m_cached_schema(schema_document, /*uri=*/nullptr, /*uriLength=*/0,
                      &m_remote_document_provider) {}

bool Json_schema_validator::initialize(
    MEM_ROOT *mem_root, const char *json_schema_str, size_t json_schema_length,
    const JsonSchemaErrorHandler &error_handler,
    const JsonErrorHandler &depth_handler) {
  rapidjson::Document schema_document;
  if (parse_json_schema(json_schema_str, json_schema_length, error_handler,
                        depth_handler, &schema_document)) {
    return true;
  }

  m_json_schema_validator =
      new (mem_root) Json_schema_validator_impl(schema_document);
  return m_json_schema_validator == nullptr;
}

bool Json_schema_validator::is_valid(
    const char *document_str, size_t document_length,
    const JsonSchemaErrorHandler &error_handler,
    const JsonErrorHandler &depth_handler, bool *is_valid,
    Json_schema_validation_report *report) const {
  return m_json_schema_validator->is_valid_json_schema(
      document_str, document_length, error_handler, depth_handler, is_valid,
      report);
}

Json_schema_validator::~Json_schema_validator() {
  if (m_json_schema_validator != nullptr) {
    ::destroy_at(m_json_schema_validator);
  }
}

bool Json_schema_validator_impl::is_valid_json_schema(
    const char *document_str, size_t document_length,
    const JsonSchemaErrorHandler &error_handler,
    const JsonErrorHandler &depth_handler, bool *is_valid,
    Json_schema_validation_report *validation_report) const {
  // Set up the JSON Schema validator using Syntax_check_handler that will catch
  // JSON documents that are too deeply nested.
  Syntax_check_handler syntaxCheckHandler(depth_handler);
  rapidjson::GenericSchemaValidator<rapidjson::SchemaDocument,
                                    Syntax_check_handler>
      validator(m_cached_schema, syntaxCheckHandler);

  rapidjson::Reader reader;
  rapidjson::MemoryStream stream(document_str, document_length);

  // Wrap this in a try-catch since rapidjson calls std::regex_search
  // (which isn't noexcept).
  try {
    rapidjson::ParseResult parse_success = reader.Parse(stream, validator);
    // We may end up in a few different error scenarios here:
    // 1) The document is valid JSON, but invalid according to schema.
    //   - parse_success will indicate error, and validator.IsValid() is false.
    // 2) The JSON document is invalid (parsing failed), but not too deep.
    //   - parse_success will indicate error, and validator.IsValid() is true.
    // 3) The JSON document is too deep.
    //   - parse_success will indicate error, and validator.IsValid() is false.
    //     The only way do distinguish this from case 1, is to see if the
    //     syntax check handler has raised an error.
    if (syntaxCheckHandler.too_deep_error_raised()) {
      // The JSON document was too deep, and an error is already reported by the
      // Syntax_check_handler.
      return true;
    }

    if (!parse_success && validator.IsValid()) {
      // Couldn't parse the JSON document.
      std::pair<std::string, size_t> error = get_error_from_reader(reader);
      error_handler.InvalidJsonText(2, error.first.c_str(), error.second);
      return true;
    }

    // Otherwise, we have a syntactically correct JSON document, so we can
    // safely check the result from the validator.
  } catch (...) {
    error_handler.HandleStdExceptions();
    return true;
  }

  // If we encountered a remote reference in the JSON schema, report an error
  // back to the user that this isn't supported.
  if (m_remote_document_provider.used()) {
    error_handler.NotSupported();
    return true;
  }

  *is_valid = validator.IsValid();
  if (!validator.IsValid() && validation_report != nullptr) {
    // Populate the validation report. Since the validator is local to this
    // function, all strings provided by the validator must be allocated so
    // that they survive beyond this function.
    rapidjson::StringBuffer string_buffer;

    // Where in the JSON Schema the validation failed.
    validator.GetInvalidSchemaPointer().StringifyUriFragment(string_buffer);
    std::string schema_location(string_buffer.GetString(),
                                string_buffer.GetSize());

    // Where in the JSON document the validation failed.
    string_buffer.Clear();
    validator.GetInvalidDocumentPointer().StringifyUriFragment(string_buffer);
    std::string document_location(string_buffer.GetString(),
                                  string_buffer.GetSize());

    validation_report->set_error_report(std::move(schema_location),
                                        validator.GetInvalidSchemaKeyword(),
                                        std::move(document_location));
  }

  return false;
}

std::string Json_schema_validation_report::human_readable_reason() const {
  std::string reason;
  reason.append("The JSON document location '");
  reason.append(document_location());
  reason.append("' failed requirement '");
  reason.append(schema_failed_keyword());
  reason.append("' at JSON Schema location '");
  reason.append(schema_location());
  reason.append("'");
  return reason;
}
