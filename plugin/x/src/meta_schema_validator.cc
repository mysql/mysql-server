/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/meta_schema_validator.h"

#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "plugin/x/src/xpl_log.h"

namespace xpl {

namespace {

static const char *const k_invalid_document =
    "Validation schema is not a valid JSON document";

inline std::string get_pointer_string(const rapidjson::Pointer &pointer) {
  rapidjson::StringBuffer buff;
  pointer.StringifyUriFragment(buff);
  return {buff.GetString(), buff.GetSize()};
}

using Any = Mysqlx::Datatypes::Any;
using Array = Mysqlx::Datatypes::Array;
using Scalar = Mysqlx::Datatypes::Scalar;
using Object = Mysqlx::Datatypes::Object;

bool scalar_to_json(const Scalar &scalar, rapidjson::Value *value) {
  switch (scalar.type()) {
    case Scalar::V_SINT:
      if (!scalar.has_v_signed_int()) return false;
      value->SetInt64(scalar.v_signed_int());
      break;

    case Scalar::V_UINT:
      if (!scalar.has_v_unsigned_int()) return false;
      value->SetUint64(scalar.v_unsigned_int());
      break;

    case Scalar::V_NULL:
      value->SetNull();
      break;

    case Scalar::V_OCTETS:
      if (!scalar.has_v_octets() || !scalar.v_octets().has_value())
        return false;
      value->SetString(scalar.v_octets().value().c_str(),
                       scalar.v_octets().value().length());
      break;

    case Scalar::V_DOUBLE:
      if (!scalar.has_v_double()) return false;
      value->SetDouble(scalar.v_double());
      break;

    case Scalar::V_FLOAT:
      if (!scalar.has_v_float()) return false;
      value->SetFloat(scalar.v_float());
      break;

    case Scalar::V_BOOL:
      if (!scalar.has_v_bool()) return false;
      value->SetBool(scalar.v_bool());
      break;

    case Scalar::V_STRING:
      if (!scalar.has_v_string() || !scalar.v_string().has_value())
        return false;
      value->SetString(scalar.v_string().value().c_str(),
                       scalar.v_string().value().length());
      break;
  }
  return true;
}

bool any_to_json(const Any &any, rapidjson::Value *value,
                 rapidjson::Document::AllocatorType *alloc);

bool array_to_json(const Array &array, rapidjson::Value *value,
                   rapidjson::Document::AllocatorType *alloc) {
  value->SetArray();
  for (const auto &a : array.value()) {
    rapidjson::Value tmp;
    if (!any_to_json(a, &tmp, alloc)) return false;
    value->PushBack(tmp, *alloc);
  }
  return true;
}

bool object_to_json(const Object &object, rapidjson::Value *value,
                    rapidjson::Document::AllocatorType *alloc) {
  value->SetObject();
  for (const auto &o : object.fld()) {
    rapidjson::Value tmp_value;
    if (!any_to_json(o.value(), &tmp_value, alloc)) return false;
    rapidjson::Value name;
    name.SetString(o.key().c_str(), o.key().length());
    value->AddMember(name, tmp_value, *alloc);
  }
  return true;
}

bool any_to_json(const Any &any, rapidjson::Value *value,
                 rapidjson::Document::AllocatorType *alloc) {
  switch (any.type()) {
    case Any::SCALAR:
      return scalar_to_json(any.scalar(), value);

    case Any::OBJECT:
      return object_to_json(any.obj(), value, alloc);

    case Any::ARRAY:
      return array_to_json(any.array(), value, alloc);
  }
  return false;
}

inline void json_to_string(const rapidjson::Document &document,
                           std::string *json_string) {
  rapidjson::StringBuffer buff;
  rapidjson::Writer<rapidjson::StringBuffer> w(buff);
  document.Accept(w);
  json_string->assign(buff.GetString(), buff.GetSize());
}

}  // namespace

ngs::Error_code Meta_schema_validator::validate(
    const std::string &schema) const {
  rapidjson::Document input_document;
  const rapidjson::ParseResult ok = input_document.Parse(schema.c_str());
  if (!ok) {
    log_debug("JSON schema parse error: %s",
              rapidjson::GetParseError_En(ok.Code()));
    return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA, k_invalid_document);
  }
  return validate_impl(&input_document);
}

