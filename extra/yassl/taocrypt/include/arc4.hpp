/*
   Copyright (C) 2000-2007 MySQL AB
   Use is subject to license terms

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

/* arc4.hpp defines ARC4
*/


#ifndef TAO_CRYPT_ARC4_HPP
#define TAO_CRYPT_ARC4_HPP

#include "misc.hpp"

namespace TaoCrypt {


// ARC4 encryption and decryption
class ARC4 {
public:
    enum { STATE_SIZE = 256 };

    typedef ARC4 Encryption;
    typedef ARC4 Decryption;

    ARC4() {}

    void Process(byte*, const byte*, word32);
    void SetKey(const byte*, word32);
private:
    byte x_;
    byte y_;
    byte state_[STATE_SIZE];

    ARC4(const ARC4&);                  // hide copy
    const ARC4 operator=(const ARC4&);  // and assign

    void AsmProcess(byte*, const byte*, word32);
};

} // namespace


#endif // TAO_CRYPT_ARC4_HPP

