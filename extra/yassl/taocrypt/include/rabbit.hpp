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

/* rabbit.hpp defines Rabbit
*/


#ifndef TAO_CRYPT_RABBIT_HPP
#define TAO_CRYPT_RABBIT_HPP

#include "misc.hpp"

namespace TaoCrypt {


// Rabbit encryption and decryption
class Rabbit {
public:

    typedef Rabbit Encryption;
    typedef Rabbit Decryption;

    enum RabbitCtx { Master = 0, Work = 1 };

    Rabbit() {}

    void Process(byte*, const byte*, word32);
    void SetKey(const byte*, const byte*);
private:
    struct Ctx {
        word32 x[8];
        word32 c[8];
        word32 carry;
    };

    Ctx masterCtx_;
    Ctx workCtx_;

    void NextState(RabbitCtx);
    void SetIV(const byte*);

    Rabbit(const Rabbit&);                  // hide copy
    const Rabbit operator=(const Rabbit&);  // and assign
};

} // namespace


#endif // TAO_CRYPT_RABBIT_HPP