ngs::Error_code Meta_schema_validator::validate(
    const Any &schema, std::string *schema_string) const {
  rapidjson::Document input_document;

  switch (schema.type()) {
    case Any::OBJECT: {
      if (!object_to_json(schema.obj(), &input_document,
                          &input_document.GetAllocator()))
        return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA, k_invalid_document);
    } break;

    case Any::SCALAR: {
      if (!schema.scalar().has_v_string() ||
          !schema.scalar().v_string().has_value())
        return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA, k_invalid_document);
      if (schema.scalar().v_string().value().empty()) {
        *schema_string = "";
        return ngs::Success();
      }
      if (input_document.Parse(schema.scalar().v_string().value().c_str())
              .HasParseError())
        return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA, k_invalid_document);
    } break;

    default: {
      return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA, k_invalid_document);
    }
  }

  json_to_string(input_document, schema_string);
  return validate_impl(&input_document);
}

ngs::Error_code Meta_schema_validator::validate_impl(
    rapidjson::Document *input_document) const {
  const auto e = pre_validate(input_document);
  if (e) return e;

  rapidjson::SchemaValidator validator{get_meta_schema()};
  if (input_document->Accept(validator)) return ngs::Success();

  return ngs::Error(
      ER_X_INVALID_VALIDATION_SCHEMA,
      "JSON validation schema location %s failed requirement: "
      "'%s' at meta schema location '%s'",
      get_pointer_string(validator.GetInvalidDocumentPointer()).c_str(),
      validator.GetInvalidSchemaKeyword(),
      get_pointer_string(validator.GetInvalidSchemaPointer()).c_str());
}

ngs::Error_code Meta_schema_validator::pre_validate(
    rapidjson::Document *document) const {
  if (!document->IsObject())
    return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA,
                      "Validation schema is not a valid JSON object");
  return pre_validate_impl(0, rapidjson::Pointer(), document, document);
}

namespace {

bool is_reference_valid(const rapidjson::Document &document,
                        const rapidjson::Value &reference) {
  if (!reference.IsString()) return false;
  const auto pointer =
      rapidjson::Pointer(reference.GetString(), reference.GetStringLength());
  if (!pointer.IsValid()) return false;
  return pointer.Get(document);
}

}  // namespace

ngs::Error_code Meta_schema_validator::pre_validate_impl(
    const uint32_t level, const rapidjson::Pointer &pointer,
    rapidjson::Document *document, rapidjson::Value *value) const {
  if (level > m_max_depth)
    return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA,
                      "Validation schema exceeds the maximum depth on %s",
                      get_pointer_string(pointer).c_str());

  if (value->IsArray()) {
    for (rapidjson::SizeType i = 0; i < value->Size(); i++) {
      const auto e = pre_validate_impl(level + 1, pointer.Append(i), document,
                                       &(*value)[i]);
      if (e) return e;
    }
    return ngs::Success();
  }

  if (!value->IsObject()) return ngs::Success();

  using It = rapidjson::Value::MemberIterator;
  static const rapidjson::Value k_ref("$ref");

  // 'json schema reference' is an object with one member named '$ref'
  It ref = value->FindMember(k_ref);
  // if not '$ref' member - proceed as regular object
  if (ref == value->MemberEnd()) {
    for (It i = value->MemberBegin(); i != value->MemberEnd(); ++i) {
      const auto e = pre_validate_impl(level + 1, pointer.Append(i->name),
                                       document, &(i->value));
      if (e) return e;
    }
    return ngs::Success();
  }

  // this is reference object - check reference string
  if (!is_reference_valid(*document, ref->value))
    return ngs::Error(ER_X_INVALID_VALIDATION_SCHEMA,
                      "Validation schema reference '%s' is not valid",
                      get_pointer_string(pointer).c_str());

  return ngs::Success();
}

const rapidjson::SchemaDocument &Meta_schema_validator::get_meta_schema()
    const {
  static const rapidjson::SchemaDocument meta_schema{get_schema_document()};
  return meta_schema;
}

const rapidjson::Document &Meta_schema_validator::get_schema_document() const {
  static rapidjson::Document document;
  document.Parse(k_reference_schema);
  // additional requirement to raw meta-schema:
  // - not allow additional properties (to avoid typos).
  document.AddMember("additionalProperties", false, document.GetAllocator());
  // - allow '$ref' property as internal json reference
  rapidjson::Document::ValueType reference;
  reference.SetObject();
  reference.AddMember("type", "string", document.GetAllocator());
  reference.AddMember("pattern", "^#(/(.+))*$", document.GetAllocator());
  const auto properties = document.FindMember("properties");
  properties->value.AddMember("$ref", reference, document.GetAllocator());
  return document;
}

