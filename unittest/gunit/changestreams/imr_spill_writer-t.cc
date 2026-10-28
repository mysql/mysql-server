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
/// Unit tests for mysql::csa::Spill_file_writer: the private
/// on-disk spill-file writer. The writer provisions a spill file under an
/// "#in_memory_relaylog_temp_files" subdirectory of the channel's relay log
/// directory, names it "imr_sp_<lowercase-unique-id>", lays down
/// the relay-log prefix (BINLOG_MAGIC + serialized FDE), appends raw event
/// bytes, flushes without fsync, tracks a monotonically advancing end position,
/// and deletes the file on destruction. These tests exercise the write side
/// only by reading the file's raw bytes back (the Relaylog_file_reader
/// round-trip is covered by the read-side tests).

#include <gtest/gtest.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>  // geteuid
#endif

#include "my_inttypes.h"
#include "mysql/strings/m_ctype.h"  // my_charset_utf8mb3_general_ci
#include "mysys_err.h"              // EE_WRITE
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // ensure_spill_directory
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"  // Trx_envelope_queue (spill gauges)
#include "sql/log_event.h"  // Format_description_log_event, BINLOG_MAGIC
#include "sql/mysqld_cs.h"  // system_charset_info
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"  // test_memory_resource
#include "unittest/gunit/test_utils.h"  // Server_initializer::set_expected_error

#ifndef _WIN32
// The write() hook in mysys/my_write.cc, for failing a flush.
extern ssize_t (*mock_write)(int fd, const void *buf, size_t count);
#endif

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
    m_relay_log_dir = (fs::temp_directory_path() /
                       ("imr_relaylog_" + std::to_string(::getpid()) + "_" +
                        std::to_string(counter.fetch_add(1))))
                          .string();
    fs::create_directories(m_relay_log_dir);
    // The spill-files subdirectory is provisioned once at server start by
    // ensure_spill_directory() (from init_replica); Spill_file_writer::open()
    // no longer creates it lazily. This harness bypasses server startup, so
    // provision it here through the same production entry point.
    (void)ensure_spill_directory(m_relay_log_dir);
    // The writer encodes channel names from the server's system charset.
    m_charset = system_charset_info;
    system_charset_info = &my_charset_utf8mb3_general_ci;
  }

  void TearDown() override {
    system_charset_info = m_charset;
    std::error_code ec;
    fs::remove_all(m_relay_log_dir, ec);
  }

  std::string m_relay_log_dir;
  CHARSET_INFO *m_charset{nullptr};
};

// ---------------------------------------------------------------------------
// Location and naming.
// ---------------------------------------------------------------------------

// The spill file lives in "<relay_log_dir>/#in_memory_relaylog_temp_files/" and
// is named "imr_sp_<lowercase-unique-id>".
TEST_F(ImrSpillWriterTest, PlacesFileInTempSubdirWithExpectedName) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir);
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  const fs::path temp_dir = writer.temp_dir();
  EXPECT_EQ(temp_dir.filename().string(), "#in_memory_relaylog_temp_files");
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

// A named channel embeds its name in the spill file name as
// "imr_sp_<channel>_<unique_id>", so spill files of concurrently running
// channels sharing the one server-level spill directory do not collide.
TEST_F(ImrSpillWriterTest, NamedChannelEmbedsChannelInFileName) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir, "ch1");
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  const std::string base = fs::path(writer.file_name()).filename().string();
  EXPECT_EQ(base.rfind("imr_sp_ch1_", 0), 0u)
      << "named-channel spill file must be imr_sp_<channel>_<id>: " << base;
  EXPECT_GT(base.size(), std::strlen("imr_sp_ch1_"))
      << "name must carry a non-empty unique id after the channel: " << base;
}

// A channel name with a path separator is encoded as in relay log file names.
TEST_F(ImrSpillWriterTest, ChannelWithSlashIsEncodedInFileName) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir, "a/b");
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  const std::string base = fs::path(writer.file_name()).filename().string();
  EXPECT_EQ(base.rfind("imr_sp_a@002fb_", 0), 0u)
      << "channel must be in the filename charset: " << base;
}

