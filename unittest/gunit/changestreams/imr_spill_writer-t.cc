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
/// Unit tests for mysql::csa::Spill_file_writer (tasks.md task 2): the private
/// on-disk spill-file writer. The writer provisions a spill file under an
/// "in_memory_relaylog_temp_files" subdirectory of the channel's relay log
/// directory (FR17), names it "imr_sp_<lowercase-unique-id>" (FR24), lays down
/// the relay-log prefix (BINLOG_MAGIC + serialized FDE), appends raw event
/// bytes, flushes without fsync, tracks a monotonically advancing end position,
/// and deletes the file on destruction. These tests exercise the write side
/// only by reading the file's raw bytes back (the Relaylog_file_reader
/// round-trip is covered by the read-side task).

#include <gtest/gtest.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"
#include "sql/log_event.h"  // Format_description_log_event, BINLOG_MAGIC

namespace mysql::csa::unittests {

namespace fs = std::filesystem;

namespace {

/// Reads a whole file into a byte vector. Returns empty on open failure.
std::vector<unsigned char> read_whole_file(const std::string &name) {
  std::ifstream in(name, std::ios::binary);
  if (!in.good()) return {};
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(in),
                                    std::istreambuf_iterator<char>());
}

/// A fresh relay-log FDE shared_ptr, matching how the sink holds its FDE.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// Serialize a real Format_description_log_event into bytes, standing in for
/// the raw wire bytes of an event the receiver would append. A default server
/// FDE serializes with checksum OFF and needs no THD.
std::vector<unsigned char> serialize_event() {
  Format_description_log_event ev;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing a stand-in event must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

const char *as_char(const std::vector<unsigned char> &v) {
  return reinterpret_cast<const char *>(v.data());
}

bool is_all_lowercase(const std::string &s) {
  for (unsigned char c : s) {
    if (std::isupper(c)) return false;
  }
  return true;
}

}  // namespace

/// Fixture that gives each test a private, empty "relay log directory" and
/// tears the whole tree down afterwards (including the spill files and the
/// temp-files subdirectory the writer creates inside it).
class ImrSpillWriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    m_relay_log_dir =
        (fs::temp_directory_path() /
         ("imr_relaylog_" + std::to_string(::getpid()) + "_" +
          std::to_string(counter.fetch_add(1))))
            .string();
    fs::create_directories(m_relay_log_dir);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(m_relay_log_dir, ec);
  }

  std::string m_relay_log_dir;
};

// ---------------------------------------------------------------------------
// FR17 / FR24: location and naming.
// ---------------------------------------------------------------------------

// The spill file lives in "<relay_log_dir>/in_memory_relaylog_temp_files/" and
// is named "imr_sp_<lowercase-unique-id>".
TEST_F(ImrSpillWriterTest, PlacesFileInTempSubdirWithExpectedName) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir);
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  const fs::path temp_dir = writer.temp_dir();
  EXPECT_EQ(temp_dir.filename().string(), "in_memory_relaylog_temp_files");
  EXPECT_EQ(temp_dir.parent_path(), fs::path(m_relay_log_dir));
  EXPECT_TRUE(fs::is_directory(temp_dir));

  const fs::path file = writer.file_name();
  EXPECT_EQ(file.parent_path(), temp_dir);
  const std::string base = file.filename().string();
  EXPECT_EQ(base.rfind("imr_sp_", 0), 0u) << "name must start with imr_sp_";
  EXPECT_TRUE(is_all_lowercase(base)) << "name must be lowercase: " << base;
  EXPECT_GT(base.size(), std::strlen("imr_sp_"))
      << "name must carry a non-empty unique id";
}

// Each writer gets its own distinct file within the same channel's temp dir.
TEST_F(ImrSpillWriterTest, DistinctFilesPerWriter) {
  Spill_file_writer a(make_fde(), m_relay_log_dir);
  Spill_file_writer b(make_fde(), m_relay_log_dir);
  ASSERT_FALSE(a.open()) << a.get_error_str();
  ASSERT_FALSE(b.open()) << b.get_error_str();
  EXPECT_EQ(a.temp_dir(), b.temp_dir());  // same subdirectory
  EXPECT_NE(a.file_name(), b.file_name());
}

// An empty relay log directory is rejected.
TEST_F(ImrSpillWriterTest, EmptyRelayLogDirIsError) {
  Spill_file_writer writer(make_fde(), "");
  EXPECT_TRUE(writer.open());
  EXPECT_FALSE(writer.is_open());
  EXPECT_FALSE(writer.get_error_str().empty());
}

