/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_CONTENT_FILE_FROM_OPTIONS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_CONTENT_FILE_FROM_OPTIONS_H_

#include <map>
#include <string>
#include <vector>

#include "helper/container/generic.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "mrs/database/entry/content_file.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/interface/universal_id.h"
#include "mrs/json/parse_file_sharing_options.h"
#include "mrs/rest/entry/app_content_file.h"

namespace mrs {
namespace database {

using DbObjectEntries = std::vector<entry::DbObject>;
using ContentFileEntries = std::vector<entry::ContentFile>;

class FileFromOptions {
 public:
  using Counters = std::map<UniversalId, uint64_t>;

  void analyze_global(bool enabled, const std::string &options) {
    Counters local_global_files;

    content_files_.clear();

    extract_files(get_global_config(enabled, options), &global_files_,
                  &local_global_files);
    assign(&global_files_, local_global_files);
  }

  void analyze(const ContentFileEntries &entries) {
    Counters local_service_files;
    Counters local_content_set_files;

    content_files_.clear();

    for (const entry::ContentFile &e : entries) {
      extract_files(get_service_config(e), &service_files_,
                    &local_service_files);
      extract_files(get_content_set_config(e), &content_set_files_,
                    &local_content_set_files);
    }

    assign(&service_files_, local_service_files);
    assign(&content_set_files_, local_content_set_files);
  }

  void analyze(const DbObjectEntries &entries) {
    Counters local_service_files;
    Counters local_schema_files;
    Counters local_db_objects_files;

    content_files_.clear();

    for (const entry::DbObject &e : entries) {
      extract_files(get_service_config(e), &service_files_,
                    &local_service_files);
      extract_files(get_schema_config(e), &schema_files_, &local_schema_files);
      extract_files(get_db_object_config(e), &db_objects_files_,
                    &local_db_objects_files);
    }

    assign(&service_files_, local_service_files);
    assign(&schema_files_, local_schema_files);
    assign(&db_objects_files_, local_db_objects_files);
  }

  std::vector<rest::entry::AppContentFile> content_files_;

 private:
  struct Config {
    struct Ids {
      UniversalId service;
      UniversalId schema;
      UniversalId object;
    };

    std::string service;
    std::string schema;
    std::string object;
    bool active;
    std::string options;
    Ids ids;
    bool require_auth;
  };

  Config get_service_config(const entry::DbObject &o) {
    return {o.service_path,
            {},
            {},
            o.active_service,
            o.options_json_service,
            {o.service_id, {}, o.service_id},
            false};
  }

  Config get_global_config(bool enabled, const std::string &options) {
    return {{}, {}, {}, enabled, options, {{}, {}, {}}, false};
  }

  Config get_service_config(const entry::ContentFile &o) {
    return {o.service_path,
            {},
            {},
            o.active_service,
            o.options_json_service,
            {o.service_id, {}, o.service_id},
            false};
  }

  Config get_content_set_config(const entry::ContentFile &o) {
    return {o.service_path,
            o.schema_path,
            {},
            o.active_service && o.active_set,
            o.options_json_schema,
            {o.service_id, o.content_set_id, o.content_set_id},
            o.schema_requires_authentication};
  }

  Config get_schema_config(const entry::DbObject &o) {
    return {o.service_path,
            o.schema_path,
            {},
            o.active_service && o.active_schema,
            o.options_json_schema,
            {o.service_id, o.schema_id, o.schema_id},
            o.schema_requires_authentication};
  }

  Config get_db_object_config(const entry::DbObject &o) {
    return {o.service_path,
            o.schema_path,
            o.object_path,
            o.active_service && o.active_schema && o.active_object,
            o.options_json,
            {o.service_id, o.schema_id, o.id},
            o.requires_authentication || o.schema_requires_authentication};
  }

  void assign(Counters *destination, const Counters source) {
    for (const auto &[k, v] : source) {
      (*destination)[k] = v;
    }
  }

