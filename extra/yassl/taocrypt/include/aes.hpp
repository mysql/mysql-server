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

/* aes.hpp defines AES
*/


#ifndef TAO_CRYPT_AES_HPP
#define TAO_CRYPT_AES_HPP

#include "misc.hpp"
#include "modes.hpp"


#if defined(TAOCRYPT_X86ASM_AVAILABLE) && defined(TAO_ASM)
    #define DO_AES_ASM
#endif



namespace TaoCrypt {


enum { AES_BLOCK_SIZE = 16 };


// AES encryption and decryption, see FIPS-197
class AES : public Mode_BASE {
public:
    enum { BLOCK_SIZE = AES_BLOCK_SIZE };

    AES(CipherDir DIR, Mode MODE)
        : Mode_BASE(BLOCK_SIZE, DIR, MODE) {}

#ifdef DO_AES_ASM
    void Process(byte*, const byte*, word32);
#endif
    void SetKey(const byte* key, word32 sz, CipherDir fake = ENCRYPTION);
    void SetIV(const byte* iv) { memcpy(r_, iv, BLOCK_SIZE); }
private:
    static const word32 rcon_[];

    word32      rounds_;
    word32      key_[60];                        // max size

    static const word32 Te[5][256];
    static const word32 Td[5][256];

    static const word32* Te0;
    static const word32* Te1;
    static const word32* Te2;
    static const word32* Te3;
    static const word32* Te4;

    static const word32* Td0;
    static const word32* Td1;
    static const word32* Td2;
    static const word32* Td3;
    static const word32* Td4;

    void encrypt(const byte*, const byte*, byte*) const;
    void AsmEncrypt(const byte*, byte*, void*) const;
    void decrypt(const byte*, const byte*, byte*) const;
    void AsmDecrypt(const byte*, byte*, void*) const;

    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;

    AES(const AES&);            // hide copy
    AES& operator=(const AES&); // and assign
};


typedef BlockCipher<ENCRYPTION, AES, ECB> AES_ECB_Encryption;
typedef BlockCipher<DECRYPTION, AES, ECB> AES_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, AES, CBC> AES_CBC_Encryption;
typedef BlockCipher<DECRYPTION, AES, CBC> AES_CBC_Decryption;



} // naemspace

#endif // TAO_CRYPT_AES_HPP
