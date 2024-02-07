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

#include "auth_kerberos_client_io.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <string>

#include "log_client.h"

/* Log into server log. */
extern Logger_client *g_logger_client;

Kerberos_client_io::Kerberos_client_io(MYSQL_PLUGIN_VIO *vio) : m_vio{vio} {}

Kerberos_client_io::~Kerberos_client_io() {}

/*
  Data Format:
  SPN string length two bytes <B1> <B2> +
  SPN string +
  UPN realm string length two bytes <B1> <B2> +
  UPN realm string
*/
bool Kerberos_client_io::read_spn_realm_from_server(
    std::string &service_principal_name, std::string &upn_realm) {
  std::stringstream log_client_stream;
  /*
  As par RFC, max realm  or UPN size is 256.
  max SPN size 256.
  */
  constexpr int max_kerberos_object_size{256};
  int rc_server_read{-1};
  /*
    size, SPN length + SPN + UPN realm length + UPN realm
  */
  constexpr int max_buffer_size{(max_kerberos_object_size + 4) * 2};
  unsigned char buffer[max_buffer_size]{'\0'};
  unsigned char buffer_tmp[max_buffer_size]{'\0'};
  unsigned char *read_data{nullptr};
  short cur_pos{0};

  if (m_vio == nullptr) {
    return false;
  }
  auto parse_client_config = [&buffer, &cur_pos, &buffer_tmp,
                              &rc_server_read]() -> bool {
    short length{0};
    memset(buffer_tmp, '\0', sizeof(buffer_tmp));
    if ((cur_pos + 2) <= rc_server_read) {
      length = static_cast<unsigned char>(buffer[cur_pos + 1]) << 8 |
               static_cast<unsigned char>(buffer[cur_pos]);
      cur_pos += 2;
    } else {
      return false;
    }
    if (length == 0) {
      return false;
    }
    /* Read kerberos configuration */
    if ((cur_pos + length) <= rc_server_read) {
      memcpy(buffer_tmp, buffer + cur_pos, length);
      cur_pos += length;
      return true;
    } else {
      return false;
    }
  };
  /* Get "SPN length 2 bytes + SPN + UPN realm length 2 bytes + UPN realm from
   * the server. */
  rc_server_read =
      m_vio->read_packet(m_vio, static_cast<unsigned char **>(&read_data));

  if (rc_server_read >= 0 && rc_server_read < max_buffer_size) {
    memcpy(buffer, (const char *)read_data, rc_server_read);
    buffer[rc_server_read] = '\0';
    g_logger_client->log_client_plugin_data_exchange(buffer, rc_server_read);
  } else if (rc_server_read > max_buffer_size) {
    rc_server_read = -1;
    buffer[0] = '\0';
    log_client_stream
        << "Kerberos_client_io::read_spn_realm_from_server : SPN + "
           "UPN realm "
        << "is greater then allowed limit of 1024 characters.";
    log_client_error(log_client_stream.str());
    return false;
  } else {
    buffer[0] = '\0';
    log_client_stream
        << "Kerberos_client_io::read_spn_realm_from_server : Plugin has "
        << "failed to read the SPN + UPN realm, make sure that default "
        << "authentication plugin and SPN + UPN realm specified at "
        << "server are correct.";
    log_client_dbg(log_client_stream.str());
    return false;
  }

  /* Read SPN. */
  if (parse_client_config()) {
    service_principal_name = (char *)(buffer_tmp);

  } else {
    return false;
  }

  /* Read user realm. */
  if (parse_client_config()) {
    upn_realm = (char *)(buffer_tmp);
  } else {
    return false;
  }
  log_client_stream.str("");
  log_client_stream << "Parsed service principal name : "
                    << service_principal_name.c_str()
                    << " User realm configured in auth string: "
                    << upn_realm.c_str();
  log_client_info(log_client_stream.str());
  return true;
}

bool Kerberos_client_io::write_gssapi_buffer(const unsigned char *buffer,
                                             int buffer_len) {
  int rc_server{1};
  std::stringstream log_client_stream;

  if (m_vio == nullptr || buffer == nullptr) {
    return false;
  }

  /* Send the request to the MySQL server. */
  log_client_stream << "Kerberos_client_io::write_gssapi_buffer length: "
                    << buffer_len;
  log_client_info(log_client_stream.str());
  g_logger_client->log_client_plugin_data_exchange(buffer, buffer_len);
  rc_server = m_vio->write_packet(m_vio, buffer, buffer_len);
  if (rc_server == 1) {
    log_client_error(
        "Kerberos client plug-in has failed to write data to the server. ");
    return false;
  } else {
    log_client_dbg(
        "Kerberos_client_io::write_gssapi_buffer: kerberos write to server "
        "has succeed ");
    return true;
  }
}

bool Kerberos_client_io::read_gssapi_buffer(unsigned char **gssapi_buffer,
                                            size_t *buffer_len) {
  std::stringstream log_client_stream;

  if (m_vio == nullptr || !buffer_len || !gssapi_buffer) {
    return false;
  }
  /* Get the kerberos response from the MySQL server. */
  *buffer_len = m_vio->read_packet(m_vio, gssapi_buffer);
  if ((*buffer_len) <= 0 || (*gssapi_buffer == nullptr)) {
    log_client_error("Kerberos plug-in has failed to read data from server.");
    return false;
  }
  log_client_stream << "Kerberos client plug-in data read length: "
                    << *buffer_len;
  log_client_info(log_client_stream.str().c_str());
  g_logger_client->log_client_plugin_data_exchange(*gssapi_buffer, *buffer_len);
  return true;
}
