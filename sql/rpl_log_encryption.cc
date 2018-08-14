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

#include "sql/rpl_log_encryption.h"
#include <sql/mysqld.h>
#include <string.h>
#include <algorithm>
#include <sstream>
#include "event_reader.h"
#include "my_byteorder.h"

#ifdef MYSQL_SERVER
#include "my_aes.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/service_mysql_keyring.h"

/*
  TODO: Note to reviewers: This function shall be extended on later steps of
        WL#10957 implementing some caching mechanism to avoid consulting the
        keyring repeatedly for a key asked many times.
*/
std::pair<Rpl_encryption::Keyring_status, Key_string> Rpl_encryption::get_key(
    const std::string &key_id, const std::string &key_type) {
  DBUG_ENTER("Rpl_encryption::get_key(std::string)");
  Key_string key_str;

  auto tuple = fetch_key_from_keyring(key_id, key_type);
  if (std::get<1>(tuple)) {
    DBUG_EXECUTE_IF("corrupt_replication_encryption_key", {
      unsigned char *first =
          reinterpret_cast<unsigned char *>(std::get<1>(tuple));
      first[0] = ~(first[0]);
    });
    key_str.append(reinterpret_cast<unsigned char *>(std::get<1>(tuple)),
                   std::get<2>(tuple));
    my_free(std::get<1>(tuple));
  }

  auto result = std::make_pair(std::get<0>(tuple), key_str);
  DBUG_RETURN(result);
}

std::pair<Rpl_encryption::Keyring_status, Key_string> Rpl_encryption::get_key(
    const std::string &key_id, const std::string &key_type, size_t key_size) {
  DBUG_ENTER("Rpl_encryption::get_key(std::string, int)");
  auto pair = get_key(key_id, key_type);
  if (pair.first == Keyring_status::SUCCESS) {
    DBUG_EXECUTE_IF("corrupt_replication_encryption_key_size",
                    { pair.second.resize(key_size / 2); });
    if (pair.second.length() != key_size)
      pair.first = Keyring_status::UNEXPECTED_KEY_SIZE;
  }
  DBUG_RETURN(pair);
}

std::tuple<Rpl_encryption::Keyring_status, void *, size_t>
Rpl_encryption::fetch_key_from_keyring(const std::string &key_id,
                                       const std::string &key_type) {
  DBUG_ENTER("Rpl_encryption::fetch_key_from_keyring");
  size_t key_len = 0;
  char *retrieved_key_type = nullptr;
  void *key = nullptr;
  Keyring_status error = Keyring_status::SUCCESS;

  /* Error fetching the key */
  if (my_key_fetch(key_id.c_str(), &retrieved_key_type, nullptr, &key,
                   &key_len)) {
    DBUG_ASSERT(key == nullptr);
    error = Keyring_status::KEYRING_ERROR;
  } else {
    /* Key was not found in keyring */
    if (key == nullptr) {
      error = Keyring_status::KEY_NOT_FOUND;
    } else {
      DBUG_EXECUTE_IF("corrupt_replication_encryption_key_type",
                      { retrieved_key_type[0] = 0; });
      if (key_type.compare(retrieved_key_type) != 0)
        error = Keyring_status::UNEXPECTED_KEY_TYPE;
    }
  }

  if (retrieved_key_type) my_free(retrieved_key_type);

  auto result = std::make_tuple(error, key, key_len);
  DBUG_RETURN(result);
}

#endif  // MYSQL_SERVER

const char *Rpl_encryption_header::ENCRYPTION_MAGIC = "\xfd\x62\x69\x6e";

Rpl_encryption_header::~Rpl_encryption_header() {
  DBUG_ENTER("Rpl_encryption_header::~Rpl_encryption_header");
  DBUG_VOID_RETURN;
};

void throw_encryption_header_error(const char *message) {
#ifdef MYSQL_SERVER
  if (current_thd)
#endif
    my_error(ER_RPL_ENCRYPTION_HEADER_ERROR, MYF(0), message);
#ifdef MYSQL_SERVER
  else
    LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_HEADER_ERROR, message);
#endif
}

