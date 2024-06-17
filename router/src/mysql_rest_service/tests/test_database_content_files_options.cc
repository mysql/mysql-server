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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "helper/container/generic.h"
#include "helper/json/serializer_to_text.h"
#include "mrs/database/helper/content_file_from_options.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

struct Object {
  uint64_t service_id, schema_id, object_id;
};

namespace CreateFiles {

const uint32_t k_create_static = 1;
const uint32_t k_create_index = 2;
const uint32_t k_create_redirects = 4;

const uint32_t k_create_valid_index = k_create_static | k_create_index;
const uint32_t k_create_all =
    k_create_static | k_create_index | k_create_redirects;

}  // namespace CreateFiles

struct InputDataParam {
  InputDataParam();
  InputDataParam(std::initializer_list<std::string> expect, uint64_t nodf,
                 uint32_t create_files, std::initializer_list<Object> o)
      : shared_files(expect),
        number_of_deleted_files(nodf),
        create_files_{create_files},
        obj{o} {}

  std::vector<std::string> shared_files;
  uint64_t number_of_deleted_files;
  uint32_t create_files_{CreateFiles::k_create_static};
  std::vector<Object> obj;
};

using UniversalId = mrs::UniversalId;

class GenerateContentFilesFromOptionsBaseSuite : public testing::Test {
 public:
  UniversalId get_id(uint64_t id) {
    char buffer[UniversalId::k_size];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &id, std::min(sizeof(buffer), sizeof(id)));

    return UniversalId::from_cstr(buffer, sizeof(buffer));
  }

  std::string create_options(std::string name, uint64_t id,
                             const bool add_static_files = true,
                             const bool add_index = true,
                             const bool add_redirects = true) {
    using namespace std::string_literals;  // NOLINT(build/namespaces)

    if (id < 11) return {};

    if (!add_static_files && add_index) {
      ADD_FAILURE() << "When index is added, then corresponding static files "
                       "are required.";
      return {};
    }

    helper::json::SerializerToText stt;
    {
      auto root = stt.add_object();

      std::string idx_filename = name + "_index.html";
      std::string other_filename = name + "_other.html";

      if (add_static_files) {
        auto static_files = root->member_add_object("defaultStaticContent");

        static_files->member_add_value(idx_filename,
                                       "Content of "s + idx_filename + ".");
        static_files->member_add_value(other_filename,
                                       "Content of "s + other_filename + ".");
      }

      if (add_index) {
        auto index_files = root->member_add_array("directoryIndexDirective");
        index_files->add_value(idx_filename.c_str());
        index_files->add_value(other_filename.c_str());
      }

      if (add_redirects) {
        auto redirect_file = root->member_add_object("defaultRedirects");
        redirect_file->member_add_value(name + "_redirect1.html",
                                        "/some/folder/file1.txt");
        redirect_file->member_add_value(name + "_redirect2.html",
                                        "/some/folder/file2.txt");
      }
    }
    return stt.get_result();
  }

  mrs::database::DbObjectEntries create_entries(const std::vector<Object> &obj,
                                                uint32_t value_create_entries) {
    using namespace std::string_literals;  // NOLINT(build/namespaces)
    mrs::database::DbObjectEntries result;

    bool add_static_files = value_create_entries & CreateFiles::k_create_static;
    bool add_index = value_create_entries & CreateFiles::k_create_index;
    bool add_redirects = value_create_entries & CreateFiles::k_create_redirects;

    for (const auto &e : obj) {
      mrs::database::entry::DbObject obj;
      obj.active_object = obj.active_schema = obj.active_service = true;
      obj.deleted = false;
      obj.schema_path = "/schema"s + std::to_string(e.schema_id);
      obj.service_path = "/service"s + std::to_string(e.service_id);
      obj.object_path = "/object"s + std::to_string(e.object_id);
      obj.service_id = get_id(e.service_id);
      obj.schema_id = get_id(e.schema_id);
      obj.id = get_id(e.object_id);
      obj.requires_authentication = false;
      obj.schema_requires_authentication = false;

      obj.options_json = create_options("obj", e.object_id, add_static_files,
                                        add_index, add_redirects);
      obj.options_json_schema = create_options(
          "sch", e.schema_id, add_static_files, add_index, add_redirects);
      obj.options_json_service = create_options(
          "srv", e.service_id, add_static_files, add_index, add_redirects);

      result.push_back(obj);
    }

    return result;
  }

  std::string as_string(
      const std::vector<mrs::rest::entry::AppContentFile> &cfs) {
    std::string result;
    bool first = true;

    for (const auto &cf : cfs) {
      if (!first) result += ", ";
      if (cf.deleted) result += "X-";
      result += cf.id.to_string() + "-" + std::to_string(cf.key_subtype);
      if (cf.deleted) result += "-X";
      first = false;

      log_debug("service-path:%s, schema:%s, file:%s", cf.service_path.c_str(),
                cf.schema_path.c_str(), cf.file_path.c_str());
    }
    return result;
  }

  void validate_content_files(uint64_t number_of_deleted,
                              const std::vector<std::string> &expected_files) {
    std::vector<std::pair<UniversalId, uint64_t>> deleted_ids;
    for (const auto &cf : sut_.content_files_) {
      if (cf.deleted) {
        deleted_ids.emplace_back(cf.id, cf.key_subtype);
        --number_of_deleted;
        continue;
      }

      auto name = cf.service_path + cf.schema_path + cf.file_path;

      ASSERT_TRUE(helper::container::has(expected_files, name))
          << "Found unexpected file: " << name;
      ASSERT_TRUE(helper::container::has(
          deleted_ids, std::make_pair(cf.id, cf.key_subtype)));
    }

    ASSERT_EQ(0, number_of_deleted);
  }

  mrs::database::FileFromOptions sut_;
};

