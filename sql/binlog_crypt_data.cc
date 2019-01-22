/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "binlog_crypt_data.h"

#include "my_byteorder.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#ifdef MYSQL_SERVER
#include <sstream>
#include "log.h"
#include "mysql/service_mysql_keyring.h"
#include "system_key.h"
#endif

Binlog_crypt_data::Binlog_crypt_data() noexcept
    : key_length(0), key(nullptr), enabled(false), scheme(0) {}

Binlog_crypt_data::~Binlog_crypt_data() { free_key(key, key_length); }

Binlog_crypt_data::Binlog_crypt_data(const Binlog_crypt_data &b)
    : key_version(b.key_version),
      key_length(b.key_length),
      enabled(b.enabled),
      offs(b.offs) {
  if (b.key_length && b.key != nullptr) {
    key = reinterpret_cast<uchar *>(
        my_malloc(PSI_NOT_INSTRUMENTED, b.key_length, MYF(MY_WME)));
    memcpy(key, b.key, b.key_length);
  } else
    key = nullptr;

  memcpy(iv, b.iv, binary_log::Start_encryption_event::IV_LENGTH);
  memcpy(nonce, b.nonce, binary_log::Start_encryption_event::NONCE_LENGTH);
}

void Binlog_crypt_data::free_key(uchar *&key, size_t &key_length) noexcept {
  if (key != nullptr) {
    DBUG_ASSERT(key_length == 16);
    //TODO: should be memset_s(key, 512, 0, key_length) when ported;
    memset(key, 0, key_length);
    my_free(key);
    key = nullptr;
    key_length = 0;
  }
}

Binlog_crypt_data &Binlog_crypt_data::operator=(Binlog_crypt_data b) noexcept {
  enabled = b.enabled;
  key_version = b.key_version;
  key_length = b.key_length;
  std::swap(this->key, b.key);
  key_length = b.key_length;
  memcpy(iv, b.iv, binary_log::Start_encryption_event::IV_LENGTH);
  memcpy(nonce, b.nonce, binary_log::Start_encryption_event::NONCE_LENGTH);
  return *this;
}

bool Binlog_crypt_data::load_latest_binlog_key() {
  free_key(key, key_length);
  bool error = false;
#ifdef MYSQL_SERVER
  char *system_key_type = nullptr;
  size_t system_key_len = 0;
  uchar *system_key = nullptr;

  DBUG_EXECUTE_IF("binlog_encryption_error_on_key_fetch", { return true; });

  if (my_key_fetch(PERCONA_BINLOG_KEY_NAME, &system_key_type, nullptr,
                   reinterpret_cast<void **>(&system_key), &system_key_len) ||
      (system_key == nullptr &&
       (my_key_generate(PERCONA_BINLOG_KEY_NAME, "AES", nullptr, 16) ||
        my_key_fetch(PERCONA_BINLOG_KEY_NAME, &system_key_type, nullptr,
                     reinterpret_cast<void **>(&system_key), &system_key_len) ||
        system_key == nullptr)))
    return true;

  DBUG_ASSERT(strncmp(system_key_type, "AES", 3) == 0);
  my_free(system_key_type);

  error = (parse_system_key(system_key, system_key_len, &key_version, &key,
                            &key_length) == reinterpret_cast<uchar *>(NullS));
  my_free(system_key);
#endif
  return error;
}

bool Binlog_crypt_data::init_with_loaded_key(
    uint sch, const uchar *nonce MY_ATTRIBUTE((unused))) noexcept {
  scheme = sch;
#ifdef MYSQL_SERVER
  DBUG_ASSERT(key != nullptr && nonce != nullptr);
  memcpy(this->nonce, nonce, binary_log::Start_encryption_event::NONCE_LENGTH);
#endif
  enabled = true;
  return false;
}

bool Binlog_crypt_data::init(uint sch MY_ATTRIBUTE((unused)),
                             uint kv MY_ATTRIBUTE((unused)),
                             const uchar *nonce MY_ATTRIBUTE((unused))) {
  free_key(key, key_length);
#ifdef MYSQL_SERVER
  char *key_type = nullptr;
  std::ostringstream percona_binlog_with_ver_ss;
  percona_binlog_with_ver_ss << PERCONA_BINLOG_KEY_NAME << ':' << kv;
  if (my_key_fetch(percona_binlog_with_ver_ss.str().c_str(), &key_type, nullptr,
                   reinterpret_cast<void **>(&key), &key_length) ||
      key == nullptr)
    return true;
  DBUG_ASSERT(strncmp(key_type, "AES", 3) == 0);
  my_free(key_type);

  if (init_with_loaded_key(sch, nonce)) {
    free_key(key, key_length);
    return true;
  }
#endif
  return false;
}

void Binlog_crypt_data::set_iv(uchar *iv, uint32 offs) const {
  DBUG_ASSERT(key != nullptr && key_length == 16);

  uchar iv_plain[binary_log::Start_encryption_event::IV_LENGTH];
  memcpy(iv_plain, nonce, binary_log::Start_encryption_event::NONCE_LENGTH);
  int4store(iv_plain + binary_log::Start_encryption_event::NONCE_LENGTH, offs);

  my_aes_encrypt(iv_plain, binary_log::Start_encryption_event::IV_LENGTH, iv,
                 key, key_length, my_aes_128_ecb, nullptr, false);
}
