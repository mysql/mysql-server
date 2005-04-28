/* hmac.hpp                                
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

/* hamc.hpp implements HMAC, see RFC 2104
*/


#ifndef TAO_CRYPT_HMAC_HPP
#define TAO_CRYPT_HMAC_HPP

#include "hash.hpp"

namespace TaoCrypt {


// HMAC class template
template <class T>
class HMAC {
public:
    enum { IPAD = 0x36, OPAD = 0x5C };

    HMAC() { Init(); }
    void Update(const byte*, word32);
    void Final(byte*);
    void Init();

    void SetKey(const byte*, word32);
private:
    byte ipad_[T::BLOCK_SIZE];
    byte opad_[T::BLOCK_SIZE];
    byte innerHash_[T::DIGEST_SIZE];
    bool innerHashKeyed_;
    T    mac_;

    void KeyInnerHash();

    HMAC(const HMAC&);
    HMAC& operator= (const HMAC&);
};


// Setup
template <class T>
void HMAC<T>::Init()
{
    mac_.Init();
    innerHashKeyed_ = false;
}


// Key generation
template <class T>
void HMAC<T>::SetKey(const byte* key, word32 length)
{
    Init();

    if (length <= T::BLOCK_SIZE)
        memcpy(ipad_, key, length);
    else {
        mac_.Update(key, length);
        mac_.Final(ipad_);
        length = T::DIGEST_SIZE;
    }
    memset(ipad_ + length, 0, T::BLOCK_SIZE - length);

    for (word32 i = 0; i < T::BLOCK_SIZE; i++) {
        opad_[i] = ipad_[i] ^ OPAD;
        ipad_[i] ^= IPAD;
    }
}


// Inner Key Hash
template <class T>
void HMAC<T>::KeyInnerHash()
{
    mac_.Update(ipad_, T::BLOCK_SIZE);
    innerHashKeyed_ = true;
}


// Update
template <class T>
void HMAC<T>::Update(const byte* msg, word32 length)
{
    if (!innerHashKeyed_)
        KeyInnerHash();
    mac_.Update(msg, length);
}


// Final
template <class T>
void HMAC<T>::Final(byte* hash)
{
    if (!innerHashKeyed_)
        KeyInnerHash();
    mac_.Final(innerHash_);

    mac_.Update(opad_, T::BLOCK_SIZE);
    mac_.Update(innerHash_, T::DIGEST_SIZE);
    mac_.Final(hash);

    innerHashKeyed_ = false;
}


} // namespace

#endif // TAO_CRYPT_HMAC_HPP
