/* arc4.cpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* based on Wei Dai's arc4.cpp from CryptoPP */

#include "runtime.hpp"
#include "arc4.hpp"


namespace TaoCrypt {

void ARC4::SetKey(const byte* key, word32 length)
{
    x_ = 1;
    y_ = 0;

    word32 i;

    for (i = 0; i < STATE_SIZE; i++)
        state_[i] = i;

    word32 keyIndex = 0, stateIndex = 0;

    for (i = 0; i < STATE_SIZE; i++) {
        word32 a = state_[i];
        stateIndex += key[keyIndex] + a;
        stateIndex &= 0xFF;
        state_[i] = state_[stateIndex];
        state_[stateIndex] = a;

        if (++keyIndex >= length)
            keyIndex = 0;
    }
}


// local
namespace {

inline unsigned int MakeByte(word32& x, word32& y, byte* s)
{
    word32 a = s[x];
    y = (y+a) & 0xff;

    word32 b = s[y];
    s[x] = b;
    s[y] = a;
    x = (x+1) & 0xff;

    return s[(a+b) & 0xff];
}

} // namespace


void ARC4::Process(byte* out, const byte* in, word32 length)
{
    if (length == 0) return;

    byte *const s = state_;
    word32 x = x_;
    word32 y = y_;

    if (in == out)
        while (length--)
            *out++ ^= MakeByte(x, y, s);
    else
        while(length--)
            *out++ = *in++ ^ MakeByte(x, y, s);
    x_ = x;
    y_ = y;
}


}  // namespace
