/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

#include "auth_ldap_sasl_client.h"

#ifdef HAVE_SASL_SASL_H
#include <sys/types.h>
#endif

#if defined(SASL_CUSTOM_LIBRARY)
#include <dlfcn.h>
#include <link.h>
#include <string>
#endif

#include <sasl/sasl.h>
#ifndef WIN32
#include <lber.h>
#endif

#include <errmsg.h>
#include <m_string.h>
#include <mysql.h>
#include <mysql/plugin.h>
#include <mysql/plugin_auth_common.h>
#include <mysql/service_mysql_alloc.h>

#include <iomanip>
#include <sstream>

#include "log_client.h"

namespace auth_ldap_sasl_client {

/**
 Write a buffer to stream. Helper to log SASL messages.

 @param log_stream [in] the stream
 @param buf [in] the buffer
 @param buf_len [in] length of the buffer
*/
void buf_to_str(std::stringstream &log_stream, const char *buf,
                size_t buf_len) {
  log_stream << std::hex << std::setfill('0');
  for (size_t i = 0; i < buf_len; ++i)
    log_stream << std::setw(2) << (0xFF & buf[i]) << " ";
}

void Sasl_client::interact(sasl_interact_t *ilist) {
  while (ilist->id != SASL_CB_LIST_END) {
    switch (ilist->id) {
      /*
        the name of the user authenticating
      */
      case SASL_CB_USER:
        ilist->result = m_user_name;
        ilist->len = strlen((const char *)ilist->result);
        break;
      /* the name of the user acting for. (for example postman delivering mail
         for Martin might have an AUTHNAME of postman and a USER of Martin)
      */
      case SASL_CB_AUTHNAME:
        ilist->result = m_user_name;
        ilist->len = strlen((const char *)ilist->result);
        break;
      case SASL_CB_PASS:
        ilist->result = m_user_pwd;
        ilist->len = strlen((const char *)ilist->result);
        break;
      default:
        ilist->result = nullptr;
        ilist->len = 0;
    }
    ilist++;
  }
}

bool Sasl_client::set_mechanism() {
  int rc_server_read = -1;
  unsigned char *packet = nullptr;

  if (m_vio == nullptr) return false;

  /*
    If MySQL user name is empty it may be set by
    sasl_client.get_default_user_name(). We store the original, empty user name
    and reset it to mysql at the end of this function to ensure the memory is
    allocated and deallocated in the same binary.
   */
  if (m_mysql->user[0] == '\0' && !set_user()) {
    log_error("No default user, use --user option.");
    return false;
  }

  set_user_info(m_mysql->user, m_mysql->passwd);

  /** Get authentication method from the server. */
  rc_server_read = m_vio->read_packet(m_vio, &packet);
  if (rc_server_read < 0) {
    log_dbg("Authentication method not yet sent from the server.");
    return false;
  }

  if (packet[rc_server_read] != 0) {
    log_error(
        "Mechanism name returned by server is not a null terminated string.");
    return false;
  }
  if (!Sasl_mechanism::create_sasl_mechanism(reinterpret_cast<char *>(packet),
                                             m_sasl_mechanism)) {
    log_error(
        "Mechanism name returned by server: ", reinterpret_cast<char *>(packet),
        " is not supported by the client plugin.");
    return false;
  }

  return true;
}

Sasl_client::Sasl_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
    : m_connection(nullptr),
      m_vio(vio),
      m_mysql(mysql),
      m_mysql_user(nullptr),
      m_sasl_mechanism(nullptr) {
  m_user_name[0] = '\0';
  m_user_pwd[0] = '\0';
}
bool Sasl_client::preauthenticate() {
  assert(m_sasl_mechanism);
  return m_sasl_mechanism->preauthenticate(m_user_name, m_user_pwd);
}

bool Sasl_client::initilize_connection() {
  int rc_sasl = SASL_FAIL;
  assert(m_sasl_mechanism);

  /** Creating sasl connection. */
  rc_sasl = sasl_client_new(
      SASL_SERVICE_NAME, m_sasl_mechanism->get_ldap_host(), nullptr, nullptr,
      m_sasl_mechanism->get_callbacks(), 0, &m_connection);
  if (rc_sasl != SASL_OK || m_connection == nullptr) {
    std::stringstream log_stream;
    log_stream << "SASL client initialization failed with " << rc_sasl;
    log_error(log_stream.str().c_str());
    return false;
  }
  return true;
}

Sasl_client::~Sasl_client() {
  if (m_connection) {
    sasl_dispose(&m_connection);
    m_connection = nullptr;
  }
  if (m_mysql_user) {
    my_free(m_mysql->user);
    m_mysql->user = m_mysql_user;
    m_mysql_user = nullptr;
  }
  delete m_sasl_mechanism;
  m_sasl_mechanism = nullptr;
}

int Sasl_client::send_sasl_request_to_server(const char *request,
                                             int request_len, char **response,
                                             int *response_len) {
  int rc_server = 1;
  std::stringstream log_stream;

  if (m_vio == nullptr) {
    goto EXIT;
  }
  /** Send the request to the MySQL server. */
  if (request) {
    log_stream << "Sending SASL request: ";
    buf_to_str(log_stream, request, request_len);
  } else
    log_stream << "Sending empty SASL request.";
  log_dbg(log_stream.str().c_str());

  rc_server = m_vio->write_packet(
      m_vio, reinterpret_cast<const unsigned char *>(request), request_len);
  if (rc_server) {
    log_error("Failed to send SASL request to MySQL server.");
    goto EXIT;
  }

  /** Get the sasl response from the MySQL server. */
  *response_len =
      m_vio->read_packet(m_vio, reinterpret_cast<unsigned char **>(response));
  if ((*response_len) < 0 || (*response == nullptr)) {
    log_error("Failed to read SASL response from MySQL server.");
    goto EXIT;
  }
  log_stream.str("");
  log_stream << "Received SASL response: ";
  buf_to_str(log_stream, *response, *response_len);
  log_dbg(log_stream.str().c_str());
EXIT:
  return rc_server;
}

int Sasl_client::sasl_start(const char **client_output,
                            int *client_output_length) {
  int rc_sasl = SASL_FAIL;
  sasl_interact_t *interactions = nullptr;
  std::stringstream log_stream;

  assert(m_sasl_mechanism);
  assert(m_connection);

  do {
    rc_sasl = sasl_client_start(
        m_connection, m_sasl_mechanism->get_mechanism_name(), &interactions,
        client_output, reinterpret_cast<unsigned int *>(client_output_length),
        nullptr);
    if (rc_sasl == SASL_INTERACT) interact(interactions);
  } while (rc_sasl == SASL_INTERACT);
  if (rc_sasl == SASL_NOMECH) {
    log_error("SASL method '", m_sasl_mechanism->get_mechanism_name(),
              "' sent by server, ", "is not supported by the SASL client.");
    return rc_sasl;
  }
  if (*client_output) {
    log_stream << "SASL initial client request: ";
    buf_to_str(log_stream, *client_output, *client_output_length);
    log_dbg(log_stream.str().c_str());
  }

  return rc_sasl;
}

int Sasl_client::sasl_step(char *server_input, int server_input_length,
                           const char **client_output,
                           int *client_output_length) {
  int rc_sasl = SASL_FAIL;
  sasl_interact_t *interactions = nullptr;

  assert(m_connection);

  do {
    if (server_input && server_input[0] == 0x0) {
      server_input_length = 0;
      server_input = nullptr;
    }
    rc_sasl = sasl_client_step(
        m_connection, (server_input == nullptr) ? nullptr : server_input,
        (server_input == nullptr) ? 0 : server_input_length, &interactions,
        client_output, reinterpret_cast<unsigned int *>(client_output_length));
    if (rc_sasl == SASL_INTERACT) Sasl_client::interact(interactions);
  } while (rc_sasl == SASL_INTERACT);
  return rc_sasl;
}

bool Sasl_client::set_user() {
  /*
   Empty user name is accepted only in case of GSSAPI authentication when valid
   Kerberos TGT exist. The default name will be the principal name.
   */
  if (!Sasl_mechanism::create_sasl_mechanism(Sasl_mechanism::SASL_GSSAPI,
                                             m_sasl_mechanism)) {
    log_error(
        "Empty user name may be accepted only in case of GSSAPI "
        "authentication, but this mechanism is not supported by the client "
        "plugin.");
    return false;
  }

  std::string user_name;
  if (m_sasl_mechanism->get_default_user(user_name)) {
    m_mysql_user = m_mysql->user;
    m_mysql->user =
        my_strdup(PSI_NOT_INSTRUMENTED, user_name.c_str(), MYF(MY_WME));
  }

  return m_mysql->user != nullptr && m_mysql->user[0] != 0;
}

void Sasl_client::set_user_info(const char *name, const char *pwd) {
  if (name) {
    strncpy(m_user_name, name, sizeof(m_user_name) - 1);
    m_user_name[sizeof(m_user_name) - 1] = '\0';
  } else
    m_user_name[0] = 0;
  if (pwd) {
    strncpy(m_user_pwd, pwd, sizeof(m_user_pwd) - 1);
    m_user_pwd[sizeof(m_user_pwd) - 1] = '\0';
  } else
    m_user_pwd[0] = 0;
}

#ifdef __clang__
// Clang UBSAN false positive?
// Call to function through pointer to incorrect function type
static int sasl_authenticate(MYSQL_PLUGIN_VIO *vio,
                             MYSQL *mysql) SUPPRESS_UBSAN;
static int initialize_plugin(char *, size_t, int, va_list) SUPPRESS_UBSAN;
static int deinitialize_plugin() SUPPRESS_UBSAN;
#endif  // __clang__

/*
 * authenticate via SASL.
 *
 * executes the SASL full handshake.
 *
 * @returns one of the CR_xxx codes.
 * @retval CR_OK on auth success
 * @retval CR_ERROR on generic auth failure
 * @retval CR_AUTH_HANDSHAKE on handshake failure
 */
static int sasl_authenticate(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql) {
  int rc_sasl = SASL_OK;
  int rc_auth = CR_ERROR;
  char *server_packet = nullptr;
  int server_packet_len = 0;
  const char *sasl_client_output = nullptr;
  int sasl_client_output_len = 0;
  Sasl_client sasl_client(vio, mysql);

  if (!sasl_client.set_mechanism()) {
    log_info("SASL mechanism not set.");
    return CR_ERROR;
  }

  if (!sasl_client.preauthenticate()) {
    log_error("SASL preauthentication failed.");
    return CR_ERROR;
  }

  if (!sasl_client.initilize_connection()) {
    log_error("SASL client initialization failed.");
    return CR_ERROR;
  }

  log_info("SASL client initialized.");

  rc_sasl =
      sasl_client.sasl_start(&sasl_client_output, &sasl_client_output_len);
  if ((rc_sasl != SASL_OK) && (rc_sasl != SASL_CONTINUE)) {
    log_error("SASL start failed.");
    goto EXIT;
  }

  /**
    Running SASL authentication step till authentication process is concluded
    MySQL server plug-in working as proxy for SASL / LDAP server.
  */
  do {
    server_packet = nullptr;
    server_packet_len = 0;
    const int send_res = sasl_client.send_sasl_request_to_server(
        sasl_client_output, sasl_client_output_len, &server_packet,
        &server_packet_len);
    if (send_res != 0) {
      rc_auth = CR_AUTH_HANDSHAKE;

      goto EXIT;
    }
    sasl_client_output = nullptr;
    rc_sasl =
        sasl_client.sasl_step((char *)server_packet, server_packet_len,
                              &sasl_client_output, &sasl_client_output_len);
    if (sasl_client_output_len == 0) {
      log_dbg("SASL step: empty client output.");
    }

  } while (rc_sasl == SASL_CONTINUE);

  if (rc_sasl == SASL_OK) {
    rc_auth = CR_OK;

    log_info("SASL authentication successful.");
    /**
      From client side, authentication has succeeded, but in case of some
      mechanism (e.g. GSSAPI) we need to send data to server side.
    */
    if (sasl_client.require_conclude_by_server()) {
      server_packet = nullptr;
      if (sasl_client.send_sasl_request_to_server(
              sasl_client_output, sasl_client_output_len, &server_packet,
              &server_packet_len))
        log_warning(
            "sasl_authenticate client failed to send conclusion to the "
            "server.");
      rc_auth = CR_OK;
    }
  } else {
    log_error("SASL authentication failed.");
  }

EXIT:
  if (rc_sasl != SASL_OK) {
    std::stringstream log_stream;
    log_stream << "SASL function failed with " << rc_sasl;
    log_error(log_stream.str().c_str());
  }
  return rc_auth;
}

#if defined(SASL_CUSTOM_LIBRARY) || defined(WIN32)
/**
  Tell SASL where to look for plugins:

  Custom versions of libsasl2.so and libscram.so will be copied to
    <build directory>/library_output_directory/
  and
    <build directory>/library_output_directory/sasl2
  respectively during build, and to
    <install directory>/lib/private
    <install directory>/lib/private/sasl2
  after 'make install'.

  sasl_set_path() must be called before sasl_client_init(),
  and is not thread-safe.
 */
static int set_sasl_plugin_path() {
#if defined(WIN32)

  char libsasl_dir[MAX_PATH] = "";
  const char *sub_dir = "\\sasl2";
  HMODULE dll_handle(nullptr);
  int libsasl_path_len(0);

  /**
  Compute SASL plugins directory as follows :
   1) sasl_set_path() is provided by libsasl.dll, get handle to libsasl.dll
   2) using this handle get full path of libsasl.dll
   3) from that path extract the dir -this is the dir with all 3. party dlls
   4) SASL plugins are located in the \sasl2 subdir of this dir
   */

  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&sasl_set_path),
                         &dll_handle)) {
    log_error(
        "SASL client initialization failed: cannot get handle of libsasl.dll.");
    return 1;
  }

  libsasl_path_len = GetModuleFileName(dll_handle, libsasl_dir, MAX_PATH);
  if ((libsasl_path_len == 0) || (libsasl_path_len == sizeof(libsasl_dir))) {
    log_error(
        "SASL client initialization failed: cannot get path of libsasl.dll.");
    return 1;
  }

  char *path_end = strrchr(libsasl_dir, '\\');
  if (path_end == nullptr) path_end = libsasl_dir;
  if (libsasl_dir + sizeof(libsasl_dir) < path_end + sizeof(sub_dir)) {
    log_error(
        "SASL client initialization failed: cannot compute SASL plugins dir.");
    return 1;
  }
  strcpy(path_end, sub_dir);
  sasl_set_path(SASL_PATH_TYPE_PLUGIN, libsasl_dir);
  log_info("SASL client initialization: SASL plugin path is ", libsasl_dir);
  return 0;
