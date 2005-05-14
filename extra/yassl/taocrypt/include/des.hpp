/* des.hpp                                
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

/* des.hpp defines DES, DES_EDE2, and DES_EDE3
   see FIPS 46-2 and FIPS 81
*/


#ifndef TAO_CRYPT_DES_HPP
#define TAO_CRYPT_DES_HPP

#include <string.h>
#include "misc.hpp"
#include "modes.hpp"

namespace TaoCrypt {

enum { DES_BLOCK_SIZE = 8 };

// Base for all DES types
class DES_BASE : public Mode_BASE<DES_BLOCK_SIZE> {
public:
    enum { BLOCK_SIZE = DES_BLOCK_SIZE, KEY_SIZE = 32, BOXES = 8,
           BOX_SIZE = 64 };

    DES_BASE(CipherDir DIR, Mode MODE) : dir_(DIR), mode_(MODE) {}

    void Process(byte*, const byte*, word32);
protected:
    CipherDir dir_;
    Mode      mode_;
private:
    DES_BASE(const DES_BASE&);              // hide copy
    DES_BASE& operator=(const DES_BASE&);   // and assign
};


// DES 
class DES : public DES_BASE {
public:
    DES(CipherDir DIR, Mode MODE) : DES_BASE(DIR, MODE) {}

    void SetKey(const byte*, word32, CipherDir dir);
    void RawProcessBlock(word32&, word32&) const;
    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;
private:
    word32 k_[KEY_SIZE];
};


// DES_EDE2
class DES_EDE2 : public DES_BASE {
public:
    DES_EDE2(CipherDir DIR, Mode MODE) 
        : DES_BASE(DIR, MODE), des1_(DIR, MODE), des2_(DIR, MODE) {}

    void SetKey(const byte*, word32, CipherDir dir);
    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;
private:
    DES des1_;
    DES des2_;
};


// DES_EDE3
class DES_EDE3 : public DES_BASE {
public:
    DES_EDE3(CipherDir DIR, Mode MODE) 
        : DES_BASE(DIR, MODE), des1_(DIR, MODE), des2_(DIR, MODE),
                               des3_(DIR, MODE) {}

    void SetKey(const byte*, word32, CipherDir dir);
    void ProcessAndXorBlock(const byte*, const byte*, byte*) const;
private:
    DES des1_;
    DES des2_;
    DES des3_;
};


typedef BlockCipher<ENCRYPTION, DES, ECB> DES_ECB_Encryption;
typedef BlockCipher<DECRYPTION, DES, ECB> DES_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, DES, CBC> DES_CBC_Encryption;
typedef BlockCipher<DECRYPTION, DES, CBC> DES_CBC_Decryption;

typedef BlockCipher<ENCRYPTION, DES_EDE2, ECB> DES_EDE2_ECB_Encryption;
typedef BlockCipher<DECRYPTION, DES_EDE2, ECB> DES_EDE2_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, DES_EDE2, CBC> DES_EDE2_CBC_Encryption;
typedef BlockCipher<DECRYPTION, DES_EDE2, CBC> DES_EDE2_CBC_Decryption;

typedef BlockCipher<ENCRYPTION, DES_EDE3, ECB> DES_EDE3_ECB_Encryption;
typedef BlockCipher<DECRYPTION, DES_EDE3, ECB> DES_EDE3_ECB_Decryption;

typedef BlockCipher<ENCRYPTION, DES_EDE3, CBC> DES_EDE3_CBC_Encryption;
typedef BlockCipher<DECRYPTION, DES_EDE3, CBC> DES_EDE3_CBC_Decryption;


} // namespace


#endif // TAO_CRYPT_DES_HPP
