/*
   Copyright (c) 2006, 2012, Oracle and/or its affiliates

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

/* blowfish.hpp defines Blowfish
*/


#ifndef TAO_CRYPT_BLOWFISH_HPP
#define TAO_CRYPT_BLOWFISH_HPP

#include "misc.hpp"
#include "modes.hpp"
#ifdef USE_SYS_STL
    #include <algorithm>
#else
    #include "algorithm.hpp"
#endif


namespace STL = STL_NAMESPACE;


#if defined(TAOCRYPT_X86ASM_AVAILABLE) && defined(TAO_ASM)
    #define DO_BLOWFISH_ASM
#endif


namespace TaoCrypt {

enum { BLOWFISH_BLOCK_SIZE = 8 };


// Blowfish encryption and decryption, see 
class Blowfish : public Mode_BASE {
public:
    enum { BLOCK_SIZE = BLOWFISH_BLOCK_SIZE, ROUNDS = 16 };

    Blowfish(CipherDir DIR, Mode MODE)
        : Mode_BASE(BLOCK_SIZE, DIR, MODE), sbox_(pbox_ + ROUNDS + 2) {}

#ifdef DO_BLOWFISH_ASM
    void Process(byte*, const byte*, word32);
#endif
    void SetKey(const byte* key, word32 sz, CipherDir fake = ENCRYPTION);
    void SetIV(const byte* iv) { memcpy(r_, iv, BLOCK_SIZE); }
private:
    static const word32 p_init_[ROUNDS + 2];
    static const word32 s_init_[4 * 256];

    word32 pbox_[ROUNDS + 2 + 4 * 256];
    word32* sbox_;

    void crypt_block(const word32 in[2], word32 out[2]) const;
    void AsmProcess(const byte* in, byte* out) const;
    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;

    Blowfish(const Blowfish&);            // hide copy
    Blowfish& operator=(const Blowfish&); // and assign
};


typedef BlockCipher<ENCRYPTION, Blowfish, ECB> Blowfish_ECB_Encryption;
typedef BlockCipher<DECRYPTION, Blowfish, ECB> Blowfish_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, Blowfish, CBC> Blowfish_CBC_Encryption;
typedef BlockCipher<DECRYPTION, Blowfish, CBC> Blowfish_CBC_Decryption;



} // namespace

#endif // TAO_CRYPT_BLOWFISH_HPP