// The default channel (empty name) keeps the unqualified "imr_sp_<unique_id>"
// form: no channel segment, and in particular no doubled underscore.
TEST_F(ImrSpillWriterTest, DefaultChannelHasNoChannelSegment) {
  Spill_file_writer writer(make_fde(), m_relay_log_dir, /*channel_name=*/"");
  ASSERT_FALSE(writer.open()) << writer.get_error_str();

  const std::string base = fs::path(writer.file_name()).filename().string();
  EXPECT_EQ(base.rfind("imr_sp_", 0), 0u) << "name must start with imr_sp_";
  EXPECT_NE(base.rfind("imr_sp__", 0), 0u)
      << "default channel must not add a channel segment (no doubled '_'): "
      << base;
  EXPECT_GT(base.size(), std::strlen("imr_sp_"))
      << "name must carry a non-empty unique id";
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
    ASSERT_FALSE(writer.append_raw(c.data(), c.size()))
        << writer.get_error_str();
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

// ---------------------------------------------------------------------------
// SPILL_BYTES_USED accounting: the gauge counts the bytes on disk. Encryption
// is off in gunit, so the encryption-header overhead is 0 and the gauge, the
// file size and end_position() are all equal.
// ---------------------------------------------------------------------------

TEST_F(ImrSpillWriterTest, SpillBytesGaugeEqualsFileSize) {
  Trx_envelope_queue queue(/*memory_limit=*/1u << 20,
                           /*spill_threshold=*/1u << 16,
                           test_memory_resource(1u << 20));
  {
    Spill_file_writer writer(make_fde(), m_relay_log_dir, /*channel_name=*/"",
                             &queue);
    ASSERT_FALSE(writer.open()) << writer.get_error_str();
    ASSERT_FALSE(writer.flush()) << writer.get_error_str();
    EXPECT_EQ(queue.spill_file_count(), 1u);
    EXPECT_EQ(queue.spill_bytes_used(), fs::file_size(writer.file_name()));
    EXPECT_EQ(queue.spill_bytes_used(), writer.end_position());

    for (int i = 0; i < 3; ++i) {
      const auto ev = serialize_event();
      ASSERT_FALSE(writer.append_raw(as_char(ev), ev.size()))
          << writer.get_error_str();
      ASSERT_FALSE(writer.flush()) << writer.get_error_str();
      EXPECT_EQ(queue.spill_bytes_used(), fs::file_size(writer.file_name()));
      EXPECT_EQ(queue.spill_bytes_used(), writer.end_position());
    }
  }
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_EQ(queue.spill_file_count(), 0u);
}

// close_stream() keeps the file, its gauges and the disk reservation; a second
// call does nothing, and the destructor still deletes the file and returns
// the gauges and the reservation.
TEST_F(ImrSpillWriterTest, CloseStreamKeepsFileAndAccounting) {
  const std::size_t disk_limit = 1u << 20;
  const std::size_t trx_length = 1u << 17;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(/*memory_limit=*/1u << 20,
                           /*spill_threshold=*/1u << 16,
                           test_memory_resource(1u << 20),
                           /*relay_log_dir=*/{}, /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::SPILL));

  std::string name;
  {
    Spill_file_writer writer(make_fde(), m_relay_log_dir, /*channel_name=*/"",
                             &queue, trx_length);
    ASSERT_FALSE(writer.open()) << writer.get_error_str();
    const auto ev = serialize_event();
    ASSERT_FALSE(writer.append_raw(as_char(ev), ev.size()))
        << writer.get_error_str();
    ASSERT_FALSE(writer.flush()) << writer.get_error_str();
    name = writer.file_name();
    const auto size = fs::file_size(name);

    writer.close_stream();
    writer.close_stream();

    EXPECT_TRUE(fs::exists(name)) << "close_stream() must keep the file";
    EXPECT_EQ(fs::file_size(name), size);
    EXPECT_EQ(size, writer.end_position());
    EXPECT_EQ(queue.spill_file_count(), 1u);
    EXPECT_EQ(queue.spill_bytes_used(), size);
    EXPECT_EQ(disk->m_available.load(),
              static_cast<long long>(disk_limit - trx_length));
  }

  EXPECT_FALSE(fs::exists(name)) << "the destructor must delete the file";
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
  EXPECT_FALSE(queue.spill_delete_failed());
}

#ifndef _WIN32
// A spill file that cannot be deleted stays on disk, but the writer still
// returns its gauges and disk reservation, and records the failure on the
// queue. A read-only spill directory makes the unlink fail; root ignores
// directory permissions, so the test is skipped there.
TEST_F(ImrSpillWriterTest, FailedDeleteReleasesAccountingAndFlagsQueue) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can delete from a read-only dir";
  const std::size_t disk_limit = 1u << 20;
  const std::size_t trx_length = 1u << 17;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(/*memory_limit=*/1u << 20,
                           /*spill_threshold=*/1u << 16,
                           test_memory_resource(1u << 20),
                           /*relay_log_dir=*/{}, /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::SPILL));
  ASSERT_EQ(disk->m_available.load(),
            static_cast<long long>(disk_limit - trx_length));

  std::string name;
  std::string spill_dir;
  {
    Spill_file_writer writer(make_fde(), m_relay_log_dir, /*channel_name=*/"",
                             &queue, trx_length);
    ASSERT_FALSE(writer.open()) << writer.get_error_str();
    const auto ev = serialize_event();
    ASSERT_FALSE(writer.append_raw(as_char(ev), ev.size()))
        << writer.get_error_str();
    ASSERT_FALSE(writer.flush()) << writer.get_error_str();
    name = writer.file_name();
    spill_dir = writer.temp_dir();
    EXPECT_EQ(queue.spill_file_count(), 1u);
    EXPECT_EQ(queue.spill_bytes_used(), fs::file_size(name));
    EXPECT_FALSE(queue.spill_delete_failed());
    fs::permissions(spill_dir, fs::perms::owner_read | fs::perms::owner_exec,
                    fs::perm_options::replace);
  }
  fs::permissions(spill_dir, fs::perms::owner_all, fs::perm_options::replace);

  EXPECT_TRUE(fs::exists(name)) << "an undeletable file must stay on disk";
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
  EXPECT_TRUE(queue.spill_delete_failed());
}

