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

#include "helper/media_detector.h"

namespace helper {

MediaDetector::MediaDetector() {
  add_media_type(typeJpg, {{0, {0xff, 0xD8}}});
  add_media_type(typeGif, {{0, "GIF8"}});
  add_media_type(typeBmp, {{0, {0x42, 0x4d}}});
  add_media_type(typePng, {{0, "\x89PNG"}});
  add_media_type(typeAvi, {{0, "RIFF"}, {8, "AVI "}});
  add_media_type(typeWav, {{0, "RIFF"}, {8, "WAVEfmt"}});
}

MediaType MediaDetector::detect(const std::string &value) {
  for (const auto &media : media_) {
    bool match = true;
    for (auto entry : media.second) {
      for (uint32_t i = 0; i < entry.value.length(); ++i) {
        int index = entry.offset < 0 ? value.length() - entry.offset -
                                           entry.value.length() + i
                                     : entry.offset + i;

        if (index >= (int)value.length() || index < 0) {
          match = false;
          break;
        }

        if (value[index] != entry.value[i]) {
          match = false;
          break;
        }
      }
    }

    if (match) return media.first;
  }

  return typeUnknownBinary;
}

void MediaDetector::add_media_type(MediaType type, Entries &&entries) {
  media_[type] = entries;
}

}  // namespace helper
