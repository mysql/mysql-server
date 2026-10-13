/*****************************************************************************

Copyright (c) 2026 Amazon.com, Inc.  All rights reserved.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "manifest.h"

namespace manifest_unittest {

using manifest::kComponentSeparator;
using manifest::Manifest_reader;
using manifest::merge_component_lists;

/* ------------------------------------------------------------------ */
/* Helper: split a component string the way the consumer does —       */
/* always on comma, matching the historical get_next_component().      */
/* When the separator constant is wrong (e.g. ";"), the split will    */
/* not find the delimiter and the test fails.                         */
/* ------------------------------------------------------------------ */
static std::vector<std::string> consumer_split(const std::string &s) {
  std::vector<std::string> result;
  if (s.empty()) return result;
  const std::string sep(","); /* hardcoded — this is the contract */
  std::string::size_type start = 0;
  std::string::size_type pos;
  while ((pos = s.find(sep, start)) != std::string::npos) {
    result.push_back(s.substr(start, pos - start));
    start = pos + sep.size();
  }
  result.push_back(s.substr(start));
  return result;
}

/* ================================================================== */
/* (a) merge_component_lists round-trips through the separator        */
/* ================================================================== */

TEST(ManifestMerge, MergeJoinsWithConsumerSeparator) {
  const std::string global = "file://component_a";
  const std::string local = "file://component_b";
  const std::string merged = merge_component_lists(global, local);

  /* The merged string must split back into exactly the two inputs
     when the consumer splits on comma — this catches the ";" bug. */
  auto parts = consumer_split(merged);
  ASSERT_EQ(2u, parts.size());
  EXPECT_EQ(global, parts[0]);
  EXPECT_EQ(local, parts[1]);
}

/* ================================================================== */
/* (b) edge cases                                                     */
/* ================================================================== */

TEST(ManifestMerge, EmptyGlobalReturnsLocal) {
  EXPECT_EQ("file://local", merge_component_lists("", "file://local"));
}

TEST(ManifestMerge, EmptyLocalReturnsGlobal) {
  EXPECT_EQ("file://global", merge_component_lists("file://global", ""));
}

TEST(ManifestMerge, BothEmptyReturnsEmpty) {
  EXPECT_EQ("", merge_component_lists("", ""));
}

TEST(ManifestMerge, NoTrailingSeparator) {
  const std::string merged = merge_component_lists("a", "b");
  const std::string sep(kComponentSeparator);
  /* Must not end with the separator. */
  EXPECT_NE(sep, merged.substr(merged.size() - sep.size()));
  /* Must not start with the separator. */
  EXPECT_NE(sep, merged.substr(0, sep.size()));
}

/* ================================================================== */
/* (d) de-duplication                                                  */
/* ================================================================== */

TEST(ManifestMerge, DuplicateAcrossGlobalLocalKeptOnceGlobalPosition) {
  /* 'shared' appears in both; it should appear once, in the global
     position, and the local-only URN appended after. */
  const std::string merged =
      merge_component_lists("file://a,file://shared", "file://shared,file://b");
  auto parts = consumer_split(merged);
  ASSERT_EQ(3u, parts.size());
  EXPECT_EQ("file://a", parts[0]);
  EXPECT_EQ("file://shared", parts[1]);
  EXPECT_EQ("file://b", parts[2]);
}

TEST(ManifestMerge, LocalEntirelyContainedInGlobalYieldsGlobalUnchanged) {
  const std::string global = "file://a,file://b,file://c";
  const std::string merged = merge_component_lists(global, "file://b,file://a");
  EXPECT_EQ(global, merged);
}

TEST(ManifestMerge, DuplicatesWithinOneSideCollapsed) {
  /* Duplicates within global should collapse; likewise within local. */
  const std::string merged =
      merge_component_lists("file://x,file://x", "file://y,file://y");
  auto parts = consumer_split(merged);
  ASSERT_EQ(2u, parts.size());
  EXPECT_EQ("file://x", parts[0]);
  EXPECT_EQ("file://y", parts[1]);
}

TEST(ManifestMerge, EmptyTokensFromDoubleCommaDoNotProduceEmptyURNs) {
  /* Consecutive separators (',,') must not yield empty strings. */
  const std::string merged =
      merge_component_lists("file://a,,file://b", ",,file://c,,");
  auto parts = consumer_split(merged);
  ASSERT_EQ(3u, parts.size());
  EXPECT_EQ("file://a", parts[0]);
  EXPECT_EQ("file://b", parts[1]);
  EXPECT_EQ("file://c", parts[2]);
}

/* ================================================================== */
/* (c) Manifest_reader via temp manifest files                        */
/* ================================================================== */

#ifndef _WIN32
/* mkdtemp()/rmdir() are POSIX-only; the "/tmp" path does not exist  */
/* on Windows.  Guard the entire fixture so it compiles everywhere.   */

/* Helper: write a manifest file for a fake executable and return     */
/* the executable path that Manifest_reader expects.                  */
class ManifestReaderTest : public ::testing::Test {
 protected:
  std::string tmpdir_;

  void SetUp() override {
    /* Create a unique temporary directory. */
    char tmpl[] = "/tmp/manifest_test_XXXXXX";
    char *d = mkdtemp(tmpl);
    ASSERT_NE(nullptr, d);
    tmpdir_ = d;
  }

