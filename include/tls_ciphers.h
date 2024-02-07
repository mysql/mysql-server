/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef TLS_CIPHERS_INCLUDED
#define TLS_CIPHERS_INCLUDED

namespace {

/**
  Configuring list of ciphers

  TLSv1.2
  =======
  Server: Specify in folllowing order:
  1. Blocked ciphers
  2. Approved ciphers

  Client: Specify in following order:
  1. Blocked ciphers
  2. Approved ciphers
  3. Client specific ciphers

  TLSv1.3
  =======
  Server: Specify in folllowing order:
  1. Blocked ciphers (None atm)
  2. Approved ciphers

  Client: Specify in following order:
  1. Blocked ciphers (None atm)
  2. Approved ciphers
  3. Client specific ciphers (None atm)

*/

/*
  List of TLSv1.3 ciphers in order to their priority.
  Addition to the list must be done keeping priority of the
  new cipher in mind.
  The last entry must not contain a trailing ":".

  Current criteria for inclusion is:
  1. Must provide Perfect Forward Secrecy
  2. Uses SHA2 in cipher/certificate
  3. Uses AES in GCM or any other AEAD algorithms/modes
*/
const char default_tls13_ciphers[] = {
    "TLS_AES_128_GCM_SHA256:"
    "TLS_AES_256_GCM_SHA384:"
    "TLS_CHACHA20_POLY1305_SHA256:"
    "TLS_AES_128_CCM_SHA256"};

/*
  List of TLSv1.2 ciphers in order to their priority.
  Addition to the list must be done keeping priority of the
  new cipher in mind.
  The last entry must not contain a trailing ":".

  Current criteria for inclusion is:
  1. Must provide Perfect Forward Secrecy
  2. Uses SHA2 in cipher/certificate
  3. Uses AES in GCM or any other AEAD algorithms/modes
*/
const char default_tls12_ciphers[] = {
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-ECDSA-AES256-GCM-SHA384:"
    "ECDHE-RSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES256-GCM-SHA384:"
    "ECDHE-ECDSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-ECDSA-AES256-CCM:"
    "ECDHE-ECDSA-AES128-CCM:"
    "DHE-RSA-AES128-GCM-SHA256:"
    "DHE-RSA-AES256-GCM-SHA384:"
    "DHE-RSA-AES256-CCM:"
    "DHE-RSA-AES128-CCM:"
    "DHE-RSA-CHACHA20-POLY1305"};

/*
  Following ciphers (or categories of ciphers) are not permitted
  because they are too weak to provide required security.

  New cipher/category can be added at any position.

  Care must be taken to prefix cipher/category with "!"
*/
const char blocked_tls12_ciphers[] = {
    "!aNULL:"
    "!eNULL:"
    "!EXPORT:"
    "!LOW:"
    "!MD5:"
    "!DES:"
    "!3DES:"
    "!RC2:"
    "!RC4:"
    "!PSK:"
    "!kDH"};

/*
  Following ciphers are added to the list of permissible ciphers
  while configuring the ciphers on client side.

  This is done to provide backward compatbility.
*/
const char additional_client_ciphers[] = {
    "ECDHE-ECDSA-AES256-CCM8:"
    "ECDHE-ECDSA-AES128-CCM8:"
    "DHE-RSA-AES256-CCM8:"
    "DHE-RSA-AES128-CCM8:"
    "ECDHE-ECDSA-AES128-SHA256:"
    "ECDHE-RSA-AES128-SHA256:"
    "ECDHE-ECDSA-AES256-SHA384:"
    "ECDHE-RSA-AES256-SHA384:"
    "DHE-DSS-AES256-GCM-SHA384:"
    "DHE-DSS-AES128-GCM-SHA256:"
    "DHE-DSS-AES128-SHA256:"
    "DHE-DSS-AES256-SHA256:"
    "DHE-RSA-AES256-SHA256:"
    "DHE-RSA-AES128-SHA256:"
    "DHE-RSA-CAMELLIA256-SHA256:"
    "DHE-RSA-CAMELLIA128-SHA256:"
    "ECDHE-RSA-AES128-SHA:"
    "ECDHE-ECDSA-AES128-SHA:"
    "ECDHE-RSA-AES256-SHA:"
    "ECDHE-ECDSA-AES256-SHA:"
    "DHE-DSS-AES128-SHA:"
    "DHE-RSA-AES128-SHA:"
    "DHE-RSA-AES256-SHA:"
    "DHE-DSS-AES256-SHA:"
    "DHE-RSA-CAMELLIA256-SHA:"
    "DHE-RSA-CAMELLIA128-SHA:"
    "ECDH-ECDSA-AES128-SHA256:"
    "ECDH-RSA-AES128-SHA256:"
    "ECDH-RSA-AES256-SHA384:"
    "ECDH-ECDSA-AES256-SHA384:"
    "ECDH-ECDSA-AES128-SHA:"
    "ECDH-ECDSA-AES256-SHA:"
    "ECDH-RSA-AES128-SHA:"
    "ECDH-RSA-AES256-SHA:"
    "AES128-GCM-SHA256:"
    "AES128-CCM:"
    "AES128-CCM8:"
    "AES256-GCM-SHA384:"
    "AES256-CCM:"
    "AES256-CCM8:"
    "AES128-SHA256:"
    "AES256-SHA256:"
    "AES128-SHA:"
    "AES256-SHA:"
    "CAMELLIA256-SHA:"
    "CAMELLIA128-SHA:"
    "ECDH-ECDSA-AES128-GCM-SHA256:"
    "ECDH-ECDSA-AES256-GCM-SHA384:"
    "ECDH-RSA-AES128-GCM-SHA256:"
    "ECDH-RSA-AES256-GCM-SHA384"};

}  // namespace

#endif /* TLS_CIPHERS_INCLUDED */
