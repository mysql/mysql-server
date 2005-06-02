#include "runtime.hpp"
#include "handshake.hpp"
#include "yassl_int.hpp"
#include "crypto_wrapper.hpp"
#include "hmac.hpp"
#include "md5.hpp"
#include "sha.hpp"
#include "ripemd.hpp"
#include "openssl/ssl.h"

#ifdef __GNUC__
#if !defined(USE_CRYPTOPP_LIB)
namespace TaoCrypt {
template class HMAC<MD5>;
template class HMAC<SHA>;
template class HMAC<RIPEMD160>;
}
#endif

namespace mySTL {
template class mySTL::list<unsigned char*>;
template yaSSL::del_ptr_zero mySTL::for_each(mySTL::list<unsigned char*>::iterator, mySTL::list<unsigned char*>::iterator, yaSSL::del_ptr_zero);
template mySTL::pair<int, yaSSL::Message* (*)()>* mySTL::uninit_copy<mySTL::pair<int, yaSSL::Message* (*)()>*, mySTL::pair<int, yaSSL::Message* (*)()>*>(mySTL::pair<int, yaSSL::Message* (*)()>*, mySTL::pair<int, yaSSL::Message* (*)()>*, mySTL::pair<int, yaSSL::Message* (*)()>*);
template mySTL::pair<int, yaSSL::HandShakeBase* (*)()>* mySTL::uninit_copy<mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*, mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*>(mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*, mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*, mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*);
template void mySTL::destroy<mySTL::pair<int, yaSSL::Message* (*)()>*>(mySTL::pair<int, yaSSL::Message* (*)()>*, mySTL::pair<int, yaSSL::Message* (*)()>*);
template void mySTL::destroy<mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*>(mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*, mySTL::pair<int, yaSSL::HandShakeBase* (*)()>*);
template mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>* mySTL::uninit_copy<mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*>(mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*);
template void mySTL::destroy<mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*>(mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ServerKeyBase* (*)()>*);
template mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>* mySTL::uninit_copy<mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*>(mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*);
template class mySTL::list<TaoCrypt::Signer*>;
template class mySTL::list<yaSSL::SSL_SESSION*>;
template class mySTL::list<yaSSL::input_buffer*>;
template class mySTL::list<yaSSL::output_buffer*>;
template class mySTL::list<yaSSL::x509*>;
template void mySTL::destroy<mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*>(mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*, mySTL::pair<int, yaSSL::ClientKeyBase* (*)()>*);
template yaSSL::del_ptr_zero mySTL::for_each<mySTL::list<TaoCrypt::Signer*>::iterator, yaSSL::del_ptr_zero>(mySTL::list<TaoCrypt::Signer*>::iterator, mySTL::list<TaoCrypt::Signer*>::iterator, yaSSL::del_ptr_zero);
template yaSSL::del_ptr_zero mySTL::for_each<mySTL::list<yaSSL::SSL_SESSION*>::iterator, yaSSL::del_ptr_zero>(mySTL::list<yaSSL::SSL_SESSION*>::iterator, mySTL::list<yaSSL::SSL_SESSION*>::iterator, yaSSL::del_ptr_zero);
template yaSSL::del_ptr_zero mySTL::for_each<mySTL::list<yaSSL::input_buffer*>::iterator, yaSSL::del_ptr_zero>(mySTL::list<yaSSL::input_buffer*>::iterator, mySTL::list<yaSSL::input_buffer*>::iterator, yaSSL::del_ptr_zero);
template yaSSL::del_ptr_zero mySTL::for_each<mySTL::list<yaSSL::output_buffer*>::iterator, yaSSL::del_ptr_zero>(mySTL::list<yaSSL::output_buffer*>::iterator, mySTL::list<yaSSL::output_buffer*>::iterator, yaSSL::del_ptr_zero);
template yaSSL::del_ptr_zero mySTL::for_each<mySTL::list<yaSSL::x509*>::iterator, yaSSL::del_ptr_zero>(mySTL::list<yaSSL::x509*>::iterator, mySTL::list<yaSSL::x509*>::iterator, yaSSL::del_ptr_zero);
}

namespace yaSSL {
template void ysDelete<SSL_CTX>(yaSSL::SSL_CTX*);
template void ysDelete<SSL>(yaSSL::SSL*);
template void ysDelete<BIGNUM>(yaSSL::BIGNUM*);
template void ysDelete<unsigned char>(unsigned char*);
template void ysDelete<DH>(yaSSL::DH*);
template void ysDelete<TaoCrypt::Signer>(TaoCrypt::Signer*);
template void ysDelete<SSL_SESSION>(yaSSL::SSL_SESSION*);
template void ysDelete<input_buffer>(input_buffer*);
template void ysDelete<output_buffer>(output_buffer*);
template void ysDelete<x509>(x509*);
template void ysDelete<Auth>(Auth*);
template void ysDelete<HandShakeBase>(HandShakeBase*);
template void ysDelete<ServerKeyBase>(ServerKeyBase*);
template void ysDelete<ClientKeyBase>(ClientKeyBase*);
template void ysDelete<SSL_METHOD>(SSL_METHOD*);
template void ysDelete<DiffieHellman>(DiffieHellman*);
template void ysDelete<BulkCipher>(BulkCipher*);
template void ysDelete<Digest>(Digest*);
template void ysDelete<X509>(X509*);
template void ysDelete<Message>(Message*);
template void ysArrayDelete<unsigned char>(unsigned char*);
template void ysArrayDelete<char>(char*);
}
#endif
