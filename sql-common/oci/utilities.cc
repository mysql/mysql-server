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

#include <fstream>
#include <regex>

#include "sql-common/oci/utilities.h"

#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#else  // defined(_WIN32)
#include <pwd.h>
#include <unistd.h>
#endif  // defined(_WIN32)

namespace oci {
/**
 * Return the realpath(~)
 */
std::string get_home_folder() {
#if defined(_WIN32)
  return {getenv("USERPROFILE")};
#else   // defined(_WIN32)
  struct passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;
  return homedir;
#endif  // defined(_WIN32)
}

/**
 * Parse ~/.oci/config file to extract location of the private key.
 */
OCI_config_file parse_oci_config_file(const std::string &oci_config) {
  constexpr char KEY_FILE[]{"key_file="};
  constexpr char FINGERPRINT[]{"fingerprint="};
  std::ifstream file(oci_config);
  if (!file.good()) return {};
  bool isDefault = false;  // Are we in the [DEFAULT] section?
  std::string line;
  OCI_config_file result;
  while (std::getline(file, line)) {
    if (line.rfind(KEY_FILE, 0) == 0) {
      // Found 'key_file='.
      if (isDefault || result.key_file.empty()) {
        // Replace the non-default value with the one from the [DEFAULT] section
        line.erase(0, sizeof(KEY_FILE) - 1);
        result.key_file = std::regex_replace(line, std::regex("[[:s:]]+$"), "");
      }
      continue;
    }
    if (line.rfind(FINGERPRINT, 0) == 0) {
      // Found 'fingerprint='.
      if (isDefault || result.fingerprint.empty()) {
        // Replace the non-default value with the one from the [DEFAULT] section
        line.erase(0, sizeof(FINGERPRINT) - 1);
        result.fingerprint =
            std::regex_replace(line, std::regex("[[:s:]]+$"), "");
      }
      continue;
    }
    auto default_pos = line.find("[DEFAULT]");
    if (default_pos != std::string::npos) {
      isDefault = true;
      continue;
    }
    if (isDefault && line[0] == '[') {
      // Non-default section
      isDefault = false;
      continue;
    }
  }

  if (!result.key_file.empty() && result.key_file[0] == '~') {
    /*
     Resolve "~" if present in key file path.
     As per OCI SDK & CLI docs, "~" should be
     resolved to $HOME on *nix/Mac OS and
     %HOMEDRIVE%%HOMEPATH% on Windows
   */
    std::string updated_path{};
#ifdef _WIN32
    if (getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
      updated_path += getenv("HOMEDRIVE");
      updated_path += getenv("HOMEPATH");
    }
#else
    if (getenv("HOME")) {
      updated_path += getenv("HOME");
    }
#endif
    if (updated_path.length()) result.key_file.replace(0, 1, updated_path);
  }
  return result;
}

/**
 * Return the default location of ~/.oci/config file if not specified.
 */
std::string get_oci_config_file_location(const char *oci_config) {
  if (oci_config != nullptr && oci_config[0] != '\0') return {oci_config};
  return {get_home_folder() + "/.oci/config"};
}

/**
 * JSON format the client signed response.
 */
std::string prepare_response(const std::string &fingerprint,
                             const std::string &signature) {
  return "{\"fingerprint\":\"" + fingerprint + "\",\"signature\":\"" +
         signature + "\"}";
}
}  // namespace oci
