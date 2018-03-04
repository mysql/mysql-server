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

/* random.hpp provides a crypto secure Random Number Generator using an OS
   specific seed
*/


#ifndef TAO_CRYPT_RANDOM_HPP
#define TAO_CRYPT_RANDOM_HPP

#include "arc4.hpp"
#include "error.hpp"

namespace TaoCrypt {


// OS specific seeder
class OS_Seed {
public:
    OS_Seed();
    ~OS_Seed();

    void   GenerateSeed(byte*, word32 sz);
    Error  GetError() const { return error_; }
private:
#if defined(_WIN32)
    #if defined(_WIN64)
        typedef unsigned __int64 ProviderHandle;
        // type HCRYPTPROV, avoid #include <windows.h>
    #else
        typedef unsigned long ProviderHandle;
    #endif
    ProviderHandle handle_;
#else
    int fd_;
#endif
    Error error_;

    OS_Seed(const OS_Seed&);              // hide copy
    OS_Seed& operator=(const OS_Seed&);   // hide assign
};


// secure Random Nnumber Generator
class RandomNumberGenerator {
public:
    RandomNumberGenerator();
    ~RandomNumberGenerator() {}

    void GenerateBlock(byte*, word32 sz);
    byte GenerateByte();

    ErrorNumber GetError() const { return seed_.GetError().What(); }
private:
    OS_Seed seed_;
    ARC4    cipher_;

    RandomNumberGenerator(const RandomNumberGenerator&);           // hide copy
    RandomNumberGenerator operator=(const RandomNumberGenerator&); // && assign
};




}  // namespace

#endif // TAO_CRYPT_RANDOM_HPP

