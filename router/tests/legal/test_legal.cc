/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _WIN32  // this test fails on Windows due to Git/shell problems

#include "cmd_exec.h"
#include "mysql/harness/filesystem.h"
#include "router_test_helpers.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "gmock/gmock.h"

#ifdef GTEST_USES_POSIX_RE
#include <regex.h>
#endif
using mysql_harness::Path;

struct GitInfo {
  Path file;
  int year_first_commit;
  int year_last_commit;
};

Path g_origin;
Path g_source_dir;
std::vector<GitInfo> g_git_tracked_files;

const std::vector<std::string> kLicenseSnippets{
    "This program is free software; you can redistribute it",
    "it under the terms of the GNU General Public License, version 2.0,", "",
    "This program is also distributed with certain software (including",  // the
                                                                          // openssl
                                                                          // exception
    "This program is distributed in the hope that",
    "02110-1301",  // last line of the copyright header
};

// Ignored file extensions
const std::vector<std::string> kIgnoredExtensions{
    ".o",   ".pyc",  ".pyo", ".conf.in", ".cfg.in", ".cfg", ".html",
    ".css", ".conf", ".ini", ".swp",     ".json",   ".md",  ".js"};

const std::vector<std::string> kIgnoredFileNames{
    ".gitignore", "nt_servc.cc", "nt_servc.h", "License.txt", "Doxyfile.in",
#ifndef _WIN32
    "README.md"  // symlink on Unix-like, doesn't work on Windows
#endif
};

// Paths to ignore; relative to repository root
const std::vector<Path> kIgnoredPaths{
    Path("src/harness/internal/"),
    Path("src/harness/README.txt"),
    Path("packaging"),
    Path("internal"),
    Path(".git"),
    Path(".idea"),
    Path("build"),
    Path("ext"),
    Path("tests/fuzzers/corpus/"),
    Path("tests/fuzzers/README.txt"),
};

