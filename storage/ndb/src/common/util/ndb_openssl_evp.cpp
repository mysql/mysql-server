/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "util/ndb_openssl_evp.h"

#include <assert.h> // assert()
#include <stdlib.h> // abort()
#include <string.h>

#include <new>

#include "openssl/conf.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/rand.h"
#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include "portlib/NdbThread.h"
#include "portlib/NdbMutex.h"
#include "openssl/engine.h"
#endif

#define RETURN(rv) return(rv)
//#define RETURN(rv) abort()

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static unsigned long ndb_openssl_id()
{
  NdbThread* thread = NdbThread_GetNdbThread();
  if (sizeof(unsigned long) >= sizeof(uintptr_t))
  {
    return (uintptr_t)thread;
  }
  else
  {
    int id = NdbThread_GetTid(thread);
    require(id != -1);
    return (unsigned)id;
  }
}

static NdbMutex* ndb_openssl_lock_array = nullptr;

static void ndb_openssl_lock(int mode, int n, const char*, int)
{
  require(n >= 0);
  require(n < CRYPTO_num_locks());

  switch (mode)
  {
    case CRYPTO_LOCK | CRYPTO_READ:
    case CRYPTO_LOCK | CRYPTO_WRITE:
      NdbMutex_Lock(&ndb_openssl_lock_array[n]);
      break;
    case CRYPTO_UNLOCK | CRYPTO_READ:
    case CRYPTO_UNLOCK | CRYPTO_WRITE:
      NdbMutex_Unlock(&ndb_openssl_lock_array[n]);
      break;
    default:
      abort();
  }
}
#endif

/*
 * OPENSSL library initialization and cleanup
 *
 * See ssl_start() and vio_ssl_end() in vio/viosslfactories.cc
 * and https://wiki.openssl.org/index.php/Library_Initialization
 */

int ndb_openssl_evp::library_init()
{
  SSL_library_init();
  OpenSSL_add_ssl_algorithms();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ERR_load_crypto_strings();
  RAND_set_rand_engine(nullptr);
#if OPENSSL_VERSION_NUMBER < 0x10100000L

  int num_locks = CRYPTO_num_locks();
  ndb_openssl_lock_array =
    (NdbMutex*)OPENSSL_malloc(num_locks * sizeof(ndb_openssl_lock_array[0]));
  for (int i = 0; i < num_locks; i++)
    NdbMutex_Init(&ndb_openssl_lock_array[i]);

  CRYPTO_set_locking_callback(&ndb_openssl_lock);
  CRYPTO_set_id_callback(&ndb_openssl_id);
#endif
  return 0;
}

int ndb_openssl_evp::library_end()
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  CRYPTO_set_locking_callback(nullptr);
  CRYPTO_set_id_callback(nullptr);

  int num_locks = CRYPTO_num_locks();
  for (int i = 0; i < num_locks; i++)
    NdbMutex_Deinit(&ndb_openssl_lock_array[i]);

  OPENSSL_free(ndb_openssl_lock_array);
  ndb_openssl_lock_array = nullptr;

  ENGINE_cleanup();
  ERR_remove_thread_state(nullptr);
#endif
  FIPS_mode_set(0);
  CONF_modules_unload(1);
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
  return 0;
}

ndb_openssl_evp::ndb_openssl_evp()
: m_evp_cipher(nullptr),
  m_padding(false),
  m_has_key_iv(false),
  m_mix_key_iv_pair(false),
  m_data_unit_size(0),
  m_key_iv_set(nullptr)
{
  memset(m_key_iv, 0, sizeof(m_key_iv));
}


ndb_openssl_evp::~ndb_openssl_evp()
{
}


int ndb_openssl_evp::reset()
{
  m_evp_cipher = nullptr;
  m_padding = false;
  m_has_key_iv = false;
  m_mix_key_iv_pair = false;
  m_data_unit_size = 0;
  return 0;
}