// Meta-schema used for validation of a user provided schemas,
// taken from http://json-schema.org/draft-04/schema
const char *const Meta_schema_validator::k_reference_schema = R"({
  "id": "http://json-schema.org/draft-04/schema#",
  "$schema": "http://json-schema.org/draft-04/schema#",
  "description": "Core schema meta-schema",
  "definitions": {
    "schemaArray": {
      "type": "array",
      "minItems": 1,
      "items": { "$ref": "#" }
    },
    "positiveInteger": {
      "type": "integer",
      "minimum": 0
    },
    "positiveIntegerDefault0": {
      "allOf": [
        { "$ref": "#/definitions/positiveInteger" },
        { "default": 0 }
      ]
    },
    "simpleTypes": {
      "enum": [ "array", "boolean", "integer", "null", "number", "object", "string" ]
    },
    "stringArray": {
      "type": "array",
      "items": { "type": "string" },
      "minItems": 1,
      "uniqueItems": true
    }
  },
  "type": "object",
  "properties": {
    "id": { "type": "string" },
    "$schema": { "type": "string" },
    "title": { "type": "string" },
    "description": { "type": "string" },
    "default": {},
    "multipleOf": {
      "type": "number",
      "minimum": 0,
      "exclusiveMinimum": true
    },
    "maximum": { "type": "number" },
    "exclusiveMaximum": {
      "type": "boolean",
      "default": false
    },
    "minimum": { "type": "number" },
    "exclusiveMinimum": {
      "type": "boolean",
      "default": false
    },
    "maxLength": { "$ref": "#/definitions/positiveInteger" },
    "minLength": { "$ref": "#/definitions/positiveIntegerDefault0" },
    "pattern": {
      "type": "string",
      "format": "regex"
    },
    "additionalItems": {
      "anyOf": [
        { "type": "boolean" },
        { "$ref": "#" }
      ],
      "default": {}
    },
    "items": {
      "anyOf": [
        { "$ref": "#" },
        { "$ref": "#/definitions/schemaArray" }
      ],
      "default": {}
    },
    "maxItems": { "$ref": "#/definitions/positiveInteger" },
    "minItems": { "$ref": "#/definitions/positiveIntegerDefault0" },
    "uniqueItems": {
      "type": "boolean",
      "default": false
    },
    "maxProperties": { "$ref": "#/definitions/positiveInteger" },
    "minProperties": { "$ref": "#/definitions/positiveIntegerDefault0" },
    "required": { "$ref": "#/definitions/stringArray" },
    "additionalProperties": {
      "anyOf": [
        { "type": "boolean" },
        { "$ref": "#" }
      ],
      "default": {}
    },
    "definitions": {
      "type": "object",
      "additionalProperties": { "$ref": "#" },
      "default": {}
    },
    "properties": {
      "type": "object",
      "additionalProperties": { "$ref": "#" },
      "default": {}
    },
    "patternProperties": {
      "type": "object",
      "additionalProperties": { "$ref": "#" },
      "default": {}
    },
    "dependencies": {
      "type": "object",
      "additionalProperties": {
        "anyOf": [
          { "$ref": "#" },
          { "$ref": "#/definitions/stringArray" }
        ]
      }
    },
    "enum": {
      "type": "array",
      "minItems": 1,
      "uniqueItems": true
    },
    "type": {
      "anyOf": [
        { "$ref": "#/definitions/simpleTypes" },
        {
          "type": "array",
          "items": { "$ref": "#/definitions/simpleTypes" },
          "minItems": 1,
          "uniqueItems": true
        }
      ]
    },
    "format": { "type": "string" },
    "allOf": { "$ref": "#/definitions/schemaArray" },
    "anyOf": { "$ref": "#/definitions/schemaArray" },
    "oneOf": { "$ref": "#/definitions/schemaArray" },
    "not": { "$ref": "#" }
  },
  "dependencies": {
    "exclusiveMaximum": [ "maximum" ],
    "exclusiveMinimum": [ "minimum" ]
  },
  "default": {}
})";

}  // namespace xpl
