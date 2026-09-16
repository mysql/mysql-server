/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace mcp
{

/**
  The name of the request or notification method.
 */
static constexpr const char *kMcpMethodName = "mcp.method.name";

/**
  The <a href="https://modelcontextprotocol.io/specification/versioning">version</a> of the Model
  Context Protocol used.
 */
static constexpr const char *kMcpProtocolVersion = "mcp.protocol.version";

/**
  The value of the resource uri.
  <p>
  This is a URI of the resource provided in the following requests or notifications: @code
  resources/read @endcode, @code resources/subscribe @endcode, @code resources/unsubscribe @endcode,
  or @code notifications/resources/updated @endcode.
 */
static constexpr const char *kMcpResourceUri = "mcp.resource.uri";

/**
  Identifies <a
  href="https://modelcontextprotocol.io/specification/2025-06-18/basic/transports#session-management">MCP
  session</a>.
 */
static constexpr const char *kMcpSessionId = "mcp.session.id";

namespace McpMethodNameValues
{
/**
  Notification cancelling a previously-issued request.
 */
static constexpr const char *kNotificationsCancelled = "notifications/cancelled";

/**
  Request to initialize the MCP client.
 */
static constexpr const char *kInitialize = "initialize";

/**
  Notification indicating that the MCP client has been initialized.
 */
static constexpr const char *kNotificationsInitialized = "notifications/initialized";

/**
  Notification indicating the progress for a long-running operation.
 */
static constexpr const char *kNotificationsProgress = "notifications/progress";

/**
  Request to check that the other party is still alive.
 */
static constexpr const char *kPing = "ping";

/**
  Request to list resources available on server.
 */
static constexpr const char *kResourcesList = "resources/list";

/**
  Request to list resource templates available on server.
 */
static constexpr const char *kResourcesTemplatesList = "resources/templates/list";

/**
  Request to read a resource.
 */
static constexpr const char *kResourcesRead = "resources/read";

/**
  Notification indicating that the list of resources has changed.
 */
static constexpr const char *kNotificationsResourcesListChanged =
    "notifications/resources/list_changed";

/**
  Request to subscribe to a resource.
 */
static constexpr const char *kResourcesSubscribe = "resources/subscribe";

/**
  Request to unsubscribe from resource updates.
 */
static constexpr const char *kResourcesUnsubscribe = "resources/unsubscribe";

/**
  Notification indicating that a resource has been updated.
 */
static constexpr const char *kNotificationsResourcesUpdated = "notifications/resources/updated";

/**
  Request to list prompts available on server.
 */
static constexpr const char *kPromptsList = "prompts/list";

/**
  Request to get a prompt.
 */
static constexpr const char *kPromptsGet = "prompts/get";

/**
  Notification indicating that the list of prompts has changed.
 */
static constexpr const char *kNotificationsPromptsListChanged =
    "notifications/prompts/list_changed";

/**
  Request to list tools available on server.
 */
static constexpr const char *kToolsList = "tools/list";

/**
  Request to call a tool.
 */
static constexpr const char *kToolsCall = "tools/call";

/**
  Notification indicating that the list of tools has changed.
 */
static constexpr const char *kNotificationsToolsListChanged = "notifications/tools/list_changed";

/**
  Request to set the logging level.
 */
static constexpr const char *kLoggingSetLevel = "logging/setLevel";

/**
  Notification indicating that a message has been received.
 */
static constexpr const char *kNotificationsMessage = "notifications/message";

/**
  Request to create a sampling message.
 */
static constexpr const char *kSamplingCreateMessage = "sampling/createMessage";

/**
  Request to complete a prompt.
 */
static constexpr const char *kCompletionComplete = "completion/complete";

/**
  Request to list roots available on server.
 */
static constexpr const char *kRootsList = "roots/list";

/**
  Notification indicating that the list of roots has changed.
 */
static constexpr const char *kNotificationsRootsListChanged = "notifications/roots/list_changed";

/**
  Request from the server to elicit additional information from the user via the client
 */
static constexpr const char *kElicitationCreate = "elicitation/create";

}  // namespace McpMethodNameValues

}  // namespace mcp
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
