/***********************************************************************

Copyright (c) 2019, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/

/** @file os/os0enc.cc
 Encryption code. */

#include "os0enc.h"
#include "fil0fil.h"
#ifdef UNIV_HOTBACKUP
#include "fsp0file.h"
#endif /* UNIV_HOTBACKUP */
#include "log0files_io.h"
#include "mach0data.h"
#include "os0file.h"
#include "page0page.h"
#include "ut0crc32.h"
#include "ut0mem.h"

#include <errno.h>
#include <mysql/components/services/keyring_generator.h>
#include <mysql/components/services/keyring_reader_with_status.h>
#include <mysql/components/services/keyring_writer.h>
#include <scope_guard.h>
#include "keyring_operations_helper.h"
#include "my_aes.h"
#include "my_rnd.h"
#include "mysql/service_mysql_keyring.h"
#include "mysqld.h"

namespace innobase {
namespace encryption {
#ifndef UNIV_HOTBACKUP
SERVICE_TYPE(keyring_reader_with_status) *keyring_reader_service = nullptr;
SERVICE_TYPE(keyring_writer) *keyring_writer_service = nullptr;
SERVICE_TYPE(keyring_generator) *keyring_generator_service = nullptr;

/**
  Initialize keyring component service handles

  @param [in] reg_srv Handle to registry service

  @returns status of keyring service initialization
    @retval true  Success
    @retval false Error
*/
bool init_keyring_services(SERVICE_TYPE(registry) * reg_srv) {
  DBUG_TRACE;

  if (reg_srv == nullptr) {
    return false;
  }

  my_h_service h_keyring_reader_service = nullptr;
  my_h_service h_keyring_writer_service = nullptr;
  my_h_service h_keyring_generator_service = nullptr;

  auto cleanup = [&]() {
    if (h_keyring_reader_service) {
      reg_srv->release(h_keyring_reader_service);
    }
    if (h_keyring_writer_service) {
      reg_srv->release(h_keyring_writer_service);
    }
    if (h_keyring_generator_service) {
      reg_srv->release(h_keyring_generator_service);
    }

    keyring_reader_service = nullptr;
    keyring_writer_service = nullptr;
    keyring_generator_service = nullptr;
  };

  if (reg_srv->acquire("keyring_reader_with_status",
                       &h_keyring_reader_service) ||
      reg_srv->acquire_related("keyring_writer", h_keyring_reader_service,
                               &h_keyring_writer_service) ||
      reg_srv->acquire_related("keyring_generator", h_keyring_reader_service,
                               &h_keyring_generator_service)) {
    cleanup();
    return false;
  }

  keyring_reader_service =
      reinterpret_cast<SERVICE_TYPE(keyring_reader_with_status) *>(
          h_keyring_reader_service);
  keyring_writer_service = reinterpret_cast<SERVICE_TYPE(keyring_writer) *>(
      h_keyring_writer_service);
  keyring_generator_service =
      reinterpret_cast<SERVICE_TYPE(keyring_generator) *>(
          h_keyring_generator_service);

  return true;
}

/**
  Deinitialize keyring component service handles

  @param [in] reg_srv Handle to registry service
*/
void deinit_keyring_services(SERVICE_TYPE(registry) * reg_srv) {
  DBUG_TRACE;

  if (reg_srv == nullptr) {
    return;
  }

  using keyring_reader_t = SERVICE_TYPE_NO_CONST(keyring_reader_with_status);
  using keyring_writer_t = SERVICE_TYPE_NO_CONST(keyring_writer);
  using keyring_generator_t = SERVICE_TYPE_NO_CONST(keyring_generator);

  if (keyring_reader_service) {
    reg_srv->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_reader_t *>(keyring_reader_service)));
  }
  if (keyring_writer_service) {
    reg_srv->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_writer_t *>(keyring_writer_service)));
  }
  if (keyring_generator_service) {
    reg_srv->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_generator_t *>(keyring_generator_service)));
  }

  keyring_reader_service = nullptr;
  keyring_writer_service = nullptr;
  keyring_generator_service = nullptr;
}

/**
  Generate a new key

  @param [in] key_id     Key identifier
  @param [in] key_type   Type of the key
  @param [in] key_length Length of the key

  @returns status of key generation
    @retval true  Success
    @retval false Error. No error is raised.
*/
bool generate_key(const char *key_id, const char *key_type, size_t key_length) {
  if (key_id == nullptr || key_type == nullptr || key_length == 0) {
    return false;
  }

  return keyring_generator_service->generate(key_id, nullptr, key_type,
                                             key_length) == 0;
}

/**
  Remove a key from keyring

  @param [in] key_id Key to be removed
*/
void remove_key(const char *key_id) {
  if (key_id == nullptr) {
    return;
  }

  /* We don't care about the removal status */
  (void)keyring_writer_service->remove(key_id, nullptr);
}

#else

bool init_keyring_services(SERVICE_TYPE(registry) *) { return false; }

void deinit_keyring_services(SERVICE_TYPE(registry) *) { return; }

#endif  // !UNIV_HOTBACKUP
}  // namespace encryption
}  // namespace innobase

/** Minimum length needed for encryption */
constexpr size_t MIN_ENCRYPTION_LEN = 2 * MY_AES_BLOCK_SIZE + FIL_PAGE_DATA;
/** Key type */
constexpr char innodb_key_type[] = "AES";

/** Current master key id */
uint32_t Encryption::s_master_key_id = Encryption::DEFAULT_MASTER_KEY_ID;

/** Current uuid of server instance */
char Encryption::s_uuid[Encryption::SERVER_UUID_LEN + 1] = {0};

/** Tablespaces whose key needs to be reencrypted */
std::vector<space_id_t> Encryption::s_tablespaces_to_reencrypt;

void Encryption::set(const Encryption_metadata &metadata) noexcept {
  set_type(metadata.m_type);
  set_key(metadata.m_key);
  set_key_length(metadata.m_key_len);
  set_initial_vector(metadata.m_iv);
}

const char *Encryption::to_string(Type type) noexcept {
  switch (type) {
    case NONE:
      return ("N");
    case AES:
      return ("Y");
  }

  ut_d(ut_error);

  ut_o(return ("<UNKNOWN>"));
}

void Encryption::random_value(byte *value) noexcept {
  ut_ad(value != nullptr);

  my_rand_buffer(value, KEY_LEN);
}

