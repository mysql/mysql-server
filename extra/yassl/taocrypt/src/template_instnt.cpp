#include "integer.hpp"
#include "rsa.hpp"
#include "algebra.hpp"
#include "vector.hpp"
#include "hash.hpp"

#ifdef __GNUC__
namespace TaoCrypt {
#if defined(SSE2_INTRINSICS_AVAILABLE)
template AlignedAllocator<unsigned int>::pointer StdReallocate<unsigned int, AlignedAllocator<unsigned int> >(AlignedAllocator<unsigned int>&, unsigned int*, AlignedAllocator<unsigned int>::size_type, AlignedAllocator<unsigned int>::size_type, bool);
#endif
template class RSA_Decryptor<RSA_BlockType2>;
template class RSA_Encryptor<RSA_BlockType1>;
template class RSA_Encryptor<RSA_BlockType2>;
template void tcDelete<HASH>(HASH*);
template void tcArrayDelete<byte>(byte*);
template AllocatorWithCleanup<byte>::pointer StdReallocate<byte, AllocatorWithCleanup<byte> >(AllocatorWithCleanup<byte>&, byte*, AllocatorWithCleanup<byte>::size_type, AllocatorWithCleanup<byte>::size_type, bool);
template void tcArrayDelete<word>(word*);
template AllocatorWithCleanup<word>::pointer StdReallocate<word, AllocatorWithCleanup<word> >(AllocatorWithCleanup<word>&, word*, AllocatorWithCleanup<word>::size_type, AllocatorWithCleanup<word>::size_type, bool);
#ifndef TAOCRYPT_SLOW_WORD64 // defined when word != word32
template void tcArrayDelete<word32>(word32*);
template AllocatorWithCleanup<word32>::pointer StdReallocate<word32, AllocatorWithCleanup<word32> >(AllocatorWithCleanup<word32>&, word32*, AllocatorWithCleanup<word32>::size_type, AllocatorWithCleanup<word32>::size_type, bool);
#endif
template void tcArrayDelete<char>(char*);
}

namespace mySTL {
template vector<TaoCrypt::Integer>* uninit_fill_n<vector<TaoCrypt::Integer>*, size_t, vector<TaoCrypt::Integer> >(vector<TaoCrypt::Integer>*, size_t, vector<TaoCrypt::Integer> const&);
template void destroy<vector<TaoCrypt::Integer>*>(vector<TaoCrypt::Integer>*, vector<TaoCrypt::Integer>*);
template TaoCrypt::Integer* uninit_copy<TaoCrypt::Integer*, TaoCrypt::Integer*>(TaoCrypt::Integer*, TaoCrypt::Integer*, TaoCrypt::Integer*);
template TaoCrypt::Integer* uninit_fill_n<TaoCrypt::Integer*, size_t, TaoCrypt::Integer>(TaoCrypt::Integer*, size_t, TaoCrypt::Integer const&);
template void destroy<TaoCrypt::Integer*>(TaoCrypt::Integer*, TaoCrypt::Integer*);
}

#endif
