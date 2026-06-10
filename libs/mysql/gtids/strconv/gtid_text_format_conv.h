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

#ifndef MYSQL_GTIDS_STRCONV_GTID_TEXT_FORMAT_CONV_H
#define MYSQL_GTIDS_STRCONV_GTID_TEXT_FORMAT_CONV_H

/// @file
/// Experimental API header

#include "mysql/gtids/gtid.h"                      // Is_gtid
#include "mysql/gtids/gtid_set.h"                  // Is_gtid_set
#include "mysql/gtids/sequence_number.h"           // Sequence_number
#include "mysql/gtids/strconv/gtid_text_format.h"  // Gtid_text_format
#include "mysql/gtids/tag.h"                       // Is_tag
#include "mysql/gtids/tsid.h"                      // Is_tsid
#include "mysql/strconv/strconv.h"                 // Is_string_target
#include "mysql/utils/return_status.h"             // Return_status

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::strconv {

// ==== Tags ====

void encode_impl(const Gtid_text_format &format, Is_string_target auto &target,
                 const mysql::gtids::Is_tag auto &tag) {
  target.write(format, tag.string_view());
}

void decode_impl(const Gtid_text_format &, Parser &parser,
                 mysql::gtids::Is_tag auto &tag) {
  // Compute length.
  auto opt_len = tag.valid_prefix_length(parser.remaining_str());
  if (!opt_len.has_value() || *opt_len == 0) {
    parser.set_parse_error("Invalid tag format");
    return;
  }
  // Make `sv` point to the relevant substring of the input.
  std::string_view sv;
  if (parser.read(Fixstr_binary_format{*opt_len}, sv) !=
      mysql::utils::Return_status::ok)
    return;
  // Copy and normalize characters.
  [[maybe_unused]] auto ret =
      mysql::utils::void_to_ok([&] { return tag.assign(sv); });
  // Can't fail because this is a "valid prefix" and the tag does not allocate.
  assert(ret == mysql::utils::Return_status::ok);
}

// ==== Tsids ====

void encode_impl(const Gtid_text_format &format, Is_string_target auto &target,
                 const mysql::gtids::Is_tsid auto &tsid) {
  target.write(format, tsid.uuid());
  if (tsid.tag()) {
    target.concat(format,
                  Gtid_text_format::m_uuid_tag_number_separator_for_output,
                  tsid.tag());
  }
}

void decode_impl(const Gtid_text_format &format, Parser &parser,
                 mysql::gtids::Is_tsid auto &tsid) {
  tsid.tag().clear();
  parser
      .fluent(format)     //
      .read(tsid.uuid())  // UUID
      .end_optional()     // remainder is optional
      .literal(Gtid_text_format::m_uuid_tag_number_separator)  // ":"
      .read(tsid.tag());                                       // tag
}

// ==== Gtids ====

void encode_impl(const Gtid_text_format &format, Is_string_target auto &target,
                 const mysql::gtids::Is_gtid auto &gtid) {
  target.concat(format, gtid.tsid(),
                Gtid_text_format::m_uuid_tag_number_separator_for_output,
                gtid.get_sequence_number());
}

void decode_impl(const Gtid_text_format &format, Parser &parser,
                 mysql::gtids::Is_gtid auto &gtid) {
  mysql::gtids::Sequence_number sequence_number{
      mysql::gtids::sequence_number_min};
  parser
      .fluent(format)                                          //
      .read(gtid.tsid())                                       // TSID
      .literal(Gtid_text_format::m_uuid_tag_number_separator)  // ":"
      .read(sequence_number)                                   // N
      .check_prev_token([&] {                                  // check range
        if (gtid.set_sequence_number(sequence_number) !=
            mysql::utils::Return_status::ok) {
          parser.set_parse_error("GTID sequence number out of range");
        }
      });
}

// ==== Gtid sets ====

void encode_impl(const Gtid_text_format &format, Is_string_target auto &target,
                 const mysql::gtids::Is_gtid_set auto &gtid_set) {
  std::optional<mysql::uuids::Uuid> last_uuid{};
  for (const auto &[tsid, interval_set] : gtid_set) {
    bool first = !last_uuid.has_value();
    assert(first || *last_uuid != tsid.uuid() || tsid.tag());
    if (first || *last_uuid != tsid.uuid()) {
      if (!first)
        target.write_raw(Gtid_text_format::m_uuid_uuid_separator_for_output);
      target.write(format, tsid.uuid());
    }
    if (tsid.tag()) {
      target.write_raw(
          Gtid_text_format::m_uuid_tag_number_separator_for_output);
      target.write(format, tsid.tag());
    }
    target.write_raw(Gtid_text_format::m_uuid_tag_number_separator_for_output);
    target.write(format, interval_set);
    last_uuid = tsid.uuid();
  }
}

template <mysql::gtids::Is_gtid_set Gtid_set_t>
void decode_impl(const Gtid_text_format &format, Parser &parser,
                 Gtid_set_t &gtid_set) {
  using Interval_set_t = typename Gtid_set_t::Mapped_t;
  auto fluent = parser.fluent(format);

  mysql::gtids::Tsid tsid;
  Interval_set_t interval_set(gtid_set.get_memory_resource());

  // ":"
  auto parse_sep = [&] {
    fluent.literal(Gtid_text_format::m_uuid_tag_number_separator);
  };

  // INTERVAL_SET
  auto parse_interval_set = [&] {
    fluent                       //
        .read(interval_set)      // parse INTERVAL_SET
        .check_prev_token([&] {  // add to output
          if (gtid_set.inplace_union(tsid, std::move(interval_set)) !=
              mysql::utils::Return_status::ok) {
            // With current boundary container implementation this is not
            // reachable because move semantics is guaranteed: same type, same
            // allocator, and `has_fast_insertions` implies that the storage is
            // map-based. But it could hypothetically be reached for
            // user-defined storage types that support fast insertions but not
            // move semantics.
            parser.set_oom();
          }
          // It is valid to call `clear` after the object has been moved-from.
          // NOLINENEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved)
          interval_set.clear();
        });
  };

  // TAG_SET := (":" TAG)* (":" INTERVAL_SET)?
  auto parse_tag_and_interval_set = [&] {
    fluent                          //
        .call_any([&] {             // (":" TAG)*
          fluent                    //
              .call(parse_sep)      // ":"
              .read(tsid.tag());    // TAG
        })                          //
        .end_optional()             // may end here
        .call(parse_sep)            // ":"
        .call(parse_interval_set);  // INTERVAL_SET
  };

  // UUID_SET := UUID (TAG_SET)?
  auto parse_uuid_and_tags_and_interval_sets = [&] {
    fluent                                      //
        .read(tsid.uuid())                      // UUID
        .end_optional()                         // may end here
        .call([&] { tsid.tag().clear(); })      // reset the tag
        .call_any(parse_tag_and_interval_set);  // TAG_SET
  };

  skip_whitespace(parser);

  // ","* (UUID_SET (","+ UUID_SET)*)? ","*
  fluent.call_repeated_with_separators(
      parse_uuid_and_tags_and_interval_sets,    // UUID_SET
      Gtid_text_format::m_uuid_uuid_separator,  // ","
      Repeat::any(), Allow_repeated_separators::yes,
      Leading_separators::optional, Trailing_separators::optional);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_STRCONV_GTID_TEXT_FORMAT_CONV_H