void Encryption::create_master_key(byte **master_key) noexcept {
#ifndef UNIV_HOTBACKUP
  size_t key_len;
  char *key_type = nullptr;
  char key_name[MASTER_KEY_NAME_MAX_LEN];

  /* If uuid does not match with current server uuid,
  set uuid as current server uuid. */
  if (strcmp(s_uuid, server_uuid) != 0) {
    memcpy(s_uuid, server_uuid, sizeof(s_uuid) - 1);
  }

  /* Generate new master key */
  snprintf(key_name, MASTER_KEY_NAME_MAX_LEN, "%s-%s-" UINT32PF,
           MASTER_KEY_PREFIX, s_uuid, s_master_key_id + 1);

  /* We call keyring API to generate master key here. */
  bool ret =
      innobase::encryption::generate_key(key_name, innodb_key_type, KEY_LEN);

  /* We call keyring API to get master key here. */
  int retval = keyring_operations_helper::read_secret(
      innobase::encryption::keyring_reader_service, key_name, nullptr,
      master_key, &key_len, &key_type, PSI_INSTRUMENT_ME);

  if (retval == -1 || *master_key == nullptr) {
    ib::error(ER_IB_MSG_831) << "Encryption can't find master key,"
                             << " please check the keyring is loaded."
                             << " ret=" << ret;

    *master_key = nullptr;
  } else {
    ++s_master_key_id;
  }

  if (key_type != nullptr) {
    my_free(key_type);
  }
#endif /* !UNIV_HOTBACKUP */
}

void Encryption::get_master_key(uint32_t master_key_id, char *srv_uuid,
                                byte **master_key) noexcept {
  size_t key_len = 0;
  char *key_type = nullptr;
  char key_name[MASTER_KEY_NAME_MAX_LEN];

  memset(key_name, 0x0, sizeof(key_name));

  if (srv_uuid != nullptr) {
    ut_ad(strlen(srv_uuid) > 0);

    snprintf(key_name, MASTER_KEY_NAME_MAX_LEN, "%s-%s-" UINT32PF,
             MASTER_KEY_PREFIX, srv_uuid, master_key_id);
  } else {
    /* For compatibility with 5.7.11, we need to get master key with
    server id. */

    snprintf(key_name, MASTER_KEY_NAME_MAX_LEN, "%s-%lu-" UINT32PF,
             MASTER_KEY_PREFIX, server_id, master_key_id);
  }

#ifndef UNIV_HOTBACKUP
  /* We call keyring API to get master key here. */
  int ret =
      (keyring_operations_helper::read_secret(
           innobase::encryption::keyring_reader_service, key_name, nullptr,
           master_key, &key_len, &key_type, PSI_INSTRUMENT_ME) > -1)
          ? 0
          : 1;
#else  /* !UNIV_HOTBACKUP */
  /* We call MEB to get master key here. */
  int ret = meb_key_fetch(key_name, &key_type, nullptr,
                          reinterpret_cast<void **>(master_key), &key_len);
#endif /* !UNIV_HOTBACKUP */

  if (key_type != nullptr) {
    my_free(key_type);
  }

  if (ret != 0) {
    *master_key = nullptr;

    ib::error(ER_IB_MSG_832) << "Encryption can't find master key,"
                             << " please check the keyring is loaded.";
  }

#ifdef UNIV_ENCRYPT_DEBUG
  if (ret == 0 && *master_key != nullptr) {
    std::ostringstream msg;

    ut_print_buf(msg, *master_key, key_len);

    ib::info(ER_IB_MSG_833)
        << "Fetched master key: " << master_key_id << "{" << msg.str() << "}";
  }
#endif /* UNIV_ENCRYPT_DEBUG */
}

void Encryption::get_master_key(uint32_t *master_key_id,
                                byte **master_key) noexcept {
#ifndef UNIV_HOTBACKUP
  size_t key_len;
  char *key_type = nullptr;
  char key_name[MASTER_KEY_NAME_MAX_LEN];
  extern ib_mutex_t master_key_id_mutex;
  int retval;
  bool key_id_locked = false;

  if (s_master_key_id == DEFAULT_MASTER_KEY_ID) {
    /* Take mutex as master_key_id is going to change. */
    mutex_enter(&master_key_id_mutex);
    key_id_locked = true;
  }

  memset(key_name, 0x0, sizeof(key_name));

  /* Check for s_master_key_id again, as a parallel rotation might have caused
  it to change. */
  if (s_master_key_id == DEFAULT_MASTER_KEY_ID) {
    ut_ad(strlen(server_uuid) > 0);
    memset(s_uuid, 0x0, sizeof(s_uuid));

    /* If m_master_key is DEFAULT_MASTER_KEY_ID, it means there's
    no encrypted tablespace yet. Generate the first master key now and store
    it to keyring. */
    memcpy(s_uuid, server_uuid, sizeof(s_uuid) - 1);

    /* Prepare the server s_uuid. */
    snprintf(key_name, MASTER_KEY_NAME_MAX_LEN, "%s-%s-1", MASTER_KEY_PREFIX,
             s_uuid);

    /* We call keyring API to generate master key here. */
    (void)innobase::encryption::generate_key(key_name, innodb_key_type,
                                             KEY_LEN);

    /* We call keyring API to get master key here. */
    retval = keyring_operations_helper::read_secret(
        innobase::encryption::keyring_reader_service, key_name, nullptr,
        master_key, &key_len, &key_type, PSI_INSTRUMENT_ME);

    if (retval > -1 && *master_key != nullptr) {
      ++s_master_key_id;
      *master_key_id = s_master_key_id;
    }
#ifdef UNIV_ENCRYPT_DEBUG
    if (retval > -1 && *master_key != nullptr) {
      std::ostringstream msg;

      ut_print_buf(msg, *master_key, key_len);

      ib::info(ER_IB_MSG_834)
          << "Generated new master key: {" << msg.str() << "}";
    }
#endif /* UNIV_ENCRYPT_DEBUG */
  } else {
    *master_key_id = s_master_key_id;

    snprintf(key_name, MASTER_KEY_NAME_MAX_LEN, "%s-%s-" UINT32PF,
             MASTER_KEY_PREFIX, s_uuid, *master_key_id);

    /* We call keyring API to get master key here. */
    retval = keyring_operations_helper::read_secret(
        innobase::encryption::keyring_reader_service, key_name, nullptr,
        master_key, &key_len, &key_type, PSI_INSTRUMENT_ME);

    /* For compitability with 5.7.11, we need to try to get master
    key with server id when get master key with server uuid
    failure. */
    if (retval != 1) {
      ut_ad(key_type == nullptr);
      if (key_type != nullptr) {
        my_free(key_type);
        key_type = nullptr;
      }

      snprintf(key_name, MASTER_KEY_NAME_MAX_LEN, "%s-%lu-" UINT32PF,
               MASTER_KEY_PREFIX, server_id, *master_key_id);

      retval = keyring_operations_helper::read_secret(
          innobase::encryption::keyring_reader_service, key_name, nullptr,
          master_key, &key_len, &key_type, PSI_INSTRUMENT_ME);
    }

#ifdef UNIV_ENCRYPT_DEBUG
    if (retval == 1) {
      std::ostringstream msg;

      ut_print_buf(msg, *master_key, key_len);

      ib::info(ER_IB_MSG_835) << "Fetched master key: " << *master_key_id
                              << ": {" << msg.str() << "}";
    }
#endif /* UNIV_ENCRYPT_DEBUG */
  }

  if (retval == -1) {
    *master_key = nullptr;
    ib::error(ER_IB_MSG_836) << "Encryption can't find master key, please check"
                             << " the keyring is loaded.";
  }

  if (key_type != nullptr) {
    my_free(key_type);
    key_type = nullptr;
  }

  if (key_id_locked) {
    mutex_exit(&master_key_id_mutex);
  }

#endif /* !UNIV_HOTBACKUP */
}

