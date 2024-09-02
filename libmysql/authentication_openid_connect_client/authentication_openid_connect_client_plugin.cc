/* Copyright (c) 2024, Oracle and/or its affiliates.

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

/*
  This is a CLIENT_ONLY plugin, so allocation functions are my_malloc,
  my_free etc.
*/
#include <my_dir.h>
#include <my_sys.h>
#include <mysql/client_plugin.h>
#include <mysql/service_mysql_alloc.h>
#include <fstream>
#include <iostream>
#include "mysql_com.h"

#define MAX_MESSAGE_SIZE 20000

static char *s_id_token_location = nullptr;
static const int s_max_token_size = 10000;
static constexpr const char *base64url_chars{
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890+/-_="};

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
  Extract a part from JWT

  @param [in]  jwt  JSON Web Token
  @param [out] part Part extracted

  @returns Success status
    @retval false Success
    @retval true  Failure
*/
bool get_part(std::string &jwt, std::string &part) {
  const size_t pos = jwt.find_first_of('.');
  if (pos == std::string::npos) return true;
  part = jwt.substr(0, pos);
  if (part.empty() ||
      part.find_first_not_of(base64url_chars) != std::string::npos)
    return true;
  jwt = jwt.substr(pos + 1);
  return false;
}

/**
  Extract head, body and signature from JWT

  @param [in]  jwt  JSON Web Token
  @param [out] head Token's head
  @param [out] body Token's body
  @param [out] sig  Token's signature

  @returns Success status
    @retval false Success
    @retval true  Failure
*/
bool get_jwt_parts(std::string jwt, std::string &head, std::string &body,
                   std::string &sig) {
  /*
    JWT consists of base64URL-encoded header, body and signature, separated by
    '.', e.g. "<base64URL>.<base64URL>.<base64URL>".
  */
  if (get_part(jwt, head)) return true;

  if (get_part(jwt, body)) return true;

  sig = jwt;
  if (sig.empty() ||
      sig.find_first_not_of(base64url_chars) != std::string::npos)
    return true;
  return false;
}

/**
  client auth function

  * read stuff via the VIO. try to *read first*
  * get (login) data from the MYSQL handle: mysql->user, mysql->passwd
  * return CR_OK on success, CR_ERROR on failure
*/
static int openid_connect_authentication_client_plugin(MYSQL_PLUGIN_VIO *vio,
                                                       MYSQL * /*mysql*/) {
  /**
   * Step 1: Read the id token.
   */
  if (s_id_token_location == nullptr) {
    log_error("The path to ID token file is not set.");
    return CR_AUTH_USER_CREDENTIALS;
  }
  const char *filename = s_id_token_location;
  std::string token, id_token_file(s_id_token_location);
  // Check if the file exists
  const int fd = open(filename, O_RDONLY);
  free_plugin_option(s_id_token_location);
  if (fd == -1) {
    log_error("Unable to open ID token file: " + id_token_file);
    return CR_AUTH_USER_CREDENTIALS;
  }
  // Get the file size
  struct stat fileStat;
  if (fstat(fd, &fileStat) == -1) {
    log_error("Unable to get ID token file size.");
    close(fd);
    return CR_AUTH_USER_CREDENTIALS;
  }
  const off_t fileSize = fileStat.st_size;

  if (fileSize > s_max_token_size) {
    log_error("The id token file: " + id_token_file +
              " is not acceptable, file size should be less than 10k.");
    return CR_AUTH_USER_CREDENTIALS;
  }
  // Allocate buffer to read file contents
  char *buffer = new char[fileSize + 1];
  buffer[fileSize] = '\0';  // Null terminate the buffer

  // Read file contents
  const ssize_t bytesRead = read(fd, buffer, fileSize);
  if (bytesRead == -1) {
    log_error("Unable to read ID token file: " + id_token_file);
    delete[] buffer;
    close(fd);
    return CR_AUTH_USER_CREDENTIALS;
  }
  token = buffer;

  // Clean up
  delete[] buffer;
  close(fd);

  if (token.empty()) {
    log_error("The id token file: " + id_token_file + " is empty.");
    return CR_AUTH_USER_CREDENTIALS;
  }

  // Sometimes a '\n' is read on linux platforms which makes it an invalid JWT
  // Check if the string ends with '\n'
  if (token.back() == '\n') {
    // Remove the last character
    token.pop_back();
  }

  // Check if token is a valid JWT
  std::string head, body, sig;
  if (get_jwt_parts(token, head, body, sig)) {
    log_error("The id token file: " + id_token_file +
              " does not contain a valid JWT.");
    return CR_AUTH_USER_CREDENTIALS;
  }

  /**
   * Step 2: Check if connection is secure.
   */
  MYSQL_PLUGIN_VIO_INFO vio_info;
  vio->info(vio, &vio_info);
  if (vio_info.is_tls_established ||
      vio_info.protocol == MYSQL_PLUGIN_VIO_INFO::MYSQL_VIO_SOCKET ||
      vio_info.protocol == MYSQL_PLUGIN_VIO_INFO::MYSQL_VIO_MEMORY) {
    /**
     * Step 3: Send the id token to the server for verification.
     */
    unsigned char message[MAX_MESSAGE_SIZE];
    unsigned char *pos = message;
    unsigned short capability = 1;
    *pos = *reinterpret_cast<unsigned char *>(&capability);
    pos++;
    auto length = token.length();
    pos = net_store_length(pos, length);
    memcpy(pos, token.c_str(), length);
    pos += length;
    if (vio->write_packet(vio, message, (int)(pos - message))) {
      log_error("An error occurred during the client server handshake.");
      return CR_AUTH_HANDSHAKE;
    }
  } else {
    log_error(
        "The client-server connection is insecure. Please make sure either a "
        "TLS, socket or shared memory connection is established between the "
        "client and the server.");
    return CR_ERROR;
  }
  return CR_OK;
}

static int initialize_plugin(char *, size_t, int, va_list) { return 0; }

static int deinitialize_plugin() {
  free_plugin_option(s_id_token_location);
  return 0;
}

/**
  authentication_openid_connect_client_option plugin API to allow server to pass
  optional data for plugin to process
*/
static int authentication_openid_connect_client_option(const char *option,
                                                       const void *val) {
  const char *value = static_cast<const char *>(val);
  if (strcmp(option, "id-token-file") == 0) {
    free_plugin_option(s_id_token_location);
    if (value == nullptr) return 0;
    const std::ifstream file(value);
    if (file.good()) {
      s_id_token_location = my_strdup(PSI_NOT_INSTRUMENTED, value, MYF(MY_WME));
      return 0;
    }
  }
  return 1;
}

mysql_declare_client_plugin(
    AUTHENTICATION) "authentication_openid_connect_client",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE,
    "OpenID Connect Client Authentication Plugin", {0, 1, 0}, "COMMUNITY",
    nullptr, initialize_plugin, deinitialize_plugin,
    authentication_openid_connect_client_option,
    nullptr, openid_connect_authentication_client_plugin,
    nullptr mysql_end_client_plugin;
