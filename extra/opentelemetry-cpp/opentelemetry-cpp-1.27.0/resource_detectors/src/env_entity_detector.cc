// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/string_util.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/resource_detectors/env_entity_detector.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/env_variables.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/resource_detector.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace resource_detector
{

namespace
{

constexpr const char *kOtelEntities = "OTEL_ENTITIES";

struct ParsedEntity
{
  std::string type;
  opentelemetry::sdk::resource::ResourceAttributes id_attrs;
  opentelemetry::sdk::resource::ResourceAttributes desc_attrs;
  std::string schema_url;
  std::string identity_key;  // Pre-computed identity key for duplicate detection
};

std::string BuildEntityIdentityKey(const std::string &type,
                                   const opentelemetry::sdk::resource::ResourceAttributes &id_attrs)
{
  using AttrPtr =
      const std::pair<const std::string, opentelemetry::sdk::common::OwnedAttributeValue> *;
  std::vector<AttrPtr> items;
  items.reserve(id_attrs.size());
  for (const auto &kv : id_attrs)
  {
    items.push_back(&kv);
  }
  std::sort(items.begin(), items.end(), [](AttrPtr a, AttrPtr b) { return a->first < b->first; });

  std::string key = type + "|";
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (i > 0)
    {
      key += ",";
    }
    key += items[i]->first;
    key += "=";
    const auto *str_val = opentelemetry::nostd::get_if<std::string>(&items[i]->second);
    if (str_val != nullptr)
    {
      key += *str_val;
    }
  }
  return key;
}

std::string PercentDecode(opentelemetry::nostd::string_view value) noexcept
{
  if (value.find('%') == opentelemetry::nostd::string_view::npos)
  {
    return std::string(value);
  }

  std::string result;
  result.reserve(value.size());

  auto IsHex = [](char c) {
    return std::isdigit(static_cast<unsigned char>(c)) || (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
  };

  auto FromHex = [](char c) -> char {
    return static_cast<char>(std::isdigit(static_cast<unsigned char>(c))
                                 ? c - '0'
                                 : std::toupper(static_cast<unsigned char>(c)) - 'A' + 10);
  };

  for (size_t i = 0; i < value.size(); ++i)
  {
    if (value[i] == '%' && i + 2 < value.size() && IsHex(value[i + 1]) && IsHex(value[i + 2]))
    {
      result.push_back(static_cast<char>((FromHex(value[i + 1]) << 4) | FromHex(value[i + 2])));
      i += 2;
    }
    else
    {
      result.push_back(value[i]);
    }
  }

  return result;
}

bool IsValidSchemaUrl(const std::string &url) noexcept
{
  if (url.empty())
  {
    return false;
  }

  // If absolute URI (has ://), validate scheme
  size_t scheme_end = url.find("://");
  if (scheme_end != std::string::npos)
  {
    if (scheme_end == 0 || scheme_end + 3 >= url.size())
    {
      return false;  // Empty scheme or no content after ://
    }
    // Scheme must start with letter
    if (!std::isalpha(static_cast<unsigned char>(url[0])))
    {
      return false;
    }
    // Scheme can contain letters, digits, +, -, .
    for (size_t i = 1; i < scheme_end; ++i)
    {
      char c = url[i];
      if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.'))
      {
        return false;
      }
    }
    return true;
  }

  // Relative URI - accept any non-empty string
  return true;
}

void ParseKeyValueList(const std::string &input,
                       opentelemetry::sdk::resource::ResourceAttributes &out)
{
  std::istringstream iss(input);
  std::string token;
  while (std::getline(iss, token, ','))
  {
    token = std::string{opentelemetry::common::StringUtil::Trim(token)};
    if (token.empty())
    {
      continue;
    }
    size_t pos = token.find('=');
    if (pos == std::string::npos)
    {
      continue;
    }
    std::string key   = token.substr(0, pos);
    std::string value = token.substr(pos + 1);
    key               = std::string{opentelemetry::common::StringUtil::Trim(key)};
    value             = std::string{opentelemetry::common::StringUtil::Trim(value)};
    if (key.empty())
    {
      continue;
    }
    out[key] = PercentDecode(value);
  }
}

bool ParseSingleEntity(const std::string &entity_str, ParsedEntity &out)
{
  if (entity_str.empty())
  {
    return false;
  }

  // type is everything before first '{'
  size_t brace_pos = entity_str.find('{');
  if (brace_pos == std::string::npos || brace_pos == 0)
  {
    return false;
  }

  out.type = std::string{opentelemetry::common::StringUtil::Trim(entity_str.substr(0, brace_pos))};

  // Validate type matches [a-zA-Z][a-zA-Z0-9._-]*
  if (out.type.empty() || !std::isalpha(static_cast<unsigned char>(out.type[0])))
  {
    return false;
  }
  for (size_t i = 1; i < out.type.size(); ++i)
  {
    char c = out.type[i];
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-'))
    {
      return false;
    }
  }

  // Extract id_attrs in {...}
  size_t id_start = brace_pos + 1;
  size_t id_end   = entity_str.find('}', id_start);
  if (id_end == std::string::npos || id_end <= id_start)
  {
    return false;
  }
  std::string id_block = std::string{
      opentelemetry::common::StringUtil::Trim(entity_str.substr(id_start, id_end - id_start))};
  ParseKeyValueList(id_block, out.id_attrs);
  if (out.id_attrs.empty())
  {
    return false;
  }

  // Pre-compute identity key for duplicate detection.
  out.identity_key = BuildEntityIdentityKey(out.type, out.id_attrs);

  size_t cursor = id_end + 1;

  // Optional desc_attrs in [...]
  if (cursor < entity_str.size() && entity_str[cursor] == '[')
  {
    size_t desc_start = cursor + 1;
    size_t desc_end   = entity_str.find(']', desc_start);
    if (desc_end == std::string::npos || desc_end <= desc_start)
    {
      return false;
    }
    std::string desc_block = std::string{opentelemetry::common::StringUtil::Trim(
        entity_str.substr(desc_start, desc_end - desc_start))};
    ParseKeyValueList(desc_block, out.desc_attrs);
    cursor = desc_end + 1;
  }

  // Optional schema URL: '@...'
  if (cursor < entity_str.size() && entity_str[cursor] == '@')
  {
    out.schema_url =
        std::string{opentelemetry::common::StringUtil::Trim(entity_str.substr(cursor + 1))};

    // TODO: Use a proper Schema URL validator when available.
    if (!IsValidSchemaUrl(out.schema_url))
    {
      OTEL_INTERNAL_LOG_WARN(
          "[EnvEntityDetector] Invalid schema URL in OTEL_ENTITIES, ignoring schema URL.");
      out.schema_url.clear();
    }
  }

  return true;
}

std::vector<ParsedEntity> ParseEntities(const std::string &entities_str)
{
  std::vector<ParsedEntity> entities;

  std::istringstream iss(entities_str);
  std::string token;
  while (std::getline(iss, token, ';'))
  {
    token = std::string{opentelemetry::common::StringUtil::Trim(token)};
    if (token.empty())
    {
      continue;
    }
    ParsedEntity entity;
    if (ParseSingleEntity(token, entity))
    {
      entities.push_back(std::move(entity));
    }
    else
    {
      OTEL_INTERNAL_LOG_WARN(
          "[EnvEntityDetector] Skipping malformed entity definition in OTEL_ENTITIES.");
    }
  }

  return entities;
}

}  // namespace

opentelemetry::sdk::resource::Resource EnvEntityDetector::Detect() noexcept
{
  std::string entities_str;
  bool exists =
      opentelemetry::sdk::common::GetStringEnvironmentVariable(kOtelEntities, entities_str);

  if (!exists || entities_str.empty())
  {
    return opentelemetry::sdk::resource::ResourceDetector::Create({});
  }

  auto parsed_entities = ParseEntities(entities_str);
  if (parsed_entities.empty())
  {
    return opentelemetry::sdk::resource::ResourceDetector::Create({});
  }

  opentelemetry::sdk::resource::ResourceAttributes resource_attrs;
  std::string schema_url;

  std::unordered_map<std::string, size_t> entity_index_by_identity;
  entity_index_by_identity.reserve(parsed_entities.size());
  for (size_t i = 0; i < parsed_entities.size(); ++i)
  {
    const std::string &identity_key = parsed_entities[i].identity_key;
    auto it                         = entity_index_by_identity.find(identity_key);
    if (it != entity_index_by_identity.end())
    {
      OTEL_INTERNAL_LOG_WARN(
          "[EnvEntityDetector] Duplicate entity definition in OTEL_ENTITIES, using last "
          "occurrence.");
      it->second = i;
      continue;
    }
    entity_index_by_identity.emplace(identity_key, i);
  }

  for (size_t i = 0; i < parsed_entities.size(); ++i)
  {
    const std::string &identity_key = parsed_entities[i].identity_key;
    auto it                         = entity_index_by_identity.find(identity_key);

    // Only process if this is the last occurrence for this identity.
    if (it == entity_index_by_identity.end() || it->second != i)
    {
      continue;
    }

    const auto &entity = parsed_entities[i];

    // Add identifying attributes.
    for (const auto &attr : entity.id_attrs)
    {
      auto existing = resource_attrs.find(attr.first);
      if (existing != resource_attrs.end() && existing->second != attr.second)
      {
        OTEL_INTERNAL_LOG_WARN(
            "[EnvEntityDetector] Conflicting identifying attribute in OTEL_ENTITIES, "
            "preserving value from last entity.");
      }
      resource_attrs[attr.first] = attr.second;
    }

    // Add descriptive attributes.
    for (const auto &attr : entity.desc_attrs)
    {
      auto existing = resource_attrs.find(attr.first);
      if (existing != resource_attrs.end() && existing->second != attr.second)
      {
        OTEL_INTERNAL_LOG_WARN(
            "[EnvEntityDetector] Conflicting descriptive attribute in OTEL_ENTITIES, "
            "using value from last entity.");
      }
      resource_attrs[attr.first] = attr.second;
    }

    if (!entity.schema_url.empty())
    {
      schema_url = entity.schema_url;
    }
  }

  return opentelemetry::sdk::resource::ResourceDetector::Create(resource_attrs, schema_url);
}

}  // namespace resource_detector
OPENTELEMETRY_END_NAMESPACE
