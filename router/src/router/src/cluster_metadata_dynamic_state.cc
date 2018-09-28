/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

struct ClusterMetadataDynamicState::Pimpl {
  mysql_harness::DynamicState *base_state_;
  std::unique_ptr<mysql_harness::JsonValue> section_;
};

ClusterMetadataDynamicState::ClusterMetadataDynamicState(
    mysql_harness::DynamicState *base_config)
    : pimpl_(new Pimpl()) {
  pimpl_->base_state_ = base_config;
}

ClusterMetadataDynamicState::~ClusterMetadataDynamicState() {}

void ClusterMetadataDynamicState::save_section() {
  JsonValue section(rapidjson::kObjectType);

  // write cluster name
  JsonAllocator allocator;
  JsonValue val;
  val.SetString(gr_id_.c_str(), gr_id_.length());
  section.AddMember("group-replication-id", val, allocator);

  // write metadata servers
  JsonValue metadata_servers(rapidjson::kArrayType);
  for (auto &metadata_server : metadata_servers_) {
    val.SetString(metadata_server.c_str(), metadata_server.length());
    metadata_servers.PushBack(val, allocator);
  }
  section.AddMember("cluster-metadata-servers", metadata_servers, allocator);

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

  gr_id_.clear();
  if (pimpl_->section_->HasMember("group-replication-id")) {
    const auto &gr_id = section["group-replication-id"];
    assert(gr_id.IsString());
    gr_id_ = gr_id.GetString();
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

std::string ClusterMetadataDynamicState::get_gr_id() const { return gr_id_; }

void ClusterMetadataDynamicState::set_group_replication_id(
    const std::string &gr_id) {
  if (gr_id_ != gr_id) {
    gr_id_ = gr_id;
    changed_ = true;
  }
}
