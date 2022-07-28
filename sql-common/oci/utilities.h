/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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

#ifndef AUTH_OCI_CLIENT_UTILITIES_H
#define AUTH_OCI_CLIENT_UTILITIES_H

#include <string>

namespace oci {
struct OCI_config_file {
  std::string key_file;
  std::string fingerprint;
};
OCI_config_file parse_oci_config_file(const std::string &oci_config);
std::string get_oci_config_file_location(const char *oci_config);
std::string prepare_response(const std::string &fingerprint,
                             const std::string &signature);
}  // namespace oci
#endif  // AUTH_OCI_CLIENT_UTILITIES_H