class GenerateContentFilesFromOptionsSuite
    : public GenerateContentFilesFromOptionsBaseSuite,
      public testing::WithParamInterface<InputDataParam> {
 public:
};

TEST_P(GenerateContentFilesFromOptionsSuite, verify_creation_of_content_files) {
  const auto &p = GetParam();
  const auto e = create_entries(p.obj, p.create_files_);

  sut_.analyze(e);

  auto size = static_cast<int>(e.size());
  for (int i = 0; i < size; ++i) {
    log_debug("[%i] -> options_json %s", i, e[i].options_json.c_str());
    log_debug("[%i] -> options_json_schema %s", i,
              e[i].options_json_schema.c_str());
    log_debug("[%i] -> options_json_service %s", i,
              e[i].options_json_service.c_str());
  }

  ASSERT_EQ(p.number_of_deleted_files + p.shared_files.size(),
            sut_.content_files_.size())
      << as_string(sut_.content_files_);
  uint64_t num_o_deleted{0};

  for (const auto &e : sut_.content_files_) {
    if (e.deleted) num_o_deleted++;
    auto name = e.service_path + e.schema_path + e.file_path;
    ASSERT_TRUE(helper::container::has(p.shared_files, name))
        << "The expected container, doesn't contains: " << name;
  }

  ASSERT_EQ(num_o_deleted, p.number_of_deleted_files);
}

