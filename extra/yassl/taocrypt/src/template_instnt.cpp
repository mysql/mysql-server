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


/*  Explicit template instantiation requests 
 */


#include "runtime.hpp"
#include "integer.hpp"
#include "rsa.hpp"
#include "sha.hpp"
#include "md5.hpp"
#include "hmac.hpp"
#include "ripemd.hpp"
#include "pwdbased.hpp"
#include "algebra.hpp"
#include "vector.hpp"
#include "hash.hpp"

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
namespace TaoCrypt {

#if defined(SSE2_INTRINSICS_AVAILABLE)
template AlignedAllocator<unsigned int>::pointer StdReallocate<unsigned int, AlignedAllocator<unsigned int> >(AlignedAllocator<unsigned int>&, unsigned int*, AlignedAllocator<unsigned int>::size_type, AlignedAllocator<unsigned int>::size_type, bool);
#endif

template class RSA_Decryptor<RSA_BlockType2>;
template class RSA_Encryptor<RSA_BlockType1>;
template class RSA_Encryptor<RSA_BlockType2>;
template void tcDelete<HASH>(HASH*);
template void tcDelete<Integer>(Integer*);
template void tcArrayDelete<byte>(byte*);
template AllocatorWithCleanup<byte>::pointer StdReallocate<byte, AllocatorWithCleanup<byte> >(AllocatorWithCleanup<byte>&, byte*, AllocatorWithCleanup<byte>::size_type, AllocatorWithCleanup<byte>::size_type, bool);
template void tcArrayDelete<word>(word*);
template AllocatorWithCleanup<word>::pointer StdReallocate<word, AllocatorWithCleanup<word> >(AllocatorWithCleanup<word>&, word*, AllocatorWithCleanup<word>::size_type, AllocatorWithCleanup<word>::size_type, bool);

#ifndef TAOCRYPT_SLOW_WORD64 // defined when word != word32
template void tcArrayDelete<word32>(word32*);
template AllocatorWithCleanup<word32>::pointer StdReallocate<word32, AllocatorWithCleanup<word32> >(AllocatorWithCleanup<word32>&, word32*, AllocatorWithCleanup<word32>::size_type, AllocatorWithCleanup<word32>::size_type, bool);
#endif

template void tcArrayDelete<char>(char*);

template class PBKDF2_HMAC<SHA>;
template class HMAC<MD5>;
template class HMAC<SHA>;
template class HMAC<RIPEMD160>;
}

namespace mySTL {
template vector<TaoCrypt::Integer>* uninit_fill_n<vector<TaoCrypt::Integer>*, size_t, vector<TaoCrypt::Integer> >(vector<TaoCrypt::Integer>*, size_t, vector<TaoCrypt::Integer> const&);
template void destroy<vector<TaoCrypt::Integer>*>(vector<TaoCrypt::Integer>*, vector<TaoCrypt::Integer>*);
template TaoCrypt::Integer* uninit_copy<TaoCrypt::Integer*, TaoCrypt::Integer*>(TaoCrypt::Integer*, TaoCrypt::Integer*, TaoCrypt::Integer*);
template TaoCrypt::Integer* uninit_fill_n<TaoCrypt::Integer*, size_t, TaoCrypt::Integer>(TaoCrypt::Integer*, size_t, TaoCrypt::Integer const&);
template void destroy<TaoCrypt::Integer*>(TaoCrypt::Integer*, TaoCrypt::Integer*);
template TaoCrypt::byte* GetArrayMemory<TaoCrypt::byte>(size_t);
template void FreeArrayMemory<TaoCrypt::byte>(TaoCrypt::byte*);
template TaoCrypt::Integer* GetArrayMemory<TaoCrypt::Integer>(size_t);
template void FreeArrayMemory<TaoCrypt::Integer>(TaoCrypt::Integer*);
template vector<TaoCrypt::Integer>* GetArrayMemory<vector<TaoCrypt::Integer> >(size_t);
template void FreeArrayMemory<vector<TaoCrypt::Integer> >(vector<TaoCrypt::Integer>*);
template void FreeArrayMemory<void>(void*);
}

#endif