bool Encryption::fill_encryption_info(
    const Encryption_metadata &encryption_metadata, bool encrypt_key,
    byte *encrypt_info) noexcept {
  byte *master_key = nullptr;
  uint32_t master_key_id = DEFAULT_MASTER_KEY_ID;

#ifndef UNIV_HOTBACKUP
  /* Server uuid must have already been generated */
  ut_ad(strlen(server_uuid) > 0);
#endif

  /* Get master key from keyring. */
  if (encrypt_key) {
    get_master_key(&master_key_id, &master_key);

    if (master_key == nullptr) {
      return (false);
    }

    ut_ad(master_key_id != DEFAULT_MASTER_KEY_ID);
    ut_ad(memcmp(master_key, DEFAULT_MASTER_KEY, sizeof(DEFAULT_MASTER_KEY)) !=
          0);
  }

  /* Encryption info to be filled in following format
    --------------------------------------------------------------------------
   | Magic bytes | master key id | server uuid | tablespace key|iv | checksum |
    --------------------------------------------------------------------------
  */
  ut_ad(encrypt_info != nullptr);
  memset(encrypt_info, 0, INFO_SIZE);

  auto ptr = encrypt_info;

  /* Write Magic bytes */
  memcpy(ptr, KEY_MAGIC_V3, MAGIC_SIZE);
  ptr += MAGIC_SIZE;

  /* Write master key id. */
  mach_write_to_4(ptr, master_key_id);
  ptr += sizeof(uint32_t);

  /* Write server uuid. */
  memcpy(reinterpret_cast<char *>(ptr), s_uuid, sizeof(s_uuid));
  ptr += sizeof(s_uuid) - 1;

  /* Write (and encrypt if needed) key and iv */
  byte key_info[KEY_LEN * 2];
  memset(key_info, 0x0, sizeof(key_info));

  memcpy(key_info, encryption_metadata.m_key, KEY_LEN);

  memcpy(key_info + KEY_LEN, encryption_metadata.m_iv, KEY_LEN);

  if (encrypt_key) {
    /* Encrypt key and iv. */
    auto elen = my_aes_encrypt(key_info, sizeof(key_info), ptr, master_key,
                               KEY_LEN, my_aes_256_ecb, nullptr, false);

    if (elen == MY_AES_BAD_DATA) {
      my_free(master_key);
      return (false);
    }
  } else {
    /* Keep tablespace key unencrypted. Used by clone. */
    memcpy(ptr, key_info, sizeof(key_info));
  }
  ptr += sizeof(key_info);

  /* Write checksum bytes. */
  auto crc = ut_crc32(key_info, sizeof(key_info));
  mach_write_to_4(ptr, crc);

  if (encrypt_key) {
    ut_ad(master_key != nullptr);
    my_free(master_key);
  }

  return (true);
}

const byte *Encryption::get_master_key_from_info(const byte *encrypt_info,
                                                 Version version,
                                                 uint32_t *m_key_id,
                                                 char *srv_uuid,
                                                 byte **master_key) noexcept {
  const byte *ptr = encrypt_info;
  *m_key_id = 0;

  /* Get master key id. */
  uint32_t key_id = mach_read_from_4(ptr);
  ptr += sizeof(uint32_t);

  /* Handle different version encryption information. */
  switch (version) {
    case VERSION_1:
      /* For version 1, it's possible master key id occupied 8 bytes. */
      if (mach_read_from_4(ptr) == 0) {
        ptr += sizeof(uint32);
      }

      /* Get master key. */
      get_master_key(key_id, nullptr, master_key);
      if (*master_key == nullptr) {
        return (encrypt_info);
      }

      *m_key_id = key_id;
      return (ptr);

    case VERSION_2:
      /* For version 2, it's also possible master key id occupied 8 bytes. */
      if (mach_read_from_4(ptr) == 0) {
        ptr += sizeof(uint32);
      }

      /* Read server uuid. */
      memset(srv_uuid, 0, SERVER_UUID_LEN + 1);
      memcpy(srv_uuid, ptr, SERVER_UUID_LEN);

      ut_ad(strlen(srv_uuid) != 0);
      ptr += SERVER_UUID_LEN;

      /* Get master key. */
      get_master_key(key_id, srv_uuid, master_key);
      if (*master_key == nullptr) {
        return (encrypt_info);
      }

      *m_key_id = key_id;
      break;

    case VERSION_3:
      /* Read server uuid. */
      memset(srv_uuid, 0, SERVER_UUID_LEN + 1);
      memcpy(srv_uuid, ptr, SERVER_UUID_LEN);

      ptr += SERVER_UUID_LEN;

      if (key_id == DEFAULT_MASTER_KEY_ID) {
        *master_key = static_cast<byte *>(
            ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, KEY_LEN));
        memcpy(*master_key, DEFAULT_MASTER_KEY, strlen(DEFAULT_MASTER_KEY));
      } else {
        ut_ad(strlen(srv_uuid) != 0);

        /* Get master key. */
        get_master_key(key_id, srv_uuid, master_key);
        if (*master_key == nullptr) {
          return (encrypt_info);
        }
      }

      *m_key_id = key_id;
      break;
  }

  ut_ad(*master_key != nullptr);

  return (ptr);
}

bool Encryption::is_encrypted_with_version(
    const byte *encryption_info, const char *version_magic_bytes) noexcept {
  return std::memcmp(encryption_info, version_magic_bytes,
                     Encryption::MAGIC_SIZE) == 0;
}