bool is_ignored_path(Path path, const std::vector<Path> ignored_paths) {
  // Check paths we are ignoring
  Path fullpath(Path(g_source_dir).real_path());
  for (auto &it : ignored_paths) {
    auto tmp = Path(fullpath).join(it);
    if (tmp == path) {
      return true;
    }
    auto tmp_dirname = path.dirname().str();
    if (path.dirname().str().find(tmp.str()) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool is_ignored(const std::string &filepath) {
  Path p(filepath);
  std::string dirname = p.dirname().str();
  std::string basename = p.basename().str();

  // Check extensions
  for (auto &it : kIgnoredExtensions) {
    if (ends_with(basename, it)) {
      return true;
    }
  }

  return std::find(kIgnoredFileNames.begin(), kIgnoredFileNames.end(),
                   basename) != kIgnoredFileNames.end() ||
         is_ignored_path(p, kIgnoredPaths);
}

void prepare_git_tracked_files() {
  if (!g_git_tracked_files.empty()) {
    return;
  }
  // Get all files in the Git repository
  std::ostringstream os_cmd;
  // For Git v1.7 we need to change directory first
  os_cmd << "git ls-files --error-unmatch";
  auto result = cmd_exec(os_cmd.str(), false, g_source_dir.str());
  std::istringstream cmd_output(result.output);
  std::string tracked_file;

  while (std::getline(cmd_output, tracked_file, '\n')) {
    Path tmp_path(g_source_dir);
    tmp_path.append(tracked_file);
    Path real_path = tmp_path.real_path();
    if (!real_path.is_set()) {
      std::cerr << "realpath failed for " << tracked_file << ": "
                << strerror(errno) << std::endl;
      continue;
    }
    tracked_file = real_path.str();
    if (!is_ignored(tracked_file)) {
      os_cmd.str("");
      os_cmd << "git log HEAD --pretty=format:%ad --date=short "
                "--diff-filter=AM -- "
             << tracked_file;
      result = cmd_exec(os_cmd.str(), false, g_source_dir.str());
      // Result should contain at least 1 line with a year.
      if (result.output.size() < 10) {
        std::cerr << "Failed getting Git log info for " << tracked_file
                  << std::endl;
        continue;
      }
      try {
        g_git_tracked_files.push_back(GitInfo{
            Path(tracked_file),
            // Both first and year last modification could be the same
            std::stoi(result.output.substr(result.output.size() - 10, 4)),
            std::stoi(result.output.substr(0, 4))});
      } catch (...) {
        std::cerr << "Failed conversion: " << result.output << " , "
                  << tracked_file << std::endl;
      }
    }
  }
}

void prepare_all_files() {
  if (!g_git_tracked_files.empty()) {
    return;
  }
  // Get all files in the source repository
  std::ostringstream os_cmd;
#ifdef _WIN32
  os_cmd << "dir /b /s /a:-d";  // dump all files (no directories)
#else
  os_cmd << "find . -type f";
#endif
  auto result = cmd_exec(os_cmd.str(), false, g_source_dir.str());
  std::istringstream cmd_output(result.output);
  std::string tracked_file;

  // if CMAKE_BINARY_DIR is set, check if it isn't inside CMAKE_SOURCE_DIR
  // if yes, ignore files that are inside CMAKE_BINARY_DIR
  const char *cmake_binary_dir = std::getenv("CMAKE_BINARY_DIR");
  std::string binary_real_path;
  if (cmake_binary_dir != nullptr) {
    Path binary_dir(cmake_binary_dir);
    binary_real_path = binary_dir.real_path().str();
  }

  while (std::getline(cmd_output, tracked_file, '\n')) {
#ifdef _WIN32
    // path is already absolute
    Path real_path(tracked_file);
#else
    // make path absolute
    Path tmp_path(g_source_dir);
    tmp_path.append(tracked_file);
    Path real_path = tmp_path.real_path();
#endif
    if (!real_path.is_set()) {
      std::cerr << "realpath failed for " << tracked_file << ": "
                << strerror(errno) << std::endl;
      continue;
    }
    tracked_file = real_path.str();

    if (is_ignored(tracked_file)) continue;

    // ignore all files that start with the release folder
    if (cmake_binary_dir != nullptr &&
        tracked_file.size() > binary_real_path.size() &&
        tracked_file.compare(0, binary_real_path.size(), binary_real_path) == 0)
      continue;

    g_git_tracked_files.push_back(GitInfo{Path(tracked_file), -1, -1});
  }
}

class CheckLegal : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (Path(g_source_dir).join(".git").is_directory()) {
      prepare_git_tracked_files();
    } else {
      prepare_all_files();
    }
  }

  virtual void TearDown() {}
};

/* test if the all files that are in git have the proper copyright line
 *
 * A proper copyright line is:
 *
 * - copyright years: if start year == end year, start year may be omitted
 * - copyright start year: at least first git commit
 * - copyright end year: at least last git commit
 * - copyright line: fixed format
 *
 * The copyright years start before recorded history in git as the files
 * may come from another source. Similar to end date as git author-date
 * may contain too old date.
 */
TEST_F(CheckLegal, Copyright) {
  if (g_git_tracked_files.size() == 0) {
    std::cout << "[ SKIPPED  ] couldn't determine source files from "
                 "CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR"
              << std::endl;
    return;
  }

#ifdef GTEST_USES_POSIX_RE
  // gtest uses either simple-re or posix-re. Only the posix-re supports
  // captures which allows to extract the dates easily.
  //
  // if gtest uses posix-re, we can use posix-re directly too.
  const char *prefix = "Copyright (c)";
  std::string needle;
  needle = "Copyright \\(c\\) (([0-9]{4}), )?";  // m[1] and m[2]
  needle += "([0-9]{4}), ";                      // m[3]
  needle += "Oracle and/or its affiliates. All rights reserved.";

  // extract the years
  regex_t re;
  char re_err[1024];
  regmatch_t m[4];
  int err_code = regcomp(&re, needle.c_str(), REG_EXTENDED);
  if (err_code != 0) {
    EXPECT_LE(regerror(err_code, &re, re_err, sizeof(re_err)), sizeof(re_err));
    ASSERT_EQ(err_code, 0) << re_err;
  }

  for (auto &it : g_git_tracked_files) {
    std::ifstream curr_file(it.file.str());

    std::string line;
    bool copyright_found = false;

    while (std::getline(curr_file, line, '\n')) {
      if (line.find(prefix) != std::string::npos) {
        copyright_found = true;  // some copyright found

        EXPECT_THAT(line, ::testing::ContainsRegex(needle))
            << " in file: " << it.file.str();

        // match the needly again, but this time extract the copyright years.
        err_code =
            regexec(&re, line.c_str(), sizeof(m) / sizeof(regmatch_t), m, 0);
        if (err_code != 0) {
          if (err_code != REG_NOMATCH) {
            // REG_NOMATCH is handled by the ContainsRegex() already
            EXPECT_LE(regerror(err_code, &re, re_err, sizeof(re_err)),
                      sizeof(re_err));
            EXPECT_EQ(err_code, 0) << re_err;
          }
          break;
        }

        if (it.year_first_commit == -1 && it.year_last_commit == -1) {
          // break early, in case we don't have any git history.
          break;
        }

        // check that the start copyright year is less or equal to what we have
        // a commit for
        //
        // allow copyright years that are less than the recorded history in git
        ASSERT_GE(m[3].rm_so, 0) << m[3].rm_so;
        ASSERT_GE(m[3].rm_eo, 0) << m[3].rm_eo;
        ASSERT_GT(m[3].rm_eo, m[3].rm_so) << m[3].rm_so << " < " << m[3].rm_eo;
        std::string copyright_end_year =
            line.substr(static_cast<size_t>(m[3].rm_so),
                        static_cast<size_t>(m[3].rm_eo - m[3].rm_so));

        if (m[2].rm_so != -1) {
          ASSERT_GE(m[2].rm_so, 0) << m[2].rm_so;
          ASSERT_GE(m[2].rm_eo, 0) << m[2].rm_eo;
          ASSERT_GT(m[2].rm_eo, m[2].rm_so)
              << m[2].rm_so << " < " << m[2].rm_eo;
          std::string copyright_start_year =
              line.substr(static_cast<size_t>(m[2].rm_so),
                          static_cast<size_t>(m[2].rm_eo - m[2].rm_so));
          EXPECT_LE(std::stoi(copyright_start_year), it.year_first_commit)
              << " in file: " << it.file.str();
        } else {
          // no start-year in copyright.
          EXPECT_LE(std::stoi(copyright_end_year), it.year_first_commit)
              << " in file: " << it.file.str();
        }

        // copyright end year has to at least the one of the last commit
        //
        // allow copyright years that are larger than the recorded history in
        // git
        EXPECT_GE(std::stoi(copyright_end_year), it.year_last_commit)
            << " in file: " << it.file.str();
        break;
      }
    }
    curr_file.close();

    EXPECT_TRUE(copyright_found) << it.file.str() << ": No copyright found";
  }

  regfree(&re);
#endif
}

// Disabling this test. As we are now part of the server repository
// this check should be done elsewhere.
#if 0
TEST_F(CheckLegal, GPLLicense) {
#ifdef HAVE_LICENSE_COMMERCIAL
  std::cout << "[ SKIPPED  ] commerical build, not checking for GPL license headers" << std::endl;
  return;
#else
  if (g_git_tracked_files.size() == 0) {
    std::cout << "[ SKIPPED  ] couldn't determine source files from CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR" << std::endl;
    return;
  }

  std::vector<Path> extra_ignored{
      Path("README.txt"),
  };

  for (auto &it: g_git_tracked_files) {

    if (is_ignored_path(Path(it.file), extra_ignored)) {
      continue;
    }

    std::string line;
    std::string problem;
    bool found = false;
    size_t index = 0;

    std::ifstream curr_file(it.file.str());
    while (std::getline(curr_file, line, '\n')) {
      SCOPED_TRACE(line);
      if (line.find(kLicenseSnippets[index]) != std::string::npos) {
        found = true;
        index++;
        if (index == kLicenseSnippets.size()) {// matched last line
          break;
        }
        continue;
      }
    }
    curr_file.close();

    EXPECT_TRUE(found) << it.file << ": No license";
    EXPECT_EQ(index, kLicenseSnippets.size()) << it.file << ": Didn't find '" << kLicenseSnippets.at(index) << "' in license header";
  }
#endif
}
#endif  // #if 0

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  g_origin = Path(argv[0]).dirname();
  g_source_dir = get_cmake_source_dir();

  EXPECT_TRUE(g_source_dir.is_set());

  return RUN_ALL_TESTS();
}

#else

int main(int, char *) { return 0; }

#endif  // #ifndef _WIN32
