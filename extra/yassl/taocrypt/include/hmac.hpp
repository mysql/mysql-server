/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

    HMAC() : ipad_(reinterpret_cast<byte*>(&ip_)), 
             opad_(reinterpret_cast<byte*>(&op_)),
             innerHash_(reinterpret_cast<byte*>(&innerH_)) 
    { 
        Init(); 
    }
    void Update(const byte*, word32);
    void Final(byte*);
    void Init();

    void SetKey(const byte*, word32);
private:
    byte* ipad_;
    byte* opad_;
    byte* innerHash_;
    bool  innerHashKeyed_;
    T     mac_;

    // MSVC 6 HACK, gives compiler error if calculated in array
    enum { HMAC_BSIZE = T::BLOCK_SIZE  / sizeof(word32),
           HMAC_DSIZE = T::DIGEST_SIZE / sizeof(word32) };

    word32 ip_[HMAC_BSIZE];          // align ipad_ on word32
    word32 op_[HMAC_BSIZE];          // align opad_ on word32
    word32 innerH_[HMAC_DSIZE];      // align innerHash_ on word32

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