int ndb_openssl_evp::set_memory(void* mem, size_t size)
{
  assert(!m_has_key_iv);
  if (m_has_key_iv)
  {
    return -1;
  }

  assert(m_key_iv_set == nullptr);
  if (m_key_iv_set != nullptr)
  {
    return -1;
  }

  if (size < sizeof(*m_key_iv_set))
  {
    return -1;
  }

  m_key_iv_set = new (mem) key256_iv256_set();
  return 0;
}


int ndb_openssl_evp::set_aes_256_cbc(bool padding, size_t data_unit_size)
{
  require(m_evp_cipher == nullptr);

  require(EVP_CIPHER_key_length(EVP_aes_256_cbc()) == CBC_KEY_LEN);
  require(EVP_CIPHER_iv_length(EVP_aes_256_cbc()) == CBC_IV_LEN);
  require(EVP_CIPHER_block_size(EVP_aes_256_cbc()) == CBC_BLOCK_LEN);

  m_evp_cipher = EVP_aes_256_cbc();
  m_padding = padding;

  if (data_unit_size % CBC_BLOCK_LEN != 0)
  {
    return -1;
  }

  if (m_padding && data_unit_size > 0)
  {
    return -1;
  }

  m_data_unit_size = data_unit_size;
  m_mix_key_iv_pair = true;
  return 0;
}


int ndb_openssl_evp::set_aes_256_xts(size_t data_unit_size)
{
  require(m_evp_cipher == nullptr);
  require(m_padding == false);

  assert(data_unit_size % XTS_BLOCK_LEN == 0);
  if (data_unit_size % XTS_BLOCK_LEN != 0)
  {
    return -1;
  }

  require(EVP_CIPHER_key_length(EVP_aes_256_xts()) == XTS_KEY_LEN);
  require(EVP_CIPHER_iv_length(EVP_aes_256_xts()) == XTS_IV_LEN);
  require(EVP_CIPHER_block_size(EVP_aes_256_xts()) == XTS_BLOCK_LEN);

  m_evp_cipher = EVP_aes_256_xts();
  m_data_unit_size = data_unit_size;
  m_mix_key_iv_pair = true;
  return 0;
}


int ndb_openssl_evp::generate_salt256(byte salt[SALT_LEN])
{
  int r = RAND_bytes(salt, SALT_LEN);
  if (r != 1)
  {
    return -1;
  }
  return 0;
}


int ndb_openssl_evp::derive_and_add_key_iv_pair(const byte pwd[],
                                                size_t pwd_len,
                                                size_t iter_count,
                                                const byte salt[SALT_LEN])
{
  byte *key_iv;
  if (m_key_iv_set == nullptr)
  {
    if (m_has_key_iv)
    {
      RETURN(-1);
    }

    key_iv = m_key_iv;
  }
  else
  {
    if (m_key_iv_set->get_next_key_iv_slot(&key_iv) == -1)
    {
      RETURN(-1);
    }
  }

  // RFC2898 PKCS #5: Password-Based Cryptography Specification Version 2.0
  const char* pass = reinterpret_cast<const char*>(pwd);
  int r = PKCS5_PBKDF2_HMAC(pass ? pass : "",
                            pwd_len,
                            salt,
                            SALT_LEN,
                            iter_count,
                            EVP_sha256(),
                            KEY_LEN + IV_LEN,
                            key_iv);
  if (r != 1)
  {
    RETURN(-1);
  }

  if (m_key_iv_set != nullptr)
  {
    require(m_key_iv_set->commit_next_key_iv_slot() != -1);
  }
  else
  {
    m_has_key_iv = true;
  }

  return 0;
}


int ndb_openssl_evp::remove_all_key_iv_pairs()
{
  if (m_has_key_iv)
  {
    require(m_key_iv_set == nullptr);
    m_has_key_iv = false;
    return 0;
  }
  if (m_key_iv_set != nullptr)
  {
    require(!m_has_key_iv);
    return m_key_iv_set->clear();
  }
  return -1;
}



