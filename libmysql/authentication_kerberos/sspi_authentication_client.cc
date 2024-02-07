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

/*
  This symbol should be used when accessing the security API from a user-mode
  application
*/
#define SECURITY_WIN32

#include "sspi_authentication_client.h"

#include <Lmcons.h>

#include "auth_kerberos_client_io.h"
#include "log_client.h"
#include "sspi_utility.h"

extern Logger_client *g_logger_client;

Sspi_client::Sspi_client(const std::string &spn, MYSQL_PLUGIN_VIO *vio,
                         const std::string &upn, const std::string &password,
                         const std::string &kdc_host)
    : m_service_principal{spn},
      m_vio{vio},
      m_upn{upn},
      m_password{password},
      m_kdc_host{kdc_host} {}

bool Sspi_client::obtain_store_credentials() {
  TimeStamp ticket_validy_time{{0, 0}};
  SECURITY_STATUS sspi_status{0};
  /*
    Package name as described in:
    https://docs.microsoft.com/en-us/windows/win32/secauthn/acquirecredentialshandle--kerberos
    https://docs.microsoft.com/en-us/windows/win32/secgloss/k-gly
  */
  char mechanism[]{"Kerberos"};
  SEC_WINNT_AUTH_IDENTITY identity{
      nullptr, 0, nullptr, 0, nullptr, 0, SEC_WINNT_AUTH_IDENTITY_ANSI};

  if (m_upn.empty()) {
    log_client_info("Sspi obtain and store TGT: empty user name.");
    return false;
  }
  if (m_password.empty()) {
    log_client_info(
        "Sspi obtain and store TGT: plug-in is using preexisting credentials "
        "using system logon");
    SecInvalidateHandle(&m_cred);
  } else {
    log_client_info(
        "Sspi obtain and store TGT: Plug-in is trying to obtain tickets using "
        "user name and password.");
    if (!m_kdc_host.empty()) {
      identity.Domain = reinterpret_cast<unsigned char *>(
          const_cast<char *>(m_kdc_host.c_str()));
      identity.DomainLength = m_kdc_host.length();
    }
    if (!m_upn.empty()) {
      identity.User =
          reinterpret_cast<unsigned char *>(const_cast<char *>(m_upn.c_str()));
      identity.UserLength = m_upn.length();
    }
    if (!m_password.empty()) {
      identity.Password = reinterpret_cast<unsigned char *>(
          const_cast<char *>(m_password.c_str()));
      identity.PasswordLength = m_password.length();
    }
    identity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
  }

  sspi_status = AcquireCredentialsHandle(
      m_password.empty() ? nullptr : const_cast<char *>(m_upn.c_str()),
      mechanism, SECPKG_CRED_BOTH, nullptr,
      m_password.empty() ? nullptr : &identity, nullptr, nullptr, &m_cred,
      &ticket_validy_time);

  /*
    If the AcquireCredentialsHandle succeeds, the AcquireCredentialsHandle
    returns SEC_E_OK.
    https://docs.microsoft.com/en-us/windows/win32/secauthn/acquirecredentialshandle--kerberos
  */
  if (sspi_status != SEC_E_OK) {
    log_client_sspi_error(sspi_status, "Failed to acquire credentials handle");
    return false;
  }
  return true;
}

std::string Sspi_client::get_user_name() {
  log_client_dbg("Getting user name from windows logged-in.");

  char user_name[UNLEN + 1]{'\0'};
  std::string logged_in_name;
  DWORD size{UNLEN + 1};

  if (GetUserNameA(user_name, &size)) {
    logged_in_name = user_name;
  }
  return logged_in_name;
}

