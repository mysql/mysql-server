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

#ifndef SOCKET_AUTHENTICATOR_HPP
#define SOCKET_AUTHENTICATOR_HPP

#include "util/NdbSocket.h"

/* client_authenticate() and server_authenticate() return a value
   less than AuthOk on failure. They return a value greater than or
   equal to AuthOk on success.
*/

class SocketAuthenticator {
 public:
  SocketAuthenticator() {}
  virtual ~SocketAuthenticator() {}
  virtual int client_authenticate(const NdbSocket &) = 0;
  virtual int server_authenticate(const NdbSocket &) = 0;

  static constexpr int AuthOk = 0;
  static const char *error(int);  // returns error message for code

  static constexpr int negotiation_failed = -4, unexpected_response = -3,
                       peer_requires_cleartext = -2, peer_requires_tls = -1,
                       negotiate_cleartext_ok = 0, /* AuthOk */
      negotiate_tls_ok = 1;
};

class SocketAuthSimple : public SocketAuthenticator {
 public:
  SocketAuthSimple() {}
  ~SocketAuthSimple() override {}
  int client_authenticate(const NdbSocket &) override;
  int server_authenticate(const NdbSocket &) override;
};

class SocketAuthTls : public SocketAuthenticator {
 public:
  SocketAuthTls(const class TlsKeyManager *km, bool requireTls)
      : m_tls_keys(km), tls_required(requireTls) {}
  ~SocketAuthTls() override {}
  int client_authenticate(const NdbSocket &) override;
  int server_authenticate(const NdbSocket &) override;

 private:
  const class TlsKeyManager *m_tls_keys;
  const bool tls_required;
};

#endif  // SOCKET_AUTHENTICATOR_HPP