// ---------------------------------------------------------------------------
// open(): lays down the BINLOG_MAGIC + FDE prefix.
// ---------------------------------------------------------------------------

// After open(): a backing file exists, the stream is open, the file starts with
// the 4-byte magic, and the end position equals the on-disk prefix length.
TEST_F(ImrSpillWriterTest, OpenWritesRelayLogPrefix) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir);
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  EXPECT_TRUE(writer.is_open());
  ASSERT_FALSE(writer.file_name().empty());
  // Prefix is magic (4 bytes) plus a non-empty serialized FDE.
  EXPECT_GT(writer.end_position(), static_cast<my_off_t>(4));

  ASSERT_FALSE(writer.flush()) << writer.get_error_str();

  const auto bytes = read_whole_file(writer.file_name());
  ASSERT_GE(bytes.size(), static_cast<std::size_t>(4));
  EXPECT_EQ(0, std::memcmp(bytes.data(), BINLOG_MAGIC, 4))
      << "file must begin with BINLOG_MAGIC";
  // Everything written so far is on disk after flush().
  EXPECT_EQ(bytes.size(), writer.end_position());
}

// ---------------------------------------------------------------------------
// append_raw(): the end position advances by exactly the appended length, and
// only ever forward (monotonic).
// ---------------------------------------------------------------------------

TEST_F(ImrSpillWriterTest, AppendAdvancesEndPositionMonotonically) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir);
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  const std::vector<std::string> chunks = {"a", "bcbcbc", "1234567890",
                                           std::string(4096, 'x')};
  my_off_t prev = writer.end_position();
  for (const auto &c : chunks) {
    ASSERT_FALSE(writer.append_raw(c.data(), c.size())) << writer.get_error_str();
    EXPECT_EQ(writer.end_position(), prev + c.size());
    EXPECT_GT(writer.end_position(), prev) << "end position must advance";
    prev = writer.end_position();
  }

  // A zero-length append is a no-op and does not move the position.
  ASSERT_FALSE(writer.append_raw("", 0));
  EXPECT_EQ(writer.end_position(), prev);
}

// ---------------------------------------------------------------------------
// Raw round-trip: prefix + several appended "events" read back byte-for-byte.
// ---------------------------------------------------------------------------

TEST_F(ImrSpillWriterTest, RawRoundTripBytes) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir);
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  // Capture the prefix length (magic + FDE) so we can slice the body out.
  const my_off_t prefix_len = writer.end_position();

  // Append a handful of stand-in events and accumulate what we expect the body
  // region of the file to contain.
  std::vector<unsigned char> expected_body;
  for (int i = 0; i < 5; ++i) {
    const auto ev = serialize_event();
    ASSERT_FALSE(writer.append_raw(as_char(ev), ev.size()))
        << writer.get_error_str();
    expected_body.insert(expected_body.end(), ev.begin(), ev.end());
  }
  ASSERT_FALSE(writer.flush()) << writer.get_error_str();

  const auto bytes = read_whole_file(writer.file_name());
  ASSERT_EQ(bytes.size(), writer.end_position());
  ASSERT_GE(bytes.size(), static_cast<std::size_t>(prefix_len));
  EXPECT_EQ(0, std::memcmp(bytes.data(), BINLOG_MAGIC, 4));

  // The bytes after the prefix are exactly the concatenation of what we fed to
  // append_raw(), in order.
  const std::vector<unsigned char> body(bytes.begin() + prefix_len,
                                        bytes.end());
  EXPECT_EQ(body, expected_body);
}

// ---------------------------------------------------------------------------
// Destruction removes the spill file (the temp subdirectory is left in place).
// ---------------------------------------------------------------------------

TEST_F(ImrSpillWriterTest, DestructionRemovesFile) {
  std::string name;
  {
    Spill_file_writer writer(make_fde(), m_relay_log_dir);
    ASSERT_FALSE(writer.open()) << writer.get_error_str();
    name = writer.file_name();
    ASSERT_FALSE(name.empty());
    ASSERT_FALSE(writer.append_raw("payload-bytes", 13));
    ASSERT_FALSE(writer.flush());
    EXPECT_TRUE(fs::exists(name)) << "file should exist while writer is alive";
  }
  EXPECT_FALSE(fs::exists(name))
      << "spill file must be deleted when the writer is destroyed";
}

}  // namespace mysql::csa::unittests