bool Sspi_client::authenticate() {
  if (m_vio == nullptr) {
    log_client_info(
        "Sspi authentication failed: MySQL client IO is not valid.");
    return false;
  }
  if (m_service_principal.empty()) {
    log_client_info(
        "Sspi authentication failed: SPN is empty, please configure it in the "
        "server.");
    return false;
  }

  bool rc_auth{false};
  bool rc_io{false};
  Kerberos_client_io client_io{m_vio};
  CtxtHandle context;
  ULONG attribs{0};
  TimeStamp ticket_validy_time{{0, 0}};
  SECURITY_STATUS sspi_status{SEC_E_LOGON_DENIED};
  SecBufferDesc input_buf_desc;
  SecBuffer input_buf;
  SecBufferDesc output_buf_desc;
  SecBuffer output_buf;

  SecInvalidateHandle(&context);

  /* Prepare input buffers */
  input_buf.BufferType = SECBUFFER_TOKEN;
  input_buf.cbBuffer = 0;
  input_buf.pvBuffer = nullptr;
  input_buf_desc.ulVersion = SECBUFFER_VERSION;
  // Message Seq No
  input_buf_desc.cBuffers = 1;
  input_buf_desc.pBuffers = &input_buf;

  /* Prepare output buffers */
  output_buf.BufferType = SECBUFFER_TOKEN;
  output_buf.pvBuffer = nullptr;
  output_buf_desc.ulVersion = SECBUFFER_VERSION;
  // Message Seq No
  output_buf_desc.cBuffers = 1;
  output_buf_desc.pBuffers = &output_buf;

  do {
    /*
      On the first call of InitializeSecurityContext, context is null and filled
      in via phNewContext parameter. On the next calls, context is partially
      filled.

      Details:
      https://docs.microsoft.com/en-us/windows/win32/api/sspi/nf-sspi-initializesecuritycontexta
    */
    sspi_status = InitializeSecurityContext(
        &m_cred, SecIsValidHandle(&context) ? &context : nullptr,
        const_cast<char *>(m_service_principal.c_str()),
        ISC_REQ_ALLOCATE_MEMORY, 0, SECURITY_NATIVE_DREP,
        input_buf.cbBuffer ? &input_buf_desc : nullptr, 0, &context,
        &output_buf_desc, &attribs, &ticket_validy_time);

    if (!succeeded(sspi_status)) {
      log_client_sspi_error(sspi_status,
                            "Initialize Security Context failed, discontinuing "
                            "authentication process.");
      break;
    }

    if (sspi_status != SEC_E_OK && sspi_status != SEC_I_CONTINUE_NEEDED) {
      log_client_sspi_error(
          sspi_status, "Unexpected response from Initialize Security Context");
      break;
    }

    if (output_buf.cbBuffer) {
      rc_io = client_io.write_gssapi_buffer(
          (unsigned char *)output_buf.pvBuffer, output_buf.cbBuffer);

      FreeContextBuffer(output_buf.pvBuffer);
      output_buf.pvBuffer = nullptr;
      output_buf.cbBuffer = 0;

      if (!rc_io) {
        break;
      }

      if (sspi_status == SEC_I_CONTINUE_NEEDED) {
        log_client_dbg("SSPI client authentication, next step.");
        rc_io =
            client_io.read_gssapi_buffer((unsigned char **)&input_buf.pvBuffer,
                                         (size_t *)&input_buf.cbBuffer);

        if (!rc_io) {
          break;
        }
      }
    }
  } while (sspi_status == SEC_I_CONTINUE_NEEDED);

  if (sspi_status == SEC_E_OK) {
    log_client_dbg("SSPI client authentication, concluded with success.");
    rc_auth = true;
  }

  if (output_buf.pvBuffer) {
    FreeContextBuffer(output_buf.pvBuffer);
    output_buf.pvBuffer = nullptr;
    output_buf.cbBuffer = 0;
  }
  if (SecIsValidHandle(&context)) DeleteSecurityContext(&context);

  if (rc_auth) {
    log_client_dbg("SSPI client authentication successful");
  } else {
    log_client_error("SSPI client authenticate failed");
  }
  return rc_auth;
}
