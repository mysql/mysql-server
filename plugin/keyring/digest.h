/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_DGST_H
#define MYSQL_DGST_H

#include <algorithm>

#include "my_inttypes.h"
#include "sha2.h"

namespace keyring {

enum DigestKind
{
  SHA256
};

class Digest
{
public:
  explicit Digest(DigestKind digestKind= SHA256);
  Digest(DigestKind digestKind, const char* value);

  ~Digest();

  void assign(const char *value);
  bool operator==(const Digest &digest);
  Digest& operator=(const Digest &digest);
  void compute(uchar* memory, size_t memory_size);

  unsigned char *value;
  bool is_empty;
  unsigned int length;
protected:
  void set_digest_kind(DigestKind digest_kind);
};

}//namespace keyring

#endif //MYSQL_DGST_H
