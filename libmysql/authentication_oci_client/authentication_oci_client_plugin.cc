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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <ostream>

#include <my_compiler.h>
#include <mysql.h>
#include <mysql/client_plugin.h>

#include "sql-common/oci/signing_key.h"
#include "sql-common/oci/ssl.h"
#include "sql-common/oci/utilities.h"

static const char *s_oci_config_location = nullptr;
static oci::OCI_config_file *s_oci_config_file = nullptr;

// Helper functions.
/**
 * Parse the system variables for the location of the ~/.oci/config file.
 * Extract the key_file= value from the ~/.oci/config file.
 */
oci::OCI_config_file parse_oci_config_file() {
  return oci::parse_oci_config_file(
      oci::get_oci_config_file_location(s_oci_config_location));
}

/**
  client auth function

  * read stuff via the VIO. try to *read first*
  * get (login) data from the MYSQL handle: mysql->user, mysql->passwd
  * return CR_OK on success, CR_ERROR on failure
*/
static int oci_authenticate_client_plugin(MYSQL_PLUGIN_VIO *vio,
                                          MYSQL * /*mysql*/) {
  /**
   * Step 1: Receive the nonce from the server.
   */
  unsigned char *server_nonce = nullptr;
  int server_nonce_length = vio->read_packet(vio, &server_nonce);
  if (server_nonce_length <= 0) {
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
    return CR_AUTH_PLUGIN_ERROR;
  }

  /**
   * Step 3: Prepare the response.
   */

  auto response = oci::prepare_response(s_oci_config_file->fingerprint,
                                        oci::ssl::base64_encode(encoded));

  /**
   * Step 4: Send the encrypted nonce back to the server for verification.
   */

  if (vio->write_packet(
          vio, reinterpret_cast<const unsigned char *>(response.c_str()),
          response.length())) {
    return CR_AUTH_HANDSHAKE;
  }
  return CR_OK;
}

/**
 Try to parse and assess the currently effective config file.

 @retval 0 : success
 @retval 1 : parsing failed or not initialized

*/
static int try_parse_and_set_config_file() {
  auto config = parse_oci_config_file();
  if (!config.key_file.empty() && !config.fingerprint.empty() &&
      s_oci_config_file != nullptr) {
    s_oci_config_file->key_file.assign(config.key_file);
    s_oci_config_file->fingerprint.assign(config.fingerprint);
    return 0;
  }
  return 1;
}

static int initialize_plugin(char *, size_t, int, va_list) {
  s_oci_config_file = new (std::nothrow) oci::OCI_config_file{};
  if (s_oci_config_file == nullptr) return 1;
  try_parse_and_set_config_file();
  return 0;
}

static int deinitialize_plugin() {
  if (s_oci_config_file != nullptr) delete s_oci_config_file;
  return 0;
}

/**
  oci_authenticate_client_option plugin API to allow server to pass optional
  data for plugin to process
*/
static int oci_authenticate_client_option(const char *option, const void *val) {
  if (strcmp(option, "oci-config-file") == 0 && val != nullptr) {
    s_oci_config_location = static_cast<const char *>(val);
    return try_parse_and_set_config_file();
  }
  return 1;
}

mysql_declare_client_plugin(AUTHENTICATION) "authentication_oci_client",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE, "OCI Client Authentication Plugin",
    {0, 1, 0}, "COMMUNITY", nullptr, initialize_plugin, deinitialize_plugin,
    oci_authenticate_client_option, nullptr, oci_authenticate_client_plugin,
    nullptr mysql_end_client_plugin;
