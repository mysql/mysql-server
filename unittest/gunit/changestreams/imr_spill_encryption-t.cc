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
/// Unit tests for the in-memory relay log's master-key rotation
/// (sql/changestreams/apply/storage/in_memory/spill_encryption.h): the
/// read-only v1 encryption header parser, which reads the key id, a pin of its
/// layout against the server's Rpl_encryption_header, and the header rewrap
/// pass driven with a fake rewrapper. gunit has no keyring, so the expected key
/// ids are built with Rpl_encryption_header::seqno_to_key_id() and do not
/// depend on server_uuid. The server rewrapper, spill_dirs_of_live_runtimes()
/// and imr_reencrypt_spill_files() need rpl_encryption, a keyring and channel
/// globals; MTR covers them (rpl_csa_imr_spill_encryption_rotation{,_errors}).

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "sql/basic_istream.h"  // Basic_istream
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/storage/in_memory/spill_encryption.h"
#include "sql/rpl_log_encryption.h"  // Rpl_encryption_header

namespace mysql::csa::unittests {

namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::size_t kHeaderSize = Rpl_encryption_header_v1::HEADER_SIZE;

/// The encryption magic followed by the version byte.
Bytes magic_and_version(unsigned char version = 1) {
  Bytes b(Rpl_encryption_header::ENCRYPTION_MAGIC,
          Rpl_encryption_header::ENCRYPTION_MAGIC +
              Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE);
  b.push_back(version);
  return b;
}

Bytes key_id_field(const std::string &key_id) {
  Bytes b{1, static_cast<unsigned char>(key_id.size())};
  b.insert(b.end(), key_id.begin(), key_id.end());
  return b;
}

Bytes password_field(unsigned char fill = 0xab) {
  Bytes b{2};
  b.insert(b.end(), Rpl_encryption_header_v1::PASSWORD_FIELD_SIZE, fill);
  return b;
}

Bytes iv_field(unsigned char fill = 0xcd) {
  Bytes b{3};
  b.insert(b.end(), Rpl_encryption_header_v1::IV_FIELD_SIZE, fill);
  return b;
}

/// A 512-byte header: magic, @p version, @p fields in order, zero padding.
Bytes make_header(const std::vector<Bytes> &fields, unsigned char version = 1) {
  Bytes b = magic_and_version(version);
  for (const Bytes &f : fields) b.insert(b.end(), f.begin(), f.end());
  EXPECT_LE(b.size(), kHeaderSize) << "test header too long";
  b.resize(kHeaderSize, 0);
  return b;
}

/// The canonical header the server writes for @p seqno.
Bytes valid_header(std::uint32_t seqno) {
  return make_header(
      {key_id_field(Rpl_encryption_header::seqno_to_key_id(seqno)),
       password_field(), iv_field()});
}

/// A header whose key id is this server's prefix followed by @p suffix.
Bytes header_with_suffix(const std::string &suffix) {
  return make_header(
      {key_id_field(Rpl_encryption_header::key_id_prefix() + "_" + suffix),
       password_field(), iv_field()});
}

Spill_header_kind parse(const Bytes &b, std::string *key_id = nullptr) {
  std::string dummy;
  return parse_spill_header(b.data(), b.size(),
                            key_id != nullptr ? key_id : &dummy);
}

/// Reads from a byte buffer, standing in for a file stream.
class Buffer_istream : public Basic_istream {
 public:
  explicit Buffer_istream(const Bytes &buf) : m_buf(buf) {}
  ssize_t read(unsigned char *buffer, size_t length) override {
    const size_t n = std::min(length, m_buf.size() - m_pos);
    std::memcpy(buffer, m_buf.data() + m_pos, n);
    m_pos += n;
    return static_cast<ssize_t>(n);
  }

 private:
  const Bytes &m_buf;
  size_t m_pos{0};
};

void write_file(const fs::path &path, const Bytes &content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(content.data()),
            static_cast<std::streamsize>(content.size()));
  ASSERT_TRUE(out.good()) << path;
}