INSTANTIATE_TEST_SUITE_P(
    InstantiationMyTestSuite, GenerateContentFilesFromOptionsSuite,
    testing::Values(
        InputDataParam({}, 0, CreateFiles::k_create_static, {{1, 1, 1}}),
        InputDataParam({}, 0, CreateFiles::k_create_static,
                       {{1, 1, 1}, {2, 2, 2}}),
        InputDataParam({}, 0, CreateFiles::k_create_static,
                       {{1, 1, 1}, {2, 2, 2}, {3, 3, 3}}),

        // #####################
        // Generate objects with Option files set (all ID that are above 10,
        //
        // generate a set of files) Generate files at service level
        InputDataParam({"/service11/srv_index.html",
                        "/service11/srv_other.html"},
                       0, CreateFiles::k_create_static, {{11, 1, 1}}),

        // Generate files at schema level
        InputDataParam({"/service2/schema12/sch_index.html",
                        "/service2/schema12/sch_other.html"},
                       0, CreateFiles::k_create_static,
                       {{1, 1, 1}, {2, 12, 2}}),

        // Generate files at object level
        InputDataParam({"/service3/schema3/object13/obj_index.html",
                        "/service3/schema3/object13/obj_other.html"},
                       0, CreateFiles::k_create_static,
                       {{1, 1, 1}, {2, 2, 2}, {3, 3, 13}}),

        // #####################
        // Duplicate same Options for differe sub objects
        //
        // generate a set of files) Generate files at service level
        InputDataParam(
            {"/service11/srv_index.html", "/service11/srv_other.html"}, 0,
            CreateFiles::k_create_static, {{11, 1, 5}, {11, 2, 6}}),

        // Generate files at schema level
        InputDataParam({"/service2/schema12/sch_index.html",
                        "/service2/schema12/sch_other.html"},
                       0, CreateFiles::k_create_static,
                       {{2, 12, 2}, {2, 12, 3}}),

        // #######################################
        // # Verify INDEX
        InputDataParam({}, 0, CreateFiles::k_create_valid_index, {{1, 1, 1}}),
        InputDataParam({}, 0, CreateFiles::k_create_valid_index,
                       {{1, 1, 1}, {2, 2, 2}}),
        InputDataParam({}, 0, CreateFiles::k_create_valid_index,
                       {{1, 1, 1}, {2, 2, 2}, {3, 3, 3}}),

        // #####################
        // Generate objects with Option files set (all ID that are above 10,
        //
        // generate a set of files) Generate files at service level
        InputDataParam({"/service11/srv_index.html",
                        "/service11/srv_other.html", "/service11",
                        "/service11/"},
                       0, CreateFiles::k_create_valid_index, {{11, 1, 1}}),

        // Generate files at schema level
        InputDataParam({"/service2/schema12/sch_index.html",
                        "/service2/schema12/sch_other.html",
                        "/service2/schema12", "/service2/schema12/"},
                       0, CreateFiles::k_create_valid_index,
                       {{1, 1, 1}, {2, 12, 2}}),

        // Generate files at object level
        InputDataParam({"/service3/schema3/object13/obj_index.html",
                        "/service3/schema3/object13/obj_other.html",
                        "/service3/schema3/object13",
                        "/service3/schema3/object13/"},
                       0, CreateFiles::k_create_valid_index,
                       {{1, 1, 1}, {2, 2, 2}, {3, 3, 13}}),

        // #####################
        // Duplicate same Options for differe sub objects
        //
        // generate a set of files) Generate files at service level
        InputDataParam({"/service11/srv_index.html",
                        "/service11/srv_other.html", "/service11",
                        "/service11/"},
                       0, CreateFiles::k_create_valid_index,
                       {{11, 1, 5}, {11, 2, 6}}),

        // Generate files at schema level
        InputDataParam({"/service2/schema12/sch_index.html",
                        "/service2/schema12/sch_other.html",
                        "/service2/schema12", "/service2/schema12/"},
                       0, CreateFiles::k_create_valid_index,
                       {{2, 12, 2}, {2, 12, 3}})));

TEST_F(GenerateContentFilesFromOptionsBaseSuite,
       subsequence_calls_with_the_same_set_or_arguments_with_no_options) {
  auto e = create_entries({{1, 1, 1}, {1, 2, 3}}, CreateFiles::k_create_static);

  sut_.analyze(e);

  ASSERT_EQ(0, sut_.content_files_.size());

  e = create_entries({{1, 1, 1}, {1, 2, 3}}, CreateFiles::k_create_static);
  sut_.analyze(e);
  ASSERT_EQ(0, sut_.content_files_.size());
}

