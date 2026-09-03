// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <ryml.hpp>
#include <string>

#include "opentelemetry/sdk/configuration/document.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class RymlDocument : public Document
{
public:
  static std::unique_ptr<Document> Parse(const std::string &source, const std::string &content);

  RymlDocument() : event_handler_(MakeCallbacks()) {}
  RymlDocument(RymlDocument &&)                      = delete;
  RymlDocument(const RymlDocument &)                 = delete;
  RymlDocument &operator=(RymlDocument &&)           = delete;
  RymlDocument &operator=(const RymlDocument &other) = delete;
  ~RymlDocument() override                           = default;

  int ParseDocument(const std::string &source, const std::string &content);

  std::unique_ptr<DocumentNode> GetRootNode() override;

  DocumentNodeLocation Location(ryml::ConstNodeRef node) const;

private:
  static ryml::Callbacks MakeCallbacks();

  ryml::ParserOptions opts_;
  ryml::Parser::handler_type event_handler_;
  std::unique_ptr<ryml::Parser> parser_;
  ryml::Tree tree_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
