/*
   Copyright (c) 2017, 2026, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#define LOG_COMPONENT_TAG "caching_sha2_password"

#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <charconv>  // from_chars
#include <iomanip>   /* std::setfill(), std::setw() */
#include <iostream>  /* For debugging               */
#include <string>
#include <unordered_map>
#include <utility>

#include "base64_encode.h"
#include "crypt_genhash_impl.h"
#include "lex_string.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"     /* DBUG instrumentation        */
#include "my_inttypes.h" /* typedefs                    */
#include "my_macros.h"
#include "mysql/components/my_service.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_rwlock_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/util/event_tracking/event_tracking_authentication_consumer_helper.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/plugin.h"
#include "mysql/plugin_auth.h"        /* MYSQL_SERVER_AUTH_INFO      */
#include "mysql/plugin_auth_common.h" /* MYSQL_PLUGIN_VIO            */
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/service_my_plugin_log.h" /* plugin_log_level            */
#include "mysql/service_mysql_password_policy.h"
#include "mysql_com.h"
#include "mysqld_error.h"       /* ER_*                        */
#include "rwlock_scoped_lock.h" /* rwlock_scoped_lock          */
#include "scope_guard.h"
#include "sql/auth/auth_common.h"
#include "sql/auth/i_sha2_password.h" /* Internal classes            */
#include "sql/auth/i_sha2_password_common.h"
#include "sql/auth/sql_auth_cache.h" /* ACL_USER                    */
#include "sql/auth/sql_authentication.h"
#include "sql/debug_sync.h"       // DEBUG_SYNC
#include "sql/mysqld.h"           // registry_registration
#include "sql/protocol_classic.h" /* Protocol_classic            */
#include "sql/sql_class.h"
#include "sql/sql_const.h" /* MAX_FIELD_WIDTH             */
#include "violite.h"

#include <openssl/kdf.h>
#include <openssl/ssl.h>
namespace {
template <typename T>
constexpr auto to_integral(T t) noexcept
  requires(std::is_enum_v<T>)
{
  return static_cast<std::underlying_type_t<T>>(t);
}

using sha2_password::Stored_digest_info;
constexpr Stored_digest_info str_to_stored_digest_info(
    const std::string_view &str) {
  if (str == "A") return Stored_digest_info::CRYPT5;
  if (str == "B") return Stored_digest_info::PBKDF2_SHA512;
  return Stored_digest_info::LAST;
}

bool caching_sha2_proxy_users = false;
unsigned int caching_sha2_digest_rounds =
    sha2_password::DEFAULT_STORED_DIGEST_ROUNDS;
unsigned long caching_sha2_storage_format(
    to_integral(sha2_password::Stored_digest_info::CRYPT5));
bool caching_sha2_enforce_storage_format = false;
}  // namespace
class THD;
struct SYS_VAR;

char *caching_sha2_rsa_private_key_path;
char *caching_sha2_rsa_public_key_path;
bool caching_sha2_auto_generate_rsa_keys = true;

Rsa_authentication_keys *g_caching_sha2_rsa_keys = nullptr;

static bool init_event_tracking_authentication();
static bool deinit_event_tracking_authentication();

