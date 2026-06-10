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

  RymlDocument(ryml::Tree tree) : tree_(std::move(tree)) {}
  RymlDocument(RymlDocument &&)                      = delete;
  RymlDocument(const RymlDocument &)                 = delete;
  RymlDocument &operator=(RymlDocument &&)           = delete;
  RymlDocument &operator=(const RymlDocument &other) = delete;
  ~RymlDocument() override                           = default;

  std::unique_ptr<DocumentNode> GetRootNode() override;

private:
  ryml::Tree tree_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
