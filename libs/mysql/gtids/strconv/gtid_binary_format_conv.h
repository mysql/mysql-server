// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_GTIDS_STRCONV_GTID_BINARY_FORMAT_CONV_H
#define MYSQL_GTIDS_STRCONV_GTID_BINARY_FORMAT_CONV_H

/// @file
/// Experimental API header

#include <map>                                       // map
#include <new>                                       // bad_alloc
#include "mysql/gtids/gtid.h"                        // Is_gtid
#include "mysql/gtids/gtid_set.h"                    // Is_gtid_set
#include "mysql/gtids/has_tags.h"                    // has_tags
#include "mysql/gtids/strconv/gtid_binary_format.h"  // Gtid_binary_format
#include "mysql/gtids/tag.h"                         // Is_tag
#include "mysql/gtids/tsid.h"                        // Is_tsid
#include "mysql/strconv/strconv.h"                   // Is_string_target
#include "mysql/utils/enumeration_utils.h"           // to_underlying

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::strconv {

// ==== Tags ====

void encode_impl(const Gtid_binary_format &format,
                 Is_string_target auto &target,
                 const mysql::gtids::Is_tag auto &tag) {
  switch (format.m_version_policy) {
    case Gtid_binary_format::Version_policy::v0_tagless:
      assert(tag.empty());
      break;
    case Gtid_binary_format::Version_policy::automatic:
    case Gtid_binary_format::Version_policy::v1_tags:
    case Gtid_binary_format::Version_policy::v2_tags_compact:
      target.write(format, tag.string_view());
      break;
  }
}

void decode_impl(const Gtid_binary_format &format, Parser &parser,
                 mysql::gtids::Is_tag auto &tag) {
  if (format.m_version_policy ==
      Gtid_binary_format::Version_policy::v0_tagless) {
    tag.clear();
    return;
  }
  // Get a string_view that points into the input.
  std::string_view sv;
  if (parser.read(Binary_format{}, sv) != mysql::utils::Return_status::ok)
    return;
  if (!mysql::gtids::Tag::is_valid(sv)) {
    parser.set_parse_error("Invalid tag");
    return;
  }
  // Copy and normalize characters.
  [[maybe_unused]] auto ret =
      mysql::utils::void_to_ok([&] { return tag.assign(sv); });
  // Can't fail because is_valid returned true and the tag does not allocate.
  assert(ret == mysql::utils::Return_status::ok);
}

// ==== Tsids ====

void encode_impl(const Gtid_binary_format &format,
                 Is_string_target auto &string_target,
                 const mysql::gtids::Is_tsid auto &tsid) {
  string_target.concat(format, tsid.uuid(), tsid.tag());
}

void decode_impl(const Gtid_binary_format &format, Parser &parser,
                 mysql::gtids::Is_tsid auto &tsid) {
  using mysql::utils::Return_status;
  if (parser.read(format, tsid.uuid()) != mysql::utils::Return_status::ok)
    return;
  std::ignore = parser.read(format, tsid.tag());
}

// ==== Gtids ====

void encode_impl(const Gtid_binary_format &format,
                 Is_string_target auto &string_target,
                 const mysql::gtids::Is_gtid auto &gtid) {
  string_target.write(format, gtid.tsid());
  string_target.write(Binary_format{}, gtid.get_sequence_number());
}

void decode_impl(const Gtid_binary_format &format, Parser &parser,
                 mysql::gtids::Is_gtid auto &gtid) {
  using mysql::utils::Return_status;
  if (parser.read(format, gtid.tsid()) != mysql::utils::Return_status::ok)
    return;
  mysql::gtids::Sequence_number sequence_number;
  auto check_sequence_number = Checker([&] {
    if (!mysql::gtids::is_valid_sequence_number(sequence_number)) {
      parser.set_parse_error("GTID sequence number out of range");
    }
  });
  if (parser.read(Binary_format{} | check_sequence_number, sequence_number) !=
      mysql::utils::Return_status::ok)
    return;
  [[maybe_unused]] auto ret = gtid.set_sequence_number(sequence_number);
  assert(ret == mysql::utils::Return_status::ok);
}

}  // namespace mysql::strconv