namespace sha2_password {
using std::min;

/**
  Add an entry in cache
  We manage our own memory

  @param [in] authorization_id   Key
  @param [in] entry_to_be_cached Value

  @returns status of addition
    @retval false Successful insertion
    @retval true Error
*/

bool SHA2_password_cache::add(const std::string &authorization_id,
                              const sha2_cache_entry &entry_to_be_cached) {
  DBUG_TRACE;
  auto ret = m_password_cache.try_emplace(authorization_id, entry_to_be_cached);
  return !ret.second;
}

/**
  Remove an entry from the cache

  @param [in] authorization_id  AuthID to search against

  @return out of the deletion
    @retval false Entry successfully removed
    @retval true Error removing the entry
*/

bool SHA2_password_cache::remove(const std::string &authorization_id) {
  DBUG_TRACE;
  auto it = m_password_cache.find(authorization_id);
  if (it != m_password_cache.end()) {
    m_password_cache.erase(it);
    return false;
  }
  return true;
}

/**
  Search an entry from the cache

  @param [in]  authorization_id   AuthID to search against
  @param [out] cache_entry Stored Password for given AuthID

  Assumption : Memory for password is allocated by the caller.

  @returns Status of search operation
    @retval false Entry found. Password contains the stored credential
    @retval true Entry not found.
*/

bool SHA2_password_cache::search(const std::string &authorization_id,
                                 sha2_cache_entry &cache_entry) {
  DBUG_TRACE;
  auto it = m_password_cache.find(authorization_id);
  if (it != m_password_cache.end()) {
    const sha2_cache_entry &stored_entry = it->second;
    for (unsigned int i = 0; i < MAX_PASSWORDS; ++i) {
      memcpy(cache_entry.digest_buffer[i], stored_entry.digest_buffer[i],
             sizeof(cache_entry.digest_buffer[i]));
    }
    return false;
  }
  return true;
}

/** Clear the cache - Release all memory */
void SHA2_password_cache::clear_cache() {
  if (!m_password_cache.empty()) m_password_cache.clear();
}

static const char *category = "sha2_auth";
static PSI_rwlock_key key_m_cache_lock;
static PSI_rwlock_info all_rwlocks[] = {
    {&key_m_cache_lock, "key_m_cache_lock", 0, 0, PSI_DOCUMENT_ME}};

/**
  Caching_sha2_password constructor - Initializes rw lock

  @param [in] plugin_handle           MYSQL_PLUGIN reference
  @param [in] stored_digest_rounds    Number of rounds for
                                      stored digest generation
  @param [in] digest_type             SHA2 type to be used
  @param [in] fast_digest_rounds      Number of rounds for
                                      fast digest generation
  @param [in] enforce_storage_format  Whether storage format is strictly
                                      enforced or not
*/
Caching_sha2_password::Caching_sha2_password(
    MYSQL_PLUGIN plugin_handle, size_t stored_digest_rounds,
    Stored_digest_info digest_type,  /* = Stored_digest_info::CRYPT5 */
    unsigned int fast_digest_rounds, /* = DEFAULT_FAST_DIGEST_ROUNDS */
    bool enforce_storage_format)     /* = false */
    : m_plugin_info(plugin_handle),
      m_stored_digest_rounds(stored_digest_rounds),
      m_stored_digest_type(digest_type),
      m_fast_digest_rounds(fast_digest_rounds),
      m_enforce_storage_format(enforce_storage_format) {
  int count = array_elements(all_rwlocks);
  mysql_rwlock_register(category, all_rwlocks, count);
  mysql_rwlock_init(key_m_cache_lock, &m_cache_lock);

  if (fast_digest_rounds > MAX_FAST_DIGEST_ROUNDS ||
      fast_digest_rounds < MIN_FAST_DIGEST_ROUNDS)
    m_fast_digest_rounds = DEFAULT_FAST_DIGEST_ROUNDS;

  if (stored_digest_rounds > MAX_STORED_DIGEST_ROUNDS ||
      stored_digest_rounds < MIN_STORED_DIGEST_ROUNDS)
    m_stored_digest_rounds = DEFAULT_STORED_DIGEST_ROUNDS;
}

/**
  Caching_sha2_password destructor - destroy rw lock
*/
Caching_sha2_password::~Caching_sha2_password() {
  mysql_rwlock_destroy(&m_cache_lock);
}

/*
  Compare given string against stored transformation

  @param [in]  src               String to compare
  @param [in]  stored            Stored transformation
  @param [in]  authorization_id  AuthID to report error
  @param [out] digest_type       Password transformation information

  @returns an std::pair<bool, bool> where first value denotes results of
  comparison (false - Success) and second value denotes sanity check status on
  stored transformation (true - Valid)
*/
std::pair<bool, bool> Caching_sha2_password::compare_against_stored(
    const std::string &src, const std::string_view &stored,
    const std::optional<std::string> &authorization_id,
    Stored_digest_info &digest_type) {
  std::string salt;
  std::string digest;
  std::string generated_digest;
  size_t iterations;
  bool match_error{true};
  bool valid{false};

  auto raise_error = [&](longlong code) {
    if (m_plugin_info && authorization_id.has_value()) {
      LogPluginErr(ERROR_LEVEL, code, authorization_id.value().c_str());
    }
  };

  /*
    Get digest type, iterations, salt and digest
    from the authentication string.
  */
  if (deserialize(stored, digest_type, salt, digest, iterations)) {
    raise_error(ER_SHA_PWD_FAILED_TO_PARSE_AUTH_STRING);
    return std::make_pair(match_error, valid);
  }

  valid = true;
  switch (digest_type) {
    case Stored_digest_info::CRYPT5:
      /*
        Generate multiple rounds of sha2 hash using plaintext password
        and salt retrieved from the authentication string.
      */

      if (this->generate_crypt5(src, salt, generated_digest, iterations)) {
        raise_error(ER_SHA_PWD_FAILED_TO_GENERATE_MULTI_ROUND_HASH);
        return std::make_pair(match_error, valid);
      }
      match_error = (memcmp(digest.c_str(), generated_digest.c_str(),
                            STORED_SHA256_DIGEST_LENGTH) != 0);
      break;
    case Stored_digest_info::PBKDF2_SHA512:
      if (this->generate_pbkdf2(src, salt, generated_digest, iterations)) {
        raise_error(ER_SHA_PWD_FAILED_TO_GENERATE_MULTI_ROUND_HASH);
        return std::make_pair(match_error, valid);
      }
      match_error = (memcmp(digest.c_str(), generated_digest.c_str(),
                            STORED_PBKDF2_DIGEST_LENGTH) != 0);
      break;
    default:
      assert(0);
      break;
  }

  return std::make_pair(match_error, valid);
}

/**
  Perform slow authentication.

  1. Disect serialized_string and retrieve
    a. Salt
    b. Hash iteration count
    c. Expected hash
  2. Use plaintext password, salt and hash iteration count to generate
     hash.
  3. Validate generated hash against expected hash.

  In case of successful authentication, update password cache.

  @param [in]  authorization_id           User information
  @param [in]  serialized_string          Information retrieved from
                                          mysql.authentication_string column
  @param [in]  plaintext_password         Password as received from client
  @param [out] set_password_expired_flag  Whether to suggest server to set
                                          password expired flag

  @returns Outcome of comparison against expected hash and whether
           second password was used or not.
*/

std::pair<bool, bool> Caching_sha2_password::authenticate(
    const std::string &authorization_id,
    const std::string_view *serialized_string,
    const std::string &plaintext_password, bool &set_password_expired_flag) {
  DBUG_TRACE;

  /* Don't process the password if it is longer than maximum limit. */
  if (plaintext_password.length() > CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH)
    return std::make_pair(true, false);

  /* Empty authentication string. */
  if (!serialized_string[0].length())
    return std::make_pair(plaintext_password.length() ? true : false, false);

  bool second = false;
  for (unsigned int i = 0;
       i < MAX_PASSWORDS && serialized_string[i].length() > 0; ++i) {
    second = i > 0;

    Stored_digest_info digest_info;
    if (compare_against_stored(plaintext_password, serialized_string[i],
                               authorization_id, digest_info)
            .first == false) {
      if (get_enforce_storage_format() &&
          digest_info != get_stored_digest_type()) {
        set_password_expired_flag = true;
      } else {
        set_password_expired_flag = false;
        /*
          If authentication is successful, we would want to make
          entry in cache for fast authentication. Subsequent
          authentication attempts would use the fast authentication
          to speed up the process.
        */
        sha2_cache_entry fast_digest;
        memset(&fast_digest, 0, sizeof(fast_digest));

        if (generate_fast_digest(plaintext_password, fast_digest, i)) {
          DBUG_PRINT("info", ("Failed to generate multi-round hash for %s. "
                              "Fast authentication won't be possible.",
                              authorization_id.c_str()));
          return std::make_pair(false, second);
        }

        const rwlock_scoped_lock wrlock(&m_cache_lock, true, __FILE__,
                                        __LINE__);
        if (m_cache.add(authorization_id, fast_digest)) {
          sha2_cache_entry stored_digest;
          m_cache.search(authorization_id, stored_digest);

          /* Same digest is already added, so just return. */
          if (memcmp(fast_digest.digest_buffer[i],
                     stored_digest.digest_buffer[i],
                     sizeof(fast_digest.digest_buffer[i])) == 0)
            return std::make_pair(false, second);

          /* Update the digest. */
          const uint retain_index = i ? 0 : 1;
          memcpy(fast_digest.digest_buffer[retain_index],
                 stored_digest.digest_buffer[retain_index],
                 sizeof(fast_digest.digest_buffer[retain_index]));
          m_cache.remove(authorization_id);
          m_cache.add(authorization_id, fast_digest);
          DBUG_PRINT("info", ("An old digest for %s was recorded in cache. "
                              "It has been replaced with the latest digest.",
                              authorization_id.c_str()));
        }
      }
      return std::make_pair(false, second);
    }
  }
  return std::make_pair(true, second);
}

/**
  Perform fast authentication

  1. Retrieve hash from cache
  2. Validate it against received scramble

  @param [in] authorization_id User information
  @param [in] random           Per session random number
  @param [in] random_length    Length of the random number
  @param [in] scramble         Scramble received from the client
  @param [in] check_second     Check secondary credentials

  @returns Outcome of scramble validation and whether
           second password was used or not.
*/

std::pair<bool, bool> Caching_sha2_password::fast_authenticate(
    const std::string &authorization_id, const unsigned char *random,
    unsigned int random_length, const unsigned char *scramble,
    bool check_second) {
  DBUG_TRACE;
  if (!scramble || !random) {
    DBUG_PRINT("info", ("For authorization id : %s,"
                        "Scramble is null - %s :"
                        "Random is null - %s :",
                        authorization_id.c_str(), !scramble ? "true" : "false",
                        !random ? "true" : "false"));
    return std::make_pair(true, false);
  }

  const rwlock_scoped_lock rdlock(&m_cache_lock, false, __FILE__, __LINE__);
  sha2_cache_entry digest;

  if (m_cache.search(authorization_id, digest)) {
    DBUG_PRINT("info", ("Could not find entry for %s in cache.",
                        authorization_id.c_str()));
    return std::make_pair(true, false);
  }

  /* Entry found, so validate scramble against it */
  Validate_scramble validate_scramble_first(scramble, digest.digest_buffer[0],
                                            random, random_length);
  bool retval = validate_scramble_first.validate();
  bool second = false;
  if (retval && check_second) {
    second = true;
    Validate_scramble validate_scramble_second(
        scramble, digest.digest_buffer[1], random, random_length);
    retval = validate_scramble_second.validate();
  }
  return std::make_pair(retval, second);
}

/**
  Remove an entry from the cache.

  This can happen due to one of the following:
  a. DROP USER
  b. RENAME USER

  @param [in] authorization_id User name
*/

void Caching_sha2_password::remove_cached_entry(
    const std::string &authorization_id) {
  const rwlock_scoped_lock wrlock(&m_cache_lock, true, __FILE__, __LINE__);
  /* It is possible that entry is not present at all, but we don't care */
  (void)m_cache.remove(authorization_id);
}

/**
  Deserialize obtained hash and retrieve various parts.

  From stored string, following parts are retrieved:
    Digest type
    Salt
    Iteration count
    hash

  Expected format
  DELIMITER[digest_type]DELIMITER[iterations]DELIMITER[salt][digest]

  digest_type:
  A => SHA256

  iterations:
  005 => 5*ITERATION_MULTIPLIER

  salt:
  Random string. Length SALT_LENGTH

  digest:
  SHA2 digest. Length STORED_SHA256_DIGEST_LENGTH

  @param [in]  serialized_string serialized string
  @param [out] digest_type       Digest algorithm
  @param [out] salt              Random string used for hashing
  @param [out] digest            Digest stored
  @param [out] iterations        Number of hash iterations

  @returns status of parsing
    @retval false. Success. out variables updated.
    @retval true. Failure. out variables should not be used.
*/

bool Caching_sha2_password::deserialize(
    const std::string_view &serialized_string, Stored_digest_info &digest_type,
    std::string &salt, std::string &digest, size_t &iterations) {
  DBUG_TRACE;
  if (!serialized_string.length()) return true;
  /* Digest Type */
  std::string::size_type delimiter = serialized_string.find(DELIMITER, 0);
  if (delimiter == std::string::npos) {
    DBUG_PRINT("info", ("Digest string is not in expected format."));
    return true;
  }
  const std::string_view digest_type_info =
      serialized_string.substr(delimiter + 1, DIGEST_INFO_LENGTH);
  digest_type = str_to_stored_digest_info(digest_type_info);
  if (digest_type == Stored_digest_info::LAST) {
    DBUG_PRINT("info", ("Digest string is not in expected format."
                        "Missing digest type information."));
    return true;
  }

  /* Iteration */
  delimiter = serialized_string.find(DELIMITER, delimiter + 1);
  if (delimiter == std::string::npos) {
    DBUG_PRINT("info", ("Digest string is not in expected format."
                        "Missing iteration count information."));
    return true;
  }
  const std::string::size_type delimiter_2 =
      serialized_string.find(DELIMITER, delimiter + 1);
  if (delimiter_2 == std::string::npos || delimiter_2 - delimiter != 4) {
    DBUG_PRINT("info", ("Digest string is not in expected format."
                        "Invalid iteration count information."));
    return true;
  }
  const std::string_view iteration_info =
      serialized_string.substr(delimiter + 1, ITERATION_LENGTH);
  unsigned long int iteration_count = 0;
  std::from_chars(iteration_info.data(),
                  iteration_info.data() + iteration_info.size(),
                  iteration_count, 16);
  if (!iteration_count) {
    DBUG_PRINT("info", ("Digest string is not in expected format."
                        "Invalid iteration count information."));
    return true;
  }
  iterations = std::min(iteration_count * ITERATION_MULTIPLIER, MAX_ITERATIONS);

  /* Salt */
  delimiter = delimiter_2;
  salt = serialized_string.substr(delimiter + 1, SALT_LENGTH);
  if (salt.length() != SALT_LENGTH) {
    DBUG_PRINT("info", ("Digest string is not in expected format."
                        "Invalid m_salt information."));
    return true;
  }

  /* Digest */
  digest =
      serialized_string.substr(delimiter + 1 + SALT_LENGTH, std::string::npos);
  size_t expected_digest_size{0};
  switch (digest_type) {
    case Stored_digest_info::CRYPT5:
      expected_digest_size = STORED_SHA256_DIGEST_LENGTH;
      break;
    case Stored_digest_info::PBKDF2_SHA512:
      expected_digest_size = STORED_PBKDF2_DIGEST_LENGTH;
      break;
    default:
      return true;
  };

  if (digest.length() != expected_digest_size) {
    DBUG_PRINT("info", ("Digest string is not in expected format."
                        "Invalid digest length."));
    return true;
  }
  return false;
}

/**
  Serialize following:
    a. Digest type
    b. Iteration count
    c. Salt
    d. Hash
  Expected output format:
  DELIMITER[digest_type]DELIMITER[iterations]DELIMITER[salt][digest]

  digest_type:
  A => CRYPT5
  B => PBKDF2

  iterations:
  3 digit hex * 1000
  E.g.
  For 5000 => 005

  salt:
  Random string. Length CRYPT_SALT_LENGTH

  digest:
  SHA2 digest. Length STORED_SHA256_DIGEST_LENGTH

  @param [out] serialized_string String to be stored
  @param [in]  digest_type       Digest algorithm
  @param [in]  salt              Random string used for hashing
  @param [in]  digest            Generated Digest
  @param [in]  iterations        Number of hash iterations
*/

bool Caching_sha2_password::serialize(std::string &serialized_string,
                                      const Stored_digest_info &digest_type,
                                      const std::string &salt,
                                      const std::string &digest,
                                      size_t iterations) {
  DBUG_TRACE;
  std::stringstream ss;
  /* Digest type */
  switch (digest_type) {
    case Stored_digest_info::CRYPT5:
      ss << DELIMITER << "A" << DELIMITER;
      break;
    case Stored_digest_info::PBKDF2_SHA512:
      ss << DELIMITER << "B" << DELIMITER;
      break;
    default:
      return true;
  }

  /* Iterations */
  if (iterations < ITERATION_MULTIPLIER || iterations > MAX_ITERATIONS) {
    DBUG_PRINT("info", ("Invalid iteration count information."));
    return true;
  }
  unsigned int iteration_info = iterations / ITERATION_MULTIPLIER;
  ss << std::setfill('0') << std::setw(3) << std::uppercase << std::hex
     << iteration_info << DELIMITER;
  serialized_string = ss.str();

  /* Salt */
  if (salt.length() != SALT_LENGTH) {
    DBUG_PRINT("info", ("Invalid m_salt."));
    return true;
  }
  serialized_string.append(salt.c_str(), salt.length());

  /* Digest */
  size_t expected_length{0};
  switch (digest_type) {
    case Stored_digest_info::CRYPT5:
      expected_length = STORED_SHA256_DIGEST_LENGTH;
      break;
    case Stored_digest_info::PBKDF2_SHA512:
      expected_length = STORED_PBKDF2_DIGEST_LENGTH;
      break;
    default:
      return true;
  };
  if (digest.length() != expected_length) {
    DBUG_PRINT("info", ("Invalid digest size."));
    return true;
  }
  serialized_string.append(digest.c_str(), digest.length());
  return false;
}

/**
  Generate digest based on get_fast_digest_rounds()

  @param [out] digest Digest output buffer
  @param [in]  plaintext_password Source text
  @param [in] pos Position of the digest

  @returns status of digest generation
    @retval false Success.
    @retval true Error. Don't rely on digest.
*/

bool Caching_sha2_password::generate_fast_digest(
    const std::string &plaintext_password, sha2_cache_entry &digest,
    unsigned int pos) {
  DBUG_TRACE;
  assert(pos < MAX_PASSWORDS);
  SHA256_digest sha256_digest;
  auto fast_digest_rounds = get_fast_digest_rounds();
  unsigned char digest_buffer[CACHING_SHA2_DIGEST_LENGTH];
  assert(sizeof(digest.digest_buffer[pos]) == sizeof(digest_buffer));

  if (sha256_digest.update_digest(plaintext_password.c_str(),
                                  plaintext_password.length()) ||
      sha256_digest.retrieve_digest(digest_buffer,
                                    CACHING_SHA2_DIGEST_LENGTH)) {
    DBUG_PRINT("info", ("Failed to generate SHA256 digest for password"));
    return true;
  }

  for (unsigned int i = 1; i < fast_digest_rounds; ++i) {
    sha256_digest.scrub();
    if (sha256_digest.update_digest(digest_buffer,
                                    CACHING_SHA2_DIGEST_LENGTH) ||
        sha256_digest.retrieve_digest(digest_buffer,
                                      CACHING_SHA2_DIGEST_LENGTH)) {
      DBUG_PRINT("info", ("Failed to generate SHA256 of SHA256 "
                          "digest for password"));
      return true;
    }
  }

  /* Calculated digest is stored in digest */
  memcpy(digest.digest_buffer[pos], digest_buffer,
         sizeof(digest.digest_buffer[pos]));
  return false;
}

/**
  Generate multi-round sha2 hash using source and random string.
  This is a wrapper around my_crypt_genhash

  @param [in]  source    Source text
  @param [in]  salt      Random text
  @param [out] digest    Generated sha2 digest
  @param [in] iterations Number of hash iterations

  @returns result of hash generation
    @retval false Success
    @retval true  Error
*/

bool Caching_sha2_password::generate_crypt5(const std::string &source,
                                            const std::string &salt,
                                            std::string &digest,
                                            unsigned int iterations) {
  DBUG_TRACE;
  char buffer[CRYPT_MAX_PASSWORD_SIZE + 1];
  memset(buffer, 0, sizeof(buffer));
  assert(source.length() <= CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH);
  my_crypt_genhash(buffer, CRYPT_MAX_PASSWORD_SIZE, source.c_str(),
                   source.length(), salt.c_str(), nullptr, &iterations);

  /*
    Returned value in buffer would be in format:
    $5$<SALT_LENGTH><STORED_SHA256_DIGEST_LENGTH>
    We need to extract STORED_SHA256_DIGEST_LENGTH chars from it
  */
  digest.assign(buffer + 3 + SALT_LENGTH + 1, STORED_SHA256_DIGEST_LENGTH);
  return false;
}

/**
  Transform given password into PKBDF2 digest

  @param [in]  source     Source text
  @param [in]  salt       Random text
  @param [out] digest     Generated PBKDF2 digest
  @param [in]  iterations Number of iterations

  @returns Result of the digest generation
    @retval false Success
    @retval true  Error
 */
bool Caching_sha2_password::generate_pbkdf2(const std::string &source,
                                            const std::string &salt,
                                            std::string &digest,
                                            unsigned int iterations) {
  unsigned char derived_key[PBKDF2_DIGEST_LENGTH];
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_KDF *kdf{nullptr};
  EVP_KDF_CTX *ctx{nullptr};
  OSSL_PARAM params[5];
  char sha512[] = "SHA512";

  kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
  ctx = EVP_KDF_CTX_new(kdf);

  auto cleanup_guard = create_scope_guard([&] {
    if (ctx) EVP_KDF_CTX_free(ctx);
    if (kdf) EVP_KDF_free(kdf);
  });

  params[0] = OSSL_PARAM_construct_utf8_string("digest", sha512, 0);
  params[1] = OSSL_PARAM_construct_octet_string(
      "salt", static_cast<void *>(const_cast<char *>(salt.c_str())),
      salt.length());
  params[2] = OSSL_PARAM_construct_uint("iter", (unsigned int *)&iterations);
  params[3] = OSSL_PARAM_construct_octet_string(
      "pass", static_cast<void *>(const_cast<char *>(source.c_str())),
      source.length());
  params[4] = OSSL_PARAM_construct_end();

  if (EVP_KDF_derive(ctx, derived_key, PBKDF2_DIGEST_LENGTH, params) != 1)
    return true;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  if (PKCS5_PBKDF2_HMAC(source.c_str(), source.length(),
                        reinterpret_cast<const unsigned char *>(salt.c_str()),
                        salt.length(), iterations, EVP_sha512(),
                        PBKDF2_DIGEST_LENGTH, derived_key) != 1)
    return true;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  digest = oci::ssl::base64_encode(derived_key, PBKDF2_DIGEST_LENGTH);
  assert(digest.length() == STORED_PBKDF2_DIGEST_LENGTH);
  return false;
}

/*
  Generates password transformation from give source string and create
  serialized string that contains required information such as digest type,
  number of iterations, salt digest.

  @param [in]  src             Source string
  @param [out] stored_digest   Serialized digest

  @returns Result of digest generation
    @retval false Success
    @retval true  Error
*/
bool Caching_sha2_password::generate_stored_digest(
    const std::string &src, std::string &serialized_string) {
  DBUG_TRACE;
  auto digest_type = get_stored_digest_type();
  auto rounds = get_stored_digest_rounds();
  char salt[sha2_password::SALT_LENGTH + 1];
  generate_user_salt(salt, sha2_password::SALT_LENGTH + 1);
  std::string salt_string(salt, sha2_password::SALT_LENGTH);
  std::string generated_digest;
  bool return_value = true;

  switch (digest_type) {
    case Stored_digest_info::CRYPT5:
      return_value =
          generate_crypt5(src, salt_string, generated_digest, rounds);
      break;
    case Stored_digest_info::PBKDF2_SHA512:
      return_value =
          generate_pbkdf2(src, salt_string, generated_digest, rounds);
      break;
    default:
      assert(false);
      return true;
  }

  /* Digest generation function returned an error. So we stop here. */
  if (return_value) return true;

  return_value = serialize(serialized_string, digest_type, salt_string,
                           generated_digest, rounds);

  return return_value;
}

/**
  Get cache count

  @returns number of elements in the cache
*/

size_t Caching_sha2_password::get_cache_count() {
  DBUG_TRACE;
  const rwlock_scoped_lock rdlock(&m_cache_lock, false, __FILE__, __LINE__);
  return m_cache.size();
}

/** Clear the password cache */
void Caching_sha2_password::clear_cache() {
  DBUG_TRACE;
  const rwlock_scoped_lock wrlock(&m_cache_lock, true, __FILE__, __LINE__);
  m_cache.clear_cache();
}

/**
  Validate a hash format

  @param [in] serialized_string Supplied hash

  @returns result of validation
    @retval false Valid hash
    @retval true  Invalid hash
*/
bool Caching_sha2_password::validate_hash(
    const std::string &serialized_string) {
  DBUG_TRACE;
  Stored_digest_info digest_type;
  std::string salt;
  std::string digest;
  size_t iterations;

  if (serialized_string.empty()) {
    DBUG_PRINT("info", ("0 length digest."));
    return false;
  }

  return deserialize(serialized_string, digest_type, salt, digest, iterations);
}

}  // namespace sha2_password

