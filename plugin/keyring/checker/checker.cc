/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/keyring/checker/checker.h"

#include <mysql/psi/mysql_file.h>
#include <memory>

#include "my_compiler.h"

namespace keyring {

const my_off_t Checker::EOF_TAG_SIZE = 3;
const std::string Checker::eofTAG = "EOF";

/**
  checks if keyring file structure is invalid
  @param (I) file       - file handle to be checked
  @param (I) file_size  - total file size
  @param (I) digest     - file digest value
  @param (O) type       - (optional) architecture type of file int lengths

  @return
    @retval false   - file structure is valid
    @retval true    - file structure is invalid
 */
bool Checker::check_file_structure(File file, size_t file_size, Digest *digest,
                                   Converter::Arch *arch) {
  // should we detect file architecture
  if (arch != nullptr) {
    // detect architecture, leave on error
    *arch = detect_architecture(file, file_size);
    if (*arch == Converter::Arch::UNKNOWN) return true;
  }

  // default assumptions
  bool is_invalid = true;

  // file size affects required validation steps
  if (file_size == 0)
    is_invalid = !is_empty_file_correct(digest);
  else
    is_invalid = !is_file_size_correct(file_size) ||
                 !is_file_tag_correct(file) || !is_file_version_correct(file) ||
                 !is_dgst_correct(file, digest);

  return is_invalid;
}

bool Checker::is_empty_file_correct(Digest *digest) {
  return strlen(dummy_digest) == digest->length &&
         strncmp(dummy_digest, reinterpret_cast<const char *>(digest->value),
                 std::min(static_cast<unsigned int>(strlen(dummy_digest)),
                          digest->length)) == 0;
}

bool Checker::is_file_tag_correct(File file) {
  uchar tag[EOF_TAG_SIZE + 1];
  mysql_file_seek(file, 0, MY_SEEK_END, MYF(0));
  if (unlikely(mysql_file_tell(file, MYF(0)) < EOF_TAG_SIZE))
    return false;  // File does not contain tag

  if (file_seek_to_tag(file) ||
      unlikely(mysql_file_read(file, tag, EOF_TAG_SIZE, MYF(0)) !=
               EOF_TAG_SIZE))
    return false;
  tag[3] = '\0';
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return eofTAG == reinterpret_cast<char *>(tag);
}

bool Checker::is_file_version_correct(File file) {
  std::unique_ptr<uchar[]> version(new uchar[file_version.length() + 1]);
  version.get()[file_version.length()] = '\0';
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  if (unlikely(mysql_file_read(file, version.get(), file_version.length(),
                               MYF(0)) != file_version.length() ||
               file_version != reinterpret_cast<char *>(version.get())))
    return false;

  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return true;
}

/**
  detects machine architecture of serialized key data length
 */
Converter::Arch Checker::detect_architecture(File file, size_t file_size) {
  // empty file should use default integer format
  auto native_arch = Converter::get_native_arch();
  if (file_size == 0 || file_size == file_version.length() + eof_size())
    return native_arch;

  // determine detection order for candidates
  Converter::Arch detection_order[] = {
      Converter::Arch::LE_64, Converter::Arch::LE_32, Converter::Arch::BE_64,
      Converter::Arch::BE_32};

  // length conversion variables
  uchar src[8] = {0};
  char dst[8] = {0};
  size_t length[5] = {0};

  for (auto arch : detection_order) {
    size_t location = file_version.length();
    bool skip_arch = false;

    // determine new word width, rewind the keyring file
    auto arch_width = Converter::get_width(arch);
    if (mysql_file_seek(file, location, MY_SEEK_SET, MYF(0)) ==
        MY_FILEPOS_ERROR)
      return Converter::Arch::UNKNOWN;

    // we'll read if there's at least one key worth of data ahead
    while (location + 5 * arch_width + eof_size() <= file_size) {
      // load and calculate sizes
      for (size_t i = 0; i < 5; i++) {
        // failure to read is detection failure
        if (mysql_file_read(file, src, arch_width, MYF(0)) != arch_width)
          return Converter::Arch::UNKNOWN;

        // conversion must be possible
        if (!Converter::convert(reinterpret_cast<char *>(src), dst, arch,
                                native_arch)) {
          skip_arch = true;
          break;
        }

        // store dimensions, increase position
        length[i] = Converter::native_value(dst);
        location += arch_width;
      }

      if (skip_arch) break;

      // key size has to be memory aligned
      if (length[0] % arch_width != 0) {
        skip_arch = true;
        break;
      }

      // verify that native values add up within padding size
      auto total =
          5 * arch_width + length[1] + length[2] + length[3] + length[4];
      if (total > length[0] || total + arch_width < length[0]) {
        skip_arch = true;
        break;
      }

      // we move location according to total size
      location += length[0] - 5 * arch_width;
      mysql_file_seek(file, location, MY_SEEK_SET, MYF(0));
    }

    if (skip_arch) continue;

    // there were no errors - detection is successful
    if (location == file_size - eof_size()) return arch;
  }

  return Converter::Arch::UNKNOWN;
}

}  // namespace keyring
