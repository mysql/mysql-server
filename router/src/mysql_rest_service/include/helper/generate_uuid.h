/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_GENERATE_UUID_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_GENERATE_UUID_H_

#include <array>
#include <cinttypes>
#include <random>

namespace helper {

using UUID = std::array<uint8_t, 16>;

inline UUID generate_uuid_v4() {
  UUID result;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

  uint32_t *parts32 = reinterpret_cast<uint32_t *>(result.data());
  uint16_t *parts16 = reinterpret_cast<uint16_t *>(result.data());

  parts32[0] = dis(gen);

  parts16[0 + 4] = dis(gen);
  parts16[1 + 4] = (dis(gen) & 0x0FFF) | 0x4000;

  parts16[2 + 4] = (dis(gen) & 0x3FFF) | 0x8000;
  parts16[3 + 4] = dis(gen);

  parts32[3] = dis(gen);

  return result;
}

inline std::string to_uuid_string(const UUID &uuid) {
  const uint32_t *parts32 = reinterpret_cast<const uint32_t *>(uuid.data());
  const uint16_t *parts16 = reinterpret_cast<const uint16_t *>(uuid.data());
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(8) << parts32[0];
  oss << "-";
  oss << std::setw(4) << parts16[4] << "-" << std::setw(4) << parts16[5] << "-"
      << std::setw(4) << parts16[6] << "-";
  oss << std::setw(4) << parts16[7] << std::setw(8) << parts32[3];

  return oss.str();
}

}  // namespace helper

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_GENERATE_UUID_H_ */