/** Length of encrypted packet */
const int MAX_CIPHER_LENGTH = 1024;

/** Caching_sha2_password handle */
sha2_password::Caching_sha2_password *g_caching_sha2_password = nullptr;

/** caching_sha2_password plugin handle - Mostly used for logging */
static MYSQL_PLUGIN caching_sha2_auth_plugin_ref;

/** Interface for querying the MYSQL_PUBLIC_VIO about encryption state */
static int my_vio_is_secure(MYSQL_PLUGIN_VIO *vio) {
  MPVIO_EXT *mpvio = (MPVIO_EXT *)vio;
  return is_secure_transport(mpvio->protocol->get_vio()->type);
}

/**
  Save the scramble in mpvio for future re-use.

  It is useful when we need to pass the scramble to another plugin.
  Especially in case when old 5.1 client with no CLIENT_PLUGIN_AUTH capability
  tries to connect to server with default 1FA set to
  caching_sha2_password

  @param vio      Virtual Input-Output interface
  @param scramble Scramble to be saved
*/

void static inline auth_save_scramble(MYSQL_PLUGIN_VIO *vio,
                                      const char *scramble) {
  MPVIO_EXT *mpvio = (MPVIO_EXT *)vio;
  strncpy(mpvio->scramble, scramble, SCRAMBLE_LENGTH + 1);
}

