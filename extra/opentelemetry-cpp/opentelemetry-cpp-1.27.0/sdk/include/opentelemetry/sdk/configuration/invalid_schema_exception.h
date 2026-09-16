// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>

#include "opentelemetry/sdk/configuration/document.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class InvalidSchemaException : public std::runtime_error
{
public:
  InvalidSchemaException(const DocumentNodeLocation &location, const std::string &msg)
      : std::runtime_error(msg), location_(location)
  {}

  std::string Where() const { return location_.ToString(); }

private:
  DocumentNodeLocation location_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
