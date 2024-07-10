/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#ifndef OCI_SIGNING_KEYS_H
#define OCI_SIGNING_KEYS_H

#include <string>

#include "include/base64_encode.h"
#include "include/encode_ptr.h"

namespace oci {
class Signing_Key {
  /**
   * @brief Private key from a local file, used to Signing_Key::sign() requests.
   *
   */
 private:
  ssl::EVP_PKEY_ptr m_private_key;
  std::string m_public_key;

 public:
  Signing_Key(const std::string &file_name);
  explicit Signing_Key(ssl::Key_Content);
  Signing_Key();
  Signing_Key(const Signing_Key &) = delete;
  Signing_Key &operator=(const Signing_Key &) = delete;
  Signing_Key(Signing_Key &&) = default;
  Signing_Key &operator=(Signing_Key &&) = delete;
  operator bool() const { return m_private_key.operator bool(); }
  std::string get_public_key() { return m_public_key; }

  // main operation
  Data sign(const std::string &message);
  Data sign(const void *message, size_t length);
};
}  // namespace oci
#endif
