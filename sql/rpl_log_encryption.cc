/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/rpl_log_encryption.h"
#include <sql/mysqld.h>
#include <string.h>
#include <algorithm>
#include <sstream>
#include "event_reader.h"
#include "my_byteorder.h"

#ifdef MYSQL_SERVER
#include "byteorder.h"
#include "my_aes.h"
#include "my_rnd.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/service_mysql_keyring.h"
#include "sql/binlog.h"
#include "sql/rpl_slave.h"

Rpl_encryption rpl_encryption;

void Rpl_encryption::report_keyring_error(Keyring_status error) {
  Rpl_encryption::report_keyring_error(error, nullptr);
}

void Rpl_encryption::report_keyring_error(Keyring_status error,
                                          const char *key_id) {
  switch (error) {
    case Keyring_status::KEYRING_ERROR_FETCHING:
      if (current_thd)
        my_error(ER_RPL_ENCRYPTION_FAILED_TO_FETCH_KEY, MYF(0));
      else
        LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_FAILED_TO_FETCH_KEY);
      break;
    case Keyring_status::KEY_NOT_FOUND:
      if (current_thd)
        my_error(ER_RPL_ENCRYPTION_KEY_NOT_FOUND, MYF(0));
      else
        LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_KEY_NOT_FOUND);
      break;
    case Keyring_status::UNEXPECTED_KEY_SIZE:
    case Keyring_status::UNEXPECTED_KEY_TYPE:
      if (current_thd)
        my_error(ER_RPL_ENCRYPTION_KEYRING_INVALID_KEY, MYF(0));
      else
        LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_KEYRING_INVALID_KEY);
      break;
    case Keyring_status::KEY_EXISTS_UNEXPECTED:
      if (current_thd)
        my_error(ER_RPL_ENCRYPTION_KEY_EXISTS_UNEXPECTED, MYF(0), key_id);
      else
        LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_KEY_EXISTS_UNEXPECTED,
               key_id);
      break;
    case Keyring_status::KEYRING_ERROR_GENERATING:
      if (current_thd)
        my_error(ER_RPL_ENCRYPTION_FAILED_TO_GENERATE_KEY, MYF(0));
      else
        LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_FAILED_TO_GENERATE_KEY);
      break;
    case Keyring_status::KEYRING_ERROR_STORING:
      if (current_thd)
        my_error(ER_RPL_ENCRYPTION_FAILED_TO_STORE_KEY, MYF(0));
      else
        LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_FAILED_TO_STORE_KEY);
      break;
    case Keyring_status::KEYRING_ERROR_REMOVING:
      if (current_thd)
        push_warning(current_thd, ER_RPL_ENCRYPTION_FAILED_TO_REMOVE_KEY);
      else
        LogErr(WARNING_LEVEL, ER_SERVER_RPL_ENCRYPTION_FAILED_TO_REMOVE_KEY);
      break;
    case Keyring_status::SUCCESS:
    default:
      DBUG_ASSERT(false);
  }
}

