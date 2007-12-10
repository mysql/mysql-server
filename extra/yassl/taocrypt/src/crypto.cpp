/*
   Copyright (C) 2000-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/

/* put features that other apps expect from OpenSSL type crypto */



extern "C" {

    // for libcurl configure test, these are the signatures they use
    // locking handled internally by library
    char CRYPTO_lock() { return 0;}
    char CRYPTO_add_lock() { return 0;}


    // for openvpn, test are the signatures they use
    char EVP_CIPHER_CTX_init() { return 0; }
    char CRYPTO_mem_ctrl() { return 0; }
}  // extern "C"