Bytes plain_prefix() {
  // BINLOG_MAGIC followed by some bytes, as an unencrypted spill file.
  Bytes b{0xfe, 0x62, 0x69, 0x6e};
  b.insert(b.end(), 600, 0x11);
  return b;
}

Bytes read_whole_file(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
}

}  // namespace

// ---------------------------------------------------------------------------
// parse_spill_header(): classes of the first bytes of an IMR file.
// ---------------------------------------------------------------------------

TEST(ImrSpillEncryptionParseTest, PlainAndShortFilesAreUnencrypted) {
  EXPECT_EQ(parse(plain_prefix()), Spill_header_kind::unencrypted);
  const Bytes enc = valid_header(7);
  for (std::size_t len = 0; len < 4; ++len) {
    EXPECT_EQ(parse_spill_header(enc.data(), len, nullptr),
              Spill_header_kind::unencrypted)
        << len << " bytes";
  }
}

TEST(ImrSpillEncryptionParseTest, ValidHeaderNamesItsKeyId) {
  std::string key_id;
  EXPECT_EQ(parse(valid_header(7), &key_id), Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, Rpl_encryption_header::seqno_to_key_id(7));
  EXPECT_EQ(parse(valid_header(4294967294u), &key_id),
            Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, Rpl_encryption_header::seqno_to_key_id(4294967294u));
}

TEST(ImrSpillEncryptionParseTest, FieldsInAnotherOrder) {
  std::string key_id;
  const Bytes b =
      make_header({iv_field(), password_field(),
                   key_id_field(Rpl_encryption_header::seqno_to_key_id(7))});
  EXPECT_EQ(parse(b, &key_id), Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, Rpl_encryption_header::seqno_to_key_id(7));
}

TEST(ImrSpillEncryptionParseTest, LastKeyIdWins) {
  std::string key_id;
  const Bytes b =
      make_header({key_id_field(Rpl_encryption_header::seqno_to_key_id(3)),
                   password_field(), iv_field(),
                   key_id_field(Rpl_encryption_header::seqno_to_key_id(7))});
  EXPECT_EQ(parse(b, &key_id), Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, Rpl_encryption_header::seqno_to_key_id(7));
}

// The parser does not interpret the key id: a key of another server is read
// as it is, and the rewrap later reports it as not in the keyring.
TEST(ImrSpillEncryptionParseTest, AnotherServersKeyIsReadAsIs) {
  const std::string other =
      "MySQLReplicationKey_11111111-2222-3333-4444-555555555555_7";
  std::string key_id;
  EXPECT_EQ(
      parse(make_header({key_id_field(other), password_field(), iv_field()}),
            &key_id),
      Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, other);
}

TEST(ImrSpillEncryptionParseTest, StructuralErrorsAreCorrupt) {
  const std::string key7 = Rpl_encryption_header::seqno_to_key_id(7);

  EXPECT_EQ(
      parse(make_header({key_id_field(key7), password_field(), iv_field()}, 2)),
      Spill_header_kind::corrupt)
      << "version 2";

  Bytes unknown_type = valid_header(7);
  unknown_type[5] = 9;
  EXPECT_EQ(parse(unknown_type), Spill_header_kind::corrupt)
      << "unknown field type";

  // Two 255-byte key-id fields: the second one crosses byte 512.
  Bytes crossing = magic_and_version();
  for (int i = 0; i < 2; ++i) {
    crossing.push_back(1);
    crossing.push_back(255);
    crossing.insert(crossing.end(), 255, 'k');
  }
  crossing.resize(kHeaderSize);
  EXPECT_EQ(parse(crossing), Spill_header_kind::corrupt)
      << "key id crossing byte 512";

  Bytes short_header = valid_header(7);
  short_header.resize(300);
  EXPECT_EQ(parse(short_header), Spill_header_kind::corrupt)
      << "300 bytes with the magic";

  EXPECT_EQ(parse(make_header({password_field(), iv_field()})),
            Spill_header_kind::corrupt)
      << "missing key id";
  EXPECT_EQ(parse(make_header({key_id_field(key7), password_field(), iv_field(),
                               key_id_field("")})),
            Spill_header_kind::corrupt)
      << "last key id of length 0";
  EXPECT_EQ(parse(make_header({key_id_field(key7), iv_field()})),
            Spill_header_kind::corrupt)
      << "missing password";
  EXPECT_EQ(parse(make_header({key_id_field(key7), password_field()})),
            Spill_header_kind::corrupt)
      << "missing IV";
}

