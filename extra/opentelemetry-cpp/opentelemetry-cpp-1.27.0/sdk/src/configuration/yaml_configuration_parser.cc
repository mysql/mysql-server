// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <exception>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "opentelemetry/sdk/common/env_variables.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/configuration_parser.h"
#include "opentelemetry/sdk/configuration/document.h"
#include "opentelemetry/sdk/configuration/invalid_schema_exception.h"
#include "opentelemetry/sdk/configuration/ryml_document.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

std::unique_ptr<Configuration> YamlConfigurationParser::ParseFile(const std::string &filename)
{
  std::string input_file = filename;

  if (input_file.empty())
  {
    static std::string env_var_name("OTEL_CONFIG_FILE");
    std::string env_var;
    const bool env_exists =
        sdk::common::GetStringEnvironmentVariable(env_var_name.c_str(), env_var);

    if (env_exists)
    {
      input_file = env_var;
    }
  }

  if (input_file.empty())
  {
    input_file = "config.yaml";
  }

  std::unique_ptr<Configuration> conf;
  std::ifstream in(input_file, std::ios::binary);
  if (!in.is_open())
  {
    OTEL_INTERNAL_LOG_ERROR("Failed to open yaml file <" << input_file << ">.");
  }
  else
  {
    std::ostringstream content;
    content << in.rdbuf();
    conf = YamlConfigurationParser::ParseString(input_file, content.str());
  }

  return conf;
}

std::unique_ptr<Configuration> YamlConfigurationParser::ParseString(const std::string &source,
                                                                    const std::string &content)
{
  std::unique_ptr<Document> doc;
  std::unique_ptr<Configuration> config;
  ConfigurationParser config_parser;

  doc = RymlDocument::Parse(source, content);

  try
  {
    if (doc)
    {
      config = config_parser.Parse(std::move(doc));
    }
  }
  catch (const InvalidSchemaException &e)
  {
    OTEL_INTERNAL_LOG_ERROR(e.Where() << ": " << e.what());
  }
  catch (const std::exception &e)
  {
    OTEL_INTERNAL_LOG_ERROR(
        "[Yaml Configuration Parser] Parse failed with exception: " << e.what());
  }
  catch (...)
  {
    OTEL_INTERNAL_LOG_ERROR("[Yaml Configuration Parser] Parse failed with unknown exception.");
  }

  return config;
}

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
