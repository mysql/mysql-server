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

/* modes.hpp provides ECB and CBC modes for block cipher encryption/decryption
*/


#ifndef TAO_CRYPT_MODES_HPP
#define TAO_CRYPT_MODES_HPP

#include "misc.hpp"

namespace TaoCrypt {


enum Mode { ECB, CBC };



// BlockCipher abstraction
template<CipherDir DIR, class T, Mode MODE>
class BlockCipher {
public:
    BlockCipher() : cipher_(DIR, MODE) {}

    void Process(byte* c, const byte* p, word32 sz) 
            { cipher_.Process(c, p, sz); }
    void SetKey(const byte* k, word32 sz)   
            { cipher_.SetKey(k, sz, DIR); }
    void SetKey(const byte* k, word32 sz, const byte* iv)   
            { cipher_.SetKey(k, sz, DIR); cipher_.SetIV(iv); }
private:
    T cipher_;

    BlockCipher(const BlockCipher&);            // hide copy
    BlockCipher& operator=(const BlockCipher&); // and assign
};


// Mode Base for block ciphers, static size
class Mode_BASE : public virtual_base {
public:
    enum { MaxBlockSz = 16 };

    explicit Mode_BASE(int sz, CipherDir dir, Mode mode) 
        : blockSz_(sz), reg_(reinterpret_cast<byte*>(r_)),
          tmp_(reinterpret_cast<byte*>(t_)), dir_(dir), mode_(mode)
    {}
    virtual ~Mode_BASE() {}

    virtual void Process(byte*, const byte*, word32);

    void SetIV(const byte* iv) { memcpy(reg_, iv, blockSz_); }
protected:
    int   blockSz_;
    byte* reg_;
    byte* tmp_;

    word32 r_[MaxBlockSz / sizeof(word32)];  // align reg_ on word32
    word32 t_[MaxBlockSz / sizeof(word32)];  // align tmp_ on word32

    CipherDir dir_;
    Mode      mode_;

    void ECB_Process(byte*, const byte*, word32);
    void CBC_Encrypt(byte*, const byte*, word32);
    void CBC_Decrypt(byte*, const byte*, word32);

    Mode_BASE(const Mode_BASE&);            // hide copy
    Mode_BASE& operator=(const Mode_BASE&); // and assign

private:
    virtual void ProcessAndXorBlock(const byte*, const byte*, byte*) const = 0;
};


inline void Mode_BASE::Process(byte* out, const byte* in, word32 sz)
{
    if (mode_ == ECB)
        ECB_Process(out, in, sz);
    else if (mode_ == CBC) {
        if (dir_ == ENCRYPTION)
            CBC_Encrypt(out, in, sz);
        else
            CBC_Decrypt(out, in, sz);
    }
}


// ECB Process blocks
inline void Mode_BASE::ECB_Process(byte* out, const byte* in, word32 sz)
{
    word32 blocks = sz / blockSz_;

    while (blocks--) {
        ProcessAndXorBlock(in, 0, out);
        out += blockSz_;
        in  += blockSz_;
    }
}


// CBC Encrypt
inline void Mode_BASE::CBC_Encrypt(byte* out, const byte* in, word32 sz)
{
    word32 blocks = sz / blockSz_;

    while (blocks--) {
        xorbuf(reg_, in, blockSz_);
        ProcessAndXorBlock(reg_, 0, reg_);
        memcpy(out, reg_, blockSz_);
        out += blockSz_;
        in  += blockSz_;
    }
}


// CBC Decrypt
inline void Mode_BASE::CBC_Decrypt(byte* out, const byte* in, word32 sz)
{
    word32 blocks = sz / blockSz_;
    byte   hold[MaxBlockSz];

    while (blocks--) {
        memcpy(tmp_, in, blockSz_);
        ProcessAndXorBlock(tmp_, 0, out);
        xorbuf(out,  reg_, blockSz_);
        memcpy(hold, reg_,   blockSz_); // swap reg_ and tmp_
        memcpy(reg_,   tmp_, blockSz_);
        memcpy(tmp_, hold, blockSz_);
        out += blockSz_;
        in  += blockSz_;
    }
}


} // namespace

#endif  // TAO_CRYPT_MODES_HPP
