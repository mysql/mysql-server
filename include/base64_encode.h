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

#ifndef OCI_SSL_H
#define OCI_SSL_H

#include <string>
#include <vector>

namespace oci {
using Data = std::vector<unsigned char>;
namespace ssl {
// Using extra class to allow using the key content directly.
class Key_Content : public std::string {};

enum class Algorithm { SHA_1, SHA_256 };

std::string base64_encode(const void *binary, size_t length);
std::string base64_encode(const Data &data);
std::vector<unsigned char> base64_decode(const std::string &encoded);
}  // namespace ssl
}  // namespace oci
#endif  // OCI_SSL_H
