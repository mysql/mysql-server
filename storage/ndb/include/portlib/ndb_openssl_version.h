/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

/* NDB requires a certain minimum OpenSSL version for TLS: 1.1.1d, Sep 2019

   In general, OpenSSL 1.1 is required for the availability of TLS 1.3.
   1.1.1d is chosen as a 1.1 release that is mature, but not unreasonably new
   (about three years old at the time the TLS feature is released).
   This version is represented as 0x1010104fL

   Ubuntu 18.04 maintains an OpenSSL 1.1 tree that backports
   patches from OpenSSL 1.1 but is always identified as
   "OpenSSL 1.1.1 Sep 2018."  This version is also usable.
   It version is represented as 0x1010100fL

   Use the Ubuntu 18.04 OpenSSL version number as the minimum
   supported version for NDB TLS.
*/
#define NDB_TLS_MINIMUM_OPENSSL 0x1010100fL

/* NDB_OPENSSL_TOO_OLD is used as an error return value from functions
   to report a OpenSSL library version less than NDB_TLS_MINIMUM_OPENSSL
   at runtime.
*/
#define NDB_OPENSSL_TOO_OLD -100
