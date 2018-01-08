/*
   Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
*/

/* hc128.hpp defines HC128
*/


#ifndef TAO_CRYPT_HC128_HPP
#define TAO_CRYPT_HC128_HPP

#include "misc.hpp"

namespace TaoCrypt {


// HC128 encryption and decryption
class HC128 {
public:

    typedef HC128 Encryption;
    typedef HC128 Decryption;


    HC128() {}

    void Process(byte*, const byte*, word32);
    void SetKey(const byte*, const byte*);
private:
    word32 T_[1024];             /* P[i] = T[i];  Q[i] = T[1024 + i ]; */
    word32 X_[16];
    word32 Y_[16];
    word32 counter1024_;         /* counter1024 = i mod 1024 at the ith step */
    word32 key_[8];
    word32 iv_[8];

    void SetIV(const byte*);
    void GenerateKeystream(word32*);
    void SetupUpdate();

    HC128(const HC128&);                  // hide copy
    const HC128 operator=(const HC128&);  // and assign
};

} // namespace


#endif // TAO_CRYPT_HC128_HPP