/**
  Make hash key

  @param [in] username User part of the key
  @param [in] hostname Host part of the key
  @param [out] key     Generated hash key
*/
static void make_hash_key(const char *username, const char *hostname,
                          std::string &key) {
  DBUG_TRACE;
  key.assign(username ? username : "");
  key.push_back('\0');
  key.append(hostname ? hostname : "");
  key.push_back('\0');
}

static char request_public_key = '\2';
static char fast_auth_success = '\3';
static char perform_full_authentication = '\4';

/* clang-format off */
/**
  @page page_caching_sha2_authentication_exchanges Caching_sha2_password information

  @section sect_caching_sha2_definition Definition
  <ul>
  <li>
  The server side plugin name is *caching_sha2_password*
  </li>
  <li>
  The client side plugin name is *caching_sha2_password*
  </li>
  <li>
  Account - user account (user-host combination)
  </li>
  <li>
  authentication_string - Transformation of account password stored in mysql.user table
  </li>
  <li>
  user_password - Password known to generate authentication_string for given user account
  </li>
  <li>
  client_password - password used by client while connecting to server
  </li>
  <li>
  Nonce - 20 byte long random data
  </li>
  <li>
  Scramble - XOR(SHA256(password), SHA256(SHA256(SHA256(password)), Nonce))
  </li>
  <li>
  Hash entry - account_name -> SHA256(SHA256(user_password))
  </li>
  </ul>

  @section sect_caching_sha2_info How caching_sha2_password works?

  Plugin caching_sha2_password works in two phases.
  1. Fast authentication
  2. Complete authentication

  If server has cached hash entry for given user in memory, it uses scramble
  sent by client to perform fast authentication. If it is a success,
  authentication is done and connection will move to command phase. If there
  is an error, server will signal client to switch to full authentication that
  involves sending password over a secure connection server. Server then
  verifies password against authentication_string for given user account.
  If it is a success, server caches hash entry for the account and connection
  enters command phase. If there is an error, server sends error information
  to client and connection is terminated.

  Following section describes state transitions and message exchanges between
  server and client.

  Note that there are additional sanity checks performed by server and client
  at various steps. Such steps may result into end of communication by either
  party. However, such sanity checks are not covered in the diagram below.

  Legends

  @startuml

  "From server" -[#blue]-> [Message information] "To client"
  "From client" -[#red]-> [Message information] "To server"
  "State A" -[#yellow]-> [Action performed] "State B"
  "State M" --> [Condition checked] "Conditional action"
  "State X" -[#black]-> "Termination on either\nor both side"

  @enduml

  @startuml

  (*) --> "Authentication starts"
   "Authentication starts" --> "Server"
   "Server" -[#blue]-> [Send nonce to client] "Client"
   if (Check password) then
     -[#red]-> [Empty password:\nSend empty password] "Server: Empty password\nquick path"
     if (Account password) then
       -[#black]-> [Empty password:\nAuthentication success] SERVER_CLIENT_OK
     else
       -[#black]-> [Non empty password:\nAuthentication failure] SERVER_ERROR
     endif
   else
     -[#yellow]-> [Non empty\npassword] "Client: Fast authentication\nstart"
     "Client: Fast authentication\nstart" -[#red]-> [Generate and Send\nscramble to server] "Server: Fast authentication\nstart"
     if (Cached entry check) then
       -[#yellow]-> [Found]"Server: Scramble verification"
       if (Scramble\nvalidation) then
         -[#yellow]-> [Success]"Server: Fast auth success"
         "Server: Fast auth success" -[#blue]-> [Send fast_auth_success message\nfollowed by OK\n] "Client: Fast authentication\nend"
         "Client: Fast authentication\nend" -[#black]-> [Authentication success] SERVER_CLIENT_OK
       else
         -[#yellow]-> [Failure] "Server: Full authentication\nstart"
       endif
     else
       -[#yellow]-> [Not found] "Server: Full authentication\nstart"
       "Server: Full authentication\nstart" -[#blue]-> [Send perform_full_authentication] "Client: Full authentication\nstart"
       if (Client: Connection check) then
         -[#red]-> [Priority#1:\nTCP with TLS OR Socket Or\nShared Memory connection:\nSend password to server] "Server: received password"
       else
         if (Priority#2:\nPublic key available) then
           -[#yellow]-> [yes] "Client: Public key available"
         else
           if (No\nPriority#3: Should client get\nserver's public key) then
             -[#red]-> [Yes: Send\npublic_key_request] "Server: Public key request"
             "Server: Public key request" -[#blue]-> [Send public key] "Client: Public key available"
           else
             -[#yellow]-> [No: Can not\nsend password] CLIENT_ERROR
           endif
         endif

         "Client: Public key available" -[#yellow]-> [Encrypt password] "Client: Send encrypted password"
         "Client: Send encrypted password" -[#red]->[Send encrypted password] "Server: received encrypted password"
         "Server: received encrypted password" -[#yellow]-> [Decryption] "Server: received password"
       endif

       if (Password verification) then
         -[#yellow]-> [Successful:\nUpdate cache] "Server: Cache update"
         "Server: Cache update" -[#black]-> [Authentication success] SERVER_CLIENT_OK
       else
         -[#black]-> [Unsuccessful:\nAuthentication error] SERVER_ERROR
       endif
     endif
   endif

  @enduml
*/
/* clang-format on */

