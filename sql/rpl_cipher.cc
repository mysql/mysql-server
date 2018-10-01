/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/rpl_cipher.h"
#include <algorithm>
#include "my_byteorder.h"

int Rpl_cipher::get_header_size() { return m_header_size; }

template <Cipher_type TYPE>
bool Aes_ctr_cipher<TYPE>::open(const Key_string &password, int header_size) {
  m_header_size = header_size;
  if (EVP_BytesToKey(EVP_aes_256_ctr(), EVP_sha512(), NULL, password.data(),
                     password.length(), 1, m_file_key, m_iv) == 0)
    return true;

  /*
    AES-CTR counter is set to 0. Data stream is always encrypted beginning with
    counter 0.
  */
  return init_cipher(0);
}

template <Cipher_type TYPE>
Aes_ctr_cipher<TYPE>::~Aes_ctr_cipher<TYPE>() {
  close();
}

template <Cipher_type TYPE>
void Aes_ctr_cipher<TYPE>::close() {
  deinit_cipher();
}

template <Cipher_type TYPE>
bool Aes_ctr_cipher<TYPE>::set_stream_offset(uint64_t offset) {
  unsigned char buffer[AES_BLOCK_SIZE];
  /* A seek in the down stream would overflow the offset */
  if (offset > UINT64_MAX - m_header_size) return true;

  deinit_cipher();
  if (init_cipher(offset)) return true;
  /*
    The cipher works with blocks. While init_cipher() above is called it will
    initialize the cipher assuming it is pointing to the beginning of a block,
    the following encrypt/decrypt operations will adjust the cipher to point to
    the requested offset in the block, so next encrypt/decrypt operations will
    work fine without the need to take care of reading from/writing to the
    middle of a block.
  */
  if (TYPE == Cipher_type::ENCRYPT)
    return encrypt(buffer, buffer, offset % AES_BLOCK_SIZE);
  else
    return decrypt(buffer, buffer, offset % AES_BLOCK_SIZE);
}

template <Cipher_type TYPE>
bool Aes_ctr_cipher<TYPE>::init_cipher(uint64_t offset) {
  DBUG_ENTER(" Aes_ctr_cipher::init_cipher");

  uint64_t counter = offset / AES_BLOCK_SIZE;

  DBUG_ASSERT(m_ctx == nullptr);
  m_ctx = EVP_CIPHER_CTX_new();
  if (m_ctx == nullptr) DBUG_RETURN(true);

  /*
    AES's IV is 16 bytes.
    In CTR mode, we will use the last 8 bytes as the counter.
    Counter is stored in big-endian.
  */
  int8store(m_iv + 8, counter);
  /* int8store stores it in little-endian, so swap it to big-endian */
  std::swap(m_iv[8], m_iv[15]);
  std::swap(m_iv[9], m_iv[14]);
  std::swap(m_iv[10], m_iv[13]);
  std::swap(m_iv[11], m_iv[12]);

  int res;
  /* EVP_EncryptInit_ex() return 1 for success and 0 for failure */
  if (TYPE == Cipher_type::ENCRYPT)
    res = EVP_EncryptInit_ex(m_ctx, EVP_aes_256_ctr(), NULL, m_file_key, m_iv);
  else
    res = EVP_DecryptInit_ex(m_ctx, EVP_aes_256_ctr(), NULL, m_file_key, m_iv);
  DBUG_RETURN(res == 0);
}

template <Cipher_type TYPE>
void Aes_ctr_cipher<TYPE>::deinit_cipher() {
  if (m_ctx) EVP_CIPHER_CTX_free(m_ctx);
  m_ctx = nullptr;
}

template <Cipher_type TYPE>
bool Aes_ctr_cipher<TYPE>::encrypt(unsigned char *dest,
                                   const unsigned char *src, int length) {
  int out_len = 0;

  if (TYPE == Cipher_type::DECRYPT) {
    /* It should never be called by a decrypt cipher */
    DBUG_ASSERT(0);
    return true;
  }

  if (EVP_EncryptUpdate(m_ctx, dest, &out_len, src, length) == 0) return true;

  DBUG_ASSERT(out_len == length);
  return false;
}

template <Cipher_type TYPE>
bool Aes_ctr_cipher<TYPE>::decrypt(unsigned char *dest,
                                   const unsigned char *src, int length) {
  int out_len = 0;

  if (TYPE == Cipher_type::ENCRYPT) {
    /* It should never be called by an encrypt cipher */
    DBUG_ASSERT(0);
    return true;
  }

  if (EVP_DecryptUpdate(m_ctx, dest, &out_len, src, length) == 0) return true;
  DBUG_ASSERT(out_len == length);
  return false;
}

template class Aes_ctr_cipher<Cipher_type::ENCRYPT>;
template class Aes_ctr_cipher<Cipher_type::DECRYPT>;
