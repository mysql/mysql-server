/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "openssl_msg.h"

#include <string>
#include "mysql/harness/stdx/expected.h"

#include <openssl/ssl.h>
#include <openssl/tls1.h>

stdx::expected<std::string, std::error_code> openssl_msg_version_to_string(
    int ver) {
  switch (ver) {
#ifdef TLS1_3_VERSION
    case TLS1_3_VERSION:
      return "tls1.3";
#endif
    case TLS1_2_VERSION:
      return "tls1.2";
    case TLS1_1_VERSION:
      return "tls1.1";
    case TLS1_VERSION:
      return "tls1.0";
    case SSL3_VERSION:
      return "ssl3.0";
  }

  return "ver-" + std::to_string(ver);
}

stdx::expected<std::string, std::error_code> openssl_msg_content_type_to_string(
    int ct) {
  switch (ct) {
    case SSL3_RT_ALERT:
      return "Alert";
    case SSL3_RT_CHANGE_CIPHER_SPEC:
      return "ChangeCipherSpec";
    case SSL3_RT_HANDSHAKE:
      return "Handshake";
    case SSL3_RT_HEADER:
      return "Header";
#ifdef SSL3_RT_INNER_CONTENT_TYPE
    case SSL3_RT_INNER_CONTENT_TYPE:
      // added in openssl 1.1.1
      return "Inner";
#endif
  }

  return "content-" + std::to_string(ct);
}

stdx::expected<std::string, std::error_code> openssl_msg_content_to_string(
    int ct, const unsigned char *buf, size_t len) {
  switch (ct) {
    case SSL3_RT_ALERT:
      if (len < 2) {
        return stdx::unexpected(make_error_code(std::errc::bad_message));
      }

      // buf[0] is alert-type "fatal|warning"

      switch (auto code = buf[1]) {
        case SSL3_AD_CLOSE_NOTIFY:
          return "close_notify";
        case SSL3_AD_UNEXPECTED_MESSAGE:
          return "unexpected_message";
        case SSL3_AD_BAD_RECORD_MAC:
          return "bad_record_mac";
        case SSL3_AD_DECOMPRESSION_FAILURE:
          return "decompression_failure";
        case SSL3_AD_HANDSHAKE_FAILURE:
          return "handshake_failure";
        case SSL3_AD_NO_CERTIFICATE:
          return "no_certificate";
        case SSL3_AD_CERTIFICATE_UNKNOWN:
          return "certificate_unknown";
        case SSL3_AD_CERTIFICATE_REVOKED:
          return "certificate_revoked";
        case SSL3_AD_CERTIFICATE_EXPIRED:
          return "certificate_expired";
        case TLS1_AD_UNKNOWN_CA:
          return "unknown_ca";  // 48
        case TLS1_AD_ACCESS_DENIED:
          return "access_denied";  // 49
        case TLS1_AD_DECODE_ERROR:
          return "decode_error";  // 50
        case TLS1_AD_DECRYPT_ERROR:
          return "decrypt_error";  // 51
        case TLS1_AD_EXPORT_RESTRICTION:
          return "export_restriction";  // 60
        case TLS1_AD_PROTOCOL_VERSION:
          return "protocol_version";  // 70
        case TLS1_AD_INSUFFICIENT_SECURITY:
          return "insufficient_security";  // 71
        case TLS1_AD_INTERNAL_ERROR:
          return "internal_error";  // 80
        case TLS1_AD_USER_CANCELLED:
          return "user_cancelled";  // 90
        default:
          return std::to_string(code);
      }

      break;
    case SSL3_RT_HANDSHAKE:
      if (len < 1) {
        return stdx::unexpected(make_error_code(std::errc::bad_message));
      }

      switch (auto msg_type = buf[0]) {
        case SSL3_MT_CLIENT_HELLO:
          return "ClientHello";
        case SSL3_MT_SERVER_HELLO:
          return "ServerHello";
        case SSL3_MT_NEWSESSION_TICKET:
          return "NewSessionTicket";
#ifdef SSL3_MT_ENCRYPTED_EXTENSIONS
        case SSL3_MT_ENCRYPTED_EXTENSIONS:
          return "EncryptedExtensions";
#endif
        case SSL3_MT_CERTIFICATE:
          return "Certificate";
        case SSL3_MT_SERVER_KEY_EXCHANGE:
          return "ServerKeyExchange";
        case SSL3_MT_CERTIFICATE_REQUEST:
          return "CertificateRequest";
        case SSL3_MT_SERVER_DONE:
          return "ServerDone";
        case SSL3_MT_CERTIFICATE_VERIFY:
          return "CertificateVerify";
        case SSL3_MT_CLIENT_KEY_EXCHANGE:
          return "ClientKeyExchange";
        case SSL3_MT_FINISHED:
          return "Finished";
        default:
          return std::to_string(msg_type);
      }
  }

  return stdx::unexpected(make_error_code(std::errc::invalid_argument));
}