/**
  Authentication routine for caching_sha2_password.

  @param [in] vio  Virtual I/O interface
  @param [in] info Connection information

  Refer to @ref page_caching_sha2_authentication_exchanges
  for server-client communication in various cases

  @returns status of authentication process
    @retval CR_OK    Successful authentication
    @retval CR_ERROR Authentication failure
*/

static int caching_sha2_password_authenticate(MYSQL_PLUGIN_VIO *vio,
                                              MYSQL_SERVER_AUTH_INFO *info) {
  DBUG_TRACE;
  uchar *pkt;
  int pkt_len;
  char scramble[SCRAMBLE_LENGTH + 1];
  int cipher_length = 0;
  unsigned char plain_text[MAX_CIPHER_LENGTH + 1];
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_PKEY *private_key = nullptr;
  EVP_PKEY *public_key = nullptr;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  RSA *private_key = nullptr;
  RSA *public_key = nullptr;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

  generate_user_salt(scramble, SCRAMBLE_LENGTH + 1);

  /*
    Note: The nonce is split into 8 + 12 bytes according to
    https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_v10.html
    Native authentication sent 20 bytes + '\0' character = 21 bytes.
    This plugin must do the same to stay consistent with historical behavior
    if it is set to operate as a default plugin.
  */
  if (vio->write_packet(vio, (unsigned char *)scramble, SCRAMBLE_LENGTH + 1))
    return CR_AUTH_HANDSHAKE;

  /*
    Save the scramble so it could be used by native plugin in case
    the authentication on the server side needs to be restarted
  */
  auth_save_scramble(vio, scramble);

  /*
    After the call to read_packet() the user name will appear in
    mpvio->acl_user and info will contain current data.
  */
  if ((pkt_len = vio->read_packet(vio, &pkt)) == -1) return CR_AUTH_HANDSHAKE;

  DBUG_EXECUTE_IF("sha2_password_bad_reply", {
    /* This should cause a HANDSHAKE ERROR */
    my_error(ER_HANDSHAKE_ERROR, MYF(0));
    return CR_AUTH_HANDSHAKE;
  });

  /*
    If first packet is a 0 byte then the client isn't sending any password
    else the client will send a password.
  */
  if (!pkt_len || (pkt_len == 1 && *pkt == 0)) {
    info->password_used = PASSWORD_USED_NO;
    /*
      Send OK signal; the authentication might still be rejected based on
      host mask.
    */
    if (info->auth_string_length == 0) {
      if (caching_sha2_proxy_users) {
        *info->authenticated_as = PROXY_FLAG;
        DBUG_PRINT("info", ("caching_sha2_password_proxy_users is enabled, "
                            "setting authenticated_as to NULL"));
      }
      return CR_OK;
    } else
      return CR_AUTH_USER_CREDENTIALS;
  } else
    info->password_used = PASSWORD_USED_YES;

  MPVIO_EXT *mpvio = (MPVIO_EXT *)vio;
  std::string authorization_id;
  const char *hostname = mpvio->acl_user->host.get_host();
  make_hash_key(info->authenticated_as, hostname, authorization_id);

  if (pkt_len != sha2_password::CACHING_SHA2_DIGEST_LENGTH) return CR_ERROR;

  std::pair<bool, bool> fast_auth_result =
      g_caching_sha2_password->fast_authenticate(
          authorization_id, reinterpret_cast<unsigned char *>(scramble),
          SCRAMBLE_LENGTH, pkt,
          info->additional_auth_string_length ? true : false);

  if (fast_auth_result.first) {
    /*
      We either failed to authenticate or did not find entry in the cache.
      In either case, move to full authentication and ask the password
    */
    if (vio->write_packet(vio, (uchar *)&perform_full_authentication, 1))
      return CR_AUTH_HANDSHAKE;
  } else {
    /* Send fast_auth_success packet followed by CR_OK */
    if (vio->write_packet(vio, (uchar *)&fast_auth_success, 1))
      return CR_AUTH_HANDSHAKE;
    if (fast_auth_result.second) {
      const char *username =
          *info->authenticated_as ? info->authenticated_as : "";
      LogPluginErr(INFORMATION_LEVEL,
                   ER_CACHING_SHA2_PASSWORD_SECOND_PASSWORD_USED_INFORMATION,
                   username, hostname ? hostname : "");
    }
    if (caching_sha2_proxy_users) {
      *info->authenticated_as = PROXY_FLAG;
      DBUG_PRINT("info", ("caching_sha2_password_proxy_users is enabled, "
                          "setting authenticated_as to NULL"));
    }
    return CR_OK;
  }

  /*
    Read packet from client - It will either be request for public key
    or password. We expect the pkt_len to be at least 1 because an empty
    password is '\0'.
    See setting of plaintext_password using unencrypted vio.
  */
  if ((pkt_len = vio->read_packet(vio, &pkt)) <= 0) return CR_AUTH_HANDSHAKE;

  if (!my_vio_is_secure(vio)) {
    /*
      Since a password is being used it must be encrypted by RSA since no
      other encryption is being active.
    */
    private_key = g_caching_sha2_rsa_keys->get_private_key();
    public_key = g_caching_sha2_rsa_keys->get_public_key();

    /* Without the keys encryption isn't possible. */
    if (private_key == nullptr || public_key == nullptr) {
      if (caching_sha2_auth_plugin_ref)
        LogPluginErr(ERROR_LEVEL, ER_SHA_PWD_AUTH_REQUIRES_RSA_OR_SSL);
      return CR_ERROR;
    }

    if ((cipher_length = g_caching_sha2_rsa_keys->get_cipher_length()) >
        MAX_CIPHER_LENGTH) {
      if (caching_sha2_auth_plugin_ref)
        LogPluginErr(ERROR_LEVEL, ER_SHA_PWD_RSA_KEY_TOO_LONG,
                     g_caching_sha2_rsa_keys->get_cipher_length(),
                     MAX_CIPHER_LENGTH);
      return CR_ERROR;
    }

    /*
      Client sent a "public key request"-packet ?
      If the first packet is 1 then the client will require a public key before
      encrypting the password.
    */
    if (pkt_len == 1 && *pkt == request_public_key) {
      const uint pem_length = static_cast<uint>(
          strlen(g_caching_sha2_rsa_keys->get_public_key_as_pem()));
      if (vio->write_packet(
              vio,
              pointer_cast<const uchar *>(
                  g_caching_sha2_rsa_keys->get_public_key_as_pem()),
              pem_length))
        return CR_ERROR;
      /* Get the encrypted response from the client */
      if ((pkt_len = vio->read_packet(vio, &pkt)) <= 0) return CR_ERROR;
    }

    /*
      The packet will contain the cipher used. The length of the packet
      must correspond to the expected cipher length.
    */
    if (pkt_len != cipher_length) return CR_ERROR;

    /* Decrypt password */
    if (decrypt_RSA_private_key(pkt, cipher_length, plain_text,
                                sizeof(plain_text) - 1, private_key))
      return CR_ERROR;

    plain_text[cipher_length] = '\0';  // safety
    xor_string((char *)plain_text, cipher_length, (char *)scramble,
               SCRAMBLE_LENGTH);

    /* Set packet pointers and length for the hash digest function below */
    pkt = plain_text;
    pkt_len = strlen((char *)plain_text) + 1;  // include \0 intentionally.

    if (pkt_len == 1) return CR_AUTH_USER_CREDENTIALS;
  }  // if(!my_vio_is_encrypted())

  /* Fetch user authentication_string and extract the password salt */
  const std::string_view serialized_string[] = {
      std::string_view(info->auth_string, info->auth_string_length),
      std::string_view(info->additional_auth_string_length != 0U
                           ? info->additional_auth_string
                           : "",
                       info->additional_auth_string_length)};
  const std::string plaintext_password((char *)pkt, pkt_len - 1);
  bool set_password_expired_flag = false;
  std::pair<bool, bool> auth_success = g_caching_sha2_password->authenticate(
      authorization_id, serialized_string, plaintext_password,
      set_password_expired_flag);
  if (auth_success.first) return CR_AUTH_USER_CREDENTIALS;

  if (auth_success.second) {
    const char *username =
        *info->authenticated_as ? info->authenticated_as : "";
    LogPluginErr(INFORMATION_LEVEL,
                 ER_CACHING_SHA2_PASSWORD_SECOND_PASSWORD_USED_INFORMATION,
                 username, hostname ? hostname : "");
  }

  if (caching_sha2_proxy_users) {
    *info->authenticated_as = PROXY_FLAG;
    DBUG_PRINT("info", ("caching_sha2_password_proxy_users is enabled, "
                        "setting authenticated_as to NULL"));
  }

  return set_password_expired_flag ? CR_OK_FORCE_PASSWORD_CHANGE : CR_OK;
}