// Valid fields that end exactly at byte 512, with no type-0 terminator: the
// walk reads the type byte at offset 512, which the server reports as "Header
// is corrupted". Filler key-id fields set the length; the real key id is last.
TEST(ImrSpillEncryptionParseTest, NoTerminatorBeforeByte512IsCorrupt) {
  const std::string key7 = Rpl_encryption_header::seqno_to_key_id(7);
  std::vector<Bytes> fields{password_field(), iv_field()};
  std::size_t used = magic_and_version().size() + fields[0].size() +
                     fields[1].size() + 2 + key7.size();
  ASSERT_LT(used, kHeaderSize);
  std::size_t remaining = kHeaderSize - used;
  while (remaining > 0) {
    // Each filler is 2 + L bytes with L in [0, 255]; never leave 1 byte.
    std::size_t size = std::min<std::size_t>(remaining, 257);
    if (remaining - size == 1) size -= 2;
    ASSERT_GE(size, 2u);
    fields.push_back(key_id_field(std::string(size - 2, 'f')));
    remaining -= size;
  }
  fields.push_back(key_id_field(key7));
  Bytes b = magic_and_version();
  for (const Bytes &f : fields) b.insert(b.end(), f.begin(), f.end());
  ASSERT_EQ(b.size(), kHeaderSize);
  EXPECT_EQ(parse(b), Spill_header_kind::corrupt);

  // The same fields with one filler byte less and a terminator are valid, so
  // the case above fails only for the missing terminator.
  Bytes terminated = magic_and_version();
  fields[2] = key_id_field(std::string(fields[2].size() - 3, 'f'));
  for (const Bytes &f : fields)
    terminated.insert(terminated.end(), f.begin(), f.end());
  terminated.push_back(0);
  ASSERT_EQ(terminated.size(), kHeaderSize);
  std::string key_id;
  EXPECT_EQ(parse(terminated, &key_id), Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, key7);
}

// Any key id suffix is read as it is; only the server's keyring decides
// whether such a key exists.
TEST(ImrSpillEncryptionParseTest, AnyKeyIdSuffixIsReadAsIs) {
  for (const char *suffix : {"x7", "07", "0", "4294967295", "", "-7"}) {
    std::string key_id;
    EXPECT_EQ(parse(header_with_suffix(suffix), &key_id),
              Spill_header_kind::encrypted)
        << "suffix '" << suffix << "'";
    EXPECT_EQ(key_id, Rpl_encryption_header::key_id_prefix() + "_" + suffix);
  }
}

// ---------------------------------------------------------------------------
// Layout pin: the hand-built header is what the server reads and writes.
// ---------------------------------------------------------------------------