ndb_openssl_evp::key256_iv256_set::key256_iv256_set()
: m_key_iv_count(0)
{}


ndb_openssl_evp::key256_iv256_set::~key256_iv256_set()
{
  require(m_key_iv_count == 0);
}


int ndb_openssl_evp::key256_iv256_set::clear()
{
  m_key_iv_count = 0;
  return 0;
}


int ndb_openssl_evp::key256_iv256_set::get_next_key_iv_slot(byte **key_iv)
{
  if (m_key_iv_count >= 500)
  {
    return -1;
  }

  *key_iv = m_key_iv[m_key_iv_count].m_key_iv;
  return 0;
}


int ndb_openssl_evp::key256_iv256_set::commit_next_key_iv_slot()
{
  if (m_key_iv_count >= 500)
  {
    return -1;
  }

  m_key_iv_count++;
  return 0;
}


int ndb_openssl_evp::key256_iv256_set::get_key_iv_pair(
                       size_t index, const byte **key, const byte **iv) const
{
  size_t i = index % m_key_iv_count;
  size_t reuse = i / m_key_iv_count;
  *key = &m_key_iv[i].m_key_iv[0];
  *iv = &m_key_iv[i].m_key_iv[KEY_LEN];
  require(reuse <= INT_MAX);
  return reuse;
}


int ndb_openssl_evp::key256_iv256_set::get_key_iv_mixed_pair(
                      size_t index, const byte **key, const byte **iv) const
{
  size_t iv_index = index % m_key_iv_count;
  size_t key_index = index / m_key_iv_count % m_key_iv_count;
  size_t reuse = index / m_key_iv_count / m_key_iv_count;
  *key = &m_key_iv[key_index].m_key_iv[0];
  *iv = &m_key_iv[iv_index].m_key_iv[KEY_LEN];
  require(reuse <= INT_MAX);
  return reuse;
}


ndb_openssl_evp::operation::operation(ndb_openssl_evp* context)
: m_op_mode(NO_OP),
  m_input_position(-1),
  m_output_position(-1),
  m_context(context),
  m_evp_context(EVP_CIPHER_CTX_new())
{}


ndb_openssl_evp::operation::~operation()
{
//  require(m_op_mode == NO_OP);
  if (m_evp_context != nullptr)
  {
    EVP_CIPHER_CTX_free(m_evp_context);
  }
}


int ndb_openssl_evp::operation::setup_key_iv(off_t input_position,
                                             const byte **key,
                                             const byte **iv,
                                             byte xts_seq_num[16])
{
  if (m_context->m_data_unit_size > 0)
  {
    assert(input_position % m_context->m_data_unit_size == 0);
    if (input_position % m_context->m_data_unit_size != 0)
    {
      RETURN(-1);
    }

    if (m_context->m_has_key_iv)
    {
      *key = &m_context->m_key_iv[0];
      *iv = &m_context->m_key_iv[KEY_LEN];
    }
    else
    {
      assert(m_context->m_key_iv_set != nullptr);
      if (m_context->m_key_iv_set == nullptr)
      {
        RETURN(-1);
      }

      size_t data_unit_index = input_position / m_context->m_data_unit_size;
      int key_iv_pair_index;
      if (m_context->m_evp_cipher == EVP_aes_256_xts())
      {
        key_iv_pair_index = data_unit_index >> 16;
      }
      else
      {
        key_iv_pair_index = data_unit_index;
      }
      int rc = m_context->m_key_iv_set->get_key_iv_mixed_pair(
                                                 key_iv_pair_index, key, iv);
      if (rc == -1)
      {
        RETURN(-1);
      }

      if (m_context->m_evp_cipher == EVP_aes_256_xts())
      {
        /*
         * aes_256_xts uses double length key
         * iv should be set as 16 bit sequence number in bigendian
         * copy key and iv into one long key
         */
        memcpy(&m_context->m_key_iv[0], *key, KEY_LEN);
        memcpy(&m_context->m_key_iv[KEY_LEN], *iv, IV_LEN);
        *key = m_context->m_key_iv;

        *iv = xts_seq_num;
        for (int i = 0; i < 14; i++)
          xts_seq_num[i] = 0;
        xts_seq_num[14] = (data_unit_index & 0xff00) >> 8;
        xts_seq_num[15] = data_unit_index & 0xff;
      }
    }
  }
  else
  {
    if (!m_context->m_has_key_iv)
    {
      RETURN(-1);
    }

    *key = &m_context->m_key_iv[0];
    *iv = &m_context->m_key_iv[KEY_LEN];
  }
  return 0;
}