#elif defined(SASL_CUSTOM_LIBRARY)
  char sasl_plugin_dir[PATH_MAX]{};
  // dlopen(NULL, ) should not fail ...
  void *main_handle = dlopen(nullptr, RTLD_LAZY);
  if (main_handle == nullptr) {
    log_error(dlerror());
    return 1;
  }
  struct link_map *lm = nullptr;
  if (0 != dlinfo(main_handle, RTLD_DI_LINKMAP, &lm)) {
    log_error(dlerror());
    dlclose(main_handle);
    return 1;
  }
  size_t find_result = std::string::npos;
  for (; lm; lm = lm->l_next) {
    std::string so_path(lm->l_name);
    find_result = so_path.find("/libsasl");
    if (find_result != std::string::npos) {
      std::string so_dir(lm->l_name, find_result);
      so_dir.append("/sasl2");
      so_dir.copy(sasl_plugin_dir, so_dir.size());
      sasl_set_path(SASL_PATH_TYPE_PLUGIN, sasl_plugin_dir);
      break;
    }
  }
  dlclose(main_handle);
  if (find_result == std::string::npos) {
    log_error("Cannot find SASL plugins");
    return 1;
  }
  return 0;
#endif  // defined(SASL_CUSTOM_LIBRARY)
}
#endif  // defined(SASL_CUSTOM_LIBRARY) || defined(WIN32)

