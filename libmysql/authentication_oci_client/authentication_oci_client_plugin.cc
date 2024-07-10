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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

/*
  This is a CLIENT_ONLY plugin, so allocation functions are my_malloc,
  my_free etc.
*/
#include <mysql/service_mysql_alloc.h>

#include <my_compiler.h>
#include <my_dir.h>
#include <my_sys.h>
#include <mysql.h>
#include <mysql/client_plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <ostream>

#include "include/base64_encode.h"
#include "sql-common/oci/signing_key.h"
#include "sql-common/oci/utilities.h"

static char *s_oci_config_location = nullptr;
static char *s_authentication_oci_client_config_profile = nullptr;
static oci::OCI_config_file *s_oci_config_file = nullptr;
static std::string s_expanded_path{};
static const int s_max_token_size = 10000;

// Helper functions.
/**
  Log error message to client

  @param [in]  message  Message to be displayed
*/
void log_error(const std::string &message) { std::cerr << message << "\n"; }

/**
  Free plugin option

  @param [in] option  Plugin option to be freed
*/
inline void free_plugin_option(char *&option) {
  if (option == nullptr) return;
  my_free(option);
  option = nullptr;
}

/**
 * Parse the system variables for the location of the ~/.oci/config file.
 * Extract the key_file= value from the ~/.oci/config file.
 */
oci::OCI_config_file parse_oci_config_file(std::string &err_msg) {
  return oci::parse_oci_config_file(
      oci::get_oci_config_file_location(s_oci_config_location),
      s_authentication_oci_client_config_profile, s_expanded_path, err_msg);
}

/**
 Try to parse and assess the currently effective config file.

 @retval 0 : success
 @retval 1 : parsing failed or not initialized

*/
static int try_parse_and_set_config_file(std::string &err_msg) {
  auto config = parse_oci_config_file(err_msg);
  if (err_msg.empty()) {
    s_oci_config_file->key_file.assign(config.key_file);
    s_oci_config_file->fingerprint.assign(config.fingerprint);
    s_oci_config_file->security_token_file.assign(config.security_token_file);
    return 0;
  }
  return 1;
}

/**
  client auth function

  * read stuff via the VIO. try to *read first*
  * get (login) data from the MYSQL handle: mysql->user, mysql->passwd
  * return CR_OK on success, CR_ERROR on failure
*/
static int oci_authenticate_client_plugin(MYSQL_PLUGIN_VIO *vio,
                                          MYSQL * /*mysql*/) {
  std::string err_msg;
  if (try_parse_and_set_config_file(err_msg)) {
    log_error(err_msg);
    return CR_AUTH_USER_CREDENTIALS;
  }
  /**
   * Step 1: Receive the nonce from the server.
   */
  unsigned char *server_nonce = nullptr;
  int server_nonce_length = vio->read_packet(vio, &server_nonce);
  if (server_nonce_length <= 0) {
    log_error("An error occurred during the client server handshake.");
    return CR_AUTH_HANDSHAKE;
  }
  /**
   * Step 2: Sign the nonce with the private key.
   */
  oci::Signing_Key signer{s_oci_config_file->key_file};
  if (!signer) {
    return CR_AUTH_PLUGIN_ERROR;
  }
  auto encoded = signer.sign(server_nonce, server_nonce_length);
  if (encoded.size() == 0) {
    log_error("Authentication failed, plugin internal error.");
    return CR_AUTH_PLUGIN_ERROR;
  }
  // Read the security token
  std::string token;
  if (!s_oci_config_file->security_token_file.empty()) {
    MY_STAT file_stat;
    if (my_stat(s_oci_config_file->security_token_file.c_str(), &file_stat,
                MYF(0)) == nullptr) {
      log_error("The security token file: " +
                s_oci_config_file->security_token_file + " does not exists.");
      return CR_AUTH_USER_CREDENTIALS;
    }
    if (file_stat.st_size > s_max_token_size) {
      log_error(
          "The security token file: " + s_oci_config_file->security_token_file +
          " is not acceptable, file size should be less than 10k.");
      return CR_AUTH_USER_CREDENTIALS;
    }
    std::ifstream token_file(s_oci_config_file->security_token_file);
    if (!token_file.good()) {
      log_error("Could not read the security token file: " +
                s_oci_config_file->security_token_file);
      return CR_AUTH_USER_CREDENTIALS;
    }
    getline(token_file, token);
    if (token.empty()) {
      log_error("The security token file: " +
                s_oci_config_file->security_token_file + " is empty.");
      return CR_AUTH_USER_CREDENTIALS;
    }
  }
  /**
   * Step 3: Prepare the response.
   */
  auto response = oci::prepare_response(
      s_oci_config_file->fingerprint, oci::ssl::base64_encode(encoded), token);
  /**
   * Step 4: Send the encrypted nonce back to the server for verification.
   */
  if (vio->write_packet(
          vio, reinterpret_cast<const unsigned char *>(response.c_str()),
          response.length())) {
    log_error("An error occurred during the client server handshake.");
    return CR_AUTH_HANDSHAKE;
  }
  return CR_OK;
}

static int initialize_plugin(char *, size_t, int, va_list) {
  s_oci_config_file = new (std::nothrow) oci::OCI_config_file{};
  if (s_oci_config_file == nullptr) return 1;
    /*
      Key file and security token file paths may have "~".
      As per OCI SDK & CLI docs, "~" should be resolved to
      $HOME on *nix/Mac OS and
      %HOMEDRIVE%%HOMEPATH% on Windows
    */
#ifdef _WIN32
  if (getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
    s_expanded_path += getenv("HOMEDRIVE");
    s_expanded_path += getenv("HOMEPATH");
  }
#else
  if (getenv("HOME")) {
    s_expanded_path += getenv("HOME");
  }
#endif
  return 0;
}

static int deinitialize_plugin() {
  if (s_oci_config_file != nullptr) delete s_oci_config_file;
  free_plugin_option(s_oci_config_location);
  free_plugin_option(s_authentication_oci_client_config_profile);
  return 0;
}

/**
  oci_authenticate_client_option plugin API to allow server to pass optional
  data for plugin to process
*/
static int oci_authenticate_client_option(const char *option, const void *val) {
  const char *value = static_cast<const char *>(val);
  if (strcmp(option, "oci-config-file") == 0) {
    free_plugin_option(s_oci_config_location);
    if (value == nullptr) return 0;
    std::ifstream file(value);
    if (file.good()) {
      s_oci_config_location =
          my_strdup(PSI_NOT_INSTRUMENTED, value, MYF(MY_WME));
      return 0;
    }
  }
  if (strcmp(option, "authentication-oci-client-config-profile") == 0) {
    free_plugin_option(s_authentication_oci_client_config_profile);
    if (value == nullptr) return 0;
    s_authentication_oci_client_config_profile =
        my_strdup(PSI_NOT_INSTRUMENTED, value, MYF(MY_WME));
    return 0;
  }
  return 1;
}

mysql_declare_client_plugin(AUTHENTICATION) "authentication_oci_client",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE, "OCI Client Authentication Plugin",
    {0, 1, 0}, "COMMUNITY", nullptr, initialize_plugin, deinitialize_plugin,
    oci_authenticate_client_option, nullptr, oci_authenticate_client_plugin,
    nullptr mysql_end_client_plugin;
