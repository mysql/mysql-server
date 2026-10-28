/* Copyright (c) 2026, Oracle and/or its affiliates.

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
*/

/// @file
/// Property test for spill-purge selectivity.
///
/// **Property: The purge removes only prefixed files.**
///
/// mysql::csa::purge_orphan_spill_files(relay_log_dir) deletes stale spill
/// files from the channel's "#in_memory_relaylog_temp_files" subdirectory. It
/// must remove only regular entries whose names begin with Spill_file_writer::
/// kFileNamePrefix ("imr_sp_"), must not touch unrelated files, and must
/// neither recurse into nor remove subdirectories -- even a subdirectory that
/// itself carries the spill prefix.
///
/// Over directories seeded with a random mix of prefixed spill files, unrelated
/// files (including near-miss names that share only part of the prefix), and a
/// prefixed subdirectory holding a file, this asserts that exactly the prefixed
/// regular files are removed and everything else survives.
///
/// The ImrSpillDirectoryTest cases check ensure_spill_directory().

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>  // geteuid
#endif

#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"

namespace mysql::csa::unittests {

namespace fs = std::filesystem;

namespace {
/// Create an empty regular file at @p path.
void touch(const fs::path &path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "x";  // a byte, so the entry is unambiguously a regular file
}

/// A fresh, private relay-log directory with its spill subdirectory provisioned
/// through the production entry point, torn down by the caller.
fs::path make_relay_log_dir() {
  static std::atomic<unsigned> counter{0};
  fs::path dir =
      fs::temp_directory_path() / ("imr_purge_" + std::to_string(::getpid()) +
                                   "_" + std::to_string(counter.fetch_add(1)));
  fs::create_directories(dir);
  (void)ensure_spill_directory(dir.string());
  return dir;
}

/// A fresh, private relay-log directory without a spill subdirectory.
fs::path make_bare_relay_log_dir() {
  static std::atomic<unsigned> counter{0};
  fs::path dir = fs::temp_directory_path() /
                 ("imr_spill_dir_" + std::to_string(::getpid()) + "_" +
                  std::to_string(counter.fetch_add(1)));
  fs::create_directories(dir);
  return dir;
}

constexpr int kIterations = 150;
}  // namespace

// Random populations: only the prefixed regular files are removed; unrelated
// files, near-miss names, and a prefixed subdirectory (with its contents) all
// survive.
TEST(ImrSpillPurgePropertyTest, RemovesOnlyPrefixedRegularFiles) {
  std::mt19937 rng(20260916u);
  std::uniform_int_distribution<int> spill_count_dist(0, 6);
  std::uniform_int_distribution<int> subdir_dist(0, 1);

  const std::string prefix = Spill_file_writer::kFileNamePrefix;  // "imr_sp_"

  // Unrelated / near-miss names that must never be removed.
  const std::vector<std::string> survivor_names = {"relay-bin.000001",
                                                   "relay-bin.index",
                                                   "imr_sp",  // no trailing '_'
                                                   "imr_s",
                                                   "ximr_sp_1",
                                                   "readme.txt"};

  for (int iter = 0; iter < kIterations; ++iter) {
    const fs::path relay_dir = make_relay_log_dir();
    const fs::path spill_dir = spill_directory_path(relay_dir.string());

    std::vector<fs::path> should_be_removed;
    std::vector<fs::path> should_survive;

    // Prefixed spill files -> removed.
    const int n_spill = spill_count_dist(rng);
    for (int i = 0; i < n_spill; ++i) {
      fs::path p =
          spill_dir / (prefix + std::to_string(iter) + "_" + std::to_string(i));
      touch(p);
      should_be_removed.push_back(p);
    }

    // A random subset of the unrelated / near-miss names -> survive. The names
    // are used verbatim (no numeric suffix): appending one to a near-miss like
    // "imr_sp" would turn it into a genuinely prefixed "imr_sp_..." name.
    std::uniform_int_distribution<int> include_dist(0, 1);
    for (const std::string &base : survivor_names) {
      if (include_dist(rng) == 0) continue;
      fs::path p = spill_dir / base;
      touch(p);
      should_survive.push_back(p);
    }

    // Optionally a prefixed subdirectory with a file inside: neither the
    // directory nor its content may be touched (no recursion, no dir removal).
    fs::path prefixed_subdir;
    fs::path file_in_subdir;
    if (subdir_dist(rng) != 0) {
      prefixed_subdir = spill_dir / (prefix + "subdir_" + std::to_string(iter));
      fs::create_directory(prefixed_subdir);
      file_in_subdir = prefixed_subdir / (prefix + "nested");
      touch(file_in_subdir);
    }

    // The operation under test. Every stale file is deletable here.
    EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));