TEST_F(GenerateContentFilesFromOptionsBaseSuite,
       subsequence_calls_with_the_same_set_or_arguments_with_options) {
  const auto e =
      create_entries({{11, 1, 1}, {1, 22, 3}}, CreateFiles::k_create_static);

  sut_.analyze(e);

  ASSERT_EQ(4, sut_.content_files_.size());

  sut_.analyze(e);
  ASSERT_EQ(8, sut_.content_files_.size());
  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      4, {"/service11/srv_index.html", "/service11/srv_other.html",
          "/service1/schema22/sch_index.html",
          "/service1/schema22/sch_other.html"}));

  sut_.analyze(e);
  ASSERT_EQ(8, sut_.content_files_.size());

  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      4, {"/service11/srv_index.html", "/service11/srv_other.html",
          "/service1/schema22/sch_index.html",
          "/service1/schema22/sch_other.html"}));
}

TEST_F(
    GenerateContentFilesFromOptionsBaseSuite,
    subsequence_calls_with_the_same_set_or_arguments_with_options_redirects) {
  const auto e =
      create_entries({{11, 1, 1}, {1, 22, 3}}, CreateFiles::k_create_redirects);

  sut_.analyze(e);

  ASSERT_EQ(4, sut_.content_files_.size());

  sut_.analyze(e);
  ASSERT_EQ(8, sut_.content_files_.size());
  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      4, {"/service11/srv_redirect1.html", "/service11/srv_redirect2.html",
          "/service1/schema22/sch_redirect1.html",
          "/service1/schema22/sch_redirect2.html"}));

  sut_.analyze(e);
  ASSERT_EQ(8, sut_.content_files_.size());

  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      4, {"/service11/srv_redirect1.html", "/service11/srv_redirect2.html",
          "/service1/schema22/sch_redirect1.html",
          "/service1/schema22/sch_redirect2.html"}));
}

TEST_F(
    GenerateContentFilesFromOptionsBaseSuite,
    subsequence_calls_with_the_same_set_or_arguments_with_options_index_files) {
  const auto e = create_entries({{1, 1, 1}, {1, 2, 33}},
                                CreateFiles::k_create_valid_index);

  sut_.analyze(e);

  ASSERT_EQ(4, sut_.content_files_.size());

  sut_.analyze(e);
  ASSERT_EQ(8, sut_.content_files_.size());
  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      4, {"/service1/schema2/object33/obj_index.html",
          "/service1/schema2/object33/obj_other.html",
          "/service1/schema2/object33", "/service1/schema2/object33/"}));

  sut_.analyze(e);
  ASSERT_EQ(8, sut_.content_files_.size());

  ASSERT_NO_FATAL_FAILURE(
      validate_content_files(4, {
                                    "/service1/schema2/object33/obj_index.html",
                                    "/service1/schema2/object33/obj_other.html",
                                    "/service1/schema2/object33",
                                    "/service1/schema2/object33/",
                                }));
}

TEST_F(GenerateContentFilesFromOptionsBaseSuite,
       subsequence_calls_with_the_same_set_or_arguments_with_options_all) {
  const auto e =
      create_entries({{1, 1, 1}, {1, 2, 33}}, CreateFiles::k_create_all);

  sut_.analyze(e);

  ASSERT_EQ(6, sut_.content_files_.size());

  sut_.analyze(e);
  ASSERT_EQ(12, sut_.content_files_.size());
  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      6, {"/service1/schema2/object33/obj_redirect1.html",
          "/service1/schema2/object33/obj_redirect2.html",
          "/service1/schema2/object33/obj_index.html",
          "/service1/schema2/object33/obj_other.html",
          "/service1/schema2/object33", "/service1/schema2/object33/"}));

  sut_.analyze(e);
  ASSERT_EQ(12, sut_.content_files_.size());

  ASSERT_NO_FATAL_FAILURE(validate_content_files(
      6, {"/service1/schema2/object33/obj_redirect1.html",
          "/service1/schema2/object33/obj_redirect2.html",
          "/service1/schema2/object33/obj_index.html",
          "/service1/schema2/object33/obj_other.html",
          "/service1/schema2/object33", "/service1/schema2/object33/"}));
}
