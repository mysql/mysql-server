/*
   Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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


/* random.cpp implements a crypto secure Random Number Generator using an OS
   specific seed, switch to /dev/random for more security but may block
*/

#include "runtime.hpp"
#include "random.hpp"
#include <string.h>
#include <time.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <wincrypt.h>
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif // _WIN32

namespace TaoCrypt {


// Get seed and key cipher
RandomNumberGenerator::RandomNumberGenerator()
{
    byte key[32];
    byte junk[256];

    seed_.GenerateSeed(key, sizeof(key));
    cipher_.SetKey(key, sizeof(key));
    GenerateBlock(junk, sizeof(junk));  // rid initial state
}


// place a generated block in output
void RandomNumberGenerator::GenerateBlock(byte* output, word32 sz)
{
    memset(output, 0, sz);
    cipher_.Process(output, output, sz);
}


byte RandomNumberGenerator::GenerateByte()
{
    byte b;
    GenerateBlock(&b, 1);

    return b;
}


#if defined(_WIN32)

/* The OS_Seed implementation for windows */

OS_Seed::OS_Seed()
{
    if(!CryptAcquireContext(&handle_, 0, 0, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT))
        error_.SetError(WINCRYPT_E);
}


OS_Seed::~OS_Seed()
{
    CryptReleaseContext(handle_, 0);
}


void OS_Seed::GenerateSeed(byte* output, word32 sz)
{
    if (!CryptGenRandom(handle_, sz, output))
        error_.SetError(CRYPTGEN_E);
}


#else

/* The default OS_Seed implementation */

OS_Seed::OS_Seed()
{
    fd_ = open("/dev/urandom",O_RDONLY);
    if (fd_ == -1) {
        fd_ = open("/dev/random",O_RDONLY);
        if (fd_ == -1)
            error_.SetError(OPEN_RAN_E);
    }
}


OS_Seed::~OS_Seed() 
{
    close(fd_);
}


// may block
void OS_Seed::GenerateSeed(byte* output, word32 sz)
{
    while (sz) {
        int len = read(fd_, output, sz);
        if (len == -1) {
            error_.SetError(READ_RAN_E);
            return;
        }

        sz     -= len;
        output += len;

        if (sz)
            sleep(1);
    }
}

#endif // _WIN32



} // namespace
