/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
  Parse oci config file to extract key fingerprint, location of private key file
  and security token file.

  @param [in]  oci_config    Path to oci config file
  @param [in]  oci_profile   Config profile whose config options is to be read
  @param [in]  expanded_path Resolved path for '~'
  @param [out] err_msg       Error message in case of failure

  @returns values extracted
*/
OCI_config_file parse_oci_config_file(const std::string &oci_config,
                                      const char *oci_profile,
                                      const std::string &expanded_path,
                                      std::string &err_msg) {
  std::string profile;
  if (oci_profile == nullptr)
    profile = "[DEFAULT]";
  else
    profile = "[" + std::string{oci_profile} + "]";
  constexpr char KEY_FILE[]{"key_file="};
  constexpr char FINGERPRINT[]{"fingerprint="};
  constexpr char SECURITY_TOKEN_FILE[]{"security_token_file="};
  std::ifstream file(oci_config);
  if (!file.good()) {
    err_msg = "Could not read the config file: " + oci_config;
    return {};
  }
  bool isProfile = false;  // Are we in the profile section?
  std::string line;
  OCI_config_file result;
  while (std::getline(file, line)) {
    // generated config file may have spaces before and after '='
    size_t pos = line.find(" = ");
    if (pos != std::string::npos) {
      line.erase(line.begin() + pos);
      line.erase(line.begin() + pos + 1);
    }
    // 'key= value' and 'key =value' are not accepted format
    size_t pos_a = line.find("= ");
    size_t pos_b = line.find(" =");
    if (pos_a != std::string::npos || pos_b != std::string::npos) {
      err_msg = "Config file: " + oci_config +
                " has an invalid format near line: " + line +
                ". 'key =value' and 'key= value' are not accepted format.";
      return {};
    }
    if (isProfile) {
      if (line.rfind(KEY_FILE, 0) == 0) {
        // Found 'key_file='.
        line.erase(0, sizeof(KEY_FILE) - 1);
        result.key_file = std::regex_replace(line, std::regex("[[:s:]]+$"), "");
        continue;
      }
      if (line.rfind(FINGERPRINT, 0) == 0) {
        // Found 'fingerprint='.
        line.erase(0, sizeof(FINGERPRINT) - 1);
        result.fingerprint =
            std::regex_replace(line, std::regex("[[:s:]]+$"), "");
        continue;
      }
      if (line.rfind(SECURITY_TOKEN_FILE, 0) == 0) {
        // Found 'security_token_file='.
        line.erase(0, sizeof(SECURITY_TOKEN_FILE) - 1);
        result.security_token_file =
            std::regex_replace(line, std::regex("[[:s:]]+$"), "");
        continue;
      }
      if (line[0] == '[') {
        // profile section over
        break;
      }
    }
    auto default_pos = line.find(profile);
    if (default_pos != std::string::npos) {
      isProfile = true;
      continue;
    }
  }
  if (!isProfile) {
    err_msg = "Config profile: " + profile +
              " is not present in config file: " + oci_config;
    return {};
  }
  if (result.fingerprint.empty() || result.key_file.empty()) {
    err_msg =
        "Missing fingerprint/key_file value in config file: " + oci_config +
        " for the config profile: " + profile;
    return {};
  }
  if (result.key_file[0] == '~' && expanded_path.length())
    result.key_file.replace(0, 1, expanded_path);
  if (!result.security_token_file.empty() &&
      result.security_token_file[0] == '~' && expanded_path.length())
    result.security_token_file.replace(0, 1, expanded_path);

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
                             const std::string &signature,
                             const std::string &token) {
  return "{\"fingerprint\":\"" + fingerprint + "\",\"signature\":\"" +
         signature + "\",\"token\":\"" + token + "\"}";
}
}  // namespace oci