int ndb_openssl_evp::operation::setup_decrypt_key_iv(off_t position,
                                                     const byte* iv_)
{
  require(m_op_mode == DECRYPT || m_op_mode == NO_OP);
  size_t data_unit_size = m_context->m_data_unit_size;

  const byte *key;
  const byte *iv;
  byte xts_seq_num[16];

  if (data_unit_size == 0)
  {
    assert(position == 0 || iv_ != nullptr);
    if (position != 0 && iv_ == nullptr)
    {
      RETURN(-1);
    }
    if (position != 0) position = 0;
  }
  else
  {
    assert(position % data_unit_size == 0);
    if (position % data_unit_size != 0)
    {
      RETURN(-1);
    }
  }
  if (setup_key_iv(position, &key, &iv, xts_seq_num) == -1)
  {
    RETURN(-1);
  }
  
  if (iv_ != nullptr) iv = iv_;

  int r;
  r = EVP_DecryptInit_ex(m_evp_context,
                         m_context->m_evp_cipher,
                         nullptr,
                         key,
                         iv);
  if (r != 1)
  {
    RETURN(-1);
  }

  r = EVP_CIPHER_CTX_set_padding(m_evp_context, m_context->m_padding);
  if (r != 1)
  {
    RETURN(-1);
  }
  return 0;
}


int ndb_openssl_evp::operation::setup_encrypt_key_iv(off_t position)
{
  require(m_op_mode == ENCRYPT || m_op_mode == NO_OP);
  size_t data_unit_size = m_context->m_data_unit_size;

  const byte *key;
  const byte *iv;
  byte xts_seq_num[16];

  if (data_unit_size == 0)
  {
    assert(position == 0);
    if (position != 0)
    {
      return -1;
    }
  }
  else
  {
    assert(position % data_unit_size == 0);
    if (position % data_unit_size != 0)
    {
      return -1;
    }
  }
  if (setup_key_iv(position, &key, &iv, xts_seq_num) == -1)
  {
    return -1;
  }

  int r;
  r = EVP_EncryptInit_ex(m_evp_context,
                         m_context->m_evp_cipher,
                         nullptr,
                         key,
                         iv);
  if (r != 1)
  {
    return -1;
  }
  r = EVP_CIPHER_CTX_set_padding(m_evp_context, m_context->m_padding);
  if (r != 1)
  {
    return -1;
  }

  return 0;
}


int ndb_openssl_evp::operation::encrypt_init(off_t output_position,
                                             off_t input_position)
{
  require(m_op_mode == NO_OP);
  if (m_context->m_data_unit_size == 0)
  {
    if (setup_encrypt_key_iv(input_position) == -1)
    {
      return -1;
    }
  }
  m_op_mode = ENCRYPT;
  m_input_position = input_position;
  m_output_position = output_position;
  return 0; 
}