bool Encryption::is_encrypted_with_v3(const byte *encryption_info) noexcept {
  return is_encrypted_with_version(encryption_info, Encryption::KEY_MAGIC_V3);
}

bool Encryption::is_encrypted(const byte *encryption_info) noexcept {
  return is_encrypted_with_v3(encryption_info) ||
         is_encrypted_with_version(encryption_info, Encryption::KEY_MAGIC_V2) ||
         is_encrypted_with_version(encryption_info, Encryption::KEY_MAGIC_V1);
}

bool Encryption::decode_encryption_info(Encryption_metadata &e_metadata,
                                        const byte *encryption_info,
                                        bool decrypt_key) noexcept {
  Encryption_key e_key{e_metadata.m_key, e_metadata.m_iv};
  if (decode_encryption_info(dict_sys_t::s_invalid_space_id, e_key,
                             encryption_info, decrypt_key)) {
    e_metadata.m_key_len = Encryption::KEY_LEN;
    e_metadata.m_type = Encryption::AES;
    return true;
  }
  return false;
}

bool Encryption::decode_encryption_info(space_id_t space_id,
                                        Encryption_key &e_key,
                                        const byte *encryption_info,
                                        bool decrypt_key) noexcept {
  const byte *ptr = encryption_info;
  byte *key = e_key.m_key;
  byte *iv = e_key.m_iv;
  uint32_t &master_key_id = e_key.m_master_key_id;

  /* For compatibility with 5.7.11, we need to handle the
  encryption information which created in this old version. */
  Version version;
  if (memcmp(ptr, KEY_MAGIC_V1, MAGIC_SIZE) == 0) {
    version = VERSION_1;
  } else if (memcmp(ptr, KEY_MAGIC_V2, MAGIC_SIZE) == 0) {
    version = VERSION_2;
  } else if (memcmp(ptr, KEY_MAGIC_V3, MAGIC_SIZE) == 0) {
    version = VERSION_3;
  } else {
    /* We don't report an error during recovery, since the
    encryption info maybe hasn't written into datafile when
    the table is newly created. For clone encryption information
    should have been already correct. */
    if (recv_recovery_is_on() && !recv_sys->is_cloned_db) {
      return (true);
    }

    ib::error(ER_IB_MSG_837) << "Failed to decrypt encryption information,"
                             << " found unexpected version of it!";
    return (false);
  }
  ptr += MAGIC_SIZE;

  /* Read master key id and read (decrypt if needed) tablespace key and iv. */
  byte *master_key = nullptr;
  char srv_uuid[SERVER_UUID_LEN + 1];
  byte key_info[KEY_LEN * 2];
  if (decrypt_key) {
    /* Get master key by key id. */
    ptr = get_master_key_from_info(ptr, version, &master_key_id, srv_uuid,
                                   &master_key);

    /* If can't find the master key, return failure. */
    if (master_key == nullptr) {
      return (false);
    }

    /* Decrypt tablespace key and iv. */
    auto len = my_aes_decrypt(ptr, sizeof(key_info), key_info, master_key,
                              KEY_LEN, my_aes_256_ecb, nullptr, false);

    if (master_key_id == DEFAULT_MASTER_KEY_ID) {
      ut::free(master_key);
      /* Re-encrypt tablespace key with current master key */
    } else {
      my_free(master_key);
    }

    /* If decryption failed, return error. */
    if (len == MY_AES_BAD_DATA) {
      return (false);
    }
  } else {
    ut_ad(version == VERSION_3);
    /* Skip master Key and server UUID*/
    ptr += sizeof(uint32_t);
    ptr += SERVER_UUID_LEN;

    /* Get tablespace key information. */
    memcpy(key_info, ptr, sizeof(key_info));
  }
  ptr += sizeof(key_info);

  /* Check checksum bytes. */
  uint32_t crc1 = mach_read_from_4(ptr);
  uint32_t crc2 = ut_crc32(key_info, sizeof(key_info));
  if (crc1 != crc2) {
    /* This check could fail only while decrypting key. */
    ut_ad(decrypt_key);

    ib::error(ER_IB_MSG_839)
        << "Failed to decrypt encryption information,"
        << " please check whether key file has been changed!";

    return (false);
  }

  /* Get tablespace key */
  memcpy(key, key_info, KEY_LEN);

  /* Get tablespace iv */
  memcpy(iv, key_info + KEY_LEN, KEY_LEN);

  if (decrypt_key) {
    /* Update server uuid and master key id in encryption metadata */
    if (master_key_id > s_master_key_id) {
      s_master_key_id = master_key_id;
      memcpy(s_uuid, srv_uuid, sizeof(s_uuid) - 1);
    }

#ifndef UNIV_HOTBACKUP
    if (master_key_id == DEFAULT_MASTER_KEY_ID &&
        space_id != dict_sys_t::s_invalid_space_id) {
      /* Tablespace key needs to be reencrypted with master key */

      if (!srv_master_thread_is_active()) {
        /* Note down this space and rotate key at the end of recovery */
        s_tablespaces_to_reencrypt.push_back(space_id);
      } else {
        /* This tablespace might not be loaded yet. It's tablepace key will be
        reencrypted with new master key once it is loaded in fil_ibd_open() */
      }
    }
#endif
  }

  return (true);
}

bool Encryption::is_encrypted_page(const byte *page) noexcept {
  ulint page_type = mach_read_from_2(page + FIL_PAGE_TYPE);

  return (page_type == FIL_PAGE_ENCRYPTED ||
          page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED ||
          page_type == FIL_PAGE_ENCRYPTED_RTREE);
}

bool Encryption::is_encrypted_log(const byte *block) noexcept {
  return (log_block_get_encrypt_bit(block));
}

