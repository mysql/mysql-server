/*
   Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 
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