int ndb_openssl_evp::operation::encrypt(output_iterator* out,
                                        input_iterator* in)
{
  require(m_op_mode == ENCRYPT);
  bool progress = false;
  const size_t data_unit_size = m_context->m_data_unit_size;
  if (data_unit_size == 0)
  {
    int r;
    size_t inl = in->size();
    size_t out_sz = out->size();
    if (out_sz < BLOCK_LEN)
    {
      inl = 0;
    }
    else if (out_sz < inl + BLOCK_LEN - 1)
    {
      inl = out_sz - (BLOCK_LEN - 1);
    }
    if (inl > 0)
    {
      int outl;
      r = EVP_EncryptUpdate(m_evp_context,
                            out->begin(),
                            &outl,
                            in->cbegin(),
                            inl);
      if (r != 1)
      {
        RETURN(-1);
      }

      m_input_position += inl;
      m_output_position += outl;
      in->advance(inl);
      out->advance(outl);
      progress = true;
    }
    if (in->last() && in->empty())
    {
      if (m_context->m_padding && out->size() < BLOCK_LEN)
      {
        return progress ? 1 : 2; // Need more output buffer
      }
      int outl;
      r = EVP_EncryptFinal_ex(m_evp_context, out->begin(), &outl);
      if (r != 1)
      {
        RETURN(-1);
      }
      if (m_context->m_padding)
      {
        require(size_t(outl) <= BLOCK_LEN);
      }
      else
      {
        require(outl == 0);
      }

      m_output_position += outl;
      out->advance(outl);
      out->set_last();
      return 0;
    }
    return progress ? 1 : 2;
  }
  else
  {
    require(!m_context->m_padding);

    if (in->size() % data_unit_size != 0)
    {
      RETURN(-1);
    }
    if (out->size() < data_unit_size)
    {
      return 2;
    }

    for (;;)
    {
      if (in->empty() && in->last())
      {
        out->set_last();
        return 0;
      }
      else if (in->empty() || out->empty())
      {
        return progress ? 1 : 2;
      }

      if (setup_encrypt_key_iv(m_input_position) == -1)
      {
        RETURN(-1);
      }

      int outl;
      int r = EVP_EncryptUpdate(m_evp_context,
                                out->begin(),
                                &outl,
                                in->cbegin(),
                                data_unit_size);
      if (r != 1)
      {
        RETURN(-1);
      }

      require(size_t(outl) == data_unit_size);
      m_input_position += data_unit_size;
      m_output_position += outl;
      out->advance(outl);
      in->advance(data_unit_size);
      progress = true;

      r = EVP_EncryptFinal_ex(m_evp_context, out->begin(), &outl);
      if (r != 1)
      {
        RETURN(-1);
      }

      require(outl == 0);
    }
  }
}


int ndb_openssl_evp::operation::encrypt_end()
{
  if (m_op_mode != ENCRYPT)
    RETURN(-1);
  m_op_mode = NO_OP;
  return 0;
}


int ndb_openssl_evp::operation::decrypt_init(off_t output_position,
                                             off_t input_position)
{
  require(m_op_mode == NO_OP);
  if (m_context->m_data_unit_size == 0)
  {
    if (setup_decrypt_key_iv(output_position) == -1)
    {
      return -1;
    }
  }
  m_op_mode = DECRYPT;
  m_reverse = false;
  m_input_position = input_position;
  m_output_position = output_position;
  return 0; 
}

int ndb_openssl_evp::operation::decrypt_init_reverse(off_t output_position,
                                                     off_t input_position)
{
  require(m_op_mode == NO_OP);
  m_op_mode = DECRYPT;
  m_reverse = true;
  m_at_padding_end = m_context->m_padding;
  m_input_position = input_position;
  m_output_position = output_position;
  return 0; 
}

