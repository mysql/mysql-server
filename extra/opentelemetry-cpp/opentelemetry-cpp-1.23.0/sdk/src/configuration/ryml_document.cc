// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <exception>
#include <memory>
#include <ostream>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <string>

#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/configuration/document.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/sdk/configuration/ryml_document.h"
#include "opentelemetry/sdk/configuration/ryml_document_node.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

std::unique_ptr<Document> RymlDocument::Parse(const std::string &source, const std::string &content)
{
  ryml::ParserOptions opts;
  opts.locations(true);

  ryml::Parser::handler_type event_handler;
  ryml::Parser parser(&event_handler, opts);

  ryml::Tree tree;
  ryml::csubstr filename;
  ryml::csubstr csubstr_content;
  std::unique_ptr<Document> doc;

  filename        = ryml::to_csubstr(source);
  csubstr_content = ryml::to_csubstr(content);

  try
  {
    tree = parse_in_arena(&parser, filename, csubstr_content);
    tree.resolve();
  }
  catch (const std::exception &e)
  {
    OTEL_INTERNAL_LOG_ERROR("[Ryml Document] Parse failed with exception: " << e.what());
    return doc;
  }
  catch (...)
  {
    OTEL_INTERNAL_LOG_ERROR("[Ryml Document] Parse failed with unknown exception.");
    return doc;
  }

  doc = std::make_unique<RymlDocument>(tree);
  return doc;
}

std::unique_ptr<DocumentNode> RymlDocument::GetRootNode()
{
  auto node = std::make_unique<RymlDocumentNode>(tree_.rootref(), 0);
  return node;
}

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
