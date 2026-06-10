/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "utils.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#ifdef _WIN32
#include <WinSock2.h>

#include <Ws2tcpip.h>

#include <Iphlpapi.h>

#include <windows.h>

// for GetAdaptersAddresses()
#pragma comment(lib, "IPHLPAPI.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>  // sockaddr_in
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <errno.h>

namespace routing_guidelines {

std::string str_strip(const std::string &s, const std::string &chars) {
  size_t begin = s.find_first_not_of(chars);
  size_t end = s.find_last_not_of(chars);
  if (begin == std::string::npos) return std::string();
  return s.substr(begin, end - begin + 1);
}

std::string format_json_error(const std::string &s,
                              const rapidjson::ParseResult &ok, size_t chars) {
  std::string ret("incorrect JSON: ");
  ret += rapidjson::GetParseError_En(ok.Code());
  if (ret.back() == '.') ret.pop_back();
  ret += ", near '";
  const auto beg = ok.Offset() > chars ? ok.Offset() - chars : 0;
  ret += s.substr(beg, std::min(ok.Offset() + chars, s.length()) - beg);
  ret += "'";
  return ret;
}

std::string mysql_unescape_string(std::string_view s) {
  std::string res;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '\\' && i + 1 < s.length()) {
      switch (s[++i]) {
        case 'n':
          res.push_back('\n');
          break;
        case 't':
          res.push_back('\t');
          break;
        case 'r':
          res.push_back('\r');
          break;
        case 'b':
          res.push_back('\b');
          break;
        case '0':
          res.push_back('\0');
          break;
        case 'Z':
          res.push_back('\032');  // Win32 end of file
          break;
        default:
          res.push_back(s[i]);
      }
    } else {
      res.push_back(s[i]);
    }
  }
  return res;
}

std::string like_to_regexp(std::string_view pattern) {
  std::string rgxp;
  for (size_t i = 0; i < pattern.size(); i++) switch (pattern[i]) {
      case '.':
      case '*':
      case '+':
      case '?':
      case '{':
      case '}':
      case '(':
      case ')':
      case '[':
      case ']':
      case '^':
      case '$':
      case '|':
        rgxp.push_back('\\');
        rgxp.push_back(pattern[i]);
        break;
      case '%':
        rgxp.append(".*");
        break;
      case '_':
        rgxp.push_back('.');
        break;
      case '\\':
        if (i + 1 < pattern.size()) {
          switch (pattern[i + 1]) {
            case '\\':
              rgxp.append("\\\\");
              i++;
              break;
            case '%':
            case '_':
              rgxp.push_back(pattern[++i]);
              break;
            default:
              rgxp.append("\\\\");
          }
        } else {
          rgxp.append("\\\\");
        }
        break;
      default:
        rgxp.push_back(pattern[i]);
    }

  return rgxp;
}

/**
 * Provides the protocol family for the given literal address.
 *
 * @param address The address to be checked.
 *
 * @return AF_INET if address is an IPv4 address, AF_INET6 if it's IPv6,
 *         AF_UNSPEC in case of failure.
 */
int get_protocol_family(const std::string &address) {
  addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_NUMERICHOST;

  addrinfo *info = nullptr;
  int result = getaddrinfo(address.c_str(), nullptr, &hints, &info);
  if (result != 0) return AF_UNSPEC;

  int family = info->ai_family;
  freeaddrinfo(info);
  return family;
}

bool is_ipv4(const std::string &address) {
  return get_protocol_family(address) == AF_INET;
}

bool is_ipv6(const std::string &host) {
  if (host.empty() || '[' == host[0]) {
    // on Windows `[IPv6]` host string is considered an IPv6 address, we do this
    // test on all platforms to speed up things a bit
    return false;
  }

  // Handling of the zone ID by getaddrinfo() varies on different platforms:
  // numeric values are always accepted, some of them require the zone ID to
  // match the name of one of the network interfaces, while others accept
  // any value. To accommodate for that, we strip the zone ID part from
  // the address as it's enough to check the remaining part to decide if the
  // whole address is IPv6.
  return get_protocol_family(host.substr(0, host.find('%'))) == AF_INET6;
}

std::string network(const std::string &address, unsigned int bitlen) {
  if (bitlen > 32 || bitlen < 1)
    throw std::runtime_error(
        "Valid mask length for IPv4 address is between 1 and 32");

  struct sockaddr_in sa;
  char str[INET_ADDRSTRLEN];

  // store this IP address in sa:
  auto res = inet_pton(AF_INET, address.c_str(), &(sa.sin_addr));
  if (res <= 0)
    throw std::runtime_error(
        "Network function called on invalid IPv4 address: '" + address + "'");

  auto mask = htonl((0xFFFFFFFFUL << (32 - bitlen)) & 0xFFFFFFFFUL);
  sa.sin_addr.s_addr = sa.sin_addr.s_addr & mask;

  if (inet_ntop(AF_INET, &(sa.sin_addr), str, INET_ADDRSTRLEN) == nullptr)
    throw std::runtime_error("Unable to convert address to string");
  return str;
}

bool json_document_complete(const std::string &s) {
  if (s.empty() || s[0] != '{')
    throw std::runtime_error(
        "JSON documents needs to start with '{' character");
  int braces = 1;
  for (size_t i = 1; i < s.length(); i++) {
    if (s[i] == '{') {
      braces++;
    } else if (s[i] == '}' && --braces == 0 && i < s.length() - 1) {
      throw std::runtime_error("Malformed JSON document");
    }
  }

  return !braces;
}

}  // namespace routing_guidelines