bool Encryption::encrypt_log_block(byte *src_ptr,
                                   byte *dst_ptr) const noexcept {
  ulint len = 0;
  ulint data_len;
  ulint main_len;
  ulint remain_len;
  byte remain_buf[MY_AES_BLOCK_SIZE * 2];
  /* in-place encryption is not supported */
  ut_a(src_ptr != dst_ptr);
  /* the buffers can't even overlap */
  ut_ad(src_ptr + OS_FILE_LOG_BLOCK_SIZE <= dst_ptr ||
        dst_ptr + OS_FILE_LOG_BLOCK_SIZE <= src_ptr);
#ifdef UNIV_ENCRYPT_DEBUG
  {
    std::ostringstream msg;

    msg << "Encrypting block: " << log_block_get_hdr_no(src_ptr);
    msg << "{";
    ut_print_buf_hex(msg, src_ptr, OS_FILE_LOG_BLOCK_SIZE);
    msg << "}";

    ib::info(ER_IB_MSG_842) << msg.str();
  }
#endif /* UNIV_ENCRYPT_DEBUG */

  /* This is data size to encrypt. */
  data_len = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE;
  main_len = (data_len / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
  remain_len = data_len - main_len;

  /* Encrypt the block. */
  /* Copy the header as is. */

  /* Skip encryption if header is empty */
  if (ut::is_zeros(src_ptr, LOG_BLOCK_HDR_SIZE)) {
    /* Block is logically empty - this happens in write-ahead.
    Don't waste time on encrypting it. Also, because write-ahead
    blocks are "in future", and we might disable encryption at
    any moment, we should avoid encrypting this block, so that
    one can read it during recovery even when encryption is
    disabled, soon after this call, before block gets filled
    with data. */
    std::memset(dst_ptr, 0x00, OS_FILE_LOG_BLOCK_SIZE);
    return true;
  }

  std::memcpy(dst_ptr, src_ptr, LOG_BLOCK_HDR_SIZE);

  switch (m_type) {
    case NONE:
      ut_error;

    case AES: {
      ut_ad(m_klen == KEY_LEN);

      auto elen = my_aes_encrypt(
          src_ptr + LOG_BLOCK_HDR_SIZE, static_cast<uint32>(main_len),
          dst_ptr + LOG_BLOCK_HDR_SIZE, m_key, static_cast<uint32>(m_klen),
          my_aes_256_cbc, m_iv, false);

      if (elen == MY_AES_BAD_DATA) {
        return (false);
      }

      len = static_cast<ulint>(elen);
      ut_ad(len == main_len);

      /* Copy remain bytes. */
      memcpy(dst_ptr + LOG_BLOCK_HDR_SIZE + len,
             src_ptr + LOG_BLOCK_HDR_SIZE + len,
             OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE - len);

      /* Encrypt the remain bytes. Since my_aes_encrypt
      request the content to encrypt is
      multiple of MY_AES_BLOCK_SIZE, but the block
      content is possibly not, so, we need to handle
      the tail bytes first. */
      if (remain_len != 0) {
        remain_len = MY_AES_BLOCK_SIZE * 2;

        elen = my_aes_encrypt(
            dst_ptr + LOG_BLOCK_HDR_SIZE + data_len - remain_len,
            static_cast<uint32>(remain_len), remain_buf, m_key,
            static_cast<uint32>(m_klen), my_aes_256_cbc, m_iv, false);

        if (elen == MY_AES_BAD_DATA) {
          return (false);
        }

        memcpy(dst_ptr + LOG_BLOCK_HDR_SIZE + data_len - remain_len, remain_buf,
               remain_len);
      }

      break;
    }

    default:
      ut_error;
  }

#ifdef UNIV_ENCRYPT_DEBUG
  {
    std::ostringstream os{};
    os << "Encrypted block " << log_block_get_hdr_no(dst_ptr) << "."
       << std::endl;
    ut_print_buf_hex(os, dst_ptr, OS_FILE_LOG_BLOCK_SIZE);
    os << std::endl;
    ib::info() << os.str();

    byte check_buf[OS_FILE_LOG_BLOCK_SIZE];

    memcpy(check_buf, dst_ptr, OS_FILE_LOG_BLOCK_SIZE);
    log_block_set_encrypt_bit(check_buf, true);
    dberr_t err = decrypt_log(check_buf, OS_FILE_LOG_BLOCK_SIZE);
    if (err != DB_SUCCESS ||
        memcmp(src_ptr, check_buf, OS_FILE_LOG_BLOCK_SIZE) != 0) {
      std::ostringstream msg{};
      ut_print_buf_hex(msg, src_ptr, OS_FILE_LOG_BLOCK_SIZE);
      ib::error() << msg.str();

      msg.seekp(0);
      ut_print_buf_hex(msg, check_buf, OS_FILE_LOG_BLOCK_SIZE);
      ib::fatal(UT_LOCATION_HERE) << msg.str();
    }
  }
#endif /* UNIV_ENCRYPT_DEBUG */

  /* Set the encrypted flag. */
  log_block_set_encrypt_bit(dst_ptr, true);

  return (true);
}

bool Encryption::encrypt_log(byte *src, size_t src_len,
                             byte *dst) const noexcept {
  /* in-place encryption is not supported */
  ut_a(src != dst);
  /* the buffers can't even overlap */
  ut_ad(src + src_len <= dst || dst + src_len <= src);
  byte *src_ptr = src;
  byte *dst_ptr = dst;
  ut_ad(src_len % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_ad(m_type != NONE);

  /* Encrypt the log blocks one by one. */
  while (src_ptr != src + src_len) {
    if (!encrypt_log_block(src_ptr, dst_ptr)) {
      ib::error{ER_IB_MSG_CANT_ENCRYPT_REDO_LOG_DATA};
      return false;
    }

    src_ptr += OS_FILE_LOG_BLOCK_SIZE;
    dst_ptr += OS_FILE_LOG_BLOCK_SIZE;
  }

#ifdef UNIV_ENCRYPT_DEBUG
  {
    byte *check_buf = static_cast<byte *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, src_len));
    memcpy(check_buf, dst, src_len);

    dberr_t err = decrypt_log(check_buf, src_len);
    if (err != DB_SUCCESS || memcmp(src, check_buf, src_len) != 0) {
      std::ostringstream msg{};
      ut_print_buf_hex(msg, src, src_len);
      ib::error() << msg.str();

      msg.seekp(0);
      ut_print_buf_hex(msg, check_buf, src_len);
      ib::fatal(UT_LOCATION_HERE) << msg.str();
    }
    ut::free(check_buf);
  }
#endif /* UNIV_ENCRYPT_DEBUG */

  return true;
}