/**
  Generate hash for caching_sha2_password plugin

  @param [out] outbuf   Hash output buffer
  @param [out] buflen   Length of hash in output buffer
  @param [in]  inbuf    Plaintext password
  @param [in]  inbuflen Input password length

  @note outbuf must be larger than MAX_FIELD_WIDTH.
        It is assumed the caller asserts this.

  @returns status of hash generation
    @retval 0 Successful hash generation
    @retval 1 Error generating hash. Don't reply on outbuf/buflen
*/

static int caching_sha2_password_generate(char *outbuf, unsigned int *buflen,
                                          const char *inbuf,
                                          unsigned int inbuflen) {
  DBUG_TRACE;
  const std::string source(inbuf, inbuflen);
  std::string serialized_string;

  if (inbuflen > sha2_password::CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH)
    return 1;

  THD *thd = current_thd;
  if (!thd->m_disable_password_validation) {
    if (my_validate_password_policy(inbuf, inbuflen)) return 1;
  }

  if (inbuflen == 0) {
    *buflen = 0;
    return 0;
  }

  if (g_caching_sha2_password->generate_stored_digest(source,
                                                      serialized_string))
    return 1;

  if (serialized_string.length() > MAX_FIELD_WIDTH) {
    *buflen = 0;
    return 1;
  }

  memcpy(outbuf, serialized_string.c_str(), serialized_string.length());
  *buflen = serialized_string.length();

  return 0;
}

/**
  Validate a hash against caching_sha2_password plugin's
  hash format

  @param [in] inbuf  Hash to be validated
  @param [in] buflen Length of the hash

  @returns status of hash validation
    @retval 0 Hash is according to caching_sha2_password's expected format
    @retval 1 Hash does not match caching_sha2_password's requirement
*/