// ==== Gtid sets ====

namespace mysql::gtids::detail {
// Todo: move to math library?
/// Return a value of the given integer type having the low N bits set to 1.
template <std::integral Int_t = uint64_t>
constexpr auto low_bits(int n) {
  assert(n >= 0);
  assert(n <= std::numeric_limits<Int_t>::digits);
  if (n == std::numeric_limits<Int_t>::digits) return ~Int_t(0);
  return (Int_t(1) << n) - Int_t(1);
}

/// Helper to decode format version and tsid count in formats v0, v1, v2.
///
/// @verbatim
/// v0:
///    tsid_count: 6 byte little-endian
///    unused: 1 byte, value 0
///    version: 1 byte, value 0
/// v1 and v2:
///    version: 1 byte, value 1 or 2
///    tsid_count: 6 byte, little-endian
///    version: 1 byte, value 1 or 2
/// @endverbatim
struct Gtid_set_header {
  static constexpr auto version_mask = low_bits(8);
  static constexpr auto version_shift0 = 56;
  static constexpr auto version_shift1 = 0;
  static constexpr auto tsid_count_mask = low_bits(48);
  static constexpr auto tsid_count_shift0 = 0;
  static constexpr auto tsid_count_shift1 = 8;

  mysql::strconv::Gtid_binary_format::Version m_version;
  std::size_t m_tsid_count;
};

}  // namespace mysql::gtids::detail