bool Encryption::encrypt_low(byte *src, ulint src_len, byte *dst,
                             ulint *dst_len) const noexcept {
  const uint16_t page_type = mach_read_from_2(src + FIL_PAGE_TYPE);

  /* Shouldn't encrypt an already encrypted page. */
  ut_ad(!is_encrypted_page(src));

  /* This is data size to encrypt. */
  auto src_enc_len = src_len;

  /* In FIL_PAGE_VERSION_2, we encrypt the actual compressed data length. */
  if (page_type == FIL_PAGE_COMPRESSED) {
    src_enc_len =
        mach_read_from_2(src + FIL_PAGE_COMPRESS_SIZE_V1) + FIL_PAGE_DATA;
    /* Extend src_enc_len if needed */
    if (src_enc_len < MIN_ENCRYPTION_LEN) {
      src_enc_len = MIN_ENCRYPTION_LEN;
    }
    ut_a(src_enc_len <= src_len);
  }

  /* Only encrypt the data + trailer, leave the header alone */

  switch (m_type) {
    case NONE:
      ut_error;

    case AES: {
      ut_ad(m_klen == KEY_LEN);

      /* Total length of the data to encrypt. */
      const auto data_len = src_enc_len - FIL_PAGE_DATA;

      /* Server encryption functions expect input data to be in multiples
      of MY_AES_BLOCK SIZE. Therefore we encrypt the overlapping data of
      the chunk_len and trailer_len twice. First we encrypt the bigger
      chunk of data then we do the trailer. The trailer encryption block
      starts at 2 * MY_AES_BLOCK_SIZE bytes offset from the end of the
      enc_len.  During decryption we do the reverse of the above process. */
      ut_ad(data_len >= 2 * MY_AES_BLOCK_SIZE);

      const auto chunk_len = (data_len / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
      const auto remain_len = data_len - chunk_len;

      auto elen = my_aes_encrypt(
          src + FIL_PAGE_DATA, static_cast<uint32>(chunk_len),
          dst + FIL_PAGE_DATA, m_key, static_cast<uint32>(m_klen),
          my_aes_256_cbc, m_iv, false);

      if (elen == MY_AES_BAD_DATA) {
        const auto page_id = page_get_page_id(src);

        ib::error(ER_IB_MSG_844) << " Can't encrypt data of page " << page_id;
        *dst_len = src_len;
        return false;
      }

      const auto len = static_cast<size_t>(elen);
      ut_a(len == chunk_len);

      /* Encrypt the trailing bytes. */
      if (remain_len != 0) {
        /* Copy remaining bytes and page tailer. */
        memcpy(dst + FIL_PAGE_DATA + len, src + FIL_PAGE_DATA + len,
               remain_len);

        constexpr size_t trailer_len = MY_AES_BLOCK_SIZE * 2;
        byte buf[trailer_len];

        elen = my_aes_encrypt(dst + FIL_PAGE_DATA + data_len - trailer_len,
                              static_cast<uint32>(trailer_len), buf, m_key,
                              static_cast<uint32>(m_klen), my_aes_256_cbc, m_iv,
                              false);

        if (elen == MY_AES_BAD_DATA) {
          const auto page_id = page_get_page_id(src);

          ib::error(ER_IB_MSG_845) << " Can't encrypt data of page," << page_id;
          *dst_len = src_len;
          return false;
        }

        ut_a(static_cast<size_t>(elen) == trailer_len);

        memcpy(dst + FIL_PAGE_DATA + data_len - trailer_len, buf, trailer_len);
      }

      break;
    }

    default:
      ut_error;
  }

  /* Copy the header as is. */
  memmove(dst, src, FIL_PAGE_DATA);
  ut_ad(memcmp(src, dst, FIL_PAGE_DATA) == 0);

  /* Add encryption control information. Required for decrypting. */
  if (page_type == FIL_PAGE_COMPRESSED) {
    /* If the page is compressed, we don't need to save the
    original type, since it is done in compression already. */
    mach_write_to_2(dst + FIL_PAGE_TYPE, FIL_PAGE_COMPRESSED_AND_ENCRYPTED);
    ut_ad(memcmp(src + FIL_PAGE_TYPE + 2, dst + FIL_PAGE_TYPE + 2,
                 FIL_PAGE_DATA - FIL_PAGE_TYPE - 2) == 0);
  } else if (page_type == FIL_PAGE_RTREE) {
    /* If the page is R-tree page, we need to save original type. */
    mach_write_to_2(dst + FIL_PAGE_TYPE, FIL_PAGE_ENCRYPTED_RTREE);
  } else {
    mach_write_to_2(dst + FIL_PAGE_TYPE, FIL_PAGE_ENCRYPTED);
    mach_write_to_2(dst + FIL_PAGE_ORIGINAL_TYPE_V1, page_type);
  }

  /* Add padding 0 for unused portion */
  if (src_len > src_enc_len) {
    memset(dst + src_enc_len, 0, src_len - src_enc_len);
  }

  *dst_len = src_len;

  return (true);
}

byte *Encryption::encrypt(const IORequest &type, byte *src, ulint src_len,
                          byte *dst, ulint *dst_len) const noexcept {
  /* For encrypting redo log, take another way. */
  ut_ad(!type.is_log());

#ifdef UNIV_ENCRYPT_DEBUG

  const auto space_id =
      mach_read_from_4(src + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  const auto page_no = mach_read_from_4(src + FIL_PAGE_OFFSET);

  fprintf(stderr, "Encrypting page:%" PRIu32 ".%" PRIu32 " len: " ULINTPF "\n ",
          space_id, page_no, src_len);
  ut_print_buf(stderr, m_key, 32);
  ut_print_buf(stderr, m_iv, 32);

#endif /* UNIV_ENCRYPT_DEBUG */

  ut_ad(m_type != NONE);

  if (!encrypt_low(src, src_len, dst, dst_len)) {
    return (src);
  }

#ifdef UNIV_ENCRYPT_DEBUG
  {
    byte *check_buf = static_cast<byte *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, src_len));
    byte *buf2 = static_cast<byte *>(
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, src_len));

    memcpy(check_buf, dst, src_len);

    dberr_t err = decrypt(type, check_buf, src_len, buf2, src_len);
    if (err != DB_SUCCESS ||
        memcmp(src + FIL_PAGE_DATA, check_buf + FIL_PAGE_DATA,
               src_len - FIL_PAGE_DATA) != 0) {
      ut_print_buf(stderr, src, src_len);
      ut_print_buf(stderr, check_buf, src_len);
      ut_d(ut_error);
    }
    ut::free(buf2);
    ut::free(check_buf);

    fprintf(stderr, "Encrypted page:%" PRIu32 ".%" PRIu32 "\n", space_id,
            page_no);
  }
#endif /* UNIV_ENCRYPT_DEBUG */
  return dst;
}