std::unique_ptr<Rpl_encryption_header> Rpl_encryption_header::get_header(
    Basic_istream *istream) {
  DBUG_ENTER("Rpl_encryption_header::get_header");
  bool res = true;
  ssize_t read_len = 0;
  uint8_t version = 0;

  // This is called after reading the MAGIC.
  read_len = istream->read(&version, VERSION_SIZE);

  DBUG_EXECUTE_IF("force_encrypted_header_version_2", { version = 2; });
  DBUG_EXECUTE_IF("corrupt_encrypted_header_version", { read_len = 0; });

  std::unique_ptr<Rpl_encryption_header> header;

  if (read_len == VERSION_SIZE) {
    DBUG_PRINT("debug", ("encryption header version= %d", version));
    switch (version) {
      case 1: {
        std::unique_ptr<Rpl_encryption_header> header_v1(
            new Rpl_encryption_header_v1);
        header = std::move(header_v1);
        res = header->deserialize(istream);
        if (res) header.reset(nullptr);
        break;
      }
      default:
        throw_encryption_header_error("Unsupported encryption header version");
        break;
    }
  } else {
    throw_encryption_header_error(
        "Unable to determine encryption header version");
  }
  DBUG_RETURN(header);
}

const char *Rpl_encryption_header_v1::KEY_TYPE = "AES";

Rpl_encryption_header_v1::Rpl_encryption_header_v1(
    const std::string &key_id, const Key_string &encrypted_password,
    const Key_string &iv)
    : m_key_id(key_id), m_encrypted_password(encrypted_password), m_iv(iv) {
  DBUG_ENTER(
      "Rpl_encryption_header_v1::Rpl_encryption_header_v1(std::string, "
      "Key_string, Key_string)");
  DBUG_VOID_RETURN;
}

Rpl_encryption_header_v1::~Rpl_encryption_header_v1() {
  DBUG_ENTER("Rpl_encryption_header_v1::~Rpl_encryption_header_v1");
  DBUG_VOID_RETURN;
};

bool Rpl_encryption_header_v1::deserialize(Basic_istream *istream) {
  DBUG_ENTER("Rpl_encryption_header_v1::deserialize");
  unsigned char header[HEADER_SIZE];
  ssize_t read_len = 0;

  // This is called after reading the MAGIC + version.
  const int read_offset = ENCRYPTION_MAGIC_SIZE + VERSION_SIZE;
  read_len = istream->read(header + read_offset, HEADER_SIZE - (read_offset));

  DBUG_EXECUTE_IF("force_incomplete_encryption_header", { --read_len; });
  if (read_len < HEADER_SIZE - read_offset) {
    throw_encryption_header_error("Header is incomplete");
    DBUG_RETURN(true);
  }

  m_key_id.clear();
  m_encrypted_password.clear();
  m_iv.clear();

  const char *header_buffer = reinterpret_cast<char *>(header);
  binary_log::Event_reader reader(header_buffer, HEADER_SIZE);
  reader.go_to(OPTIONAL_FIELD_OFFSET);
  uint8_t field_type = 0;

  DBUG_EXECUTE_IF("corrupt_encryption_header_unknown_field_type",
                  { header[OPTIONAL_FIELD_OFFSET] = 255; });

  while (!reader.has_error()) {
    field_type = reader.read<uint8_t>();
    switch (field_type) {
      case 0:
        /* End of fields */
        break;
      case KEY_ID: {
        uint8_t length = reader.read<uint8_t>();
        DBUG_EXECUTE_IF("corrupt_encryption_header_read_above_header_size",
                        { reader.go_to(HEADER_SIZE - 1); });
        if (!reader.has_error()) {
          const char *key_ptr = reader.ptr(length);
          if (!reader.has_error()) m_key_id.assign(key_ptr, length);
        }
        break;
      }
      case ENCRYPTED_FILE_PASSWORD: {
        const unsigned char *password_ptr =
            reinterpret_cast<const unsigned char *>(
                reader.ptr(PASSWORD_FIELD_SIZE));
        if (!reader.has_error())
          m_encrypted_password.assign(password_ptr, PASSWORD_FIELD_SIZE);
        break;
      }
      case IV_FOR_FILE_PASSWORD: {
        const unsigned char *iv_ptr =
            reinterpret_cast<const unsigned char *>(reader.ptr(IV_FIELD_SIZE));
        if (!reader.has_error()) m_iv.assign(iv_ptr, IV_FIELD_SIZE);
        break;
      }
      default:
        throw_encryption_header_error("Unknown field type");
        DBUG_RETURN(true);
    }
    if (field_type == 0) break;
  }

  DBUG_EXECUTE_IF("corrupt_encryption_header_missing_key_id",
                  { m_key_id.clear(); });
  DBUG_EXECUTE_IF("corrupt_encryption_header_missing_password",
                  { m_encrypted_password.clear(); });
  DBUG_EXECUTE_IF("corrupt_encryption_header_missing_iv", { m_iv.clear(); });

  bool res = false;

  if (reader.has_error()) {
    /* Error deserializing header fields */
    throw_encryption_header_error("Header is corrupted");
    res = true;
  } else {
    if (m_key_id.empty()) {
      throw_encryption_header_error(
          "Header is missing the replication encryption key ID");
      res = true;
    } else if (m_encrypted_password.empty()) {
      throw_encryption_header_error("Header is missing the encrypted password");
      res = true;
    } else if (m_iv.empty()) {
      throw_encryption_header_error("Header is missing the IV");
      res = true;
    }
  }

  DBUG_RETURN(res);
}

