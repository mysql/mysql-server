/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/cluster_metadata_dynamic_state.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "mysql/harness/dynamic_state.h"

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

ClusterMetadataDynamicState::~ClusterMetadataDynamicState() = default;

void ClusterMetadataDynamicState::save_section() {
  JsonValue section(rapidjson::kObjectType);

  // write cluster id
  JsonAllocator allocator;
  JsonValue val;

  if (!cluster_type_specific_id_.empty()) {
    val.SetString(cluster_type_specific_id_.c_str(),
                  cluster_type_specific_id_.length());
    section.AddMember("group-replication-id", val, allocator);
  }

  if (!clusterset_id_.empty()) {
    val.SetString(clusterset_id_.c_str(), clusterset_id_.length());
    section.AddMember("clusterset-id", val, allocator);
  }

  // write metadata servers
  JsonValue metadata_servers(rapidjson::kArrayType);
  for (auto &metadata_server : metadata_servers_) {
    val.SetString(metadata_server.c_str(), metadata_server.length());
    metadata_servers.PushBack(val, allocator);
  }
  section.AddMember("cluster-metadata-servers", metadata_servers, allocator);

  // if this is ReplicaSet cluster or ClusterSet write view_id
  if (view_id_ > 0) {
    val.SetUint64(view_id_);
    section.AddMember("view-id", val, allocator);
  }

  pimpl_->base_state_->update_section(kSectionName, std::move(section));
}

bool ClusterMetadataDynamicState::save(std::ostream &state_stream) {
  save_section();

  if (pimpl_->base_state_->save_to_stream(state_stream, is_clusterset())) {
    changed_ = false;
    return true;
  }

  return false;
}

bool ClusterMetadataDynamicState::save() {
  save_section();

  if (pimpl_->base_state_->save(is_clusterset())) {
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

  {
    const auto it = section.FindMember("cluster-metadata-servers");

    if (it != section.MemberEnd()) {
      const auto &md_servers = it->value;
      assert(md_servers.IsArray());

      for (size_t i = 0; i < md_servers.Size(); ++i) {
        auto &server = md_servers[i];
        assert(server.IsString());
        metadata_servers_.emplace_back(server.GetString());
      }
    }
  }

  cluster_type_specific_id_.clear();
  {
    const auto it = section.FindMember("group-replication-id");

    if (it != section.MemberEnd()) {
      const auto &cluster_type_specific_id = it->value;
      assert(cluster_type_specific_id.IsString());
      cluster_type_specific_id_ = cluster_type_specific_id.GetString();
    }
  }

  {
    const auto it = section.FindMember("clusterset-id");
    if (it != section.MemberEnd()) {
      const auto &clusterset_id = it->value;
      assert(clusterset_id.IsString());
      clusterset_id_ = clusterset_id.GetString();
    }
  }

  view_id_ = 0;
  {
    const auto it = section.FindMember("view-id");
    if (it != section.MemberEnd()) {
      const auto &view_id = it->value;
      assert(view_id.IsUint64());
      view_id_ = view_id.GetUint64();
    }
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

std::string ClusterMetadataDynamicState::get_clusterset_id() const {
  return clusterset_id_;
}

bool ClusterMetadataDynamicState::is_clusterset() const {
  return !clusterset_id_.empty();
}

void ClusterMetadataDynamicState::set_cluster_type_specific_id(
    const std::string &cluster_type_specific_id) {
  if (cluster_type_specific_id_ != cluster_type_specific_id) {
    cluster_type_specific_id_ = cluster_type_specific_id;
    changed_ = true;
  }
}

void ClusterMetadataDynamicState::set_view_id(const uint64_t view_id) {
  if (view_id_ != view_id) {
    view_id_ = view_id;
    changed_ = true;
  }
}

unsigned ClusterMetadataDynamicState::get_view_id() const { return view_id_; }

void ClusterMetadataDynamicState::set_clusterset_id(
    const std::string &clusterset_id) {
  if (clusterset_id_ != clusterset_id) {
    clusterset_id_ = clusterset_id;
    changed_ = true;
  }
}
