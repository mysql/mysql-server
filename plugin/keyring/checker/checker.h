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

#ifndef MYSQL_CHECKER_H
#define MYSQL_CHECKER_H

#include "my_inttypes.h"
#include "my_io.h"
#include "plugin/keyring/common/keyring_memory.h"
#include "plugin/keyring/common/logger.h"
#include "plugin/keyring/converter.h"
#include "plugin/keyring/digest.h"

namespace keyring {

const std::string keyring_file_version_1_0("Keyring file version:1.0");
const std::string keyring_file_version_2_0("Keyring file version:2.0");
const char dummy_digest[] = "01234567890123456789012345678901";

class Checker : public keyring::Keyring_alloc {
 public:
  Checker(std::string file_version) : file_version(file_version) {}
  virtual ~Checker() {}
  virtual bool check_file_structure(File file, size_t file_size, Digest *digest,
                                    Converter::Arch *type = nullptr);
  static const my_off_t EOF_TAG_SIZE;
  static const std::string eofTAG;

 protected:
  virtual bool is_empty_file_correct(Digest *digest);
  virtual bool is_file_size_correct(size_t file_size) = 0;
  virtual bool is_file_tag_correct(File file);
  virtual bool is_file_version_correct(File file);
  virtual bool is_dgst_correct(File file, Digest *dgst) = 0;
  virtual bool file_seek_to_tag(File file) = 0;
  virtual size_t eof_size() = 0;
  virtual Converter::Arch detect_architecture(File file, size_t file_size);

  std::string file_version;
};

}  // namespace keyring

#endif  // MYSQL_CHECKER_H
