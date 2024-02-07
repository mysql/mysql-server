/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mgmapi/mgmapi_config_parameters.h"
#include "ndb_global.h"
#include "util/InputStream.hpp"
#include "util/OutputStream.hpp"
#include "util/TlsKeyManager.hpp"

#include "util/SocketAuthenticator.hpp"

const char *SocketAuthenticator::error(int result) {
  switch (result) {
    case negotiate_tls_ok:
      return "success (negotiated TLS)";
    case negotiate_cleartext_ok:
      return "success (negotiated cleartext)";
    case peer_requires_tls:
      return "peer requires TLS";
    case peer_requires_cleartext:
      return "peer requires cleartext";
    case unexpected_response:
      return "unexpected response from peer";
    case negotiation_failed:
      return "negotiation failed";
    default:
      return "[unexpected error code]";
  }
}

int SocketAuthSimple::client_authenticate(const NdbSocket &sockfd) {
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);

  // Write username and password
  s_output.println("ndbd");
  s_output.println("ndbd passwd");

  char buf[16];

  // Read authentication result
  if (s_input.gets(buf, sizeof(buf)) == nullptr) return negotiation_failed;
  buf[sizeof(buf) - 1] = 0;

  // Verify authentication result
  if (strncmp("ok", buf, 2) == 0) return AuthOk;

  return unexpected_response;
}

int SocketAuthSimple::server_authenticate(const NdbSocket &sockfd) {
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);

  char buf[256];

  // Read username
  if (s_input.gets(buf, sizeof(buf)) == nullptr) return negotiation_failed;
  buf[sizeof(buf) - 1] = 0;

  // Read password
  if (s_input.gets(buf, sizeof(buf)) == nullptr) return negotiation_failed;
  buf[sizeof(buf) - 1] = 0;

  // Write authentication result
  s_output.println("ok");

  return AuthOk;
}

/*
 * SocketAuthTls
 */

int SocketAuthTls::client_authenticate(const NdbSocket &sockfd) {
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);
  char buf[32];
  const bool tls_enabled = m_tls_keys->ctx();

  // Write first line
  if (tls_required && tls_enabled)
    s_output.println("ndbd TLS required");
  else if (tls_enabled)
    s_output.println("ndbd TLS enabled");
  else
    s_output.println("ndbd TLS disabled");

  // Write second line
  s_output.println("%s", "");

  // Read authentication result
  if (s_input.gets(buf, sizeof(buf)) == nullptr) return negotiation_failed;

  // Check authentication result
  buf[sizeof(buf) - 1] = '\0';
  if (strcmp("ok\n", buf) == 0) /* SocketAuthSimple responds "ok" */
    return tls_required ? peer_requires_cleartext : negotiate_cleartext_ok;

  if (strcmp("TLS ok\n", buf) == 0)
    return tls_enabled ? negotiate_tls_ok : unexpected_response;

  if (strcmp("TLS required\n", buf) == 0) return peer_requires_tls;

  if (strcmp("Cleartext ok\n", buf) == 0)
    return tls_required ? unexpected_response : negotiate_cleartext_ok;

  if (strcmp("Cleartext required\n", buf) == 0) return peer_requires_cleartext;

  return negotiation_failed;
}

int SocketAuthTls::server_authenticate(const NdbSocket &sockfd) {
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);
  char buf[256];
  const bool tls_enabled = m_tls_keys->ctx();

  enum { unknown, too_old, tls_off, tls_on, tls_mandatory } client_status;

  /* Read first line */
  if (s_input.gets(buf, sizeof(buf)) == nullptr) return negotiation_failed;

  /* Parse first line */
  buf[sizeof(buf) - 1] = '\0';
  if (strcmp("ndbd TLS disabled\n", buf) == 0)
    client_status = tls_off;
  else if (strcmp("ndbd TLS enabled\n", buf) == 0)
    client_status = tls_on;
  else if (strcmp("ndbd TLS required\n", buf) == 0)
    client_status = tls_mandatory;
  else if (strcmp("ndbd\n", buf) == 0)
    client_status = too_old;
  else
    client_status = unknown;

  /* Read the second line */
  if (s_input.gets(buf, sizeof(buf)) == nullptr) return negotiation_failed;

  int result = 0;
  switch (client_status) {
    case unknown:
      result = unexpected_response;
      break;
    case tls_off:
    case too_old:
      result = tls_required ? peer_requires_cleartext : negotiate_cleartext_ok;
      break;
    case tls_on:
      result = tls_required ? negotiate_tls_ok : negotiate_cleartext_ok;
      break;
    case tls_mandatory:
      result = tls_enabled ? negotiate_tls_ok : peer_requires_tls;
      break;
  }

  switch (result) {
    case negotiate_cleartext_ok:
      if (client_status == too_old)
        s_output.println("ok");
      else
        s_output.println("Cleartext ok");
      break;
    case negotiate_tls_ok:
      s_output.println("TLS ok");
      break;
    case peer_requires_tls:
      s_output.println("Cleartext required");
      break;
    case peer_requires_cleartext:
      s_output.println("TLS required");
      break;
    default:
      s_output.println("Error");
  }

  return result;
}
