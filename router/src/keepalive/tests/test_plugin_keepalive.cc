/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

////////////////////////////////////////
// Standard include files
#include <climits>
#include <fstream>
#include <memory>  // unique_ptr

////////////////////////////////////////
// Third-party include files
#include <gtest/gtest.h>

////////////////////////////////////////
// Harness interface include files
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

using mysql_harness::Loader;
using mysql_harness::Path;

Path g_here;

class KeepalivePluginTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Path here = Path(g_here);
    orig_cout_ = std::cout.rdbuf();
    std::cout.rdbuf(ssout.rdbuf());

    std::map<std::string, std::string> params;
    params["program"] = "harness";
    test_data_dir_ = mysql_harness::get_tests_data_dir(here.str());
    params["prefix"] = test_data_dir_;
    params["log_level"] = "info";
    config_ = std::make_unique<mysql_harness::LoaderConfig>(
        params, std::vector<std::string>(), mysql_harness::Config::allow_keys);
    config_->read(Path(test_data_dir_).join("keepalive.cfg"));
    loader_ = std::make_unique<Loader>("harness", *config_);
  }

  void TearDown() override { std::cout.rdbuf(orig_cout_); }

  std::unique_ptr<Loader> loader_;
  std::unique_ptr<mysql_harness::LoaderConfig> config_;
  std::string test_data_dir_;

 private:
  std::stringstream ssout;
  std::streambuf *orig_cout_;
};

TEST_F(KeepalivePluginTest, Available) {
  auto lst = loader_->available();
  EXPECT_EQ(1U, lst.size());

  EXPECT_SECTION_AVAILABLE("keepalive", loader_.get());
}

TEST_F(KeepalivePluginTest, CheckLog) {
  auto logging_folder = Path(test_data_dir_).join("/var/log/keepalive");
  const auto log_file = Path::make_path(logging_folder, "harness", "log");
  init_test_logger({"keepalive"},
                   loader_->get_config().get_default("logging_folder"),
                   "harness");

  // Make sure log file is empty
  std::fstream fs;
  fs.open(log_file.str(), std::fstream::trunc | std::ofstream::out);
  fs.close();

  ASSERT_NO_THROW(loader_->start());

  std::ifstream ifs_log(log_file.str());
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(ifs_log, line)) {
    lines.push_back(line);
  }

  auto find_line = [&lines](unsigned start_line,
                            const char *needle) -> unsigned {
    for (unsigned i = start_line; i < lines.size(); i++)
      if (lines[i].find(needle) != std::string::npos) return i;
    return UINT_MAX;
  };

  ASSERT_GE(lines.size(), 4U);
  unsigned start_line = 0;
  EXPECT_NE(UINT_MAX, start_line = find_line(
                          start_line, "keepalive started with interval 1"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "2 time(s)"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "keepalive"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "INFO"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "keepalive"));
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname().str();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
