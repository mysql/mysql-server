/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * REST API for the metadata_cache plugin.
 */

#include <array>
#include <string>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/utility/string.h"  // ::join()

#include "mysqlrouter/plugin_config.h"

#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/rest_api_component.h"

#include "rest_clusters_list.h"
#include "rest_clusters_nodes.h"
#include "rest_metadata_cache_config.h"
#include "rest_metadata_cache_list.h"
#include "rest_metadata_cache_status.h"
IMPORT_LOG_FUNCTIONS()

static const char kSectionName[]{"rest_metadata_cache"};
static const char kRequireRealm[]{"require_realm"};

// one shared setting
std::string require_realm_metadata_cache;

class RestMetadataCachePluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestMetadataCachePluginConfig(
      const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        require_realm(get_option_string(section, kRequireRealm)) {}

  std::string get_default(const std::string & /* option */) const override {
    return {};
  }

  bool is_required(const std::string &option) const override {
    if (option == kRequireRealm) return true;
    return false;
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  try {
    std::set<std::string> known_realms;
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "http_auth_realm") {
        known_realms.emplace(section->key);
      }
    }
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (!section->key.empty()) {
        log_error("[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        return;
      }

      RestMetadataCachePluginConfig config{section};

      if (!config.require_realm.empty() &&
          (known_realms.find(config.require_realm) == known_realms.end())) {
        throw std::invalid_argument(
            "unknown authentication realm for [" + std::string(kSectionName) +
            "] '" + section->key + "': " + config.require_realm +
            ", known realm(s): " + mysql_harness::join(known_realms, ","));
      }

      require_realm_metadata_cache = config.require_realm;
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

using JsonPointer = RestApiComponent::JsonPointer;
using JsonValue = RestApiComponent::JsonValue;

#define STR(s) \
  { s, strlen(s), rapidjson::kPointerInvalidIndex }
static const std::array<JsonPointer::Token, 2> metadata_name_param_tokens{
    {STR("parameters"), STR("metadataNameParam")}};

static const std::array<JsonPointer::Token, 2> cluster_name_param_tokens{
    {STR("parameters"), STR("clusterNameParam")}};

static const std::array<JsonPointer::Token, 2> metadata_status_def_tokens{
    {STR("definitions"), STR("MetadataStatus")}};

static const std::array<JsonPointer::Token, 2> metadata_list_def_tokens{
    {STR("definitions"), STR("MetadataList")}};

static const std::array<JsonPointer::Token, 2> metadata_summary_def_tokens{
    {STR("definitions"), STR("MetadataSummary")}};

static const std::array<JsonPointer::Token, 2> metadata_config_def_tokens{
    {STR("definitions"), STR("MetadataConfig")}};

static const std::array<JsonPointer::Token, 2> cluster_node_summary_def_tokens{
    {STR("definitions"), STR("ClusterNodeSummary")}};

static const std::array<JsonPointer::Token, 2> cluster_node_list_def_tokens{
    {STR("definitions"), STR("ClusterNodeList")}};

static const std::array<JsonPointer::Token, 2> cluster_summary_def_tokens{
    {STR("definitions"), STR("ClusterSummary")}};

static const std::array<JsonPointer::Token, 2> cluster_list_def_tokens{
    {STR("definitions"), STR("ClusterList")}};

static const std::array<JsonPointer::Token, 2> metadata_status_path_tokens{
    {STR("paths"), STR("/metadata/{metadataName}/status")}};

static const std::array<JsonPointer::Token, 2> metadata_config_path_tokens{
    {STR("paths"), STR("/metadata/{metadataName}/config")}};

// static const std::array<JsonPointer::Token, 2> cluster_list_path_tokens{
//    {STR("paths"), STR("/clusters")}};

// static const std::array<JsonPointer::Token, 2> cluster_node_list_path_tokens{
//    {STR("paths"), STR("/clusters/{clusterName}/nodes")}};

static const std::array<JsonPointer::Token, 2> metadata_list_path_tokens{
    {STR("paths"), STR("/metadata")}};

static const std::array<JsonPointer::Token, 2> tags_append_tokens{
    {STR("tags"), STR("-")}};

#undef STR

std::string json_pointer_stringfy(const JsonPointer &ptr) {
  rapidjson::StringBuffer sb;
  ptr.StringifyUriFragment(sb);
  return {sb.GetString(), sb.GetSize()};
}

static void spec_adder(RestApiComponent::JsonDocument &spec_doc) {
  auto &allocator = spec_doc.GetAllocator();

  {
    JsonPointer ptr(tags_append_tokens.data(), tags_append_tokens.size());

    ptr.Set(spec_doc,
            JsonValue(rapidjson::kObjectType)
                .AddMember("name", "cluster", allocator)
                .AddMember("description", "InnoDB Cluster", allocator),
            allocator);
  }

  // /parameters/metadataNameParam
  const RestApiComponent::JsonPointer metadata_name_param_ptr(
      metadata_name_param_tokens.data(), metadata_name_param_tokens.size());

  metadata_name_param_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("name", "metadataName", allocator)
          .AddMember("in", "path", allocator)
          .AddMember("description", "name of cluster", allocator)
          .AddMember("required", true, allocator)
          .AddMember("type", "string", allocator),
      allocator);

  std::string metadata_name_param_str =
      json_pointer_stringfy(metadata_name_param_ptr);

  // /parameters/clusterNameParam
  const RestApiComponent::JsonPointer cluster_name_param_ptr(
      cluster_name_param_tokens.data(), cluster_name_param_tokens.size());

  cluster_name_param_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("name", "clusterName", allocator)
          .AddMember("in", "path", allocator)
          .AddMember("description", "name of cluster", allocator)
          .AddMember("required", true, allocator)
          .AddMember("type", "string", allocator),
      allocator);

  std::string cluster_name_param_str =
      json_pointer_stringfy(cluster_name_param_ptr);

  // /definitions/MetadataStatus
  const RestApiComponent::JsonPointer metadata_status_def_ptr(
      metadata_status_def_tokens.data(), metadata_status_def_tokens.size());

  metadata_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("lastRefreshHostname",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("lastRefreshPort",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("timeLastRefreshFailed",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "data-time", allocator),
                             allocator)
                  .AddMember("timeLastRefreshSucceeded",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "data-time", allocator),
                             allocator)
                  .AddMember("refreshSucceeded",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("refreshFailed",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator),
              allocator),
      allocator);

  std::string metadata_status_def_ptr_str =
      json_pointer_stringfy(metadata_status_def_ptr);

  // /definitions/MetadataConfig
  const RestApiComponent::JsonPointer metadata_config_def_ptr(
      metadata_config_def_tokens.data(), metadata_config_def_tokens.size());

  metadata_config_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("clusterName",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("timeRefreshInMs",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("groupReplicationId",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember(
                      "nodes",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember("type", "object", allocator)
                                  .AddMember(
                                      "properties",
                                      JsonValue(rapidjson::kObjectType)
                                          .AddMember(
                                              "hostname",
                                              JsonValue(rapidjson::kObjectType)
                                                  .AddMember("type", "string",
                                                             allocator),
                                              allocator)
                                          .AddMember(
                                              "port",
                                              JsonValue(rapidjson::kObjectType)
                                                  .AddMember("type", "integer",
                                                             allocator),
                                              allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string metadata_config_def_ptr_str =
      json_pointer_stringfy(metadata_config_def_ptr);

  // /definitions/MetadataSummary
  const RestApiComponent::JsonPointer metadata_summary_def_ptr(
      metadata_summary_def_tokens.data(), metadata_summary_def_tokens.size());

  metadata_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string metadata_summary_def_ptr_str =
      json_pointer_stringfy(metadata_summary_def_ptr);

  // /definitions/MetadataList
  const RestApiComponent::JsonPointer metadata_list_def_ptr(
      metadata_list_def_tokens.data(), metadata_list_def_tokens.size());

  metadata_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          metadata_summary_def_ptr_str.data(),
                                          metadata_summary_def_ptr_str.size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string metadata_list_def_ptr_str =
      json_pointer_stringfy(metadata_list_def_ptr);

  // /definitions/ClusterNodeSummary
  const RestApiComponent::JsonPointer cluster_node_summary_def_ptr(
      cluster_node_summary_def_tokens.data(),
      cluster_node_summary_def_tokens.size());

  cluster_node_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("groupUuid",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator)
                         .AddMember("serverUuid",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string cluster_node_summary_def_ptr_str =
      json_pointer_stringfy(cluster_node_summary_def_ptr);

  // /definitions/ClusterNodeList
  const RestApiComponent::JsonPointer cluster_node_list_def_ptr(
      cluster_node_list_def_tokens.data(), cluster_node_list_def_tokens.size());

  cluster_node_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(cluster_node_summary_def_ptr_str
                                                    .data(),
                                                cluster_node_summary_def_ptr_str
                                                    .size(),
                                                allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string cluster_node_list_def_ptr_str =
      json_pointer_stringfy(cluster_node_list_def_ptr);

  // /definitions/ClusterSummary
  const RestApiComponent::JsonPointer cluster_summary_def_ptr(
      cluster_summary_def_tokens.data(), cluster_summary_def_tokens.size());

  cluster_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string cluster_summary_def_ptr_str =
      json_pointer_stringfy(cluster_summary_def_ptr);

  // /definitions/ClusterList
  const RestApiComponent::JsonPointer cluster_list_def_ptr(
      cluster_list_def_tokens.data(), cluster_list_def_tokens.size());

  cluster_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          cluster_summary_def_ptr_str.data(),
                                          cluster_summary_def_ptr_str.size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string cluster_list_def_ptr_str =
      json_pointer_stringfy(cluster_list_def_ptr);

  // /paths/metadataConfig
  {
    JsonPointer ptr(metadata_config_path_tokens.data(),
                    metadata_config_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("cluster", allocator),
                               allocator)
                    .AddMember("description",
                               "Get config of the metadata cache of "
                               "a replicaset of a cluster",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "config of metadata cache",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    metadata_config_def_ptr_str
                                                        .data(),
                                                    metadata_config_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "cache not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(metadata_name_param_str.data(),
                                                 metadata_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/metadataStatus
  {
    JsonPointer ptr(metadata_status_path_tokens.data(),
                    metadata_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("cluster", allocator),
                               allocator)
                    .AddMember("description",
                               "Get status of the metadata cache of "
                               "a replicaset of a cluster",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "status of metadata cache",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    metadata_status_def_ptr_str
                                                        .data(),
                                                    metadata_status_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "cache not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(metadata_name_param_str.data(),
                                                 metadata_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  /* The socpe of WL#12441 was limited and does not include those
  // /paths/clusters/
  {
    JsonPointer ptr(cluster_list_path_tokens.data(),
                    cluster_list_path_tokens.size());

    ptr.Set(spec_doc,
            JsonValue(rapidjson::kObjectType)
                .AddMember(
                    "get",
                    JsonValue(rapidjson::kObjectType)
                        .AddMember("tags",
                                   JsonValue(rapidjson::kArrayType)
                                       .PushBack("cluster", allocator),
                                   allocator)
                        .AddMember("description",
                                   "Get status of the metadata cache of "
                                   "a replicaset of a cluster",
                                   allocator)
                        .AddMember(
                            "responses",
                            JsonValue(rapidjson::kObjectType)
                                .AddMember(
                                    "200",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("description",
                                                   "status of metadata cache",
                                                   allocator)
                                        .AddMember(
                                            "schema",
                                            JsonValue(rapidjson::kObjectType)
                                                .AddMember(
                                                    "$ref",
                                                    JsonValue(
                                                        cluster_list_def_ptr_str
                                                            .data(),
                                                        cluster_list_def_ptr_str
                                                            .size(),
                                                        allocator),
                                                    allocator),
                                            allocator),
                                    allocator)
                                .AddMember("404",
                                           JsonValue(rapidjson::kObjectType)
                                               .AddMember("description",
                                                          "cache not found",
                                                          allocator),
                                           allocator),
                            allocator)
                    //
                    ,
                    allocator),
            allocator);
  }

  // /paths/clusters/{clusterName}/nodes/
  {
    JsonPointer ptr(cluster_node_list_path_tokens.data(),
                    cluster_node_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("cluster", allocator),
                               allocator)
                    .AddMember("description",
                               "Get status of the metadata cache of "
                               "a replicaset of a cluster",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "status of metadata cache",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    cluster_node_list_def_ptr_str
                                                        .data(),
                                                    cluster_node_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "cache not found",
                                               allocator),
                                allocator),
                        allocator),
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(cluster_name_param_str.data(),
                                                 cluster_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),

        allocator);
  }
  */

  // /paths/metadata/
  {
    JsonPointer ptr(metadata_list_path_tokens.data(),
                    metadata_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("cluster", allocator),
                               allocator)
                    .AddMember("description",
                               "Get list of the metadata cache instances",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember(
                                        "description",
                                        "list of the metadata cache instances",
                                        allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    metadata_list_def_ptr_str
                                                        .data(),
                                                    metadata_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }
}

static void start(mysql_harness::PluginFuncEnv *env) {
  auto &rest_api_srv = RestApiComponent::get_instance();

  const bool spec_adder_executed = rest_api_srv.try_process_spec(spec_adder);

  std::array<RestApiComponentPath, 3> paths{{
      {rest_api_srv, RestMetadataCacheStatus::path_regex,
       std::make_unique<RestMetadataCacheStatus>(require_realm_metadata_cache)},
      {rest_api_srv, RestMetadataCacheConfig::path_regex,
       std::make_unique<RestMetadataCacheConfig>(require_realm_metadata_cache)},
      {rest_api_srv, RestMetadataCacheList::path_regex,
       std::make_unique<RestMetadataCacheList>(require_realm_metadata_cache)},

      // The socpe of WL#12441 was limited and does not include those:
      //  {rest_api_srv, RestClustersList::path_regex,
      //                        std::make_unique<RestClustersList>(require_realm_metadata_cache)},
      //  {rest_api_srv, RestClustersNodes::path_regex,
      //                        std::make_unique<RestClustersNodes>(require_realm_metadata_cache)},
  }};

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component doesn't
  // have a callback to use
  if (!spec_adder_executed) rest_api_srv.remove_process_spec(spec_adder);
}

#if defined(_MSC_VER) && defined(rest_metadata_cache_EXPORTS)
/* We are building this library */
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

const char *rest_metadata_plugin_requires[] = {
    // "metadata_cache",
    "rest_api",
};

extern "C" {
mysql_harness::Plugin DLLEXPORT harness_plugin_rest_metadata_cache = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "REST_METADATA_CACHE",
    VERSION_NUMBER(0, 0, 1),
    sizeof(rest_metadata_plugin_requires) /
        sizeof(rest_metadata_plugin_requires[0]),
    rest_metadata_plugin_requires,  // requires
    0,
    nullptr,  // conflicts
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
};
}
