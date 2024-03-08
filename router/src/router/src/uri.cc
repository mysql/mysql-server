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

#include "mysqlrouter/uri.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mysql/harness/string_utils.h"  // split_string

using mysql_harness::split_string;

// RFC 3986
//
// recursive decent parser
//
// TODO(jan): add IPvFuture

const std::string kDigit = "0123456789";
const std::string kHexLower = "abcdef";
const std::string kHexUpper = "ABCDEF";
const std::string kAlphaLower = kHexLower + "ghijklmnopqrstuvwxyz";
const std::string kAlphaUpper = kHexUpper + "GHIJKLMNOPQRSTUVWXYZ";
const std::string kAlpha = kAlphaLower + kAlphaUpper;
const std::string kUnreserved = kAlpha + kDigit + "-" + "." + "_" + "~";
const std::string kHexDigit = kDigit + kHexLower + kHexUpper;
const std::string kGenDelims = ":/?#[]@";
const std::string kSubDelims = "!$&'()*+,;=";
const std::string kReserved = kGenDelims + kSubDelims;
const std::string kPathCharNoPctEncoded = kUnreserved + kSubDelims + ":" + "@";
const std::string kFragmentOrQuery = "/?";

namespace mysqlrouter {

URIError::URIError(const char *msg, const std::string &uri, size_t position)
    : std::runtime_error(std::string("invalid URI: ") + msg + " at position " +
                         std::to_string(position) + " for: " + uri) {}

/*
 * match zero-or-more of group of characters
 *
 * @returns matched length
 */
static size_t match_zero_or_more(const std::string &s, const std::string &pat,
                                 size_t pos_start = 0) {
  size_t pos_matched = s.find_first_not_of(pat, pos_start);

  if (pos_matched == std::string::npos) {
    pos_matched = s.length();
  }

  return pos_matched - pos_start;
}

static size_t skip(size_t pos_start, size_t match_len) {
  return pos_start + match_len;
}

static std::string capture(const std::string &s, size_t pos_start,
                           size_t match_len, size_t &pos_end) {
  pos_end = skip(pos_start, match_len);

  return s.substr(pos_start, match_len);
}

static bool is_eol(const std::string &s, size_t pos_start) {
  return pos_start == s.length();
}

static bool match_pct_encoded(const std::string &s, size_t pos_start,
                              size_t &pos_end, std::string &pct_enc) {
  // pct-encoded = "%" HEXDIG HEXDIG
  if (s.length() - pos_start < 3) {
    return false;
  }

  if (s.at(pos_start) != '%' || 0 == isxdigit(s.at(pos_start + 1)) ||
      0 == isxdigit(s.at(pos_start + 2))) {
    return false;
  }

  pct_enc = capture(s, pos_start, 3, pos_end);

  return true;
}

static bool match_path_chars(const std::string &s, size_t pos_start,
                             size_t &pos_end, std::string &path_chars) {
  // pchar       = unreserved / pct-encoded / sub-delims / ":" / "@"

  bool made_progress;
  std::string tmp;
  size_t pos_matched = pos_start;

  do {
    made_progress = false;

    size_t match_len =
        match_zero_or_more(s, kPathCharNoPctEncoded, pos_matched);

    if (match_len > 0) {
      made_progress = true;

      tmp.append(capture(s, pos_matched, match_len, pos_matched));
    }

    std::string pct_enc;
    if (match_pct_encoded(s, pos_matched, pos_matched, pct_enc)) {
      tmp.append(pct_enc);

      made_progress = true;
    }
  } while (made_progress);

  path_chars = std::move(tmp);
  pos_end = pos_matched;

  return true;
}

static bool match_scheme(const std::string &s, size_t pos_start,
                         size_t &pos_end, std::string &scheme) {
  // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  //
  size_t match_len = match_zero_or_more(s, kAlpha, pos_start);

  if (match_len == 0) {
    // no ALPHA
    return false;
  }
  match_len +=
      match_zero_or_more(s, kAlpha + kDigit + "+-.", pos_start + match_len);

  scheme = capture(s, pos_start, match_len, pos_end);

  return true;
}

static bool match_colon(const std::string &s, size_t pos_start,
                        size_t &pos_end) {
  if (is_eol(s, pos_start)) {
    return false;
  }

  if (s.at(pos_start) != ':') {
    return false;
  }
  pos_end = skip(pos_start, 1);

  return true;
}

static bool match_double_colon(const std::string &s, size_t pos_start,
                               size_t &pos_end) {
  if (s.length() - pos_start < 2) {
    return false;
  }

  if (s.at(pos_start) != ':' || s.at(pos_start + 1) != ':') {
    return false;
  }
  pos_end = skip(pos_start, 2);

  return true;
}

static bool match_userinfo(const std::string &s, size_t pos_start,
                           size_t &pos_end, std::string &user_info) {
  // userinfo    = *( unreserved / pct-encoded / sub-delims / ":" )

  bool made_progress;
  std::string tmp;
  size_t pos_matched = pos_start;

  do {
    made_progress = false;

    size_t match_len =
        match_zero_or_more(s, kUnreserved + kSubDelims + ":", pos_matched);

    if (match_len > 0) {
      made_progress = true;

      tmp.append(capture(s, pos_matched, match_len, pos_matched));
    }

    std::string pct_enc;
    if (match_pct_encoded(s, pos_matched, pos_matched, pct_enc)) {
      tmp.append(pct_enc);

      made_progress = true;
    }
  } while (made_progress);

  if (is_eol(s, pos_matched)) {
    // EOL, but we have to match also the '@'
    return false;
  }

  if (s.at(pos_matched) != '@') {
    return false;
  }

  user_info = std::move(tmp);

  // skip the @
  pos_end = pos_matched + 1;

  return true;
}

static void split_userinfo(const std::string &user_info, std::string &username,
                           std::string &password) {
  size_t pos = user_info.find(':');
  if (pos != std::string::npos) {
    username = user_info.substr(0, pos);
    password = user_info.substr(pos + 1, user_info.size() - (pos + 1));
  } else {
    // No password
    username = user_info;
    password = "";
  }
}

static bool match_port(const std::string &s, size_t pos_start, size_t &pos_end,
                       std::string &port) {
  /* port        = *DIGIT
   */
  size_t match_len = match_zero_or_more(s, kDigit, pos_start);

  port = capture(s, pos_start, match_len, pos_end);

  return true;
}

static bool match_reg_name(const std::string &s, size_t pos_start,
                           size_t &pos_end, std::string &reg_name,
                           bool with_pct_encoded) {
  /* reg-name    = *( unreserved / pct-encoded / sub-delims )
   */
  bool made_progress;
  size_t pos_matched = pos_start;

  do {
    made_progress = false;

    size_t match_len =
        match_zero_or_more(s, kUnreserved + kSubDelims, pos_matched);

    if (match_len > 0) {
      reg_name.append(capture(s, pos_matched, match_len, pos_matched));
      made_progress = true;
    }

    if (with_pct_encoded) {
      std::string pct_enc;
      if (match_pct_encoded(s, pos_matched, pos_matched, pct_enc)) {
        reg_name.append(pct_enc);

        made_progress = true;
      }
    } else if (pos_matched < s.length() && s.at(pos_matched) == '%') {
      reg_name += '%';
      made_progress = true;
      pos_matched += 1;
    }
  } while (made_progress);

  pos_end = pos_matched;

  return true;
}

static bool match_dec_octet(const std::string &s, size_t pos_start,
                            size_t &pos_end, std::string &dec_octet) {
  size_t match_len = match_zero_or_more(s, kDigit, pos_start);

  if (match_len == 0 || match_len > 3) {
    // decimal octets are 0 - 255. We should be more strict here.
    return false;
  }

  dec_octet = capture(s, pos_start, match_len, pos_end);

  return true;
}

static bool match_ipv4(const std::string &s, size_t pos_start, size_t &pos_end,
                       std::string &ipv4_addr) {
  std::string dec_octet;
  size_t pos_matched;

  if (!match_dec_octet(s, pos_start, pos_matched, dec_octet)) {
    return false;
  }
  for (size_t i = 0; i < 3; i++) {
    if (pos_matched >= s.length()) {
      return false;
    }
    if (s.at(pos_matched) != '.') {
      return false;
    }
    pos_matched += 1;
    if (!match_dec_octet(s, pos_matched, pos_matched, dec_octet)) {
      return false;
    }
  }

  // resolve ambiguity between match_ipv4 and match_reg_name
  //
  // look-ahead, non-capture: next should be EOL or one of '/:]'
  if (!is_eol(s, pos_matched)) {
    switch (s.at(pos_matched)) {
      case '/':
      case ':':
      case ']':
        break;
      default:
        return false;
    }
  }

  ipv4_addr = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_h16(const std::string &s, size_t pos_start,
                           size_t &pos_end, std::string &h16) {
  // 1*4HEXDIG
  size_t match_len = match_zero_or_more(s, kHexDigit, pos_start);

  if (match_len < 1) {
    return false;
  }

  if (match_len > 4) {
    match_len = 4;
  }

  h16 = capture(s, pos_start, match_len, pos_end);

  return true;
}

static bool match_ipv6_ls32(const std::string &s, size_t pos_start,
                            size_t &pos_end, std::string &ls32) {
  // ( h16 ":" h16 ) / IPv4address
  //
  size_t pos_matched;
  std::string tmp;

  if (!(match_ipv6_h16(s, pos_start, pos_matched, tmp) &&
        match_colon(s, pos_matched, pos_matched) &&
        match_ipv6_h16(s, pos_matched, pos_matched, tmp)) &&
      !match_ipv4(s, pos_start, pos_matched, tmp)) {
    return false;
  }

  ls32 = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_h16_colon(const std::string &s, size_t pos_start,
                                 size_t &pos_end, std::string &h16_colon) {
  // h16 ":"
  //
  // ensure that we don't make h16 "::"
  size_t pos_matched;
  std::string tmp;

  if (!match_ipv6_h16(s, pos_start, pos_matched, tmp)) {
    return false;
  }

  if (match_double_colon(s, pos_matched, pos_matched)) {
    return false;
  }

  if (!match_colon(s, pos_matched, pos_matched)) {
    return false;
  }

  h16_colon = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_1(const std::string &s, size_t pos_start,
                         size_t &pos_end, std::string &ipv6_addr) {
  // 1st line in the IPv6Address line in the RFC
  size_t pos_matched = pos_start;
  std::string tmp;
  size_t sections;

  for (sections = 0; sections < 6; sections++) {
    if (!match_ipv6_h16_colon(s, pos_matched, pos_matched, tmp)) {
      return false;
    }
  }

  if (sections != 6) {
    return false;
  }

  if (!match_ipv6_ls32(s, pos_matched, pos_matched, tmp)) {
    return false;
  }

  ipv6_addr = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_2(const std::string &s, size_t pos_start,
                         size_t &pos_end, std::string &ipv6_addr) {
  // 2nd line in the IPv6Address line in the RFC
  size_t pos_matched = pos_start;
  std::string tmp;
  size_t sections;

  if (!match_double_colon(s, pos_matched, pos_matched)) {
    return false;
  }

  for (sections = 0; sections < 5; sections++) {
    if (!match_ipv6_h16_colon(s, pos_matched, pos_matched, tmp)) {
      return false;
    }
  }

  if (sections != 5) {
    return false;
  }

  if (!match_ipv6_ls32(s, pos_matched, pos_matched, tmp)) {
    return false;
  }

  ipv6_addr = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_h16_colon_prefix(const std::string &s, size_t pos_start,
                                        size_t max_pre_double_colon,
                                        size_t &pos_end,
                                        std::string &ipv6_addr) {
  size_t pos_matched = pos_start;
  std::string tmp;
  size_t sections;

  for (sections = 0; sections < max_pre_double_colon; sections++) {
    if (!match_ipv6_h16_colon(s, pos_matched, pos_matched, tmp)) {
      break;
    }
  }

  if (!match_ipv6_h16(s, pos_matched, pos_matched, tmp)) {
    return false;
  }

  ipv6_addr = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_3(const std::string &s, size_t pos_start,
                         size_t max_pre_double_colon, size_t &pos_end,
                         std::string &ipv6_addr) {
  // 3rd-7th line in the IPv6Address line in the RFC
  size_t pos_matched = pos_start;
  std::string tmp;
  size_t sections;

  size_t post_double_colon = 4 - max_pre_double_colon;

  match_ipv6_h16_colon_prefix(s, pos_matched, max_pre_double_colon, pos_matched,
                              tmp);

  if (!match_double_colon(s, pos_matched, pos_matched)) {
    return false;
  }

  for (sections = 0; sections < post_double_colon; sections++) {
    if (!match_ipv6_h16_colon(s, pos_matched, pos_matched, tmp)) {
      return false;
    }
  }

  if (sections != post_double_colon) {
    return false;
  }

  if (!match_ipv6_ls32(s, pos_matched, pos_matched, tmp)) {
    return false;
  }

  ipv6_addr = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_8(const std::string &s, size_t pos_start,
                         size_t max_pre_double_colon, size_t &pos_end,
                         std::string &ipv6_addr) {
  // 8th-9th line in the IPv6Address line in the RFC
  size_t pos_matched = pos_start;
  std::string tmp;

  match_ipv6_h16_colon_prefix(s, pos_matched, max_pre_double_colon, pos_matched,
                              tmp);

  if (!match_double_colon(s, pos_matched, pos_matched)) {
    return false;
  }

  if (max_pre_double_colon == 5) {
    if (!match_ipv6_h16(s, pos_matched, pos_matched, tmp)) {
      return false;
    }
  }

  ipv6_addr = capture(s, pos_start, pos_matched - pos_start, pos_end);

  return true;
}

static bool match_ipv6_zoneid(const std::string &s, size_t pos_start,
                              size_t &pos_end, std::string &zoneid,
                              bool with_pct_encoded) {
  // zoneid       = unreserved / pct-encoded

  bool made_progress;
  std::string tmp;
  size_t pos_matched = pos_start;

  do {
    made_progress = false;

    size_t match_len = match_zero_or_more(s, kUnreserved, pos_matched);

    if (match_len > 0) {
      made_progress = true;

      tmp.append(capture(s, pos_matched, match_len, pos_matched));
    }

    if (with_pct_encoded) {
      std::string pct_enc;
      if (match_pct_encoded(s, pos_matched, pos_matched, pct_enc)) {
        tmp.append(pct_enc);

        made_progress = true;
      }
    } else if (s.at(pos_matched) == '%') {
      tmp += '%';

      made_progress = true;
      pos_matched += 1;
    }
  } while (made_progress);

  if (tmp.length() == 0) {
    return false;
  }

  zoneid = std::move(tmp);
  pos_end = pos_matched;

  return true;
}

static bool match_ipv6(const std::string &s, size_t pos_start, size_t &pos_end,
                       std::string &ipv6_addr) {
  // we can have max 8 sections.
  // sections of all zeros may be compressed with :: once
  // the last two sections may be IPv4 notation
  // each section is separated with a :
  //
  //
  return match_ipv6_1(s, pos_start, pos_end, ipv6_addr) ||
         match_ipv6_2(s, pos_start, pos_end, ipv6_addr) ||
         match_ipv6_3(s, pos_start, 0, pos_end, ipv6_addr) ||
         match_ipv6_3(s, pos_start, 1, pos_end, ipv6_addr) ||
         match_ipv6_3(s, pos_start, 2, pos_end, ipv6_addr) ||
         match_ipv6_3(s, pos_start, 3, pos_end, ipv6_addr) ||
         match_ipv6_3(s, pos_start, 4, pos_end, ipv6_addr) ||
         match_ipv6_8(s, pos_start, 5, pos_end, ipv6_addr) ||
         match_ipv6_8(s, pos_start, 6, pos_end, ipv6_addr);
}

static bool match_ip_literal(const std::string &s, size_t pos_start,
                             size_t &pos_end, std::string &ip_literal,
                             bool with_pct_encoded) {
  // IP-literal = "[" ( IPv6address / IPv6addrz / IPvFuture ) "]"
  //
  // RFC 4291
  //
  // * :: allowed once per address
  // * replaces a series of zeros
  // * ::1 -> 0:0:0:0:0:0:0:1
  if (is_eol(s, pos_start)) {
    return false;
  }
  if (s.at(pos_start) != '[') {
    return false;
  }

  pos_start += 1;

  std::string tmp;
  size_t pos_matched;

  if (!match_ipv6(s, pos_start, pos_matched, tmp)) {
    throw URIError("expected to find IPv6 address, but failed", s, pos_start);
  }

  if (with_pct_encoded) {
    if (match_pct_encoded(s, pos_matched, pos_matched, tmp)) {
      if (tmp.compare("%25") == 0) {
        if (!match_ipv6_zoneid(s, pos_matched, pos_matched, tmp,
                               with_pct_encoded)) {
          throw URIError("invalid zoneid", s, pos_matched);
        }
      } else {
        throw URIError("invalid pct-encoded value, expected %25", s,
                       pos_matched - 2);
      }
    }
  } else if (s.at(pos_matched) == '%') {
    pos_matched += 1;
  }

  if (is_eol(s, pos_matched) || s.at(pos_matched) != ']') {
    throw URIError("expected to find a ']'", s, pos_matched);
  }

  ip_literal = capture(s, pos_start, pos_matched - pos_start, pos_end);
  pos_end += 1;

  return true;
}

static bool match_host(const std::string &s, size_t pos_start, size_t &pos_end,
                       std::string &host, bool with_pct_encoded) {
  // host        = IP-literal / IPv4address / reg-name
  //
  // match_reg_name has to be last as it accepts the 'empty host'

  return match_ipv4(s, pos_start, pos_end, host) ||
         match_ip_literal(s, pos_start, pos_end, host, with_pct_encoded) ||
         match_reg_name(s, pos_start, pos_end, host, with_pct_encoded);
}

static bool match_authority(const std::string &s, size_t pos_start,
                            size_t &pos_end, std::string &tmp_host,
                            std::string &tmp_port, std::string &tmp_username,
                            std::string &tmp_password) {
  /* RFC 2234 defines:
   *
   * - HEXDIG
   *
   * reserved = gen-delims / sub-delims
   * gen-delims = ":" / "/" / "?" / "#" / "[" / "]" / "@"
   * sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" /
   * "="
   *
   * unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
   *
   * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
   * hier-part = "//" authority path-abempty
   *     / path-absolute
   *     / path-rootless
   *     / path-empty
   *
   * authority = [ userinfo "@" ] host [ ":" port ]
   *
   * ZoneID = 1* (reserved / pct-encoded)
   * IPv6addrz = IPv6address "%25" ZoneID
   * IPvFuture  = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
   * IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet
   * reg-name    = *( unreserved / pct-encoded / sub-delims )
   *
   * extension:
   * userinfo = username [ ":" password ]
   * username = *( unreserved / pct-encoded / sub-delims )
   * password = *( unreserved / pct-encoded / sub-delims / ":" )
   *
   */

  // if there is a "//" we have a authority
  if (s.length() - pos_start < 2) {
    return false;
  }

  if (s.compare(pos_start, 2, "//") != 0) {
    return false;
  }

  size_t pos_matched = pos_start + 2;
  std::string user_info;

  if (match_userinfo(s, pos_matched, pos_matched, user_info)) {
    split_userinfo(user_info, tmp_username, tmp_password);
  }

  if (!match_host(s, pos_matched, pos_matched, tmp_host, true)) {
    throw URIError("expected host :(", s, pos_matched);
  }

  // EOL, : or /
  if (pos_matched < s.length() && s.at(pos_matched) == ':') {
    pos_matched += 1;

    match_port(s, pos_matched, pos_matched, tmp_port);
  }

  pos_end = pos_matched;
  return true;
}

static bool match_path_segment(const std::string &s, size_t pos_start,
                               size_t &pos_end, std::string &segment) {
  return match_path_chars(s, pos_start, pos_end, segment);
}

static bool match_path_empty(const std::string &s, size_t pos_start,
                             size_t &pos_end, std::string &path) {
  std::string segment;

  if (!match_path_chars(s, pos_start, pos_end, segment)) {
    path = "";
    return true;
  }

  if (segment.length() == 0) {
    path = "";
    return true;
  }

  return false;
}

static bool match_path_absolute(const std::string &s, size_t pos_start,
                                size_t &pos_end, std::string &path) {
  // we rely on match_path_absolute being called after match_authority.
  // it allows us to simplify:
  //
  //   path-absolute = "/" [ segment-nz *( "/" segment ) ]
  //
  // to
  //
  //   path-absolute = "/" [ segment *( "/" segment ) ]
  //
  // and use this matcher for both the 'ab'-part of 'path-abempty'
  // and the 'path-absolute' case.
  if (is_eol(s, pos_start)) {
    return false;
  }

  if (s.at(pos_start) != '/') {
    return false;
  }

  size_t pos_matched = pos_start + 1;

  std::string tmp;
  do {
    std::string segment;

    if (match_path_segment(s, pos_matched, pos_matched, segment)) {
      tmp.append(segment);
    }

    if (is_eol(s, pos_matched)) {
      break;
    }

    if (s.at(pos_matched) != '/') {
      break;
    }

    tmp.append(capture(s, pos_matched, 1, pos_matched));
  } while (true);

  path = std::move(tmp);
  pos_end = pos_matched;

  return true;
}

static bool match_path_absolute_or_empty(const std::string &s, size_t pos_start,
                                         size_t &pos_end, std::string &path) {
  return match_path_absolute(s, pos_start, pos_end, path) ||
         match_path_empty(s, pos_start, pos_end, path);
}

static bool match_path_rootless(const std::string &s, size_t pos_start,
                                size_t &pos_end, std::string &path) {
  // path-rootless = segment-nz *( "/" segment )
  if (is_eol(s, pos_start)) {
    return false;
  }

  std::string tmp;
  size_t pos_matched;

  if (!match_path_segment(s, pos_start, pos_matched, tmp)) {
    return false;
  }

  if (tmp.length() == 0) {
    // we need at least one path-char
    return false;
  }

  while (!is_eol(s, pos_matched) && s.at(pos_matched) == '/') {
    std::string segment;

    tmp.append(capture(s, pos_matched, 1, pos_matched));

    if (match_path_segment(s, pos_matched, pos_matched, segment)) {
      tmp.append(segment);
    }
  }

  path = std::move(tmp);
  pos_end = pos_matched;

  return true;
}

static bool match_fragment_query_chars(const std::string &s, size_t pos_start,
                                       size_t &pos_end, std::string &chars) {
  // fragment and query matcher both share the same allowed chars after the
  // initial char
  //
  // *( pchar / "/" / "?" )
  std::string tmp;
  size_t pos_matched = pos_start;
  bool made_progress;

  do {
    std::string segment;

    made_progress = false;

    if (match_path_chars(s, pos_matched, pos_matched, segment) &&
        segment.length() > 0) {
      tmp.append(segment);
      made_progress = true;
    }

    size_t match_len = match_zero_or_more(s, kFragmentOrQuery, pos_matched);

    if (match_len > 0) {
      tmp.append(capture(s, pos_matched, match_len, pos_matched));
      made_progress = true;
    }
  } while (made_progress);

  chars = std::move(tmp);
  pos_end = pos_matched;

  return true;
}

static bool match_fragment(const std::string &s, size_t pos_start,
                           size_t &pos_end, std::string &fragment) {
  //
  // fragment    = *( pchar / "/" / "?" )
  //
  if (is_eol(s, pos_start)) {
    return false;
  }

  if (s.at(pos_start) != '#') {
    return false;
  }

  pos_start += 1;

  return match_fragment_query_chars(s, pos_start, pos_end, fragment);
}

static bool match_query(const std::string &s, size_t pos_start, size_t &pos_end,
                        std::string &query) {
  if (is_eol(s, pos_start)) {
    // already at EOL, we need to match at least ?
    return false;
  }

  if (s.at(pos_start) != '?') {
    return false;
  }

  pos_start += 1;

  return match_fragment_query_chars(s, pos_start, pos_end, query);
}

// decode a std::string with pct-encoding
std::string pct_decode(const std::string &s) {
  size_t s_len = s.length();
  std::string decoded;

  // only alloc once. We may alloc too much.
  decoded.reserve(s_len);

  for (size_t ndx = 0; ndx < s_len; ndx++) {
    if (s.at(ndx) == '%' && s_len - ndx > 2 && 0 != isxdigit(s.at(ndx + 1)) &&
        0 != isxdigit(s.at(ndx + 2))) {
      decoded.append(
          1, static_cast<char>(std::stol(s.substr(ndx + 1, 2), nullptr, 16)));
      ndx += 2;
    } else {
      decoded.append(1, s.at(ndx));
    }
  }
  return decoded;
}

static URIQuery split_query(const std::string &s) {
  URIQuery query;

  for (auto &part : split_string(s, '&', false)) {
    auto key_value = split_string(part, '=');
    if (key_value.size() < 2) {
      throw URIError("invalid URI: query-string part doesn't contain '='");
    }
    if (!key_value[0].empty()) {
      query[pct_decode(key_value[0])] = pct_decode(key_value[1]);
    }
  }

  return query;
}

std::string URIParser::decode(const std::string &uri, bool decode_plus) {
  std::string pct;
  std::string result;
  bool gather_pct = false;

  pct.reserve(3);
  for (auto c : uri) {
    if (gather_pct) {
      pct += c;
      if (3 == pct.length()) {
        result += pct_decode(pct);
        gather_pct = false;
      }
      continue;
    } else if ('?' == c) {
      decode_plus = true;
    } else if ('+' == c && decode_plus) {
      c = ' ';
    } else if ('%' == c) {
      gather_pct = true;
      pct = '%';
      continue;
    }

    result += c;
  }

  // If we did not flush the pct buffer
  if (gather_pct) {
    result += pct;
  }

  return result;
}

/**
 * if uri ~= host:port, URI(scheme="mysql", host=host, port=port)  [no-pct-enc]
 * elif uri ~= ^/, URI(schema="mysql", query={"socket": uri })     [no-pct-enc]
 * elif uri ~= ^\\, URI(schema="mysql", query={"socket": uri })    [no-pct-enc]
 * else URI(uri)                                                   [pct-enc]
 */
URI URIParser::parse_shorthand_uri(const std::string &uri,
                                   bool allow_path_rootless,
                                   const std::string &default_scheme) {
  size_t pos_matched = 0;
  std::string host;
  std::string port;

  if (uri.length() > 0 && (uri.at(0) == '/' || uri.at(0) == '\\')) {
    URI u;
    URIQuery query;

    query["socket"] = uri;
    u.scheme = default_scheme;
    u.query = query;

    return u;
  } else if (match_host(uri, 0, pos_matched, host, false)) {
    // EOL, : or /
    if (pos_matched < uri.length() && uri.at(pos_matched) == ':') {
      pos_matched += 1;

      match_port(uri, pos_matched, pos_matched, port);
    }

    if (pos_matched == uri.size()) {
      uint64_t parsed_port = 0;
      if (port.length() > 0) {
        try {
          parsed_port = std::stoul(port);
        } catch (const std::out_of_range &) {
          throw URIError(
              "invalid URI: invalid port: impossible port number for: " + uri);
        }

        if (parsed_port > USHRT_MAX) {
          // the URI class only has 'uint16' for the port, even though
          // the RFC allows arbit' large numbers.
          throw URIError(
              "invalid URI: invalid port: impossible port number for: " + uri);
        }
      }

      URI u;
      URIQuery query;

      query["socket"] = uri;
      u.scheme = default_scheme;
      u.host = host;
      u.port = static_cast<uint16_t>(parsed_port);

      return u;
    }
  }

  return URIParser::parse(uri, allow_path_rootless);
}

/*static*/ URI URIParser::parse(const std::string &uri,
                                bool allow_path_rootless, bool allow_schemeless,
                                bool path_keep_last_slash,
                                bool query_single_parameter_when_cant_parse) {
  size_t pos = 0;
  bool have_scheme = true;

  // stage: match and extract fields
  //
  std::string tmp_scheme;
  if (!match_scheme(uri, pos, pos, tmp_scheme)) {
    if (!allow_schemeless) throw URIError("no scheme", uri, pos);
    have_scheme = false;
  }

  if (have_scheme && !match_colon(uri, pos, pos)) {
    throw URIError("expected colon after scheme", uri, pos);
  }

  std::string tmp_path;
  std::string tmp_host;
  std::string tmp_username;
  std::string tmp_password;
  std::string tmp_port;

  if (match_authority(uri, pos, pos, tmp_host, tmp_port, tmp_username,
                      tmp_password)) {
    if (!match_path_absolute_or_empty(uri, pos, pos, tmp_path)) {
      throw URIError("expected absolute path or an empty path", uri, pos);
    }
  } else if (match_path_absolute(uri, pos, pos, tmp_path)) {
  } else if (allow_path_rootless &&
             match_path_rootless(uri, pos, pos, tmp_path)) {
  } else if (match_path_empty(uri, pos, pos, tmp_path)) {
  } else {
    throw URIError("neither authority nor path", uri, pos);
  }

  std::string tmp_query;
  match_query(uri, pos, pos, tmp_query);

  std::string tmp_fragment;
  match_fragment(uri, pos, pos, tmp_fragment);

  // did we match the whole string?
  if (pos != uri.length()) {
    throw URIError("unexpected characters", uri, pos);
  }

  // stage: URI is valid, check values
  //
  // post-process the values
  //
  // * split
  //
  //   * user-info
  //   * paths
  //   * query-stringa
  //
  // * decode the pct-encoding
  // * convert strings to numbers
  // * lowercase scheme

  std::transform(tmp_scheme.begin(), tmp_scheme.end(), tmp_scheme.begin(),
                 ::tolower);

  uint64_t parsed_port = 0;
  if (tmp_port.length() > 0) {
    try {
      parsed_port = std::stoul(tmp_port);
    } catch (const std::out_of_range &) {
      throw URIError("invalid URI: invalid port: impossible port number for: " +
                     uri);
    }

    if (parsed_port > USHRT_MAX) {
      // the URI class only has 'uint16' for the port, even though
      // the RFC allows arbit' large numbers.
      throw URIError("invalid URI: invalid port: impossible port number for: " +
                     uri);
    }
  }

  URI u{"", allow_path_rootless, allow_schemeless, path_keep_last_slash,
        query_single_parameter_when_cant_parse};

  u.scheme = tmp_scheme;
  u.host = pct_decode(tmp_host);
  u.port = static_cast<uint16_t>(parsed_port);
  u.username = pct_decode(tmp_username);
  u.password = pct_decode(tmp_password);
  u.set_path_from_string(tmp_path);
  u.set_query_from_string(tmp_query);
  u.fragment = pct_decode(tmp_fragment);

  return u;
}

static bool is_ipv6(const std::string &s) {
  std::string ipv6_addr;
  size_t pos_end;

  if (!match_ipv6(s, 0, pos_end, ipv6_addr)) {
    return false;
  }

  if (pos_end == s.length()) {
    return true;
  }

  if (s.at(pos_end) == '%') {
    // has a zoneid
    return true;
  }

  return false;
}

static std::string pct_encode(const std::string &s,
                              const std::string &allowed_chars) {
  std::string encoded;
  const char hexchars[] = "0123456789abcdef";

  // assume the common case that nothing has to be encoded.
  encoded.reserve(s.length());

  for (auto &c : s) {
    if (allowed_chars.find(c) == std::string::npos) {
      // not a allowed char, encode
      encoded += '%';
      encoded += hexchars[(c >> 4) & 0xf];
      encoded += hexchars[c & 0xf];
    } else {
      encoded += c;
    }
  }
  return encoded;
}

std::ostream &operator<<(std::ostream &strm, const URI &uri) {
  bool need_slash = false;

  if (!(uri.scheme.empty() && uri.allow_schemeless_)) {
    strm << uri.scheme << ":";
  } else {
    need_slash = true;
  }

  if (uri.username.length() > 0 || uri.host.length() > 0 || uri.port > 0 ||
      uri.password.length() > 0) {
    // we have a authority
    strm << "//";

    if (uri.username.length() > 0) {
      strm << pct_encode(uri.username, kUnreserved + kSubDelims);
    }

    if (uri.password.length() > 0) {
      strm << ":" << pct_encode(uri.password, kUnreserved + kSubDelims + ":");
    }

    if (uri.username.length() > 0 || uri.password.length() > 0) {
      strm << "@";
    }

    // IPv6, wrap in []
    if (is_ipv6(uri.host)) {
      strm << "[" << pct_encode(uri.host, kUnreserved + ":") << "]";
    } else {
      strm << pct_encode(uri.host, kUnreserved + kSubDelims);
    }

    if (uri.port) {
      strm << ":" << uri.port;
    }

    need_slash = true;
  }

  strm << uri.get_path_as_string(need_slash);

  if (uri.query.size() > 0) {
    strm << "?" << uri.get_query_as_string();
  }
  if (uri.fragment.size() > 0) {
    strm << "#" << pct_encode(uri.fragment, kPathCharNoPctEncoded + "/?");
  }
  return strm;
}

std::string URI::str() const {
  std::stringstream ss;

  ss << *this;

  return ss.str();
}

void URI::init_from_uri(const std::string &uri) {
  if (uri.empty()) {
    return;
  }

  *this = URIParser::parse(uri, allow_path_rootless_, allow_schemeless_,
                           path_keep_last_slash_,
                           query_single_parameter_when_cant_parse_);
}

void URI::set_path_from_string(const std::string &p) {
  path = split_string(p, '/', path_keep_last_slash_);
  const bool path_begins_with_slash = !p.empty() && *p.begin() == '/';
  const bool is_first_element_empty = !path.empty() && path[0].empty();

  if (path_begins_with_slash && is_first_element_empty) {
    path.erase(path.begin());
  }

  std::transform(path.begin(), path.end(), path.begin(), pct_decode);
}

void URI::set_query_from_string(const std::string &q) {
  try {
    query = split_query(q);
  } catch (...) {
    if (!query_single_parameter_when_cant_parse_) throw;

    query_is_signle_parameter_ = true;
    query.clear();
    query[""] = pct_decode(q);
  }
}

std::string URI::get_path_as_string(bool needs_first_slash) const {
  auto needs_slash = needs_first_slash;
  std::string result;

  auto size = path.size();
  for (const auto &p : path) size += p.length();

  result.reserve(size);

  for (auto &segment : path) {
    if (needs_slash) {
      result.append("/");
    }
    result.append(pct_encode(segment, kPathCharNoPctEncoded));

    needs_slash = true;
  }

  return result;
}

std::string URI::get_query_as_string() const {
  if (!query_is_signle_parameter_) {
    std::string result;
    bool need_amp = false;
    auto size = query.size();
    for (const auto &p : query)
      size += p.first.length() + p.second.length() + 1;

    result.reserve(size);

    for (auto &k_v : query) {
      if (need_amp) {
        result.append("&");
      }

      result += pct_encode(k_v.first, kUnreserved) + "=" +
                pct_encode(k_v.second, kUnreserved);
      need_amp = true;
    }

    return result;
  }

  if (query.empty()) return "";

  return pct_encode(query.begin()->second, kUnreserved);
}

bool URI::operator==(const URI &u2) const {
  return host == u2.host && port == u2.port && scheme == u2.scheme &&
         username == u2.username && password == u2.password &&
         path == u2.path && query == u2.query && fragment == u2.fragment;
}

bool URI::operator!=(const URI &u2) const { return !(*this == u2); }
}  // namespace mysqlrouter