int ndb_openssl_evp::operation::decrypt(output_iterator* out,
                                        input_iterator* in)
{
  require(m_op_mode == DECRYPT);
  bool progress = false;
  const size_t data_unit_size = m_context->m_data_unit_size;
  require(!m_reverse);
  if (data_unit_size == 0)
  {
    int r;
    int inl = in->size();
    int outl = out->size();
    if (size_t(outl) <= BLOCK_LEN)
    {
      inl = 0;
    }
    else if (size_t(outl) < inl + BLOCK_LEN)
    {
      inl = outl - BLOCK_LEN;
    }
    if (inl > 0)
    {
      r = EVP_DecryptUpdate(m_evp_context,
                            out->begin(),
                            &outl,
                            in->cbegin(),
                            inl);
      if (r != 1)
      {
        RETURN(-1);
      }

      m_input_position += inl;
      m_output_position += outl;
      in->advance(inl);
      out->advance(outl);
      progress = true;
    }
    if (in->last() && in->empty())
    {
      int outl;
      if (m_context->m_padding && out->size() < BLOCK_LEN)
      {
        return progress ? 1 : 2; // Need more output buffer
      }
      r = EVP_DecryptFinal_ex(m_evp_context, out->begin(), &outl);
      if (r != 1)
      {
        RETURN(-1);
      }
      if (m_context->m_padding)
      {
        require(size_t(outl) <= BLOCK_LEN);
      }
      else
      {
        require(outl == 0);
      }

      m_output_position += outl;
      out->advance(outl);
      out->set_last();
      return 0;
    }
    return progress ? 1 : 2;
  }
  else
  {
    require(!m_context->m_padding);

    if (in->size() % data_unit_size != 0)
    {
      RETURN(-1);
    }
    if (out->size() < data_unit_size)
    {
      return 2;
    }

    for (;;)
    {
      if (in->empty() && in->last())
      {
        out->set_last();
        return 0;
      }
      else if (in->empty() || out->empty())
      {
        return progress ? 1 : 2;
      }

      if (setup_decrypt_key_iv(m_output_position) == -1)
      {
        RETURN(-1);
      }

      int outl;
      int r = EVP_DecryptUpdate(m_evp_context,
                                out->begin(),
                                &outl,
                                in->cbegin(),
                                data_unit_size);
      if (r != 1)
      {
        RETURN(-1);
      }

      require(size_t(outl) == data_unit_size);
      m_input_position += data_unit_size;
      m_output_position += outl;
      out->advance(outl);
      in->advance(data_unit_size);
      progress = true;

      r = EVP_DecryptFinal_ex(m_evp_context, out->begin(), &outl);
      if (r != 1)
      {
        RETURN(-1);
      }

      require(outl == 0);
    }
  }
}

int ndb_openssl_evp::operation::decrypt_reverse(output_reverse_iterator* out,
                                                input_reverse_iterator* in)
{
  require(m_op_mode == DECRYPT);
  bool progress = false;
  const size_t data_unit_size = m_context->m_data_unit_size;
  require(m_reverse);
  require(data_unit_size == 0);

  Uint64 output_position = m_output_position;
  int r;
  size_t inl = in->size() / CBC_BLOCK_LEN * CBC_BLOCK_LEN;
  size_t outl = std::min(Uint64(out->size()), output_position);
  if (m_at_padding_end)
  {
    size_t min_inl = (outl / CBC_BLOCK_LEN + 1) * CBC_BLOCK_LEN;
    inl = std::min(inl, min_inl);
    require(m_context->m_padding);
    if (output_position < inl)
    {
      inl = (output_position / CBC_BLOCK_LEN + 1) * CBC_BLOCK_LEN;
      require(inl <= in->size());
      in->set_last();
    }
    else
    {
      require(!in->last());
    }
  }
  else
  {
    inl = std::min(inl, outl);
    if (output_position <= Uint64(inl))
    {
      in->set_last();
    }
    else
    {
      require(!in->last());
    }
  }
  const byte* iv = nullptr;
  if (!in->last())
  {
    if (in->size() >= inl + CBC_IV_LEN)
    {
      iv = in->cbegin() - inl - CBC_IV_LEN;
    }
    else if (inl > CBC_BLOCK_LEN)
    {
      inl -= CBC_BLOCK_LEN;
      iv = in->cbegin() - inl - CBC_IV_LEN;
    }
    else
    {
      return 2; // Need more input, no progress
    }
  }

  off_t in_position = m_at_padding_end
    ? (output_position / CBC_BLOCK_LEN + 1 - inl / CBC_BLOCK_LEN) *
                                                             CBC_BLOCK_LEN
    : (output_position - inl);
  require(setup_decrypt_key_iv(in_position, iv) == 0);
  const bool padding = m_at_padding_end && m_context->m_padding;
  r = EVP_CIPHER_CTX_set_padding(m_evp_context, padding);
  if (r != 1)
  {
    RETURN(-1);
  }
 
  int real_outl;
  r = EVP_DecryptUpdate(m_evp_context,
                        out->end(),
                        &real_outl,
                        in->cbegin() - inl,
                        inl);
  if (r != 1)
  {
    RETURN(-1);
  }
  require(size_t(real_outl) <= outl);
  int final_outl;
  r = EVP_DecryptFinal_ex(m_evp_context, out->end() + real_outl, &final_outl);
  if (r != 1)
  {
    RETURN(-1);
  }
  require(size_t(real_outl + final_outl) <= outl);
  memmove(out->begin() - real_outl - final_outl,
          out->end(),
          real_outl + final_outl);

  m_input_position -= inl;
  m_output_position -= real_outl + final_outl;
  in->advance(inl);
  out->advance(real_outl + final_outl);
  progress = true;
  m_at_padding_end = false;
 
  return 1;
}

