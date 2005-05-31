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
template AllocatorWithCleanup<unsigned char>::pointer StdReallocate<unsigned char, AllocatorWithCleanup<unsigned char> >(AllocatorWithCleanup<unsigned char>&, unsigned char*, AllocatorWithCleanup<unsigned char>::size_type, AllocatorWithCleanup<unsigned char>::size_type, bool);
template AllocatorWithCleanup<unsigned int>::pointer StdReallocate<unsigned int, AllocatorWithCleanup<unsigned int> >(AllocatorWithCleanup<unsigned int>&, unsigned int*, AllocatorWithCleanup<unsigned int>::size_type, AllocatorWithCleanup<unsigned int>::size_type, bool);
template class RSA_Decryptor<RSA_BlockType2>;
template class RSA_Encryptor<RSA_BlockType1>;
template class RSA_Encryptor<RSA_BlockType2>;
}

namespace mySTL {
template vector<TaoCrypt::Integer>* uninit_fill_n<vector<TaoCrypt::Integer>*, unsigned int, vector<TaoCrypt::Integer> >(vector<TaoCrypt::Integer>*, unsigned int, vector<TaoCrypt::Integer> const&);
template void destroy<vector<TaoCrypt::Integer>*>(vector<TaoCrypt::Integer>*, vector<TaoCrypt::Integer>*);
template TaoCrypt::Integer* uninit_copy<TaoCrypt::Integer*, TaoCrypt::Integer*>(TaoCrypt::Integer*, TaoCrypt::Integer*, TaoCrypt::Integer*);
template TaoCrypt::Integer* uninit_fill_n<TaoCrypt::Integer*, unsigned int, TaoCrypt::Integer>(TaoCrypt::Integer*, unsigned int, TaoCrypt::Integer const&);
template void destroy<TaoCrypt::Integer*>(TaoCrypt::Integer*, TaoCrypt::Integer*);
}

template void TaoCrypt::tcDelete<TaoCrypt::HASH>(TaoCrypt::HASH*);
template void TaoCrypt::tcArrayDelete<unsigned>(unsigned*);
template void TaoCrypt::tcArrayDelete<unsigned char>(unsigned char*);
template void TaoCrypt::tcArrayDelete<char>(char*);
#endif