char Rpl_encryption_header_v1::get_version() const { return m_version; }

const std::string &Rpl_encryption_header_v1::get_key_id() const {
  return m_key_id;
}

const Key_string &Rpl_encryption_header_v1::get_encrypted_password() const {
  return m_encrypted_password;
}

int Rpl_encryption_header_v1::get_header_size() {
  return Rpl_encryption_header_v1::HEADER_SIZE;
}

const Key_string &Rpl_encryption_header_v1::get_iv() const { return m_iv; }

#ifdef MYSQL_SERVER
Key_string Rpl_encryption_header_v1::decrypt_file_password() {
  DBUG_ENTER("Rpl_encryption_header::decrypt_file_password");
  Key_string file_password;

  if (!m_key_id.empty()) {
    auto error_and_key =
        Rpl_encryption::get_key(m_key_id, KEY_TYPE, KEY_LENGTH);

    if (error_and_key.first != Rpl_encryption::Keyring_status::SUCCESS) {
      switch (error_and_key.first) {
        case Rpl_encryption::Keyring_status::KEYRING_ERROR:
          if (current_thd)
            my_error(ER_RPL_ENCRYPTION_FAILED_TO_FETCH_KEY, MYF(0));
          else
            LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_FAILED_TO_FETCH_KEY);
          break;
        case Rpl_encryption::Keyring_status::KEY_NOT_FOUND:
          if (current_thd)
            my_error(ER_RPL_ENCRYPTION_KEY_NOT_FOUND, MYF(0));
          else
            LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_KEY_NOT_FOUND);
          break;
        case Rpl_encryption::Keyring_status::UNEXPECTED_KEY_SIZE:
        case Rpl_encryption::Keyring_status::UNEXPECTED_KEY_TYPE:
          if (current_thd)
            my_error(ER_RPL_ENCRYPTION_KEYRING_INVALID_KEY, MYF(0));
          else
            LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_KEYRING_INVALID_KEY);
          break;
        default:
          DBUG_ASSERT(0);
      }
    } else if (!error_and_key.second.empty()) {
      unsigned char buffer[Aes_ctr_decryptor::PASSWORD_LENGTH];

      if (my_aes_decrypt(m_encrypted_password.data(),
                         m_encrypted_password.length(), buffer,
                         error_and_key.second.data(),
                         error_and_key.second.length(), my_aes_256_cbc,
                         m_iv.data(), false) != MY_AES_BAD_DATA)
        file_password.append(buffer, Aes_ctr_decryptor::PASSWORD_LENGTH);
    }
  }
  DBUG_RETURN(file_password);
}
#else
Key_string Rpl_encryption_header_v1::decrypt_file_password() {
  Key_string file_password;
  return file_password;
}
#endif

std::unique_ptr<Rpl_cipher> Rpl_encryption_header_v1::get_encryptor() {
  std::unique_ptr<Rpl_cipher> cypher(new Aes_ctr_encryptor);
  return cypher;
};

std::unique_ptr<Rpl_cipher> Rpl_encryption_header_v1::get_decryptor() {
  std::unique_ptr<Rpl_cipher> cypher(new Aes_ctr_decryptor);
  return cypher;
};

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
int Aes_ctr_cipher<TYPE>::get_header_size() {
  return m_header_size;
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
