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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "gssapi_authentication_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth_kerberos_client_io.h"
#include "gssapi_utility.h"
#include "log_client.h"

extern Logger_client *g_logger_client;

Gssapi_client::Gssapi_client(const std::string &spn, MYSQL_PLUGIN_VIO *vio,
                             const std::string &upn,
                             const std::string &password)
    : m_service_principal{spn},
      m_vio{vio},
      m_user_principal_name{upn},
      m_password{password} {
  m_kerberos = std::unique_ptr<auth_kerberos_context::Kerberos>(
      new auth_kerberos_context::Kerberos(m_user_principal_name.c_str(),
                                          m_password.c_str()));
}

Gssapi_client::~Gssapi_client() {}

bool Gssapi_client::authenticate() {
  bool rc_auth{false};
  std::stringstream log_client_stream;
  OM_uint32 major{0};
  OM_uint32 minor{0};
  gss_ctx_id_t ctxt{GSS_C_NO_CONTEXT};
  gss_name_t service_name{GSS_C_NO_NAME};
  /* Import principal from plain text */
  gss_buffer_desc principal_name_buf{0, nullptr};
  gss_buffer_desc input{0, nullptr};
  gss_buffer_desc output{0, nullptr};
  gss_cred_id_t cred_id{GSS_C_NO_CREDENTIAL};
  OM_uint32 req_flag{0};
  Kerberos_client_io m_io{m_vio};
  /* For making mutual flag, uncomment below line */
  // req_flag = {GSS_C_MUTUAL_FLAG};

  principal_name_buf.length = m_service_principal.length();
  principal_name_buf.value =
      const_cast<void *>((const void *)m_service_principal.c_str());
  major = gss_import_name(&minor, &principal_name_buf, GSS_C_NT_USER_NAME,
                          &service_name);
  if (GSS_ERROR(major)) {
    log_client_gssapi_error(major, minor, "gss_import_name");
    return false;
  }
  do {
    output = {0, nullptr};
    major = gss_init_sec_context(
        &minor, cred_id, &ctxt, service_name, GSS_C_NO_OID, req_flag, 0,
        GSS_C_NO_CHANNEL_BINDINGS, &input, nullptr, &output, nullptr, nullptr);
    if (GSS_ERROR(major)) {
      log_client_gssapi_error(major, minor, "gss_init_sec_context failed");
      goto CLEANUP;
    }
    if (output.length) {
      rc_auth = m_io.write_gssapi_buffer((const unsigned char *)output.value,
                                         output.length);
      if (!rc_auth) {
        goto CLEANUP;
      }
      gss_release_buffer(&minor, &output);
      if (major & GSS_S_CONTINUE_NEEDED) {
        log_client_dbg("GSSAPI authentication, next step.");
        rc_auth = m_io.read_gssapi_buffer((unsigned char **)&input.value,
                                          (size_t *)&input.length);
        if (!rc_auth) {
          goto CLEANUP;
        }
      }
    }
  } while (major & GSS_S_CONTINUE_NEEDED);

  log_client_dbg("GSSAPI authentication, concluded with success.");
  rc_auth = true;

CLEANUP:
  gss_release_cred(&minor, &cred_id);
  if (service_name != GSS_C_NO_NAME) gss_release_name(&minor, &service_name);
  if (ctxt != GSS_C_NO_CONTEXT)
    gss_delete_sec_context(&minor, &ctxt, GSS_C_NO_BUFFER);

  if (rc_auth) {
    log_client_dbg("kerberos_authenticate authentication successful");
  } else {
    log_client_error("kerberos_authenticate client failed");
  }
  return rc_auth;
}

void Gssapi_client::set_upn_info(const std::string &upn,
                                 const std::string &pwd) {
  log_client_dbg("Set UPN.");
  m_user_principal_name = {upn};
  m_password = {pwd};
  /* Kerberos core uses UPN for all other operations. UPN has changed, releases
   * current object and create */
  if (m_kerberos.get()) {
    m_kerberos.release();
  }
  m_kerberos = std::unique_ptr<auth_kerberos_context::Kerberos>(
      new auth_kerberos_context::Kerberos(m_user_principal_name.c_str(),
                                          m_password.c_str()));
}

bool Gssapi_client::obtain_store_credentials() {
  log_client_dbg("Obtaining TGT TGS tickets from kerberos.");
  return m_kerberos->obtain_store_credentials();
}

std::string Gssapi_client::get_user_name() {
  log_client_dbg("Getting user name from Kerberos credential cache.");
  std::string cached_user_name{""};
  if (m_kerberos->get_upn(&cached_user_name)) {
    size_t pos = std::string::npos;
    /* Remove realm */
    if ((pos = cached_user_name.find("@")) != std::string::npos) {
      log_client_dbg("Trimming realm from upn.");
      cached_user_name.erase(pos, cached_user_name.length() - pos + 1);
    }
  }
  return cached_user_name;
}