static int caching_sha2_password_validate(char *const inbuf,
                                          unsigned int buflen) {
  DBUG_TRACE;
  const std::string serialized_string(inbuf, buflen);
  if (g_caching_sha2_password->validate_hash(serialized_string)) return 1;
  return 0;
}

/**
  NoOp - Salt generation for cachhing_sha2_password plugin.

  @param [in]  password     Unused
  @param [in]  password_len Unused
  @param [out] salt         Unused
  @param [out] salt_len     Length of the salt buffer

  @returns Always returns success (0)
*/

static int caching_sha2_password_salt(const char *password [[maybe_unused]],
                                      unsigned int password_len
                                      [[maybe_unused]],
                                      unsigned char *salt [[maybe_unused]],
                                      unsigned char *salt_len) {
  DBUG_TRACE;
  *salt_len = 0;
  return 0;
}

/*
Initialize caching_sha2_password plugin

@param [in] plugin_ref Plugin structure handle

@returns Status of plugin initialization
@retval 0 Success
@retval 1 Error
*/

static int caching_sha2_authentication_init(MYSQL_PLUGIN plugin_ref) {
  DBUG_TRACE;

  caching_sha2_auth_plugin_ref = plugin_ref;
  g_caching_sha2_password =
      new (std::nothrow) sha2_password::Caching_sha2_password(
          caching_sha2_auth_plugin_ref, caching_sha2_digest_rounds,
          static_cast<sha2_password::Stored_digest_info>(
              caching_sha2_storage_format),
          caching_sha2_digest_rounds, caching_sha2_enforce_storage_format);
  if (!g_caching_sha2_password) return 1;

  return init_event_tracking_authentication() ? 1 : 0;
}

/**
  Deinitialize caching_sha2_password plugin

  @param [in] arg Unused

  @returns Always returns success
*/

static int caching_sha2_authentication_deinit(void *arg [[maybe_unused]]) {
  DBUG_TRACE;
  if (g_caching_sha2_password) {
    delete g_caching_sha2_password;
    g_caching_sha2_password = nullptr;
  }
  return deinit_event_tracking_authentication() ? 1 : 0;
}

/**
  Compare a clear text password with a stored hash

  Check if stored hash is produced using a clear text password.
  To do that, first extra scrmable from the hash. Then
  calculate a new hash using extracted scramble and the supplied
  password. And finally compare the two hashes.

  @arg hash              pointer to the hashed data
  @arg hash_length       length of the hashed data
  @arg cleartext         pointer to the clear text password
  @arg cleartext_length  length of the cleat text password
  @arg[out] is_error     non-zero in case of error extracting the salt
  @retval 0              the hash was created with that password
  @retval non-zero       the hash was created with a different password
*/

static int compare_caching_sha2_password_with_hash(
    const char *hash, unsigned long hash_length, const char *cleartext,
    unsigned long cleartext_length, int *is_error) {
  DBUG_TRACE;

  const std::string_view stored(hash, hash_length);
  const std::string plaintext_password(cleartext, cleartext_length);
  sha2_password::Stored_digest_info digest_type;

  if (cleartext_length >
      sha2_password::CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH)
    return -1;

  auto ret = g_caching_sha2_password->compare_against_stored(
      plaintext_password, stored, std::nullopt, digest_type);

  *is_error = ret.second ? 0 : 1;
  return ret.first ? 1 : 0;
}

/**
  Function to display value for status variable :
  Caching_sha2_password_rsa_public_key

  @param [in]  thd MYSQL_THD handle. Unused.
  @param [out] var Status variable structure
  @param [in]  buff Value buffer
*/
static int show_caching_sha2_password_rsa_public_key(MYSQL_THD thd
                                                     [[maybe_unused]],
                                                     SHOW_VAR *var,
                                                     char *buff
                                                     [[maybe_unused]]) {
  var->type = SHOW_CHAR;
  var->value =
      const_cast<char *>(g_caching_sha2_rsa_keys->get_public_key_as_pem());
  return 0;
}

/** st_mysql_auth for caching_sha2_password plugin */
static struct st_mysql_auth caching_sha2_auth_handler {
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
      Cached_authentication_plugins::get_plugin_name(
          PLUGIN_CACHING_SHA2_PASSWORD),
      caching_sha2_password_authenticate, caching_sha2_password_generate,
      caching_sha2_password_validate, caching_sha2_password_salt,
      AUTH_FLAG_USES_INTERNAL_STORAGE, compare_caching_sha2_password_with_hash
};

static MYSQL_SYSVAR_STR(
    private_key_path, caching_sha2_rsa_private_key_path,
    PLUGIN_VAR_READONLY | PLUGIN_VAR_NOPERSIST,
    "A fully qualified path to the private RSA key used for authentication.",
    nullptr, nullptr, AUTH_DEFAULT_RSA_PRIVATE_KEY);

static MYSQL_SYSVAR_STR(
    public_key_path, caching_sha2_rsa_public_key_path,
    PLUGIN_VAR_READONLY | PLUGIN_VAR_NOPERSIST,
    "A fully qualified path to the public RSA key used for authentication.",
    nullptr, nullptr, AUTH_DEFAULT_RSA_PUBLIC_KEY);

static MYSQL_SYSVAR_BOOL(
    auto_generate_rsa_keys, caching_sha2_auto_generate_rsa_keys,
    PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NOPERSIST,
    "Auto generate RSA keys at server startup if corresponding "
    "system variables are not specified and key files are not present "
    "at the default location.",
    nullptr, nullptr, true);

static int caching_sha2_digest_round_check(MYSQL_THD thd [[maybe_unused]],
                                           SYS_VAR *var [[maybe_unused]],
                                           void *save [[maybe_unused]],
                                           struct st_mysql_value *value) {
  longlong new_value;
  if (value->val_int(value, &new_value) != 0) {
    return 1; /* NULL value */
  }

  if (new_value <
          static_cast<longlong>(sha2_password::MIN_STORED_DIGEST_ROUNDS) ||
      new_value >
          static_cast<longlong>(sha2_password::MAX_STORED_DIGEST_ROUNDS)) {
    return 1; /* Invalid value */
  }
  *(reinterpret_cast<ulonglong *>(save)) = static_cast<ulonglong>(new_value);
  return 0;
}

static void caching_sha2_digest_update_update(MYSQL_THD thd [[maybe_unused]],
                                              SYS_VAR *var [[maybe_unused]],
                                              void *var_ptr [[maybe_unused]],
                                              const void *save) {
  ulonglong new_value = *(reinterpret_cast<const ulonglong *>(save));
  caching_sha2_digest_rounds = new_value;
  g_caching_sha2_password->set_stored_digest_rounds(new_value);
}

static MYSQL_SYSVAR_UINT(
    digest_rounds,               // Name.
    caching_sha2_digest_rounds,  // Variable.
    PLUGIN_VAR_RQCMDARG,         // Argument optional for cmd line
    "Number of SHA2 rounds to be done when storing a password hash onto disk.",
    caching_sha2_digest_round_check,              // Check function.
    caching_sha2_digest_update_update,            // Update function.
    sha2_password::DEFAULT_STORED_DIGEST_ROUNDS,  // Default value.
    sha2_password::MIN_STORED_DIGEST_ROUNDS,      // Min value.
    sha2_password::MAX_STORED_DIGEST_ROUNDS,      // Max value.
    1                                             // Block size.
);

static const char *caching_sha2_storage_format_values[] = {
    "CRYPT5", "PBKDF2_SHA512", nullptr};
static TYPELIB caching_sha2_storage_format_typelib = {
    .count = array_elements(caching_sha2_storage_format_values) - 1,
    .name = "caching_sha2_storage_format_typelib",
    .type_names = caching_sha2_storage_format_values,
    .type_lengths = nullptr};

