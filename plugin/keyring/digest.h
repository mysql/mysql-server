/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_DGST_H
#define MYSQL_DGST_H

#include <algorithm>

#include "my_inttypes.h"
#include "sha2.h"

namespace keyring {

enum DigestKind { SHA256 };

class Digest {
 public:
  explicit Digest(DigestKind digestKind = SHA256);
  Digest(DigestKind digestKind, const char *value);

  ~Digest();

  void assign(const char *value);
  bool operator==(const Digest &digest);
  Digest &operator=(const Digest &digest);
  void compute(uchar *memory, size_t memory_size);

  unsigned char *value;
  bool is_empty;
  unsigned int length;

 protected:
  void set_digest_kind(DigestKind digest_kind);
};

}  // namespace keyring

#endif  // MYSQL_DGST_H