TEST(ImrSpillEncryptionParseTest, LayoutMatchesServerHeader) {
  const Bytes b = valid_header(7);
  Buffer_istream in(b);
  unsigned char magic[Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE];
  ASSERT_EQ(in.read(magic, sizeof(magic)), static_cast<ssize_t>(sizeof(magic)));
  ASSERT_EQ(std::memcmp(magic, Rpl_encryption_header::ENCRYPTION_MAGIC,
                        sizeof(magic)),
            0);
  std::unique_ptr<Rpl_encryption_header> header =
      Rpl_encryption_header::get_header(&in);
  ASSERT_NE(header, nullptr) << "the server must deserialize the header";
  EXPECT_EQ(header->get_version(), 1);
  EXPECT_EQ(header->get_header_size(), static_cast<int>(kHeaderSize));

  StringBuffer_ostream<kHeaderSize> out;
  ASSERT_FALSE(header->serialize(&out));
  ASSERT_EQ(out.length(), kHeaderSize);
  EXPECT_EQ(std::memcmp(out.ptr(), b.data(), kHeaderSize), 0)
      << "serialize() must give the same 512 bytes";

  std::string key_id;
  EXPECT_EQ(parse(b, &key_id), Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, Rpl_encryption_header::seqno_to_key_id(7));
}

// ---------------------------------------------------------------------------
// reencrypt_spill_files_in_dirs(): the header rewrap pass, with a fake
// rewrapper standing in for the server's keyring crypto.
// ---------------------------------------------------------------------------

/// The "current" key of these tests.
constexpr std::uint32_t kCurrentSeqno = 9;

/// The header the fake returns for done: names the current key, with a
/// password and IV that no input header uses.
Bytes rewrapped_header() {
  return make_header(
      {key_id_field(Rpl_encryption_header::seqno_to_key_id(kCurrentSeqno)),
       password_field(0x77), iv_field(0x88)});
}

/// A file of @p size bytes that starts with @p header and continues with a
/// byte pattern, as an encrypted spill file with appended events.
Bytes header_and_tail(const Bytes &header, std::size_t size) {
  Bytes b = header;
  for (std::size_t i = b.size(); i < size; ++i)
    b.push_back(static_cast<unsigned char>(i * 31));
  return b;
}

class ImrSpillReencryptTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    m_dir = fs::temp_directory_path() /
            ("imr_spill_reenc_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1)));
    fs::create_directories(m_dir);
  }
  void TearDown() override {
    std::error_code ec;
    // Restore the modes the tests removed, so remove_all() can descend.
    fs::permissions(m_dir, fs::perms::owner_all, fs::perm_options::add, ec);
    for (const auto &entry : fs::directory_iterator(m_dir, ec))
      fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::add,
                      ec);
    fs::remove_all(m_dir, ec);
  }

  /// A rewrapper that records each header it is given and answers with
  /// @c m_answer (done by default, writing rewrapped_header()).
  Header_rewrapper fake() {
    return [this](const unsigned char *old_header, unsigned char *new_header) {
      Bytes seen(old_header, old_header + kHeaderSize);
      m_seen.push_back(seen);
      const Rewrap_result result =
          m_answer ? m_answer(seen) : Rewrap_result{Rewrap_status::done, {}};
      if (result.status == Rewrap_status::done) {
        const Bytes out = rewrapped_header();
        std::memcpy(new_header, out.data(), kHeaderSize);
      }
      return result;
    };
  }

  bool run(std::string *error) {
    return reencrypt_spill_files_in_dirs(
        {m_dir.string()}, Rpl_encryption_header::seqno_to_key_id(kCurrentSeqno),
        fake(), error);
  }

  fs::path m_dir;
  std::vector<Bytes> m_seen;
  std::function<Rewrap_result(const Bytes &)> m_answer;
};