static void caching_sha2_storage_format_update(MYSQL_THD thd [[maybe_unused]],
                                               SYS_VAR *var [[maybe_unused]],
                                               void *var_ptr [[maybe_unused]],
                                               const void *save) {
  ulonglong new_value = *(reinterpret_cast<const ulonglong *>(save));
  caching_sha2_storage_format = static_cast<unsigned long>(new_value);
  g_caching_sha2_password->set_stored_digest_type(
      static_cast<sha2_password::Stored_digest_info>(
          caching_sha2_storage_format));
}

static MYSQL_SYSVAR_ENUM(
    storage_format,                                          // Name
    caching_sha2_storage_format,                             // Variable
    PLUGIN_VAR_RQCMDARG,                                     // Type
    "Storage format for credentials.",                       // Description
    nullptr,                                                 // Check
    caching_sha2_storage_format_update,                      // Update
    to_integral(sha2_password::Stored_digest_info::CRYPT5),  // Default
    &caching_sha2_storage_format_typelib);

static MYSQL_SYSVAR_BOOL(
    proxy_users, caching_sha2_proxy_users, PLUGIN_VAR_OPCMDARG,
    "If set to FALSE (the default), then the caching_sha2 authentication "
    "plugin will not signal for authenticated users to be checked for mapping "
    "to proxy users. If set to TRUE, the plugin will flag associated "
    "authenticated accounts to be mapped to proxy users when the server option "
    "check_proxy_users is enabled.",
    nullptr, nullptr, false);

static int caching_sha2_enforce_format_check(MYSQL_THD thd [[maybe_unused]],
                                             SYS_VAR *var [[maybe_unused]],
                                             void *save [[maybe_unused]],
                                             struct st_mysql_value *value) {
  longlong val = 0;
  const int type = value->value_type(value);
  if (type == MYSQL_VALUE_TYPE_INT) {
    if (value->val_int(value, &val)) return 1;

    if (val != 0 && val != 1) {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
               "caching_sha2_password_enforce_storage_format",
               std::to_string(val).c_str());
      return 1;
    }
  } else if (type == MYSQL_VALUE_TYPE_STRING) {
    char buffer[STRING_BUFFER_USUAL_SIZE];
    int buffer_length = sizeof(buffer);
    const char *buf = value->val_str(value, buffer, &buffer_length);

    if (buf == nullptr) return 1;

    if (!native_strcasecmp(buf, "ON") || !strcmp(buf, "1"))
      val = 1;
    else if (!native_strcasecmp(buf, "OFF") || !strcmp(buf, "0"))
      val = 0;
    else {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
               "caching_sha2_password_enforce_storage_format", buf);
      return 1;
    }
  } else {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0),
             "caching_sha2_password_enforce_storage_format");
    return 1;
  }
  *(reinterpret_cast<ulonglong *>(save)) = static_cast<ulonglong>(val);
  return 0;
}

static void caching_sha2_enforce_format_update(MYSQL_THD thd [[maybe_unused]],
                                               SYS_VAR *var [[maybe_unused]],
                                               void *var_ptr [[maybe_unused]],
                                               const void *save) {
  ulonglong new_value = *(reinterpret_cast<const ulonglong *>(save));
  caching_sha2_enforce_storage_format = (new_value != 0);
  g_caching_sha2_password->set_enforce_storage_format(new_value != 0);
}

static MYSQL_SYSVAR_BOOL(
    enforce_storage_format,               // Name
    caching_sha2_enforce_storage_format,  // Variable
    PLUGIN_VAR_OPCMDARG,                  // Type
    "Enforce storage format for credentials. If set to TRUE, then accounts "
    "that are using caching_sha2_password and have password transformation "
    "stored in format other than what is specified by "
    "--caching_sha2_password_storage_format, will be forced to change their "
    "password upon login.",              // Description
    caching_sha2_enforce_format_check,   // Check
    caching_sha2_enforce_format_update,  // Update
    false                                // Default
);

/** Array of system variables. Used in plugin declaration. */
static SYS_VAR *caching_sha2_password_sysvars[] = {
    MYSQL_SYSVAR(private_key_path),
    MYSQL_SYSVAR(public_key_path),
    MYSQL_SYSVAR(auto_generate_rsa_keys),
    MYSQL_SYSVAR(digest_rounds),
    MYSQL_SYSVAR(proxy_users),
    MYSQL_SYSVAR(storage_format),
    MYSQL_SYSVAR(enforce_storage_format),
    nullptr};

/** Array of status variables. Used in plugin declaration. */
static SHOW_VAR caching_sha2_password_status_variables[] = {
    {"Caching_sha2_password_rsa_public_key",
     (char *)&show_caching_sha2_password_rsa_public_key, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, enum_mysql_show_type(0), enum_mysql_show_scope(0)}};

const char *auth_service_implementation_name =
    "event_tracking_authentication.caching_sha2_password_plugin";
static bool event_tracking_authentication_initialized = false;

namespace Event_tracking_implementation {
mysql_event_tracking_authentication_subclass_t
    Event_tracking_authentication_implementation::filtered_sub_events =
        EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE;
bool Event_tracking_authentication_implementation::callback(
    const mysql_event_tracking_authentication_data *data) {
  /*
    If status is set to true, it indicates an error.
    In which case, don't touch the cache.
  */
  if (data->status) return false;
  switch (data->event_subclass) {
    case EVENT_TRACKING_AUTHENTICATION_FLUSH:
      g_caching_sha2_password->clear_cache();
      break;
    case EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE:
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME:
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP: {
      assert(data->user.str[data->user.length] == '\0');
      std::string authorization_id;
      make_hash_key(data->user.str, data->host.str, authorization_id);
      g_caching_sha2_password->remove_cached_entry(authorization_id);
      break;
    }
    default:
      assert(0);
  }
  return false;
}
}  // namespace Event_tracking_implementation

static bool init_event_tracking_authentication() {
  event_tracking_authentication_initialized = false;
  static IMPLEMENTS_SERVICE_EVENT_TRACKING_AUTHENTICATION(
      caching_sha2_password_plugin);

  SERVICE_TYPE_NO_CONST(event_tracking_authentication) *svc =
      const_cast<SERVICE_TYPE_NO_CONST(event_tracking_authentication) *>(
          &SERVICE_IMPLEMENTATION(caching_sha2_password_plugin,
                                  event_tracking_authentication));
  if (srv_registry_registration->register_service(
          auth_service_implementation_name,
          reinterpret_cast<my_h_service>(svc)))
    return true;
  event_tracking_authentication_initialized = true;
  return false;
}

static bool deinit_event_tracking_authentication() {
  if (event_tracking_authentication_initialized &&
      srv_registry_registration->unregister(auth_service_implementation_name)) {
    assert(0);  // this should not happen. There's leaked references if it does.
    return true;
  }
  event_tracking_authentication_initialized = false;
  return false;
}

/*
  caching_sha2_password plugin declaration
*/

mysql_declare_plugin(caching_sha2_password){
    MYSQL_AUTHENTICATION_PLUGIN, /* plugin type                   */
    &caching_sha2_auth_handler,  /* type specific descriptor      */
    Cached_authentication_plugins::get_plugin_name(
        PLUGIN_CACHING_SHA2_PASSWORD),      /* plugin name          */
    PLUGIN_AUTHOR_ORACLE,                   /* author                        */
    "Caching sha2 authentication",          /* description                   */
    PLUGIN_LICENSE_GPL,                     /* license                       */
    caching_sha2_authentication_init,       /* plugin initializer            */
    nullptr,                                /* Uninstall notifier            */
    caching_sha2_authentication_deinit,     /* plugin deinitializer          */
    0x0100,                                 /* version (1.0)                 */
    caching_sha2_password_status_variables, /* status variables              */
    caching_sha2_password_sysvars,          /* system variables              */
    nullptr,                                /* reserved                      */
    0,                                      /* flags                         */
} mysql_declare_plugin_end;