  void extract_files(const Config &conf, Counters *global_counters,
                     Counters *local_counters) {
    auto it = local_counters->find(conf.ids.object);
    auto git = global_counters->find(conf.ids.object);

    if (local_counters->end() != it) return;

    if (global_counters->end() != git) {
      for (uint64_t i = 1; i <= git->second; ++i) {
        rest::entry::AppContentFile cf;
        cf.deleted = true;
        cf.key_entry_type = entry::EntryType::key_static_sub;
        cf.key_subtype = i;

        cf.service_id = conf.ids.service;
        cf.content_set_id = conf.ids.schema;
        cf.id = conf.ids.object;

        content_files_.push_back(cf);
      }
    }

    if (conf.options.empty()) return;

    auto fs = helper::json::text_to_handler<mrs::json::ParseFileSharingOptions>(
        conf.options);

    for (const auto &[k, v] : fs.default_static_content_) {
      rest::entry::AppContentFile cf;
      cf.active_service = cf.active_set = cf.active_file = conf.active;
      cf.deleted = false;
      cf.key_entry_type = entry::EntryType::key_static_sub;
      cf.key_subtype = ++(*local_counters)[conf.ids.object];

      cf.service_id = conf.ids.service;
      cf.content_set_id = conf.ids.schema;
      cf.id = conf.ids.object;

      cf.service_path = conf.service;
      cf.schema_path = conf.schema;
      cf.file_path = conf.object + "/" + k;
      cf.content = v;
      cf.default_handling_directory_index = false;
      cf.size = v.size();
      cf.schema_requires_authentication = conf.require_auth;
      cf.requires_authentication = conf.require_auth;
      cf.options_json_schema = conf.options;
      if (helper::container::has(fs.directory_index_directive_, k)) {
        cf.is_index = true;
      }

      content_files_.push_back(cf);
    }

    for (const auto &[k, v] : fs.default_redirects_) {
      rest::entry::AppContentFile cf;
      cf.active_service = cf.active_set = cf.active_file = conf.active;
      cf.deleted = false;
      cf.key_entry_type = entry::EntryType::key_static_sub;
      cf.key_subtype = ++(*local_counters)[conf.ids.object];

      cf.service_id = conf.ids.service;
      cf.content_set_id = conf.ids.schema;
      cf.id = conf.ids.object;

      cf.service_path = conf.service;
      cf.schema_path = conf.schema;
      cf.file_path = conf.object + "/" + k;
      cf.redirect = v;
      cf.default_handling_directory_index = false;
      cf.size = v.size();
      cf.schema_requires_authentication = conf.require_auth;
      cf.requires_authentication = conf.require_auth;
      cf.options_json_schema = conf.options;
      if (helper::container::has(fs.directory_index_directive_, k)) {
        cf.is_index = true;
      }

      content_files_.push_back(cf);
    }

    for (const auto &idx : fs.directory_index_directive_) {
      auto it_index = fs.default_static_content_.find(idx);
      if (fs.default_static_content_.end() == it_index) continue;
      rest::entry::AppContentFile cf;

      cf.active_service = cf.active_set = cf.active_file = conf.active;
      cf.deleted = false;
      cf.key_entry_type = entry::EntryType::key_static_sub;
      cf.key_subtype = ++(*local_counters)[conf.ids.object];
      cf.service_id = conf.ids.service;
      cf.content_set_id = conf.ids.schema;
      cf.id = conf.ids.object;
      cf.service_path = conf.service;
      cf.schema_path = conf.schema;
      cf.file_path = conf.object;
      cf.content = it_index->second;
      cf.default_handling_directory_index = false;
      cf.size = it_index->second.size();
      cf.schema_requires_authentication = conf.require_auth;
      cf.requires_authentication = conf.require_auth;
      cf.options_json_service = conf.options;

      content_files_.push_back(cf);

      cf.file_path = conf.object + "/";
      cf.key_subtype = ++(*local_counters)[conf.ids.object];

      content_files_.push_back(cf);
      break;
    }
  }

  Counters global_files_;
  Counters service_files_;
  Counters schema_files_;
  Counters content_set_files_;
  Counters db_objects_files_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_CONTENT_FILE_FROM_OPTIONS_H_
