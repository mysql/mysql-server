/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTING_TRANSPORT_CONSTRAINTS_INCLUDED
#define ROUTING_TRANSPORT_CONSTRAINTS_INCLUDED

#include <string>

class TransportConstraints {
 public:
  enum class Constraint {
    kPlaintext,      // no encryption
    kSecure,         // used initially for kPreferred,
                     // to pick either TCP+Encrypted or Unix+Plaintext.
    kEncrypted,      // force encryption
    kHasClientCert,  // force encryption + client-cert set.
  };

  constexpr TransportConstraints(Constraint val) : val_(val) {}

  [[nodiscard]] constexpr Constraint constraint() const { return val_; }

  [[nodiscard]] std::string to_string() const {
    switch (val_) {
      case Constraint::kPlaintext:
        return "plaintext";
      case Constraint::kSecure:
        return "secure";
      case Constraint::kEncrypted:
        return "encrypted";
      case Constraint::kHasClientCert:
        return "has-client-cert";
    };

    return "unknown";
  }

 private:
  Constraint val_;
};

#endif
