/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "cluster_metadata_dynamic_state.h"
#include "mysql/harness/dynamic_state.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "common.h"
#include "dim.h"
#include "utils.h"

namespace {
constexpr const char kSectionName[] = "metadata-cache";
}  // namespace

using mysql_harness::JsonAllocator;
using mysql_harness::JsonValue;
using mysqlrouter::ClusterType;

struct ClusterMetadataDynamicState::Pimpl {
  mysql_harness::DynamicState *base_state_;
  std::unique_ptr<mysql_harness::JsonValue> section_;
};

ClusterMetadataDynamicState::ClusterMetadataDynamicState(
    mysql_harness::DynamicState *base_config, ClusterType cluster_type)
    : pimpl_(new Pimpl()), cluster_type_(cluster_type) {
  pimpl_->base_state_ = base_config;
}

ClusterMetadataDynamicState::~ClusterMetadataDynamicState() {}

void ClusterMetadataDynamicState::save_section() {
  JsonValue section(rapidjson::kObjectType);

  // write cluster name
  JsonAllocator allocator;
  JsonValue val;
  val.SetString(cluster_type_specific_id_.c_str(),
                cluster_type_specific_id_.length());
  section.AddMember("group-replication-id", val, allocator);

  // write metadata servers
  JsonValue metadata_servers(rapidjson::kArrayType);
  for (auto &metadata_server : metadata_servers_) {
    val.SetString(metadata_server.c_str(), metadata_server.length());
    metadata_servers.PushBack(val, allocator);
  }
  section.AddMember("cluster-metadata-servers", metadata_servers, allocator);

  // if this is ReplicaSet cluster write view_id
  if (cluster_type_ == ClusterType::RS_V2) {
    val.SetUint(view_id_);
    section.AddMember("view-id", val, allocator);
  }

  pimpl_->base_state_->update_section(kSectionName, std::move(section));
}

bool ClusterMetadataDynamicState::save(std::ostream &state_stream) {
  save_section();

  if (pimpl_->base_state_->save_to_stream(state_stream)) {
    changed_ = false;
    return true;
  }

  return false;
}

bool ClusterMetadataDynamicState::save() {
  save_section();

  if (pimpl_->base_state_->save()) {
    changed_ = false;
    return true;
  }

  return false;
}

void ClusterMetadataDynamicState::load() {
  pimpl_->base_state_->load();

  pimpl_->section_ = pimpl_->base_state_->get_section(kSectionName);
  JsonValue &section = *pimpl_->section_;

  metadata_servers_.clear();
  if (pimpl_->section_->HasMember("cluster-metadata-servers")) {
    const auto &md_servers = section["cluster-metadata-servers"];
    assert(md_servers.IsArray());

    for (size_t i = 0; i < md_servers.Size(); ++i) {
      auto &server = md_servers[i];
      assert(server.IsString());
      metadata_servers_.push_back(server.GetString());
    }
  }

  cluster_type_specific_id_.clear();
  if (pimpl_->section_->HasMember("group-replication-id")) {
    const auto &cluster_type_specific_id = section["group-replication-id"];
    assert(cluster_type_specific_id.IsString());
    cluster_type_specific_id_ = cluster_type_specific_id.GetString();
  }

  view_id_ = 0;
  if (pimpl_->section_->HasMember("view-id")) {
    const auto &view_id = section["view-id"];
    assert(view_id.IsUint());
    view_id_ = view_id.GetUint();
  }

  changed_ = false;
}

void ClusterMetadataDynamicState::set_metadata_servers(
    const std::vector<std::string> &metadata_servers) {
  if (metadata_servers != metadata_servers_) {
    metadata_servers_ = metadata_servers;
    changed_ = true;
  }
}

std::vector<std::string> ClusterMetadataDynamicState::get_metadata_servers()
    const {
  return metadata_servers_;
}

std::string ClusterMetadataDynamicState::get_cluster_type_specific_id() const {
  return cluster_type_specific_id_;
}

void ClusterMetadataDynamicState::set_cluster_type_specific_id(
    const std::string &cluster_type_specific_id) {
  if (cluster_type_specific_id_ != cluster_type_specific_id) {
    cluster_type_specific_id_ = cluster_type_specific_id;
    changed_ = true;
  }
}

void ClusterMetadataDynamicState::set_view_id(const unsigned view_id) {
  if (view_id_ != view_id) {
    view_id_ = view_id;
    changed_ = true;
  }
}

unsigned ClusterMetadataDynamicState::get_view_id() const { return view_id_; }
