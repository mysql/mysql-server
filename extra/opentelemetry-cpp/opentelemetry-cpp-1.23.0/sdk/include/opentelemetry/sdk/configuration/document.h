// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class Document
{
public:
  Document()                                 = default;
  Document(Document &&)                      = default;
  Document(const Document &)                 = default;
  Document &operator=(Document &&)           = default;
  Document &operator=(const Document &other) = default;
  virtual ~Document()                        = default;

  virtual std::unique_ptr<DocumentNode> GetRootNode() = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