dberr_t Encryption::decrypt_log_block(byte *const buf) const noexcept {
  /* This is the data we have to decrypt */
  byte *const data = buf + LOG_BLOCK_HDR_SIZE;
  /* This is data size to decrypt. */
  constexpr size_t data_len = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE;
  /* Temporary storage for the data part of the block, into which we put the
  originally encrypted data (after decrypting the "remainder" if necessary). */
  byte tmp[data_len];
  /* This is the number of full blocks encrypted with AES in the tmp. */
  constexpr size_t main_len =
      (data_len / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
  /* This is the number of unencrypted bytes at the end of the tmp. */
  constexpr size_t remain_len = data_len - main_len;

  switch (m_type) {
    case AES: {
      int elen;

      /* First decrypt the last 2 blocks data of data, since
      data is not block aligned. */
      if constexpr (remain_len != 0) {
        ut_ad(m_klen == KEY_LEN);

        byte two_blocks[MY_AES_BLOCK_SIZE * 2];

        static_assert(remain_len <= sizeof(two_blocks));
        static_assert(sizeof(two_blocks) <= data_len);

        /* Copy the last 2 blocks. */
        memcpy(two_blocks, data + data_len - sizeof(two_blocks),
               sizeof(two_blocks));

        elen = my_aes_decrypt(
            two_blocks, static_cast<uint32>(sizeof(two_blocks)),
            tmp + data_len - sizeof(two_blocks), m_key,
            static_cast<uint32>(m_klen), my_aes_256_cbc, m_iv, false);
        if (elen == MY_AES_BAD_DATA) {
          return (DB_IO_DECRYPT_FAIL);
        }

        /* Copy the other data bytes to temp area. */
        memcpy(tmp, data, data_len - sizeof(two_blocks));
      } else {
        ut_ad(data_len == main_len);

        /* Copy the data bytes to temp area. */
        memcpy(tmp, data, data_len);
      }

      /* Then decrypt the main data */
      elen = my_aes_decrypt(tmp, static_cast<uint32>(main_len), data, m_key,
                            static_cast<uint32>(m_klen), my_aes_256_cbc, m_iv,
                            false);
      if (elen == MY_AES_BAD_DATA) {
        return (DB_IO_DECRYPT_FAIL);
      }

      ut_ad(elen == static_cast<int>(main_len));

      /* Copy the remaining bytes. */
      memcpy(data + main_len, tmp + main_len, remain_len);

      break;
    }

    default:
      return (DB_UNSUPPORTED);
  }

#ifdef UNIV_ENCRYPT_DEBUG
  {
    std::ostringstream msg{};
    msg << "Decrypted block " << log_block_get_hdr_no(buf) << "." << std::endl;
    ut_print_buf_hex(msg, buf, OS_FILE_LOG_BLOCK_SIZE);
    msg << std::endl;
    ib::info() << msg.str();
  }
#endif

  /* Reset the encrypted flag. */
  log_block_set_encrypt_bit(buf, false);

  return (DB_SUCCESS);
}

dberr_t Encryption::decrypt_log(byte *buf, size_t buf_len) const noexcept {
  dberr_t ret;

  /* Encrypt the log blocks one by one. */
  for (byte *ptr = buf; ptr != buf + buf_len; ptr += OS_FILE_LOG_BLOCK_SIZE) {
#ifdef UNIV_ENCRYPT_DEBUG
    {
      std::ostringstream msg;

      msg << "Decrypting block: " << log_block_get_hdr_no(ptr) << std::endl;
      msg << "data={" << std::endl;
      ut_print_buf_hex(msg, ptr, OS_FILE_LOG_BLOCK_SIZE);
      msg << std::endl << "}";

      ib::info(ER_IB_MSG_847) << msg.str();
    }
#endif /* UNIV_ENCRYPT_DEBUG */

    /* If it's not an encrypted block, skip it. */
    if (!is_encrypted_log(ptr)) {
      continue;
    }

    /* Decrypt block */
    ret = decrypt_log_block(ptr);
    if (ret != DB_SUCCESS) {
      return (ret);
    }
  }

  return (DB_SUCCESS);
}

dberr_t Encryption::decrypt(const IORequest &type, byte *src, ulint src_len,
                            byte *tmp, ulint tmp_len) const noexcept {
  ulint data_len;
  ulint main_len;
  ulint remain_len;
  ulint original_type;
  byte remain_buf[MY_AES_BLOCK_SIZE * 2];
  file::Block *block;

  /* If the page is encrypted, then we need key to decrypt it. */
  if (is_encrypted_page(src) && m_type == NONE) {
    return DB_IO_DECRYPT_FAIL;
  }

  if (!is_encrypted_page(src) || m_type == NONE) {
    /* There is nothing we can do. */
    return DB_SUCCESS;
  }

  /* For compressed page, we need to get the compressed size
  for decryption */
  const ulint page_type = mach_read_from_2(src + FIL_PAGE_TYPE);

  /* Actual length of the compressed data */
  uint16_t z_len = 0;

  if (page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
    z_len = mach_read_from_2(src + FIL_PAGE_COMPRESS_SIZE_V1);
    src_len = z_len + FIL_PAGE_DATA;

    Compression::meta_t header;
    Compression::deserialize_header(src, &header);
    if (header.m_version == Compression::FIL_PAGE_VERSION_1) {
      src_len = ut_calc_align(src_len, type.block_size());
    } else {
      /* Extend src_len if needed */
      if (src_len < MIN_ENCRYPTION_LEN) {
        src_len = MIN_ENCRYPTION_LEN;
      }
    }
  }

#ifdef UNIV_ENCRYPT_DEBUG
  const page_id_t page_id(
      mach_read_from_4(src + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID),
      mach_read_from_4(src + FIL_PAGE_OFFSET));

  {
    std::ostringstream msg;

    msg << "Decrypting page: " << page_id << " len: " << src_len << std::endl;
    msg << "key={";
    ut_print_buf(msg, m_key, 32);
    msg << "}" << std::endl << "iv= {";
    ut_print_buf(msg, m_iv, 32);
    msg << "}";

    ib::info(ER_IB_MSG_848) << msg.str();
  }
#endif /* UNIV_ENCRYPT_DEBUG */

  original_type =
      static_cast<uint16_t>(mach_read_from_2(src + FIL_PAGE_ORIGINAL_TYPE_V1));

  byte *ptr = src + FIL_PAGE_DATA;

  /* The caller doesn't know what to expect */
  if (tmp == nullptr) {
    block = os_alloc_block();
    tmp = block->m_ptr;
  } else {
    ut_a(src_len <= tmp_len);
    block = nullptr;
  }

  data_len = src_len - FIL_PAGE_DATA;
  main_len = (data_len / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
  remain_len = data_len - main_len;

  switch (m_type) {
    case AES: {
      lint elen;

      /* First decrypt the last 2 blocks data of data, since
      data is no block aligned. */
      if (remain_len != 0) {
        ut_ad(m_klen == KEY_LEN);

        remain_len = MY_AES_BLOCK_SIZE * 2;

        /* Copy the last 2 blocks. */
        memcpy(remain_buf, ptr + data_len - remain_len, remain_len);

        elen = my_aes_decrypt(remain_buf, static_cast<uint32>(remain_len),
                              tmp + data_len - remain_len, m_key,
                              static_cast<uint32>(m_klen), my_aes_256_cbc, m_iv,
                              false);

        if (elen == MY_AES_BAD_DATA) {
          if (block != nullptr) {
            os_free_block(block);
          }

          return (DB_IO_DECRYPT_FAIL);
        }

        ut_ad(static_cast<ulint>(elen) == remain_len);

        /* Copy the other data bytes to temp area. */
        memcpy(tmp, ptr, data_len - remain_len);
      } else {
        ut_ad(data_len == main_len);

        /* Copy the data bytes to temp area. */
        memcpy(tmp, ptr, data_len);
      }

      /* Then decrypt the main data */
      elen = my_aes_decrypt(tmp, static_cast<uint32>(main_len), ptr, m_key,
                            static_cast<uint32>(m_klen), my_aes_256_cbc, m_iv,
                            false);
      if (elen == MY_AES_BAD_DATA) {
        if (block != nullptr) {
          os_free_block(block);
        }

        return (DB_IO_DECRYPT_FAIL);
      }

      ut_ad(static_cast<ulint>(elen) == main_len);

      /* Copy the remain bytes. */
      memcpy(ptr + main_len, tmp + main_len, data_len - main_len);

      break;
    }

    default:
      if (!type.is_dblwr()) {
        ib::error(ER_IB_MSG_849)
            << "Encryption algorithm support missing: " << to_string(m_type);
      }

      if (block != nullptr) {
        os_free_block(block);
      }

      return (DB_UNSUPPORTED);
  }

  /* Restore the original page type. If it's a compressed and
  encrypted page, just reset it as compressed page type, since
  we will do uncompress later. */

  if (page_type == FIL_PAGE_ENCRYPTED) {
    mach_write_to_2(src + FIL_PAGE_TYPE, original_type);
    mach_write_to_2(src + FIL_PAGE_ORIGINAL_TYPE_V1, 0);
  } else if (page_type == FIL_PAGE_ENCRYPTED_RTREE) {
    mach_write_to_2(src + FIL_PAGE_TYPE, FIL_PAGE_RTREE);
  } else {
    ut_ad(page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED);
    mach_write_to_2(src + FIL_PAGE_TYPE, FIL_PAGE_COMPRESSED);
  }

  if (block != nullptr) {
    os_free_block(block);
  }

#ifdef UNIV_DEBUG
  {
    /* Check if all the padding bytes are zeroes. */
    if (page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
      uint32_t padding = src_len - FIL_PAGE_DATA - z_len;
      for (uint32_t i = 0; i < padding; ++i) {
        byte *pad = src + z_len + FIL_PAGE_DATA + i;
        ut_ad(*pad == 0x0);
      }
    }
  }
#endif /* UNIV_DEBUG */

#ifdef UNIV_ENCRYPT_DEBUG
  ib::info(ER_IB_MSG_850) << "Decrypted page: " << page_id;
#endif /* UNIV_ENCRYPT_DEBUG */

  DBUG_EXECUTE_IF("ib_crash_during_decrypt_page", DBUG_SUICIDE(););

  return (DB_SUCCESS);
}

#ifndef UNIV_HOTBACKUP
bool Encryption::check_keyring() noexcept {
  bool ret = false;
  byte *master_key = nullptr;

  if (s_master_key_id == DEFAULT_MASTER_KEY_ID) {
    /* This is the first time encryption is being used or till now no encrypted
    tablespace is loaded. */
    static bool checked = false;
    if (checked) {
      return true;
    }

    /* Generate/Fetch/Delete a dummy master key to confirm keyring is up and
    running. */
    size_t key_len;
    char *key_type = nullptr;
    char key_name[MASTER_KEY_NAME_MAX_LEN];

    key_name[sizeof(DEFAULT_MASTER_KEY)] = 0;

    strncpy(key_name, DEFAULT_MASTER_KEY, sizeof(key_name));

    /*
      We call keyring API to generate master key here.
      We don't care about failure at this point because
      master key may very well be present in keyring.
      All we are trying to check is keyring is functional.
    */
    (void)innobase::encryption::generate_key(key_name, innodb_key_type,
                                             KEY_LEN);

    /* We call keyring API to get master key here. */
    int retval = keyring_operations_helper::read_secret(
        innobase::encryption::keyring_reader_service, key_name, nullptr,
        &master_key, &key_len, &key_type, PSI_INSTRUMENT_ME);

    if (retval == -1) {
      ib::error(ER_IB_MSG_851) << "Check keyring fail, please check the"
                               << " keyring is loaded.";
    } else {
      innobase::encryption::remove_key(key_name);
      ret = true;
      checked = true;
    }

    if (key_type != nullptr) {
      my_free(key_type);
    }

    if (master_key != nullptr) {
      my_free(master_key);
    }
  } else {
    uint32_t master_key_id;

    Encryption::get_master_key(&master_key_id, &master_key);
    if (master_key != nullptr) {
      my_free(master_key);
      ret = true;
    }
  }

  return (ret);
}
#endif /* !UNIV_HOTBACKUP */

Encryption::Type Encryption::get_type() const { return m_type; }

void Encryption::set_type(Encryption::Type type) { m_type = type; }

void Encryption::set_key(const byte *key) { m_key = key; }

ulint Encryption::get_key_length() const { return m_klen; }

void Encryption::set_key_length(ulint klen) { m_klen = klen; }

void Encryption::set_initial_vector(const byte *iv) { m_iv = iv; }

uint32_t Encryption::get_master_key_id() { return s_master_key_id; }

void Encryption::set_or_generate(Type type, byte *key, byte *iv,
                                 Encryption_metadata &metadata) {
  ut_ad(type != Encryption::NONE);
  metadata.m_type = type;
  metadata.m_key_len = Encryption::KEY_LEN;
  if (key == nullptr && iv == nullptr) {
    Encryption::random_value(metadata.m_key);
    Encryption::random_value(metadata.m_iv);
  } else if (key != nullptr && iv != nullptr) {
    memcpy(metadata.m_key, key, Encryption::KEY_LEN);
    memcpy(metadata.m_iv, iv, Encryption::KEY_LEN);
  } else {
    ut_error;
  }
}