namespace mysql::strconv {

void encode_impl(const Gtid_binary_format &format [[maybe_unused]],
                 Is_string_target auto &string_target,
                 const mysql::gtids::detail::Gtid_set_header &header) {
  using mysql::gtids::detail::Gtid_set_header;
  using Version = Gtid_binary_format::Version;

  uint64_t code{0};
  switch (header.m_version) {
    case Version::v0_tagless:
      code = header.m_tsid_count;
      break;
    case Version::v1_tags:
    case Version::v2_tags_compact: {
      auto version_byte =
          uint64_t(mysql::utils::to_underlying(header.m_version));
      code = (version_byte << Gtid_set_header::version_shift0) |
             (version_byte << Gtid_set_header::version_shift1) |
             (header.m_tsid_count << Gtid_set_header::tsid_count_shift1);
    } break;
  }
  string_target.write(Fixint_binary_format{}, code);
}

inline void decode_impl(const Gtid_binary_format &format, Parser &parser,
                        mysql::gtids::detail::Gtid_set_header &out) {
  using mysql::gtids::detail::Gtid_set_header;
  using Version_policy = Gtid_binary_format::Version_policy;
  using Version = Gtid_binary_format::Version;

  uint64_t header_word;
  Version version{};

  auto check_version = Checker([&] {
    auto version_byte0 = int((header_word >> Gtid_set_header::version_shift0) &
                             Gtid_set_header::version_mask);
    // Read the version from where it is stored in format 0 (byte 7).
    auto [version0, status] =
        mysql::utils::to_enumeration<Version>(version_byte0);
    if (status != mysql::utils::Return_status::ok) {
      parser.set_parse_error(
          "Unknown (future?) GTID set format version number in GTID encoding");
      return;
    }
    if (version0 != Version::v0_tagless) {
      // Read the version from where it is stored in versions > 0 (byte 0)
      auto version_byte1 =
          int((header_word >> Gtid_set_header::version_shift1) &
              Gtid_set_header::version_mask);
      // Require the two redundant version numbers to match
      if (version_byte0 != version_byte1) {
        parser.set_parse_error(
            "Inconsistent GTID set format version numbers in GTID encoding");
        return;
      }
    }
    // If caller specified a concrete format (rather than automatic), require
    // that the actual format on wire matches.
    if (format.m_version_policy != Version_policy::automatic &&
        format.m_version_policy !=
            Gtid_binary_format::to_version_policy(version0)) {
      parser.set_parse_error(
          "Disallowed GTID set format version number in GTID encoding");
      return;
    }
    version = version0;
  });

  if (parser.read(Fixint_binary_format{} | check_version, header_word) !=
      mysql::utils::Return_status::ok)
    return;

  out.m_version = version;

  auto tsid_shift = (version == Version::v0_tagless)
                        ? Gtid_set_header::tsid_count_shift0
                        : Gtid_set_header::tsid_count_shift1;
  auto tsid_count =
      (header_word >> tsid_shift) & Gtid_set_header::tsid_count_mask;
  out.m_tsid_count = tsid_count;
}

namespace detail {

void encode_v0_v1(const Gtid_binary_format &format,
                  Is_string_target auto &string_target,
                  const mysql::gtids::Is_gtid_set auto &gtid_set) {
  for (const auto &[tsid, interval_set] : gtid_set) {
    string_target.write(format, tsid);
    string_target.write(Fixint_binary_format{}, interval_set);
  }
}

void encode_v2(const Gtid_binary_format &format,
               Is_string_target auto &string_target,
               const mysql::gtids::Is_gtid_set auto &gtid_set) {
  // Don't waste bytes on tag count for empty sets.
  if (gtid_set.empty()) return;

  // Compute set of tags.
  using Tag_map = std::map<mysql::gtids::Tag, std::size_t>;
  using Tag_map_value = Tag_map::value_type;
  Tag_map tag_map;
  for (const auto &[tsid, interval_set] : gtid_set) {
    tag_map.insert(Tag_map_value{mysql::gtids::Tag{tsid.tag()}, 0});
    // Todo: uncaught std::bad_alloc
  }

  // Write and enumerate tags
  string_target.write(format, tag_map.size());
  {
    std::size_t tag_index = 0;
    for (auto &[tag, number] : tag_map) {
      string_target.write(format, tag);
      number = tag_index;
      ++tag_index;
    }
  }

  // Write interval sets
  {
    std::optional<mysql::uuids::Uuid> last_uuid;
    for (const auto &[tsid, interval_set] : gtid_set) {
      bool is_new_uuid = !last_uuid.has_value() || *last_uuid != tsid.uuid();
      uint64_t code = tag_map[tsid.tag()] << 1;
      if (is_new_uuid) code |= 1;
      string_target.write(format, code);
      if (is_new_uuid) {
        string_target.write(format, tsid.uuid());
      }
      string_target.write(format, interval_set);
      last_uuid = tsid.uuid();
    }
  }
}

}  // namespace detail

void encode_impl(const Gtid_binary_format &format,
                 Is_string_target auto &string_target,
                 const mysql::gtids::Is_gtid_set auto &gtid_set) {
  using Version_policy = Gtid_binary_format::Version_policy;
  using Version = Gtid_binary_format::Version;
  using Header = mysql::gtids::detail::Gtid_set_header;
  auto size = gtid_set.size();
  switch (format.m_version_policy) {
    case Version_policy::v0_tagless:
      string_target.write(format, Header{Version::v0_tagless, size});
      detail::encode_v0_v1(format, string_target, gtid_set);
      break;
    case Version_policy::v1_tags:
      string_target.write(format, Header{Version::v1_tags, size});
      detail::encode_v0_v1(format, string_target, gtid_set);
      break;
    case Version_policy::v2_tags_compact:
      string_target.write(format, Header{Version::v2_tags_compact, size});
      detail::encode_v2(format, string_target, gtid_set);
      break;
    case Version_policy::automatic:
      // Compute the "best" format version to use. This policy has to weigh the
      // improvements of newer formats against compatibility of older formats.
      // So when v1 has existed in 2 major versions, we can stop falling back to
      // v0, and when v2 has existed in 2 major versions, we can use that.
      auto version = mysql::gtids::has_tags(gtid_set) ? Version::v1_tags
                                                      : Version::v0_tagless;
      string_target.write(format, Header{version, size});
      detail::encode_v0_v1(
          Gtid_binary_format{Gtid_binary_format::to_version_policy(version)},
          string_target, gtid_set);
      break;
  }
}

namespace detail {

void decode_v0_v1(const Gtid_binary_format &format, Parser &parser,
                  mysql::gtids::Is_gtid_set auto &gtid_set,
                  std::size_t tsid_count) {
  auto fluent = parser.fluent(format);
  mysql::gtids::Tsid tsid;
  mysql::gtids::Gtid_interval_set interval_set;
  fluent.call_exact(tsid_count, [&] {
    fluent.read(tsid)
        .read_with_format(Fixint_binary_format{}, interval_set)
        .check_prev_token([&] {
          if (gtid_set.inplace_union(tsid, std::move(interval_set)) !=
              mysql::utils::Return_status::ok) {
            parser.set_oom();
            return;
          }
          // It is valid to clear a set after moving from it, because clear
          // has no preconditions.
          // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved)
          interval_set.clear();
        });
  });
}

void decode_v2(const Gtid_binary_format &format [[maybe_unused]],
               Parser &parser, mysql::gtids::Is_gtid_set auto &gtid_set,
               std::size_t tsid_count) {
  static auto return_ok = mysql::utils::Return_status::ok;

  // If the set is empty, it does not contain the tag count.
  if (tsid_count == 0) return;

  std::size_t tag_count{0};
  std::vector<mysql::gtids::Tag> tags;
  mysql::gtids::Tsid tsid;
  bool is_first_tsid{true};
  auto fluent = parser.fluent(Binary_format{});
  fluent
      // Read tags
      .read(tag_count)
      .check_prev_token([&] {
        try {
          tags.reserve(tag_count);
        } catch (std::bad_alloc &) {
          parser.set_oom();
        }
      })
      .call_exact(
          tag_count,
          [&] {
            mysql::gtids::Tag tag;
            if (parser.read(Binary_format{}, tag) != return_ok) return;
            tags.emplace_back(tag);  // can't oom because of 'reserve' above
          })
      // Read (Gtid, Gtid_interval_set) pairs
      .call_exact(tsid_count, [&] {
        // Read code containing tag index and uuid flag
        uint64_t code{0};
        std::size_t tag_index{0};
        bool new_uuid{false};
        auto check_code = Checker([&] {
          tag_index = std::size_t(code >> 1);
          if (tag_index >= tags.size()) {
            parser.set_parse_error("Tag index out of range");
          }
          new_uuid = ((code & 1) != 0);
          if (is_first_tsid && !new_uuid) {
            parser.set_parse_error("No UUID given for first Tsid");
            return;
          }
          is_first_tsid = false;
        });
        if (parser.read(Binary_format{} | check_code, code) != return_ok)
          return;

        // Get tag (can't oom because tsid does not throw).
        tsid.tag().assign(tags[tag_index]);

        // If uuid flag is set, read uuid. (Otherwise reuse previous uuid)
        if (new_uuid) {
          if (parser.read(Binary_format{}, tsid.uuid()) != return_ok) return;
        }

        // Read interval set and insert into set
        mysql::gtids::Gtid_interval_set interval_set;
        auto check_interval_set = Checker([&] {
          if (gtid_set.inplace_union(tsid, std::move(interval_set)) !=
              return_ok) {
            parser.set_oom();
          }
        });
        if (parser.read(Binary_format{} | check_interval_set, interval_set) !=
            return_ok)
          return;
      });
}

}  // namespace detail

void decode_impl(const Gtid_binary_format &format, Parser &parser,
                 mysql::gtids::Is_gtid_set auto &gtid_set) {
  using Version = Gtid_binary_format::Version;

  mysql::gtids::detail::Gtid_set_header header;
  if (parser.read(format, header) != mysql::utils::Return_status::ok) return;
  Gtid_binary_format concrete_format{
      Gtid_binary_format::to_version_policy(header.m_version)};
  switch (header.m_version) {
    case Version::v0_tagless:
    case Version::v1_tags:
      detail::decode_v0_v1(concrete_format, parser, gtid_set,
                           header.m_tsid_count);
      break;
    case Version::v2_tags_compact:
      detail::decode_v2(concrete_format, parser, gtid_set, header.m_tsid_count);
      break;
  }
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_STRCONV_GTID_BINARY_FORMAT_CONV_H