TEST_F(ImrSpillReencryptTest, RewrapsEveryKeyButTheCurrentOne) {
  const Bytes a = header_and_tail(valid_header(3), 2000);
  const Bytes b = valid_header(4);
  const Bytes c = valid_header(kCurrentSeqno);
  const Bytes d = plain_prefix();
  const Bytes e = make_header(
      {key_id_field(
           "MySQLReplicationKey_11111111-2222-3333-4444-555555555555_3"),
       password_field(), iv_field()});
  const Bytes g = valid_header(3);
  write_file(m_dir / "imr_sp_a", a);
  write_file(m_dir / "imr_sp_b", b);
  write_file(m_dir / "imr_sp_c", c);
  write_file(m_dir / "imr_sp_d", d);
  write_file(m_dir / "imr_sp_e", e);
  write_file(m_dir / "imr_sp_f", {});
  write_file(m_dir / "other_g", g);

  std::string error;
  EXPECT_FALSE(run(&error)) << error;

  // The fake saw exactly the old headers of a, b and e (another server's
  // key), in any order.
  ASSERT_EQ(m_seen.size(), 3u);
  const Bytes a_header(a.begin(), a.begin() + kHeaderSize);
  for (const Bytes &expected : {a_header, b, e})
    EXPECT_EQ(std::count(m_seen.begin(), m_seen.end(), expected), 1);

  // Header out: a and b now start with the fake's header; a's tail is intact.
  const Bytes new_header = rewrapped_header();
  const Bytes a_after = read_whole_file(m_dir / "imr_sp_a");
  ASSERT_EQ(a_after.size(), a.size());
  EXPECT_TRUE(
      std::equal(new_header.begin(), new_header.end(), a_after.begin()));
  EXPECT_TRUE(std::equal(a.begin() + kHeaderSize, a.end(),
                         a_after.begin() + kHeaderSize));
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_b"), new_header);
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_e"), new_header);
  std::string key_id;
  EXPECT_EQ(parse(a_after, &key_id), Spill_header_kind::encrypted);
  EXPECT_EQ(key_id, Rpl_encryption_header::seqno_to_key_id(kCurrentSeqno));

  // Current key, plain, empty and unprefixed files are untouched.
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_c"), c);
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_d"), d);
  EXPECT_TRUE(read_whole_file(m_dir / "imr_sp_f").empty());
  EXPECT_EQ(read_whole_file(m_dir / "other_g"), g);

  // A second pass finds nothing left to rewrap.
  m_seen.clear();
  EXPECT_FALSE(run(&error)) << error;
  EXPECT_TRUE(m_seen.empty());
}

TEST_F(ImrSpillReencryptTest, CorruptAndShortHeadersAreSkipped) {
  Bytes version2 = valid_header(3);
  version2[4] = 2;
  Bytes short_header = valid_header(3);
  short_header.resize(100);
  write_file(m_dir / "imr_sp_v2", version2);
  write_file(m_dir / "imr_sp_short", short_header);
  std::string error;
  EXPECT_FALSE(run(&error)) << error;
  EXPECT_TRUE(m_seen.empty());
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_v2"), version2);
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_short"), short_header);
}

TEST_F(ImrSpillReencryptTest, KeyNotFoundAndInvalidHeaderAreSkipped) {
  const Bytes nokey = valid_header(3);
  const Bytes invalid = valid_header(4);
  write_file(m_dir / "imr_sp_nokey", nokey);
  write_file(m_dir / "imr_sp_invalid", invalid);
  m_answer = [](const Bytes &header) {
    std::string key_id;
    parse(header, &key_id);
    return key_id == Rpl_encryption_header::seqno_to_key_id(3)
               ? Rewrap_result{Rewrap_status::key_not_found, "gone"}
               : Rewrap_result{Rewrap_status::invalid_header, "bad"};
  };
  std::string error;
  EXPECT_FALSE(run(&error)) << error;
  EXPECT_EQ(m_seen.size(), 2u);
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_nokey"), nokey);
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_invalid"), invalid);
}

TEST_F(ImrSpillReencryptTest, RewrapFailureFailsThePass) {
  const Bytes old = header_and_tail(valid_header(3), 700);
  write_file(m_dir / "imr_sp_boom", old);
  m_answer = [](const Bytes &) {
    return Rewrap_result{Rewrap_status::failed, "boom"};
  };
  std::string error;
  EXPECT_TRUE(run(&error));
  const std::string expected_start =
      "Failed to re-encrypt in-memory relay log file '";
  EXPECT_EQ(error.compare(0, expected_start.size(), expected_start), 0)
      << error;
  EXPECT_NE(error.find((m_dir / "imr_sp_boom").string()), std::string::npos)
      << error;
  const std::string expected_end = "': boom";
  ASSERT_GE(error.size(), expected_end.size());
  EXPECT_EQ(error.substr(error.size() - expected_end.size()), expected_end);
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_boom"), old);
}