// A failed delete after a setup failure only logs the warning: the queue
// records nothing, and the accounting is still released.
TEST_F(ImrSpillWriterTest, FailedSetupDiscardDoesNotFlagQueue) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can delete from a read-only dir";
  const std::size_t disk_limit = 1u << 20;
  const std::size_t trx_length = 1u << 17;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(/*memory_limit=*/1u << 20,
                           /*spill_threshold=*/1u << 16,
                           test_memory_resource(1u << 20),
                           /*relay_log_dir=*/{}, /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::SPILL));

  std::string name;
  std::string spill_dir;
  {
    Spill_file_writer writer(make_fde(), m_relay_log_dir, /*channel_name=*/"",
                             &queue, trx_length);
    ASSERT_FALSE(writer.open()) << writer.get_error_str();
    name = writer.file_name();
    spill_dir = writer.temp_dir();
    fs::permissions(spill_dir, fs::perms::owner_read | fs::perms::owner_exec,
                    fs::perm_options::replace);
    writer.discard_after_setup_failure();
    fs::permissions(spill_dir, fs::perms::owner_all, fs::perm_options::replace);
    EXPECT_TRUE(writer.file_name().empty());
  }

  EXPECT_TRUE(fs::exists(name)) << "an undeletable file must stay on disk";
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(disk->m_available.load(), static_cast<long long>(disk_limit));
  EXPECT_FALSE(queue.spill_delete_failed());
}

// A setup failure returns the gauges and the disk reservation before the
// writer is destroyed, and the destructor does not return them again. Covers
// an open() failure (no spill subdirectory) and a failure of only the first
// flush() (write() fails through the mysys hook).
TEST_F(ImrSpillWriterTest, SetupFailureReleasesAccountingAtOnce) {
  const std::size_t disk_limit = 1u << 20;
  const std::size_t trx_length = 1u << 17;
  Resource_entry *disk = test_spill_disk_resource(disk_limit);
  Trx_envelope_queue queue(/*memory_limit=*/1u << 20,
                           /*spill_threshold=*/1u << 16,
                           test_memory_resource(1u << 20),
                           /*relay_log_dir=*/{}, /*channel_name=*/{}, disk);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  const auto full = static_cast<long long>(disk_limit);

  // open() fails before any file exists.
  {
    ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::SPILL));
    ASSERT_EQ(disk->m_available.load(),
              static_cast<long long>(disk_limit - trx_length));
    Spill_file_writer writer(make_fde(), m_relay_log_dir + "/missing",
                             /*channel_name=*/"", &queue, trx_length);
    ASSERT_TRUE(writer.open());
    writer.discard_after_setup_failure();
    EXPECT_EQ(queue.spill_file_count(), 0u);
    EXPECT_EQ(queue.spill_bytes_used(), 0u);
    EXPECT_EQ(disk->m_available.load(), full);
  }
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_EQ(disk->m_available.load(), full);

  // open() succeeds and counts the file, then the first flush() fails.
  std::string name;
  {
    ASSERT_FALSE(queue.acquire_admission(trx_length, Envelope_path::SPILL));
    Spill_file_writer writer(make_fde(), m_relay_log_dir, /*channel_name=*/"",
                             &queue, trx_length);
    ASSERT_FALSE(writer.open()) << writer.get_error_str();
    name = writer.file_name();
    EXPECT_EQ(queue.spill_file_count(), 1u);
    EXPECT_GT(queue.spill_bytes_used(), 0u);
    mock_write = [](int, const void *, size_t) -> ssize_t {
      errno = EIO;
      return -1;
    };
    my_testing::Server_initializer::set_expected_error(EE_WRITE);
    const bool flush_failed = writer.flush();
    writer.discard_after_setup_failure();
    my_testing::Server_initializer::set_expected_error(0);
    mock_write = nullptr;
    ASSERT_TRUE(flush_failed);
    EXPECT_FALSE(fs::exists(name));
    EXPECT_EQ(queue.spill_file_count(), 0u);
    EXPECT_EQ(queue.spill_bytes_used(), 0u);
    EXPECT_EQ(disk->m_available.load(), full);
  }
  EXPECT_EQ(queue.spill_file_count(), 0u);
  EXPECT_EQ(queue.spill_bytes_used(), 0u);
  EXPECT_EQ(disk->m_available.load(), full);
  EXPECT_FALSE(queue.spill_delete_failed());
}
#endif  // _WIN32

}  // namespace mysql::csa::unittests