  void TearDown() override {
    /* Clean up temp files. */
    std::string manifest = tmpdir_ + "/mysqld.my";
    std::remove(manifest.c_str());
    std::string local_manifest = local_dir() + "mysqld.my";
    std::remove(local_manifest.c_str());
    rmdir(local_dir().c_str());
    rmdir(tmpdir_.c_str());
  }

  /* Instance path for local manifests (trailing separator, as the */
  /* server passes it).                                            */
  std::string local_dir() const { return tmpdir_ + "/instance/"; }

  /* Write content to local_dir()/mysqld.my. */
  void write_local_manifest(const std::string &content) {
    mkdir(local_dir().c_str(), 0700);
    std::ofstream ofs(local_dir() + "mysqld.my");
    ofs << content;
    ofs.close();
  }

  /* Write content to tmpdir_/mysqld.my and return the fake exe path. */
  std::string write_manifest(const std::string &content) {
    std::string manifest_path = tmpdir_ + "/mysqld.my";
    std::ofstream ofs(manifest_path);
    ofs << content;
    ofs.close();
    return tmpdir_ + "/mysqld";
  }
};

TEST_F(ManifestReaderTest, MergeLocalManifestDefaultsFalse) {
  /* A manifest with only read_local_manifest — merge_local_manifest
     must default to false. */
  std::string exe = write_manifest(R"({ "read_local_manifest": true })");

  Manifest_reader reader(exe, "");
  EXPECT_TRUE(reader.file_present());
  EXPECT_TRUE(reader.read_local_manifest());
  EXPECT_FALSE(reader.merge_local_manifest());
}

TEST_F(ManifestReaderTest, MergeLocalManifestTrueWhenSet) {
  std::string exe = write_manifest(
      R"({ "components": "file://comp_a", "merge_local_manifest": true })");

  Manifest_reader reader(exe, "");
  EXPECT_TRUE(reader.file_present());
  EXPECT_TRUE(reader.merge_local_manifest());

  std::string components;
  EXPECT_TRUE(reader.components(components));
  EXPECT_EQ("file://comp_a", components);
}

TEST_F(ManifestReaderTest, ReadLocalManifestParsesAsExpected) {
  std::string exe = write_manifest(R"({ "read_local_manifest": true })");

  Manifest_reader reader(exe, "");
  EXPECT_TRUE(reader.read_local_manifest());
  EXPECT_FALSE(reader.merge_local_manifest());
}

/* ------------------------------------------------------------------ */
/* merge_local_components: the merge_local_manifest step of           */
/* Deployed_components::load(), driven by real manifest files.        */
/* ------------------------------------------------------------------ */

TEST_F(ManifestReaderTest, MergeNoLocalManifestDedupsGlobal) {
  std::string exe = write_manifest(
      R"({ "components": "file://a,file://a", "merge_local_manifest": true })");
  Manifest_reader global(exe, "");
  Manifest_reader local(exe, local_dir()); /* absent */
  EXPECT_FALSE(local.file_present());

  std::string components;
  ASSERT_TRUE(global.components(components));
  EXPECT_TRUE(manifest::merge_local_components(global, local, components));
  EXPECT_EQ("file://a", components);
}

TEST_F(ManifestReaderTest, MergeZeroByteLocalManifestDedupsGlobal) {
  std::string exe = write_manifest(
      R"({ "components": "file://a,file://a", "merge_local_manifest": true })");
  write_local_manifest("");
  Manifest_reader global(exe, "");
  Manifest_reader local(exe, local_dir());
  EXPECT_TRUE(local.file_present());
  EXPECT_TRUE(local.empty());

  std::string components;
  ASSERT_TRUE(global.components(components));
  EXPECT_TRUE(manifest::merge_local_components(global, local, components));
  EXPECT_EQ("file://a", components);
}

TEST_F(ManifestReaderTest, MergeLocalManifestAppendedAndDeduped) {
  std::string exe = write_manifest(
      R"({ "components": "file://a,file://a", "merge_local_manifest": true })");
  write_local_manifest(R"({ "components": "file://a,file://b" })");
  Manifest_reader global(exe, "");
  Manifest_reader local(exe, local_dir());

  std::string components;
  ASSERT_TRUE(global.components(components));
  EXPECT_TRUE(manifest::merge_local_components(global, local, components));
  EXPECT_EQ("file://a,file://b", components);
}

TEST_F(ManifestReaderTest, MergeLocalIsGlobalFileDedupsGlobal) {
  /* Instance path resolving to the global manifest: not merged into */
  /* itself, but the global list is still de-duplicated.             */
  std::string exe = write_manifest(
      R"({ "components": "file://a,file://a", "merge_local_manifest": true })");
  Manifest_reader global(exe, "");
  Manifest_reader local(exe, tmpdir_ + "/");
  ASSERT_EQ(global.manifest_file(), local.manifest_file());

  std::string components;
  ASSERT_TRUE(global.components(components));
  EXPECT_TRUE(manifest::merge_local_components(global, local, components));
  EXPECT_EQ("file://a", components);
}

TEST_F(ManifestReaderTest, MergeLocalWithoutComponentsFails) {
  std::string exe = write_manifest(
      R"({ "components": "file://a", "merge_local_manifest": true })");
  write_local_manifest(R"({ "read_local_manifest": false })");
  Manifest_reader global(exe, "");
  Manifest_reader local(exe, local_dir());

  std::string components;
  ASSERT_TRUE(global.components(components));
  EXPECT_FALSE(manifest::merge_local_components(global, local, components));
  EXPECT_EQ("file://a", components); /* unchanged on failure */
}

#endif /* !_WIN32 */

}  // namespace manifest_unittest