    for (const auto &p : should_be_removed)
      EXPECT_FALSE(fs::exists(p)) << "prefixed spill file survived: " << p;
    for (const auto &p : should_survive)
      EXPECT_TRUE(fs::exists(p)) << "unrelated file was removed: " << p;
    if (!prefixed_subdir.empty()) {
      EXPECT_TRUE(fs::is_directory(prefixed_subdir))
          << "prefixed subdirectory was removed: " << prefixed_subdir;
      EXPECT_TRUE(fs::exists(file_in_subdir))
          << "purge recursed into a subdirectory: " << file_in_subdir;
    }
    // The spill directory itself must remain.
    EXPECT_TRUE(fs::is_directory(spill_dir));

    std::error_code ec;
    fs::remove_all(relay_dir, ec);
  }
}

// A repeated purge of the same directory (the shared-relay-log-directory case)
// is harmless and idempotent.
TEST(ImrSpillPurgePropertyTest, RepeatedPurgeIsIdempotent) {
  const fs::path relay_dir = make_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  const std::string prefix = Spill_file_writer::kFileNamePrefix;

  touch(spill_dir / (prefix + "a"));
  touch(spill_dir / "keep.me");

  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));
  // The second visit finds nothing.
  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));

  EXPECT_FALSE(fs::exists(spill_dir / (prefix + "a")));
  EXPECT_TRUE(fs::exists(spill_dir / "keep.me"));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A missing spill directory is not an error and removes nothing.
TEST(ImrSpillPurgePropertyTest, MissingSpillDirectoryIsNoError) {
  static std::atomic<unsigned> counter{0};
  fs::path relay_dir = fs::temp_directory_path() /
                       ("imr_purge_missing_" + std::to_string(::getpid()) +
                        "_" + std::to_string(counter.fetch_add(1)));
  fs::create_directories(relay_dir);  // relay dir exists, spill subdir does not

  // Must not throw or crash, and is not a failure.
  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));
  EXPECT_FALSE(fs::exists(spill_directory_path(relay_dir.string())));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

#ifndef _WIN32
// A stale spill file that cannot be deleted is reported as a failure,
// and the file is left in place. A read-only spill directory makes the unlink
// fail; root ignores directory permissions, so the test is skipped there.
// POSIX-only: Windows does not use directory permissions for deletion.
TEST(ImrSpillPurgePropertyTest, UndeletableFileIsReported) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can delete from a read-only dir";
  const fs::path relay_dir = make_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  const fs::path stale =
      spill_dir / (std::string(Spill_file_writer::kFileNamePrefix) + "stuck");
  touch(stale);

  fs::permissions(spill_dir, fs::perms::owner_read | fs::perms::owner_exec,
                  fs::perm_options::replace);
  EXPECT_TRUE(purge_orphan_spill_files(relay_dir.string()));
  fs::permissions(spill_dir, fs::perms::owner_all, fs::perm_options::replace);

  EXPECT_TRUE(fs::exists(stale));
  // Once the file can be deleted, the purge succeeds again.
  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));
  EXPECT_FALSE(fs::exists(stale));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}
#endif  // _WIN32

