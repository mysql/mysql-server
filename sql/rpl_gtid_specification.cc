/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string.h>

#include "my_dbug.h"
#include "mysql/strings/m_ctype.h"
#include "sql/rpl_gtid.h"

// const int Gtid_specification::MAX_TEXT_LENGTH;

#ifdef MYSQL_SERVER

using mysql::gtid::Tag;
using mysql::gtid::Tsid;
using mysql::utils::Return_status;

// This helper will return non-zero characters parsed in case text is prefixed
// with AUTOMATIC: (AUTOMATIC with a tag). Otherwise, it will return 0
std::size_t parse_automatic_prefix(const char *text) {
  auto chset = &my_charset_utf8mb3_general_ci;
  const auto &pattern = Gtid_specification::str_automatic_tagged;
  auto pattern_len = strlen(pattern);
  std::size_t pos = 0;
  while (pos < pattern_len) {
    if (text[pos] == '\0' ||
        my_tolower(chset, Gtid_specification::str_automatic_tagged[pos]) !=
            my_tolower(chset, text[pos])) {
      return 0;
    }
    ++pos;
  }
  return pos;
}

Tag Gtid_specification::generate_tag() const { return Tag(automatic_tag); }

bool Gtid_specification::is_automatic_tagged() const {
  return is_automatic() && automatic_tag.is_defined();
}

Return_status Gtid_specification::parse(Tsid_map *tsid_map, const char *text) {
  DBUG_TRACE;
  Return_status status = Return_status::ok;
  assert(text != nullptr);
  automatic_tag.clear();
  auto automatic_prefix_len = parse_automatic_prefix(text);
  gtid = {0, 0};
  if (my_strcasecmp(&my_charset_latin1, text, "AUTOMATIC") == 0) {
    type = AUTOMATIC_GTID;
  } else if (automatic_prefix_len) {
    type = AUTOMATIC_GTID;
    Tag defined_tag;
    std::size_t pos = 0;
    // for AUTOMATIC: tag must be non-empty
    std::tie(defined_tag, pos) =
        Gtid::parse_tag_str(text, automatic_prefix_len);
    if (defined_tag.is_empty() || text[pos] != '\0') {
      Gtid::report_parsing_error(text);
      return Return_status::error;
    }
    automatic_tag.set(defined_tag);
  } else if (my_strcasecmp(&my_charset_latin1, text, "ANONYMOUS") == 0) {
    type = ANONYMOUS_GTID;
  } else {
    status = gtid.parse(tsid_map, text);
    if (status == Return_status::ok) type = ASSIGNED_GTID;
  }
  return status;
}

void Gtid_specification::set(const Gtid_specification &other) {
  type = other.type;
  automatic_tag = other.automatic_tag;
  gtid = other.gtid;
}

bool Gtid_specification::is_valid(const char *text) {
  DBUG_TRACE;
  assert(text != nullptr);
  auto automatic_prefix_len = parse_automatic_prefix(text);
  if (my_strcasecmp(&my_charset_latin1, text, "AUTOMATIC") == 0)
    return true;
  else if (my_strcasecmp(&my_charset_latin1, text, "ANONYMOUS") == 0)
    return true;
  else if (automatic_prefix_len) {
    Tag parsed_tag;
    std::tie(parsed_tag, std::ignore) =
        Gtid::parse_tag_str(text, automatic_prefix_len);
    return parsed_tag.is_defined();
  } else {
    return Gtid::is_valid(text);
  }
}

bool Gtid_specification::is_tagged(const char *text) {
  DBUG_TRACE;
  assert(text != nullptr);
  auto automatic_prefix_len = parse_automatic_prefix(text);
  if (automatic_prefix_len) {
    Tag parsed_tag;
    std::tie(parsed_tag, std::ignore) =
        Gtid::parse_tag_str(text, automatic_prefix_len);
    return parsed_tag.is_defined();
  } else {
    auto [status, gtid] = Gtid::parse_gtid_from_cstring(text);
    return (status == Return_status::ok) && gtid.get_tsid().is_tagged();
  }
}

#endif  // ifdef MYSQL_SERVER

std::size_t Gtid_specification::automatic_to_string(char *buf) const {
  assert(type == AUTOMATIC_GTID);
  std::size_t pos = 0;
  auto str_auto_len = strlen(Gtid_specification::str_automatic);
  strncpy(buf + pos, Gtid_specification::str_automatic, str_auto_len + 1);
  pos += str_auto_len;
  if (automatic_tag.is_defined()) {
    auto sep_len = strlen(Gtid_specification::str_automatic_sep);
    strncpy(buf + pos, Gtid_specification::str_automatic_sep, sep_len + 1);
    pos += sep_len;
    pos += automatic_tag.to_string(buf + pos);
    buf[pos++] = '\0';
  }
  return pos;
}

int Gtid_specification::to_string(const Tsid &tsid, char *buf) const {
  DBUG_TRACE;
  switch (type) {
    case AUTOMATIC_GTID:
      return automatic_to_string(buf);
    case NOT_YET_DETERMINED_GTID:
      /*
        This can happen if user issues SELECT @@SESSION.GTID_NEXT
        immediately after a BINLOG statement containing a
        Format_description_log_event.
      */
      strcpy(buf, "NOT_YET_DETERMINED");
      return 18;
    case ANONYMOUS_GTID:
      strcpy(buf, "ANONYMOUS");
      return 9;
    /*
      UNDEFINED_GTID must be printed like ASSIGNED_GTID because of
      SELECT @@SESSION.GTID_NEXT.
    */
    case UNDEFINED_GTID:
    case ASSIGNED_GTID:
      return gtid.to_string(tsid, buf);
    case PRE_GENERATE_GTID:
      strcpy(buf, "PRE_GENERATE_GTID");
      return 17;
  }
  assert(0);
  return 0;
}

int Gtid_specification::to_string(const Tsid_map *tsid_map, char *buf,
                                  bool need_lock) const {
  return to_string(is_assigned() || is_undefined()
                       ? tsid_map->sidno_to_tsid(gtid.sidno, need_lock)
                       : Tsid(),
                   buf);
}
