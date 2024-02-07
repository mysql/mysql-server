/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include <assert.h>
#include <ios>
#include <sstream>

#include "plugin/group_replication/include/member_version.h"

Member_version::Member_version(unsigned int version) {
  this->version = version & 0x00FFFFFF;
}

uint32 Member_version::get_version() const { return this->version; }

const std::string Member_version::get_version_string() const {
  std::stringstream member_version;
  member_version << std::hex << get_major_version() << "."
                 << get_minor_version() << "." << get_patch_version();
  return member_version.str();
}

uint32 Member_version::get_major_version() const { return this->version >> 16; }

uint32 Member_version::get_minor_version() const {
  return (this->version >> 8) & 0xff;
}

uint32 Member_version::get_patch_version() const {
  return this->version & 0xff;
}

bool Member_version::operator==(const Member_version &other) const {
  return get_version() == other.get_version();
}

bool Member_version::operator!=(const Member_version &other) const {
  return get_version() != other.get_version();
}

bool Member_version::operator<(const Member_version &other) const {
  if (*this == other) return false;

  if (get_major_version() < other.get_major_version()) {
    return true;
  } else if (get_major_version() > other.get_major_version()) {
    return false;
  } else  // major version are the same
  {
    if (get_minor_version() < other.get_minor_version()) {
      return true;
    } else if (get_minor_version() > other.get_minor_version()) {
      return false;
    } else  // minor version are the same
    {
      if (get_patch_version() < other.get_patch_version()) {
        return true;
      }
    }
  }

  return false;
}

bool Member_version::operator>(const Member_version &other) const {
  if (*this == other) return false;

  return !(*this < other);
}

bool Member_version::operator>=(const Member_version &other) const {
  return (*this == other || *this > other);
}

bool Member_version::operator<=(const Member_version &other) const {
  return (*this == other || *this < other);
}

#ifndef NDEBUG
uint8_t Member_version::get_major_version_decimal() const {
  std::stringstream major_hex;
  major_hex << std::hex << get_major_version();
  uint8_t major_dec = std::stoul(major_hex.str());
  assert(major_dec <= 99);
  return major_dec;
}

uint8_t Member_version::get_minor_version_decimal() const {
  std::stringstream minor_hex;
  minor_hex << std::hex << get_minor_version();
  uint8_t minor_dec = std::stoul(minor_hex.str());
  assert(minor_dec <= 99);
  return minor_dec;
}

uint8_t Member_version::get_patch_version_decimal() const {
  std::stringstream patch_hex;
  patch_hex << std::hex << get_patch_version();
  uint8_t patch_dec = std::stoul(patch_hex.str());
  assert(patch_dec <= 99);
  return patch_dec;
}

void Member_version::update_version_decimal(const uint8_t major,
                                            const uint8_t minor,
                                            const uint8_t patch) {
  assert(major <= 99);
  assert(minor <= 99);
  assert(patch <= 99);

  /*
    Convert to a equivalent hex representation.
    5.9.30 -> 0x050930
    version= 0 x 16^5 + 5 x 16^4 + 0 x 16^3 + 9 x 16^2 + 3 x 16^1 + 0 x 16^0
  */
  uint8_t v1 = patch / 10;
  uint8_t v0 = patch - v1 * 10;
  uint8_t v3 = minor / 10;
  uint8_t v2 = minor - v3 * 10;
  uint8_t v5 = major / 10;
  uint8_t v4 = major - v5 * 10;

  this->version =
      v0 + v1 * 16 + v2 * 256 + v3 * 4096 + v4 * 65536 + v5 * 1048576;
}

void Member_version::increment_major_version() {
  uint8_t major = get_major_version_decimal();
  const uint8_t minor = get_minor_version_decimal();
  const uint8_t patch = get_patch_version_decimal();

  assert(major < 99);
  major++;
  update_version_decimal(major, minor, patch);
}

void Member_version::decrement_major_version() {
  uint8_t major = get_major_version_decimal();
  const uint8_t minor = get_minor_version_decimal();
  const uint8_t patch = get_patch_version_decimal();

  assert(major > 0);
  major--;
  update_version_decimal(major, minor, patch);
}

void Member_version::increment_minor_version() {
  const uint8_t major = get_major_version_decimal();
  uint8_t minor = get_minor_version_decimal();
  const uint8_t patch = get_patch_version_decimal();

  if (99 == minor) {
    minor = 0;
    update_version_decimal(major, minor, patch);
    increment_major_version();
  } else {
    minor++;
    update_version_decimal(major, minor, patch);
  }
}

void Member_version::decrement_minor_version() {
  const uint8_t major = get_major_version_decimal();
  uint8_t minor = get_minor_version_decimal();
  const uint8_t patch = get_patch_version_decimal();

  if (0 == minor) {
    minor = 99;
    update_version_decimal(major, minor, patch);
    decrement_major_version();
  } else {
    minor--;
    update_version_decimal(major, minor, patch);
  }
}

void Member_version::increment_patch_version() {
  const uint8_t major = get_major_version_decimal();
  const uint8_t minor = get_minor_version_decimal();
  uint8_t patch = get_patch_version_decimal();

  if (99 == patch) {
    patch = 0;
    update_version_decimal(major, minor, patch);
    increment_minor_version();
  } else {
    patch++;
    update_version_decimal(major, minor, patch);
  }
}

void Member_version::decrement_patch_version() {
  const uint8_t major = get_major_version_decimal();
  const uint8_t minor = get_minor_version_decimal();
  uint8_t patch = get_patch_version_decimal();

  if (0 == patch) {
    patch = 99;
    update_version_decimal(major, minor, patch);
    decrement_minor_version();
  } else {
    patch--;
    update_version_decimal(major, minor, patch);
  }
}
#endif /* NDEBUG */
