// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <c4/yml/version.hpp>
#include <exception>
#include <memory>
#include <ostream>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <string>

#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/configuration/document.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/sdk/configuration/invalid_schema_exception.h"
#include "opentelemetry/sdk/configuration/ryml_document.h"
#include "opentelemetry/sdk/configuration/ryml_document_node.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

#if RYML_VERSION_MINOR >= 11
[[noreturn]] static void OnErrorBasic(c4::csubstr msg,
                                      const ryml::ErrorDataBasic &errdata,
                                      void * /*user_data*/)
{
  DocumentNodeLocation loc;
  loc.offset   = errdata.location.offset;
  loc.line     = errdata.location.line;
  loc.col      = errdata.location.col;
  loc.filename = std::string(errdata.location.name.str, errdata.location.name.len);

  throw InvalidSchemaException(loc, std::string(msg.str, msg.len));
}

[[noreturn]] static void OnErrorParse(c4::csubstr msg,
                                      const ryml::ErrorDataParse &errdata,
                                      void * /*user_data*/)
{
  DocumentNodeLocation loc;
  loc.offset   = errdata.ymlloc.offset;
  loc.line     = errdata.ymlloc.line;
  loc.col      = errdata.ymlloc.col;
  loc.filename = std::string(errdata.ymlloc.name.str, errdata.ymlloc.name.len);

  throw InvalidSchemaException(loc, std::string(msg.str, msg.len));
}

[[noreturn]] static void OnErrorVisit(c4::csubstr msg,
                                      const ryml::ErrorDataVisit &errdata,
                                      void * /*user_data*/)
{
  DocumentNodeLocation loc;
  loc.offset   = errdata.cpploc.offset;
  loc.line     = errdata.cpploc.line;
  loc.col      = errdata.cpploc.col;
  loc.filename = std::string(errdata.cpploc.name.str, errdata.cpploc.name.len);

  // If rapidyaml fails internally, we declare the yaml document invalid.
  throw InvalidSchemaException(loc, std::string(msg.str, msg.len));
}

#else
[[noreturn]] static void OnError(const char *msg,
                                 size_t msg_len,
                                 ryml::Location location,
                                 void * /*user_data*/)
{
  DocumentNodeLocation loc;
  loc.offset   = location.offset;
  loc.line     = location.line;
  loc.col      = location.col;
  loc.filename = std::string(location.name.str, location.name.len);

  throw InvalidSchemaException(loc, std::string(msg, msg_len));
}
#endif

// Custom ryml error callback that throws instead of calling abort().
// This ensures the try-catch in ParseDocument works regardless of how
// ryml was compiled (with or without RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS).
ryml::Callbacks RymlDocument::MakeCallbacks()
{
  ryml::Callbacks cb = ryml::get_callbacks();
#if RYML_VERSION_MINOR >= 11
  // Starting with rapidyaml 0.11.0
  cb.m_error_basic = OnErrorBasic;
  cb.m_error_parse = OnErrorParse;
  cb.m_error_visit = OnErrorVisit;
#else
  // Up to rapidyaml 0.10.0
  cb.m_error = OnError;
#endif
  return cb;
}

std::unique_ptr<Document> RymlDocument::Parse(const std::string &source, const std::string &content)
{
  auto doc     = std::make_unique<RymlDocument>();
  const int rc = doc->ParseDocument(source, content);
  if (rc == 0)
  {
    return doc;
  }

  return nullptr;
}

int RymlDocument::ParseDocument(const std::string &source, const std::string &content)
{
  opts_.locations(true);
  parser_ = std::make_unique<ryml::Parser>(&event_handler_, opts_);

  ryml::csubstr filename;
  ryml::csubstr csubstr_content;

  filename        = ryml::to_csubstr(source);
  csubstr_content = ryml::to_csubstr(content);

  try
  {
    tree_ = parse_in_arena(parser_.get(), filename, csubstr_content);
    tree_.resolve();
  }
  catch (const std::exception &e)
  {
    OTEL_INTERNAL_LOG_ERROR("[Ryml Document] Parse failed with exception: " << e.what());
    return 1;
  }
  catch (...)
  {
    OTEL_INTERNAL_LOG_ERROR("[Ryml Document] Parse failed with unknown exception.");
    return 2;
  }

  return 0;
}

std::unique_ptr<DocumentNode> RymlDocument::GetRootNode()
{
  auto node = std::make_unique<RymlDocumentNode>(this, tree_.rootref(), 0);
  return node;
}

DocumentNodeLocation RymlDocument::Location(ryml::ConstNodeRef node) const
{
  DocumentNodeLocation loc;
#if RYML_VERSION_MINOR >= 10
  // Starting with rapidyaml 0.10.0
  auto ryml_loc = node.location(*parser_);
#else
  // Up to rapidyaml 0.9.0
  auto ryml_loc = parser_->location(node);
#endif
  loc.offset   = ryml_loc.offset;
  loc.line     = ryml_loc.line;
  loc.col      = ryml_loc.col;
  loc.filename = std::string(ryml_loc.name.str, ryml_loc.name.len);

  return loc;
}

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