// ensure_spill_directory() creates the directory and accepts an
// existing one; an empty relay log directory is nothing to do.
TEST(ImrSpillDirectoryTest, CreatesAndAcceptsDirectory) {
  EXPECT_FALSE(ensure_spill_directory(""));

  const fs::path relay_dir = make_bare_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  EXPECT_FALSE(ensure_spill_directory(relay_dir.string()));
  EXPECT_TRUE(fs::is_directory(spill_dir));
  EXPECT_FALSE(ensure_spill_directory(relay_dir.string()));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A regular file at the spill path is rejected and left in place. The
// purge then has no directory to read, which is not a failure.
TEST(ImrSpillDirectoryTest, RejectsRegularFile) {
  const fs::path relay_dir = make_bare_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  touch(spill_dir);

  EXPECT_TRUE(ensure_spill_directory(relay_dir.string()));
  EXPECT_TRUE(fs::is_regular_file(spill_dir));
  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A spill path whose parent does not exist cannot be created.
TEST(ImrSpillDirectoryTest, RejectsUncreatableDirectory) {
  const fs::path relay_dir = make_bare_relay_log_dir();
  const fs::path missing = relay_dir / "missing";

  EXPECT_TRUE(ensure_spill_directory(missing.string()));
  EXPECT_FALSE(fs::exists(missing));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

#ifndef _WIN32
// A symbolic link at the spill path is rejected, whether it points to
// a directory or nowhere. POSIX-only: the test creates the links with symlink.
TEST(ImrSpillDirectoryTest, RejectsSymbolicLink) {
  const fs::path relay_dir = make_bare_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  const fs::path target = relay_dir / "target";
  fs::create_directory(target);

  fs::create_directory_symlink(target, spill_dir);
  EXPECT_TRUE(ensure_spill_directory(relay_dir.string()));
  EXPECT_TRUE(fs::is_symlink(spill_dir));

  fs::remove(spill_dir);
  fs::create_directory_symlink(relay_dir / "dangling", spill_dir);
  EXPECT_TRUE(ensure_spill_directory(relay_dir.string()));
  EXPECT_TRUE(fs::is_symlink(fs::symlink_status(spill_dir)));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// The purge never deletes through a symbolic link at the spill path.
TEST(ImrSpillPurgePropertyTest, DoesNotPurgeThroughSymbolicLink) {
  const fs::path relay_dir = make_bare_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  const fs::path target = relay_dir / "target";
  fs::create_directory(target);
  const fs::path victim = target / "imr_sp_victim";
  touch(victim);
  fs::create_directory_symlink(target, spill_dir);

  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));
  EXPECT_TRUE(fs::exists(victim));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A stale spill file without the owner read bit is deleted too.
TEST(ImrSpillPurgePropertyTest, RemovesFileWithoutReadPermission) {
  const fs::path relay_dir = make_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  const fs::path stale = spill_dir / "imr_sp_mode000";
  touch(stale);
  fs::permissions(stale, fs::perms::none, fs::perm_options::replace);

  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));
  EXPECT_FALSE(fs::exists(stale));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A spill directory the server cannot write is rejected. Root ignores
// directory permissions, so the test is skipped there.
TEST(ImrSpillDirectoryTest, RejectsReadOnlyDirectory) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can write a read-only dir";
  const fs::path relay_dir = make_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());

  fs::permissions(spill_dir, fs::perms::owner_read | fs::perms::owner_exec,
                  fs::perm_options::replace);
  EXPECT_TRUE(ensure_spill_directory(relay_dir.string()));
  fs::permissions(spill_dir, fs::perms::owner_all, fs::perm_options::replace);
  EXPECT_FALSE(ensure_spill_directory(relay_dir.string()));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A spill directory the server can write and search but not read is rejected.
// Root ignores directory permissions, so the test is skipped there.
TEST(ImrSpillDirectoryTest, RejectsUnreadableDirectory) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can read a write-only dir";
  const fs::path relay_dir = make_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());

  fs::permissions(spill_dir, fs::perms::owner_write | fs::perms::owner_exec,
                  fs::perm_options::replace);
  EXPECT_TRUE(ensure_spill_directory(relay_dir.string()));
  fs::permissions(spill_dir, fs::perms::owner_all, fs::perm_options::replace);
  EXPECT_FALSE(ensure_spill_directory(relay_dir.string()));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}

// A spill directory that cannot be listed is reported as a failure, and its
// stale files stay in place. Skipped as root, as above.
TEST(ImrSpillPurgePropertyTest, UnreadableDirectoryIsReported) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can read a write-only dir";
  const fs::path relay_dir = make_relay_log_dir();
  const fs::path spill_dir = spill_directory_path(relay_dir.string());
  const fs::path stale =
      spill_dir / (std::string(Spill_file_writer::kFileNamePrefix) + "hidden");
  touch(stale);

  fs::permissions(spill_dir, fs::perms::owner_write | fs::perms::owner_exec,
                  fs::perm_options::replace);
  EXPECT_TRUE(purge_orphan_spill_files(relay_dir.string()));
  fs::permissions(spill_dir, fs::perms::owner_all, fs::perm_options::replace);

  EXPECT_TRUE(fs::exists(stale));
  // Once the directory can be read, the purge succeeds again.
  EXPECT_FALSE(purge_orphan_spill_files(relay_dir.string()));
  EXPECT_FALSE(fs::exists(stale));

  std::error_code ec;
  fs::remove_all(relay_dir, ec);
}
#endif  // _WIN32

}  // namespace mysql::csa::unittests
