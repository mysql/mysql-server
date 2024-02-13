/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_TLS_CONTENT_TYPE_INCLUDED
#define ROUTING_TLS_CONTENT_TYPE_INCLUDED

#include <string>

enum class TlsContentType {
  kChangeCipherSpec = 0x14,
  kAlert,
  kHandshake,
  kApplication,
  kHeartbeat
};

inline std::string tls_content_type_to_string(TlsContentType v) {
  switch (v) {
    case TlsContentType::kChangeCipherSpec:
      return "change-cipher-spec";
    case TlsContentType::kAlert:
      return "alert";
    case TlsContentType::kHandshake:
      return "handshake";
    case TlsContentType::kApplication:
      return "application";
    case TlsContentType::kHeartbeat:
      return "heartbeat";
  }

  return "unknown-" + std::to_string(static_cast<int>(v));
}

#endif