int ndb_openssl_evp::operation::decrypt_end()
{
  m_op_mode = NO_OP;
  return 0;
}

#ifdef TEST_NDB_OPENSSL_EVP

void require_fn(bool cc,
                const char cc_str[],
                const char file[],
                int line,
                const char func[])
{
  if (cc)
  {
    return;
  }
  fprintf(stderr,
          "FATAL: %s: %u: %s: require(%s) failed.\n",
          file,
          line,
          func,
          cc_str);
  abort();
}

int main(int argc, char*argv[])
{
  using byte = unsigned char;
  const char* pwd = "Not so secret";

  ndb_openssl_evp enc;
  ndb_openssl_evp::operation op(&enc);

  if (argc == 1) enc.set_aes_256_cbc(true,0);
  else switch (argv[1][0])
  {
  case 'c': enc.set_aes_256_cbc(false,argv[1][1]?atoi(&argv[1][1]):0); break;
  case 'p': enc.set_aes_256_cbc(true,0); break;
  case 'x': enc.set_aes_256_xts(argv[1][1]?atoi(&argv[1][1]):32); break;
  }

  byte salt[32];
  require(0 == enc.generate_salt256(salt));
  require(0 == enc.derive_and_add_key_iv_pair(
                 reinterpret_cast<const byte*>(pwd),
                 strlen(pwd),
                 100000,
                 salt));

  for (int argi = 2; argi < argc; argi++)
  {
    int r;
    byte bs[95];
    byte* be = &bs[sizeof(bs)];
    byte* s = reinterpret_cast<byte*>(argv[argi]);
    byte* e = reinterpret_cast<byte*>(strchr(argv[argi], 0));

    fprintf(stderr, "%s\n", argv[argi]);

    ndbxfrm_input_iterator in = {s, e, true};
    ndbxfrm_output_iterator out = {bs, be, false};

    require(0==op.encrypt_init(0, 0));
    while ((r=op.encrypt(&out, &in)) == 1);
    require(0==r);
    require(0==op.encrypt_end());
    require(in.empty());
    require(out.last());

    in = ndbxfrm_input_iterator{bs, out.begin(), out.last()};
    out = ndbxfrm_output_iterator{out.begin(), out.end(), false};

    require(0==op.decrypt_init(0, 0));
    while ((r=op.decrypt(&out, &in)) == 1);
    require(0==r);
    require(0==op.decrypt_end());
    require(in.empty());
    require(out.last());

    fprintf(stderr, "%s -> %s\n", argv[argi], out.begin() - (e - s));
    require(memcmp(s, out.begin() - (e - s), e - s) == 0);
  }

  enc.reset();
  return 0;
}

#endif