bool Rpl_encryption::initialize() {
  DBUG_ENTER("Rpl_encryption::initialize");

#ifndef DBUG_OFF
  m_initialized = true;
  DBUG_PRINT("debug", ("m_enabled= %s", m_enabled ? "true" : "false"));
  DBUG_PRINT("debug", ("m_rotate_at_startup= %s",
                       m_rotate_at_startup ? "true" : "false"));
#endif

  if (m_rotate_at_startup && !m_enabled) {
    LogErr(WARNING_LEVEL,
           ER_SERVER_RPL_ENCRYPTION_IGNORE_ROTATE_MASTER_KEY_AT_STARTUP);
  }

  /* Only recover master key if option is enabled */
  if (m_enabled) {
    if (m_rotate_at_startup && remove_seqnos_from_keyring()) DBUG_RETURN(true);
    if (recover_master_key()) DBUG_RETURN(true);
    if (m_master_key_seqno == 0 && first_time_enable(Key_rotation_step::START))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

bool Rpl_encryption::remove_seqnos_from_keyring() {
  DBUG_ENTER("Rpl_encryption::remove_seqnos_from_keyring");
  DBUG_ASSERT(m_initialized);
  DBUG_ASSERT(m_enabled);

  auto master_key_seqno = get_master_key_seqno_from_keyring();
  auto new_master_key_seqno = get_new_master_key_seqno_from_keyring();

  /* Remove master key seqno from keyring */
  if (master_key_seqno.first == Keyring_status::KEYRING_ERROR_FETCHING) {
    goto error;
  }
  if (master_key_seqno.first != Keyring_status::KEY_NOT_FOUND &&
      remove_master_key_seqno_from_keyring()) {
    goto error;
  }

  /* Remove new master key seqno from keyring */
  if (new_master_key_seqno.first == Keyring_status::KEYRING_ERROR_FETCHING) {
    goto error;
  }
  if (new_master_key_seqno.first != Keyring_status::KEY_NOT_FOUND &&
      remove_new_master_key_seqno_from_keyring()) {
    goto error;
  }

  DBUG_RETURN(false);

error:
  LogErr(ERROR_LEVEL,
         ER_SERVER_RPL_ENCRYPTION_UNABLE_TO_ROTATE_MASTER_KEY_AT_STARTUP);
  DBUG_RETURN(true);
}

bool Rpl_encryption::recover_master_key() {
  DBUG_ENTER("Rpl_encryption::recover_master_key");
  DBUG_ASSERT(m_master_key_recovered == false);
  bool master_key_index_error = false;
  bool master_key_error = false;

  DBUG_PRINT("debug", ("m_master_key_seqno=%u", m_master_key_seqno));
  /* Retrieve master key seqno from keyring */
  auto master_key_seqno = get_master_key_seqno_from_keyring();
  m_master_key_seqno = master_key_seqno.second;
  DBUG_PRINT("debug", ("m_master_key_seqno=%u", m_master_key_seqno));
  /* No keyring error */
  if (master_key_seqno.first == Keyring_status::KEYRING_ERROR_FETCHING)
    master_key_index_error = true;
  DBUG_PRINT("debug", ("master_key_index_error= %s",
                       master_key_index_error ? "true" : "false"));

  /* Retrieve master key from keyring */
  if (m_master_key_seqno) {
    m_master_key.m_id =
        Rpl_encryption_header::seqno_to_key_id(m_master_key_seqno);
    auto master_key =
        get_key(m_master_key.m_id, Rpl_encryption_header::get_key_type());
    m_master_key.m_value.assign(master_key.second);
    /* No keyring error */
    if (master_key.first == Keyring_status::KEYRING_ERROR_FETCHING)
      master_key_error = true;
    DBUG_PRINT("debug", ("master_key_error= %s",
                         master_key_index_error ? "true" : "false"));
  }

  DBUG_PRINT("debug", ("m_enabled= %s", m_enabled ? "true" : "false"));

  /* Check if master key "first time enable" finalized properly */
  auto new_master_key_seqno = get_new_master_key_seqno_from_keyring();
  if (new_master_key_seqno.first == Keyring_status::SUCCESS) {
    /* A first key generation was interrupted after DETERMINE_NEXT_SEQNO */
    Key_rotation_step recover_from = Key_rotation_step::GENERATE_NEW_MASTER_KEY;
    std::string new_master_key_id =
        Rpl_encryption_header::seqno_to_key_id(new_master_key_seqno.second);
    auto new_master_key =
        get_key(new_master_key_id, Rpl_encryption_header::get_key_type());
    if (new_master_key.first == Keyring_status::SUCCESS) {
      recover_from = Key_rotation_step::STORE_MASTER_KEY_INDEX;
      m_master_key.m_id = new_master_key_id;
      m_master_key.m_value.assign(new_master_key.second);
    }
    if (m_master_key_seqno == new_master_key_seqno.second) {
      recover_from = Key_rotation_step::ROTATE_LOGS;
    }

    /* Continue the "first time enable" procedure */
    if (rotate_master_key(recover_from, new_master_key_seqno.second))
      DBUG_RETURN(true);
    master_key_index_error = false;
    master_key_error = false;
  }

  DBUG_PRINT("debug", ("m_enabled= %s", m_enabled ? "true" : "false"));
  DBUG_PRINT("debug", ("master_key_index_error= %s",
                       master_key_index_error ? "true" : "false"));
  DBUG_PRINT("debug", ("master_key_error= %s",
                       master_key_index_error ? "true" : "false"));
  if (m_enabled && (master_key_index_error || master_key_error)) {
    if (current_thd)
      my_error(ER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED, MYF(0));
    else
      LogErr(ERROR_LEVEL, ER_SERVER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED,
             MYF(0));
    DBUG_RETURN(true);
  }

  m_master_key_recovered = true;
  DBUG_PRINT("debug", ("m_master_key_seqno= %u", m_master_key_seqno));

  DBUG_RETURN(false);
}

const Rpl_encryption::Rpl_encryption_key Rpl_encryption::get_master_key() {
  DBUG_ENTER("Rpl_encryption::get_master_key");
  DBUG_ASSERT(m_initialized);
  /* A master key shall already exists when this function is called */
  DBUG_ASSERT(!m_master_key.m_id.empty());
  DBUG_ASSERT(!m_master_key.m_value.empty());
  DBUG_RETURN(m_master_key);
}

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

bool Rpl_encryption::enable(THD *thd) {
  DBUG_ENTER("Rpl_encryption::enable");
  DBUG_ASSERT(m_initialized);

  m_enabled = true;
  m_skip_logs_rotation = false;

  DBUG_PRINT("debug", ("m_master_key_recovered= %s",
                       m_master_key_recovered ? "true" : "false"));

  bool res = false;
  /* Recover master key if not recovered yet */
  if (!m_master_key_recovered) res = recover_master_key();

  if (!res) {
    DBUG_PRINT("debug", ("m_master_key_seqno= %u", m_master_key_seqno));
    if (m_master_key_seqno == 0) {
      res = rotate_master_key();
    } else {
      if (!m_skip_logs_rotation) rotate_logs(thd);
    }
  }

  /* Revert enabling on error */
  if (res)
    m_enabled = false;
  else {
    /* Cleanup any error if we are going to enable the option */
    if (current_thd->is_error()) current_thd->clear_error();
  }
  DBUG_PRINT("debug", ("m_enabled= %s", m_enabled ? "true" : "false"));
  DBUG_RETURN(res);
}

void Rpl_encryption::disable(THD *thd) {
  DBUG_ENTER("Rpl_encryption::disable");
  DBUG_ASSERT(m_initialized);
  m_enabled = false;
  rotate_logs(thd);
  /* Cleanup any error if we are going to disable the option */
  if (current_thd->is_error()) current_thd->clear_error();
  m_master_key_recovered = false;
  DBUG_VOID_RETURN;
}

bool Rpl_encryption::is_enabled() {
  bool res = false;
  res = m_enabled &&                    // The option is enabled
        m_master_key_recovered &&       // Master key was recovered
        !m_master_key.m_id.empty() &&   // Master key ID is not empty
        !m_master_key.m_value.empty();  // Master key value is not empty
  return res;
}

const bool &Rpl_encryption::get_enabled_var() { return m_enabled; }

const bool &Rpl_encryption::get_master_key_rotation_at_startup_var() {
  return m_rotate_at_startup;
}

const char *Rpl_encryption::SEQNO_KEY_TYPE = "AES";

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
    error = Keyring_status::KEYRING_ERROR_FETCHING;
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

bool Rpl_encryption::first_time_enable(Key_rotation_step step,
                                       uint32_t new_master_key_seqno) {
  DBUG_ENTER("Rpl_encryption::first_time_enable");
  DBUG_ASSERT(m_master_key_recovered == true ||
              step != Key_rotation_step::START);

  DBUG_PRINT("debug", ("new_master_key_seqno=%u", new_master_key_seqno));
  m_master_key_recovered = false;

  switch (step) {
    case Key_rotation_step::START:
    case Key_rotation_step::DETERMINE_NEXT_SEQNO: {
      DBUG_ASSERT(new_master_key_seqno == 0);
      Keyring_status candidate_key_fetch_status;
      new_master_key_seqno = m_master_key_seqno;
      do {
        ++new_master_key_seqno;
        /* Check if the key already exists */
        std::string candidate_key_id =
            Rpl_encryption_header::seqno_to_key_id(new_master_key_seqno);
        auto pair =
            get_key(candidate_key_id, Rpl_encryption_header::get_key_type());
        /* If unable to check if the key already exists */
        if (pair.first == Keyring_status::KEYRING_ERROR_FETCHING) {
          Rpl_encryption::report_keyring_error(pair.first);
          DBUG_RETURN(true);
        }
        /* If the key already exists on keyring */
        candidate_key_fetch_status = pair.first;
      } while (candidate_key_fetch_status != Keyring_status::KEY_NOT_FOUND);
      if (set_new_master_key_seqno_on_keyring(new_master_key_seqno))
        DBUG_RETURN(true);
    }
      /* FALLTHROUGH */
    case Key_rotation_step::GENERATE_NEW_MASTER_KEY:
      if (generate_master_key_on_keyring(new_master_key_seqno))
        DBUG_RETURN(true);
      /* FALLTHROUGH */
    case Key_rotation_step::STORE_MASTER_KEY_INDEX:
      DBUG_ASSERT(m_master_key_seqno == 0);
      if (set_master_key_seqno_on_keyring(new_master_key_seqno))
        DBUG_RETURN(true);
      /* The master key is now usable */
      m_master_key_seqno = new_master_key_seqno;
      /* FALLTHROUGH */
    case Key_rotation_step::ROTATE_LOGS:
      m_master_key_recovered = true;
      /* No errors from here, just warnings */
      if (current_thd) rotate_logs(current_thd);
      m_skip_logs_rotation = true;
      /* FALLTHROUGH */
    case Key_rotation_step::REMOVE_KEY_ROTATION_TAG:
      remove_new_master_key_seqno_from_keyring();
  }

  DBUG_RETURN(false);
}

bool Rpl_encryption::rotate_master_key(Key_rotation_step step,
                                       uint32_t new_master_key_seqno) {
  DBUG_ENTER("Rpl_encryption::rotate_master_key");
  bool res = false;

  if (m_master_key_seqno == 0 ||
      (new_master_key_seqno > 0 && step != Key_rotation_step::START)) {
    /* Special case: first time enabling */
    res = first_time_enable(step, new_master_key_seqno);
  } else {
    /* This server only support master key rotation at startup */
    DBUG_ASSERT(false);
    res = true;
  }

  DBUG_RETURN(res);
}

void Rpl_encryption::rotate_logs(THD *thd) {
  DBUG_ENTER("Rpl_encryption::rotate_logs");
  if ((mysql_bin_log.is_open() && mysql_bin_log.rotate_and_purge(thd, true)) ||
      flush_relay_logs_cmd(thd)) {
    push_warning(thd, ER_RPL_ENCRYPTION_FAILED_TO_ROTATE_LOGS);
  }
  DBUG_VOID_RETURN;
}

std::pair<Rpl_encryption::Keyring_status, uint32_t>
Rpl_encryption::get_seqno_from_keyring(std::string key_id) {
  DBUG_ENTER("Rpl_encryption::get_seqno_from_keyring");
  auto fetched_key = get_key(key_id, SEQNO_KEY_TYPE, SEQNO_KEY_LENGTH);
  uint32_t seqno = 0;
  if (fetched_key.first == Keyring_status::SUCCESS) {
    const void *key = fetched_key.second.c_str();
    memcpy(&seqno, key, sizeof(seqno));
    seqno = le32toh(seqno);
  }
  DBUG_RETURN(std::make_pair(fetched_key.first, seqno));
}

bool Rpl_encryption::set_seqno_on_keyring(std::string key_id, uint32_t seqno) {
  DBUG_ENTER("Rpl_encryption::set_seqno_on_keyring");
  unsigned char key[SEQNO_KEY_LENGTH]{0};
  int4store(key, seqno);
  DBUG_PRINT("debug", ("key_id= '%s'. seqno= %u", key_id.c_str(), seqno));
  if ((DBUG_EVALUATE_IF("rpl_encryption_first_time_enable_1", 1, 0) &&
       key_id.compare(get_new_master_key_seqno_key_id()) == 0) ||
      (DBUG_EVALUATE_IF("rpl_encryption_first_time_enable_3", 1, 0) &&
       key_id.compare(get_master_key_seqno_key_id()) == 0) ||
      my_key_store(key_id.c_str(), SEQNO_KEY_TYPE, nullptr, key,
                   SEQNO_KEY_LENGTH)) {
    report_keyring_error(Keyring_status::KEYRING_ERROR_STORING);
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

bool Rpl_encryption::remove_key_from_keyring(std::string key_id) {
  DBUG_ENTER("Rpl_encryption::remove_key_from_keyring");
  if (DBUG_EVALUATE_IF("rpl_encryption_first_time_enable_4", 1, 0) ||
      my_key_remove(key_id.c_str(), nullptr)) {
    report_keyring_error(Keyring_status::KEYRING_ERROR_REMOVING);
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

std::string Rpl_encryption::get_master_key_seqno_key_id() {
  return Rpl_encryption_header::key_id_prefix();
}

std::pair<Rpl_encryption::Keyring_status, uint32_t>
Rpl_encryption::get_master_key_seqno_from_keyring() {
  DBUG_ENTER("Rpl_encryption::get_master_key_seqno_from_keyring");
  std::string key_id = get_master_key_seqno_key_id();
  DBUG_RETURN(get_seqno_from_keyring(key_id));
}

bool Rpl_encryption::set_master_key_seqno_on_keyring(uint32_t seqno) {
  DBUG_ENTER("Rpl_encryption::set_master_key_seqno_on_keyring");
  std::string key_id = get_master_key_seqno_key_id();
  DBUG_RETURN(set_seqno_on_keyring(key_id, seqno));
}

bool Rpl_encryption::remove_master_key_seqno_from_keyring() {
  DBUG_ENTER("Rpl_encryption::remove_master_key_seqno_from_keyring");
  std::string key_id = get_master_key_seqno_key_id();
  DBUG_RETURN(remove_key_from_keyring(key_id));
}

std::string Rpl_encryption::get_new_master_key_seqno_key_id() {
  return Rpl_encryption_header::key_id_with_suffix("new");
}

std::pair<Rpl_encryption::Keyring_status, uint32_t>
Rpl_encryption::get_new_master_key_seqno_from_keyring() {
  DBUG_ENTER("Rpl_encryption::get_new_master_key_seqno_from_keyring");
  std::string key_id = get_new_master_key_seqno_key_id();
  DBUG_RETURN(get_seqno_from_keyring(key_id));
}

bool Rpl_encryption::set_new_master_key_seqno_on_keyring(uint32 seqno) {
  DBUG_ENTER("Rpl_encryption::set_new_master_key_seqno_on_keyring");
  std::string key_id = get_new_master_key_seqno_key_id();
  DBUG_RETURN(set_seqno_on_keyring(key_id, seqno));
}

bool Rpl_encryption::remove_new_master_key_seqno_from_keyring() {
  DBUG_ENTER("Rpl_encryption::remove_new_master_key_seqno_from_keyring");
  std::string key_id = get_new_master_key_seqno_key_id();
  DBUG_RETURN(remove_key_from_keyring(key_id));
}

bool Rpl_encryption::generate_master_key_on_keyring(uint32 seqno) {
  DBUG_ENTER("Rpl_encryption::generate_master_key_on_keyring");

  std::string key_id = Rpl_encryption_header_v1::seqno_to_key_id(seqno);

  /* Check if the key already exists */
  auto pair = get_key(key_id, Rpl_encryption_header_v1::KEY_TYPE);
  /* If unable to check if the key already exists */
  if (pair.first == Keyring_status::KEYRING_ERROR_FETCHING) {
    Rpl_encryption::report_keyring_error(pair.first);
    DBUG_RETURN(true);
  }
  /* If the key already exists on keyring */
  if (pair.first != Keyring_status::KEY_NOT_FOUND) {
    Rpl_encryption::report_keyring_error(Keyring_status::KEY_EXISTS_UNEXPECTED,
                                         key_id.c_str());
    DBUG_RETURN(true);
  }

  /* Generate the new key */
  if (DBUG_EVALUATE_IF("rpl_encryption_first_time_enable_2", 1, 0) ||
      my_key_generate(key_id.c_str(), Rpl_encryption_header_v1::KEY_TYPE,
                      nullptr, Rpl_encryption_header_v1::KEY_LENGTH) != 0) {
    Rpl_encryption::report_keyring_error(
        Keyring_status::KEYRING_ERROR_GENERATING);
    DBUG_RETURN(true);
  }

  /* Fetch the new generated key from keyring again */
  pair = Rpl_encryption::get_key(key_id, Rpl_encryption_header_v1::KEY_TYPE,
                                 Rpl_encryption_header_v1::KEY_LENGTH);
  if (pair.first != Keyring_status::SUCCESS) {
    Rpl_encryption::report_keyring_error(pair.first);
    DBUG_RETURN(true);
  }

  /* Store the generated key as the new master key */
  m_master_key.m_id = key_id;
  m_master_key.m_value.assign(pair.second);

  DBUG_RETURN(false);
}

#endif  // MYSQL_SERVER

const char *Rpl_encryption_header::ENCRYPTION_MAGIC = "\xfd\x62\x69\x6e";

Rpl_encryption_header::~Rpl_encryption_header() {
  DBUG_ENTER("Rpl_encryption_header::~Rpl_encryption_header");
  DBUG_VOID_RETURN;
}

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

std::unique_ptr<Rpl_encryption_header>
Rpl_encryption_header::get_new_default_header() {
  DBUG_ENTER("Rpl_encryption_header::get_new_default_header");
  std::unique_ptr<Rpl_encryption_header> header(new Rpl_encryption_header_v1);
  DBUG_RETURN(header);
}

std::string Rpl_encryption_header::key_id_prefix() {
  return Rpl_encryption_header_v1::key_id_prefix();
}

std::string Rpl_encryption_header::seqno_to_key_id(uint32_t seqno) {
  return Rpl_encryption_header_v1::seqno_to_key_id(seqno);
}

std::string Rpl_encryption_header::key_id_with_suffix(const char *suffix) {
  return Rpl_encryption_header_v1::key_id_with_suffix(suffix);
}

const char *Rpl_encryption_header::get_key_type() {
  return Rpl_encryption_header_v1::KEY_TYPE;
}

const char *Rpl_encryption_header_v1::KEY_TYPE = "AES";
const char *Rpl_encryption_header_v1::KEY_ID_PREFIX = "MySQLReplicationKey";

Rpl_encryption_header_v1::~Rpl_encryption_header_v1() {
  DBUG_ENTER("Rpl_encryption_header_v1::~Rpl_encryption_header_v1");
  DBUG_VOID_RETURN;
}

bool Rpl_encryption_header_v1::serialize(Basic_ostream *ostream) {
  unsigned char header[HEADER_SIZE]{0};
  unsigned char *ptr = nullptr;

  memcpy(header, ENCRYPTION_MAGIC, ENCRYPTION_MAGIC_SIZE);
  header[VERSION_OFFSET] = m_version;

  DBUG_ASSERT(m_key_id.length() < 255);
  ptr = header + OPTIONAL_FIELD_OFFSET;
  *ptr++ = KEY_ID;
  *ptr++ = m_key_id.length();
  memcpy(ptr, m_key_id.data(), m_key_id.length());
  ptr += m_key_id.length();

  DBUG_ASSERT(m_encrypted_password.length() == PASSWORD_FIELD_SIZE);
  *ptr++ = ENCRYPTED_FILE_PASSWORD;
  memcpy(ptr, m_encrypted_password.data(), m_encrypted_password.length());
  ptr += PASSWORD_FIELD_SIZE;

  DBUG_ASSERT(m_iv.length() == IV_FIELD_SIZE);
  *ptr++ = IV_FOR_FILE_PASSWORD;
  memcpy(ptr, m_iv.data(), m_iv.length());

  bool res = DBUG_EVALUATE_IF("fail_to_serialize_encryption_header", true,
                              ostream->write(header, HEADER_SIZE));
  return res;
}

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

int Rpl_encryption_header_v1::get_header_size() {
  return Rpl_encryption_header_v1::HEADER_SIZE;
}

Key_string Rpl_encryption_header_v1::decrypt_file_password() {
  DBUG_ENTER("Rpl_encryption_header::decrypt_file_password");
  Key_string file_password;
#ifdef MYSQL_SERVER
  if (!m_key_id.empty()) {
    auto error_and_key =
        Rpl_encryption::get_key(m_key_id, KEY_TYPE, KEY_LENGTH);

    if (error_and_key.first != Rpl_encryption::Keyring_status::SUCCESS) {
      Rpl_encryption::report_keyring_error(error_and_key.first,
                                           m_key_id.c_str());
    } else if (!error_and_key.second.empty()) {
      unsigned char buffer[Aes_ctr::PASSWORD_LENGTH];

      if (my_aes_decrypt(m_encrypted_password.data(),
                         m_encrypted_password.length(), buffer,
                         error_and_key.second.data(),
                         error_and_key.second.length(), my_aes_256_cbc,
                         m_iv.data(), false) != MY_AES_BAD_DATA)
        file_password.append(buffer, Aes_ctr::PASSWORD_LENGTH);
    }
  }
#endif
  DBUG_RETURN(file_password);
}

std::unique_ptr<Rpl_cipher> Rpl_encryption_header_v1::get_encryptor() {
  return Aes_ctr::get_encryptor();
}

std::unique_ptr<Rpl_cipher> Rpl_encryption_header_v1::get_decryptor() {
  return Aes_ctr::get_decryptor();
}

Key_string Rpl_encryption_header_v1::generate_new_file_password() {
  Key_string password_str;
#ifdef MYSQL_SERVER
  Rpl_encryption::Rpl_encryption_key master_key =
      rpl_encryption.get_master_key();
  unsigned char password[Aes_ctr::PASSWORD_LENGTH];
  unsigned char encrypted_password[Aes_ctr::PASSWORD_LENGTH];
  unsigned char iv[Aes_ctr::AES_BLOCK_SIZE];
  bool error = false;

  /* Generate password, it is a random string. */
  error = my_rand_buffer(password, sizeof(password));
  if (!error) {
    password_str.append(password, sizeof(password));
  }

  /* Get the master key id */
  DBUG_ASSERT(master_key.m_id.length() > 0);
  m_key_id = master_key.m_id;

  /* Generate iv, it is a random string. */
  if (!error) {
    error = my_rand_buffer(iv, Aes_ctr::AES_BLOCK_SIZE);
    m_iv = Key_string(iv, sizeof(iv));
  }

  /* Encrypt password */
  if (!error) {
    error = (my_aes_encrypt(password_str.data(), password_str.length(),
                            encrypted_password, master_key.m_value.data(),
                            master_key.m_value.length(), my_aes_256_cbc, iv,
                            false) == MY_AES_BAD_DATA);
    m_encrypted_password =
        Key_string(encrypted_password, sizeof(encrypted_password));
  }
#endif
  return password_str;
}

std::string Rpl_encryption_header_v1::key_id_prefix() {
  std::ostringstream ostr;
#ifdef MYSQL_SERVER
  ostr << KEY_ID_PREFIX << "_" << ::server_uuid;
#endif
  return ostr.str();
}

std::string Rpl_encryption_header_v1::seqno_to_key_id(
    uint32_t seqno MY_ATTRIBUTE((unused))) {
  std::ostringstream ostr;
#ifdef MYSQL_SERVER
  ostr << key_id_prefix() << "_" << seqno;
#endif
  return ostr.str();
}

std::string Rpl_encryption_header_v1::key_id_with_suffix(
    const char *suffix MY_ATTRIBUTE((unused))) {
  std::ostringstream ostr;
#ifdef MYSQL_SERVER
  ostr << key_id_prefix() << "_" << suffix;
#endif
  return ostr.str();
}