TEST_F(ImrSpillReencryptTest, MissingDirectoryIsEmpty) {
  std::string error;
  EXPECT_FALSE(reencrypt_spill_files_in_dirs(
      {(m_dir / "absent").string()},
      Rpl_encryption_header::seqno_to_key_id(kCurrentSeqno), fake(), &error))
      << error;
  EXPECT_TRUE(m_seen.empty());
}

// ENOENT at open: a name that no longer leads to a file is skipped, and the
// open does not create one (my_open() without O_CREAT; with O_CREAT the
// target of the dangling link would be created).
TEST_F(ImrSpillReencryptTest, DanglingSymlinkIsNotCreated) {
  fs::create_symlink(m_dir / "absent", m_dir / "imr_sp_link");
  std::string error;
  EXPECT_FALSE(run(&error)) << error;
  EXPECT_TRUE(m_seen.empty());
  EXPECT_FALSE(fs::exists(m_dir / "absent"));
}

// The file is deleted after the pass opened it: the new header goes to the
// unlinked inode, the pass succeeds and the name is not re-created.
TEST_F(ImrSpillReencryptTest, FileDeletedAfterOpenIsNotRecreated) {
  write_file(m_dir / "imr_sp_gone", valid_header(3));
  const fs::path path = m_dir / "imr_sp_gone";
  m_answer = [path](const Bytes &) {
    fs::remove(path);
    return Rewrap_result{Rewrap_status::done, {}};
  };
  std::string error;
  EXPECT_FALSE(run(&error)) << error;
  EXPECT_EQ(m_seen.size(), 1u);
  EXPECT_FALSE(fs::exists(path));
}

TEST_F(ImrSpillReencryptTest, DirectoryWithPrefixIsSkipped) {
  fs::create_directories(m_dir / "imr_sp_dir");
  std::string error;
  EXPECT_FALSE(run(&error)) << error;
  EXPECT_TRUE(m_seen.empty());
}

TEST_F(ImrSpillReencryptTest, ReadOnlyFileFailsThePass) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can open a mode-0400 file";
  const Bytes old = valid_header(3);
  write_file(m_dir / "imr_sp_ro", old);
  ASSERT_EQ(::chmod((m_dir / "imr_sp_ro").c_str(), 0400), 0);
  std::string error;
  EXPECT_TRUE(run(&error));
  EXPECT_NE(error.find((m_dir / "imr_sp_ro").string()), std::string::npos)
      << error;
  EXPECT_NE(error.find("could not open the file"), std::string::npos) << error;
  EXPECT_TRUE(m_seen.empty());
  EXPECT_EQ(read_whole_file(m_dir / "imr_sp_ro"), old);
}

TEST_F(ImrSpillReencryptTest, UnreadableDirectoryFailsThePass) {
  if (::geteuid() == 0) GTEST_SKIP() << "root can list a mode-0000 directory";
  const fs::path sub = m_dir / "locked";
  fs::create_directories(sub);
  write_file(sub / "imr_sp_a", valid_header(3));
  ASSERT_EQ(::chmod(sub.c_str(), 0), 0);
  std::string error;
  EXPECT_TRUE(reencrypt_spill_files_in_dirs(
      {sub.string()}, Rpl_encryption_header::seqno_to_key_id(kCurrentSeqno),
      fake(), &error));
  EXPECT_NE(
      error.find("Failed to read the in-memory relay log spill directory"),
      std::string::npos)
      << error;
  EXPECT_TRUE(m_seen.empty());
}

}  // namespace mysql::csa::unittests
