/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_MEDIA_DETECTOR_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_MEDIA_DETECTOR_H_

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "helper/media_type.h"

namespace helper {

class MediaDetector {
 public:
  MediaDetector();

  MediaType detect(const std::string &payload);

 private:
  struct Entry {
    template <typename Char, uint32_t count>
    Entry(const int64_t o, const Char (&v)[count]) : offset{o} {
      value.resize(count);
      for (uint32_t i = 0; i < count; ++i) {
        value[i] = static_cast<char>(v[i]);
      }
    }

    Entry(const int64_t o, const char *v) : offset{o} {
      auto count = strlen(v);
      value.resize(count);
      for (uint32_t i = 0; i < count; ++i) {
        value[i] = static_cast<char>(v[i]);
      }
    }

    // Negative numbers refer to offset from the end, where
    // `-1` means last element in file.
    int64_t offset{0};
    std::string value;
  };

  using Entries = std::vector<Entry>;
  using MediaMap = std::map<MediaType, Entries>;

  void add_media_type(MediaType type, Entries &&e);

  MediaMap media_;
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_MEDIA_DETECTOR_H_