static int initialize_plugin(char *, size_t, int, va_list) {
  const char *log_level_opt = getenv("AUTHENTICATION_LDAP_CLIENT_LOG");
  if (log_level_opt == nullptr) {
    Ldap_logger::create_logger(ldap_log_level::LDAP_LOG_LEVEL_NONE);
  } else {
    int log_level_val = atoi(log_level_opt);

    if (log_level_val < ldap_log_level::LDAP_LOG_LEVEL_NONE ||
        log_level_val > ldap_log_level::LDAP_LOG_LEVEL_ALL) {
      Ldap_logger::create_logger(ldap_log_level::LDAP_LOG_LEVEL_ERROR_WARNING);
      log_warning(
          "AUTHENTICATION_LDAP_CLIENT_LOG environment variable has incorrect "
          "value, correct values are 1-5. Setting log level to WARNING.");
    } else
      Ldap_logger::create_logger(static_cast<ldap_log_level>(log_level_val));
  }

  int rc_sasl;
#if defined(SASL_CUSTOM_LIBRARY) || defined(WIN32)
  rc_sasl = set_sasl_plugin_path();
  if (rc_sasl != SASL_OK) {
    // Error already logged.
    return 1;
  }
#endif  // SASL_CUSTOM_LIBRARY

  /** Initialize client-side of SASL. */
  rc_sasl = sasl_client_init(nullptr);
  if (rc_sasl != SASL_OK) {
    std::stringstream log_stream;
    log_stream << "sasl_client_init failed with " << rc_sasl;
    log_error(log_stream.str().c_str());
    return 1;
  }

  return 0;
}

static int deinitialize_plugin() {
  Ldap_logger::destroy_logger();
#if defined(SASL_CLIENT_DONE_SUPPORTED)
  sasl_client_done();
#else
  sasl_done();
#endif
  return 0;
}

}  // namespace auth_ldap_sasl_client

mysql_declare_client_plugin(AUTHENTICATION) "authentication_ldap_sasl_client",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE, "LDAP SASL Client Authentication Plugin",
    {0, 1, 0}, "PROPRIETARY", nullptr, auth_ldap_sasl_client::initialize_plugin,
    auth_ldap_sasl_client::deinitialize_plugin, nullptr,
    nullptr, auth_ldap_sasl_client::sasl_authenticate,
    nullptr mysql_end_client_plugin;
