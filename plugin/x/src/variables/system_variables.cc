/*
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
*/

#include "plugin/x/src/variables/system_variables.h"

#include <cstdlib>
#include <limits>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)
#include "my_sys.h"       // NOLINT(build/include_subdir)
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/service_mysql_alloc.h"
#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/variables/system_variables_defaults.h"

namespace xpl {

namespace details {

template <typename Copy_type>
void update_plugin_system_variable(THD *thd, SYS_VAR *, void *tgt,
                                   const void *save) {
  *static_cast<Copy_type *>(tgt) = *static_cast<const Copy_type *>(save);

  Plugin_system_variables::fetch_plugin_variables();
}

template <typename Copy_type,
          void (xpl::iface::Client::*method)(const Copy_type value)>
void update_thd_system_variable(THD *thd, SYS_VAR *sys_var, void *tgt,
                                const void *save) {
  const auto value = *static_cast<const Copy_type *>(save);

  *static_cast<Copy_type *>(tgt) = value;

  Plugin_system_variables::fetch_plugin_variables();

  auto get_client = Plugin_system_variables::get_client_callback();
  if (get_client) {
    auto client = get_client(thd);

    if (client) (client.get()->*method)(value);
  }
}

const char *choose_system_variable_value(const char *cnf_option,
                                         const char *env_variable,
                                         const char *compile_option) {
  if (nullptr != cnf_option) {
    return cnf_option;
  }

  const char *variable_from_env = env_variable ? getenv(env_variable) : nullptr;

  if (nullptr != variable_from_env) return variable_from_env;

  return compile_option;
}

void setup_variable_value_from_env_or_compile_opt(char **cnf_option,
                                                  const char *env_variable,
                                                  const char *compile_option) {
  char *value_old = *cnf_option;
  const char *result = details::choose_system_variable_value(
      *cnf_option, env_variable, compile_option);

  if (nullptr != result)
    *cnf_option = my_strdup(PSI_NOT_INSTRUMENTED, const_cast<char *>(result),
                            MYF(MY_WME));
  else
    *cnf_option = nullptr;

  if (nullptr != value_old) my_free(value_old);
}

template <typename Commpresion_level_variable>
int check_compression_level_range(THD * /*thd*/, SYS_VAR * /*var*/, void *save,
                                  st_mysql_value *value) {
  longlong val;
  value->val_int(value, &val);
  if (Commpresion_level_variable::check_range(val)) {
    *(reinterpret_cast<int *>(save)) = val;
    return 0;
  }
  return 1;
}

}  // namespace details

using xpl_sys_var = xpl::Plugin_system_variables;

static MYSQL_SYSVAR_UINT(
    port, Plugin_system_variables::m_port,
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
    "Port on which X Plugin is going to accept incoming connections.", nullptr,
    nullptr, defaults::connectivity::k_port, 1,
    std::numeric_limits<uint16_t>::max(), 0);

static MYSQL_SYSVAR_INT(max_connections,
                        Plugin_system_variables::m_max_connections,
                        PLUGIN_VAR_OPCMDARG,
                        "Maximum number of concurrent X protocol connections. "
                        "Actual number of connections is also affected by the "
                        "general max_connections.",
                        nullptr, nullptr,
                        defaults::connectivity::k_max_connections, 1,
                        std::numeric_limits<uint16_t>::max(), 0);

static MYSQL_SYSVAR_UINT(min_worker_threads,
                         Plugin_system_variables::m_min_worker_threads,
                         PLUGIN_VAR_OPCMDARG,
                         "Minimal number of worker threads.", nullptr,
                         &details::update_plugin_system_variable<unsigned int>,
                         defaults::threads::k_min_worker_threads, 1, 100, 0);

static MYSQL_SYSVAR_UINT(
    idle_worker_thread_timeout,
    Plugin_system_variables::m_idle_worker_thread_timeout, PLUGIN_VAR_OPCMDARG,
    "Time after which an idle worker thread is terminated (in seconds).",
    nullptr, &details::update_plugin_system_variable<unsigned int>,
    defaults::threads::k_idle_worker_thread_timeout, 0, 60 * 60, 0);

static MYSQL_SYSVAR_UINT(
    max_allowed_packet, Plugin_system_variables::m_max_allowed_packet,
    PLUGIN_VAR_OPCMDARG,
    "Size of largest message that client is going to handle.", nullptr,
    &details::update_plugin_system_variable<unsigned int>,
    defaults::connectivity::k_max_allowed_packet, BYTE(512), GBYTE(1), 0);

static MYSQL_SYSVAR_UINT(
    connect_timeout, Plugin_system_variables::m_connect_timeout,
    PLUGIN_VAR_OPCMDARG,
    "Maximum allowed waiting time for connection to setup a session (in "
    "seconds).",
    nullptr, &details::update_plugin_system_variable<unsigned int>,
    defaults::timeout::k_connect_timeout, 1, 1000000000, 0);

static MYSQL_SYSVAR_STR(ssl_key,
                        Plugin_system_variables::m_ssl_config.m_ssl_key,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
                        "X509 key in PEM format.", nullptr, nullptr, nullptr);

static MYSQL_SYSVAR_STR(ssl_ca, Plugin_system_variables::m_ssl_config.m_ssl_ca,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
                        "CA file in PEM format.", nullptr, nullptr, nullptr);

static MYSQL_SYSVAR_STR(ssl_capath,
                        Plugin_system_variables::m_ssl_config.m_ssl_capath,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
                        "CA directory.", nullptr, nullptr, nullptr);

static MYSQL_SYSVAR_STR(ssl_cert,
                        Plugin_system_variables::m_ssl_config.m_ssl_cert,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
                        "X509 cert in PEM format.", nullptr, nullptr, nullptr);

static MYSQL_SYSVAR_STR(ssl_cipher,
                        Plugin_system_variables::m_ssl_config.m_ssl_cipher,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
                        "SSL cipher to use.", nullptr, nullptr, nullptr);

static MYSQL_SYSVAR_STR(ssl_crl,
                        Plugin_system_variables::m_ssl_config.m_ssl_crl,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
                        "Certificate revocation list.", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_STR(ssl_crlpath,
                        Plugin_system_variables::m_ssl_config.m_ssl_crlpath,
                        PLUGIN_VAR_READONLY,
                        "Certificate revocation list path.", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_STR(socket, Plugin_system_variables::m_socket,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG |
                            PLUGIN_VAR_MEMALLOC,
                        "X Plugin's unix socket for local connection.", nullptr,
                        nullptr, nullptr);

static MYSQL_SYSVAR_STR(
    bind_address, Plugin_system_variables::m_bind_address,
    PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
    "Address to which X Plugin should bind the TCP socket optionally "
    "followed by a network namespace delimited with /. "
    "E.g., the string value 127.0.0.1/red specifies to listen on "
    "IP address 127.0.0.1 from the network namespace 'red'.",
    nullptr, nullptr, "*");

static MYSQL_SYSVAR_UINT(
    port_open_timeout, Plugin_system_variables::m_port_open_timeout,
    PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG,
    "How long X Plugin is going to retry binding of server socket (in case of "
    "failure)",
    nullptr, &details::update_plugin_system_variable<unsigned int>,
    defaults::timeout::k_port_open_timeout, 0, 120, 0);

static MYSQL_THDVAR_UINT(
    wait_timeout, PLUGIN_VAR_OPCMDARG,
    "Number or seconds that X Plugin must wait for activity on noninteractive "
    "connection",
    nullptr,
    (&details::update_thd_system_variable<
        uint32_t, &xpl::iface::Client::set_wait_timeout>),
    defaults::timeout::k_wait_timeout, 1, 2147483, 0);

static MYSQL_SYSVAR_UINT(
    interactive_timeout, Plugin_system_variables::m_interactive_timeout,
    PLUGIN_VAR_OPCMDARG,
    "Default value for \"mysqlx_wait_timeout\", when the connection is "
    "interactive. The value defines number or seconds that X Plugin must "
    "wait for activity on interactive connection",
    nullptr, &details::update_plugin_system_variable<uint32_t>,
    defaults::timeout::k_interactive_timeout, 1, 2147483, 0);

static MYSQL_THDVAR_UINT(
    read_timeout, PLUGIN_VAR_OPCMDARG,
    "Number or seconds that X Plugin must wait for blocking read operation to "
    "complete",
    nullptr,
    (&details::update_thd_system_variable<
        uint32_t, &xpl::iface::Client::set_read_timeout>),
    defaults::timeout::k_read_timeout, 1, 2147483, 0);

static MYSQL_THDVAR_UINT(
    write_timeout, PLUGIN_VAR_OPCMDARG,
    "Number or seconds that X Plugin must wait for blocking write operation to "
    "complete",
    nullptr,
    (&details::update_thd_system_variable<
        uint32_t, &xpl::iface::Client::set_write_timeout>),
    defaults::timeout::k_write_timeout, 1, 2147483, 0);

static MYSQL_SYSVAR_UINT(
    document_id_unique_prefix,
    Plugin_system_variables::m_document_id_unique_prefix, PLUGIN_VAR_OPCMDARG,
    "Unique prefix is a value assigned by InnoDB cluster to the instance, "
    "which is meant to make document id unique across all replicasets from "
    "the same cluster",
    nullptr, &details::update_plugin_system_variable<uint32_t>,
    defaults::docstore::k_document_id_unique_prefix, 0,
    std::numeric_limits<uint16_t>::max(), 0);

static MYSQL_SYSVAR_BOOL(
    enable_hello_notice, xpl_sys_var::m_enable_hello_notice,
    PLUGIN_VAR_OPCMDARG,
    "Hello notice is a X Protocol message send by the server after connection "
    "establishment, using this variable it can be disabled",
    nullptr, &details::update_plugin_system_variable<bool>,
    defaults::connectivity::k_enable_hello_notice);

static MYSQL_SYSVAR_SET(
    compression_algorithms, *xpl_sys_var::m_compression_algorithms.value(),
    PLUGIN_VAR_OPCMDARG,
    "Compression algorithms: where option can be DEFLATE_STREAM, LZ4_MESSAGE, "
    "ZSTD_STREAM",
    nullptr, &details::update_plugin_system_variable<ulonglong>,
    7 /* default=DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM */,
    xpl_sys_var::m_compression_algorithms.typelib());

static MYSQL_SYSVAR_INT(
    deflate_default_compression_level,
    *xpl_sys_var::m_deflate_default_compression_level.value(),
    PLUGIN_VAR_OPCMDARG,
    "Default value of compression level for deflate algorithm",
    &details::check_compression_level_range<
        xpl::Compression_deflate_level_variable>,
    &details::update_plugin_system_variable<int32_t>, 3,
    xpl_sys_var::m_deflate_default_compression_level.min(),
    xpl_sys_var::m_deflate_default_compression_level.max(), 0);

static MYSQL_SYSVAR_INT(lz4_default_compression_level,
                        *xpl_sys_var::m_lz4_default_compression_level.value(),
                        PLUGIN_VAR_OPCMDARG,
                        "Default value of compression level for lz4 algorithm",
                        &details::check_compression_level_range<
                            xpl::Compression_lz4_level_variable>,
                        &details::update_plugin_system_variable<int32_t>, 2,
                        xpl_sys_var::m_lz4_default_compression_level.min(),
                        xpl_sys_var::m_lz4_default_compression_level.max(), 0);

static MYSQL_SYSVAR_INT(zstd_default_compression_level,
                        *xpl_sys_var::m_zstd_default_compression_level.value(),
                        PLUGIN_VAR_OPCMDARG,
                        "Default value of compression level for zstd algorithm",
                        &details::check_compression_level_range<
                            xpl::Compression_zstd_level_variable>,
                        &details::update_plugin_system_variable<int32_t>, 3,
                        xpl_sys_var::m_zstd_default_compression_level.min(),
                        xpl_sys_var::m_zstd_default_compression_level.max(), 0);

static MYSQL_SYSVAR_INT(
    deflate_max_client_compression_level,
    *xpl_sys_var::m_deflate_max_client_compression_level.value(),
    PLUGIN_VAR_OPCMDARG, "Max value of compression level for deflate algorithm",
    &details::check_compression_level_range<
        xpl::Compression_deflate_level_variable>,
    &details::update_plugin_system_variable<int32_t>, 5,
    xpl_sys_var::m_deflate_max_client_compression_level.min(),
    xpl_sys_var::m_deflate_max_client_compression_level.max(), 0);

static MYSQL_SYSVAR_INT(
    lz4_max_client_compression_level,
    *xpl_sys_var::m_lz4_max_client_compression_level.value(),
    PLUGIN_VAR_OPCMDARG, "Max value of compression level for lz4 algorithm",
    &details::check_compression_level_range<
        xpl::Compression_lz4_level_variable>,
    &details::update_plugin_system_variable<int32_t>, 8,
    xpl_sys_var::m_lz4_max_client_compression_level.min(),
    xpl_sys_var::m_lz4_max_client_compression_level.max(), 0);

static MYSQL_SYSVAR_INT(
    zstd_max_client_compression_level,
    *xpl_sys_var::m_zstd_max_client_compression_level.value(),
    PLUGIN_VAR_OPCMDARG, "Max value of compression level for zstd algorithm",
    &details::check_compression_level_range<
        xpl::Compression_zstd_level_variable>,
    &details::update_plugin_system_variable<int32_t>,
    xpl_sys_var::m_zstd_max_client_compression_level.min() ==
            xpl_sys_var::m_zstd_max_client_compression_level.max()
        ? xpl_sys_var::m_zstd_max_client_compression_level.min()
        : 11,
    xpl_sys_var::m_zstd_max_client_compression_level.min(),
    xpl_sys_var::m_zstd_max_client_compression_level.max(), 0);

// Storage for global variables
//
int Plugin_system_variables::m_max_connections;
unsigned int Plugin_system_variables::m_port;
unsigned int Plugin_system_variables::m_min_worker_threads;
unsigned int Plugin_system_variables::m_idle_worker_thread_timeout;
unsigned int Plugin_system_variables::m_max_allowed_packet;
unsigned int Plugin_system_variables::m_connect_timeout;
char *Plugin_system_variables::m_socket;
unsigned int Plugin_system_variables::m_port_open_timeout;
char *Plugin_system_variables::m_bind_address;
uint32_t Plugin_system_variables::m_interactive_timeout;
uint32_t Plugin_system_variables::m_document_id_unique_prefix;
bool Plugin_system_variables::m_enable_hello_notice;
Set_variable Plugin_system_variables::m_compression_algorithms{
    {"DEFLATE_STREAM", "LZ4_MESSAGE", "ZSTD_STREAM"}};

Compression_deflate_level_variable
    Plugin_system_variables::m_deflate_default_compression_level;
Compression_lz4_level_variable
    Plugin_system_variables::m_lz4_default_compression_level;
Compression_zstd_level_variable
    Plugin_system_variables::m_zstd_default_compression_level;

Compression_deflate_level_variable
    Plugin_system_variables::m_deflate_max_client_compression_level;
Compression_lz4_level_variable
    Plugin_system_variables::m_lz4_max_client_compression_level;
Compression_zstd_level_variable
    Plugin_system_variables::m_zstd_max_client_compression_level;
Ssl_config Plugin_system_variables::m_ssl_config;

// Other static data
Plugin_system_variables::Value_changed_callback
    Plugin_system_variables::m_update_callback;

Plugin_system_variables::Get_client_callback
    Plugin_system_variables::m_client_callback;

iface::Service_sys_variables *Plugin_system_variables::m_sys_var = nullptr;

struct SYS_VAR *Plugin_system_variables::m_plugin_system_variables[] = {
    MYSQL_SYSVAR(port),
    MYSQL_SYSVAR(max_connections),
    MYSQL_SYSVAR(min_worker_threads),
    MYSQL_SYSVAR(idle_worker_thread_timeout),
    MYSQL_SYSVAR(max_allowed_packet),
    MYSQL_SYSVAR(connect_timeout),
    MYSQL_SYSVAR(ssl_key),
    MYSQL_SYSVAR(ssl_ca),
    MYSQL_SYSVAR(ssl_capath),
    MYSQL_SYSVAR(ssl_cert),
    MYSQL_SYSVAR(ssl_cipher),
    MYSQL_SYSVAR(ssl_crl),
    MYSQL_SYSVAR(ssl_crlpath),
    MYSQL_SYSVAR(socket),
    MYSQL_SYSVAR(bind_address),
    MYSQL_SYSVAR(port_open_timeout),
    MYSQL_SYSVAR(wait_timeout),
    MYSQL_SYSVAR(interactive_timeout),
    MYSQL_SYSVAR(read_timeout),
    MYSQL_SYSVAR(write_timeout),
    MYSQL_SYSVAR(document_id_unique_prefix),
    MYSQL_SYSVAR(enable_hello_notice),
    MYSQL_SYSVAR(compression_algorithms),
    MYSQL_SYSVAR(deflate_default_compression_level),
    MYSQL_SYSVAR(lz4_default_compression_level),
    MYSQL_SYSVAR(zstd_default_compression_level),
    MYSQL_SYSVAR(deflate_max_client_compression_level),
    MYSQL_SYSVAR(lz4_max_client_compression_level),
    MYSQL_SYSVAR(zstd_max_client_compression_level),
    nullptr};

std::string Plugin_system_variables::get_system_variable(
    const std::string &name, bool *out_error) {
  std::string result(2048, ' ');

  char *buffer_ptr = &result[0];
  size_t buffer_size = result.length();

  if (!m_sys_var->get_variable("mysql_server", name.c_str(),
                               reinterpret_cast<void **>(&buffer_ptr),
                               &buffer_size)) {
    if (out_error) *out_error = true;
    return "";
  }

  result.resize(buffer_size);

  return result;
}

void Plugin_system_variables::fetch_plugin_variables() {
  if (m_update_callback) m_update_callback(nullptr);
}

const Timeouts_config Plugin_system_variables::get_global_timeouts() {
  return {xpl_sys_var::m_interactive_timeout, THDVAR(nullptr, wait_timeout),
          THDVAR(nullptr, read_timeout), THDVAR(nullptr, write_timeout)};
}

void Plugin_system_variables::set_thd_wait_timeout(
    THD *thd, const uint32_t timeout_value) {
  THDVAR(thd, wait_timeout) = timeout_value;
}

Plugin_system_variables::Get_client_callback
Plugin_system_variables::get_client_callback() {
  return m_client_callback;
}

void Plugin_system_variables::initialize(iface::Service_sys_variables *sys_var,
                                         Value_changed_callback update_callback,
                                         Get_client_callback client_callback,
                                         const bool fetch) {
  m_sys_var = sys_var;
  m_update_callback = update_callback;
  m_client_callback = client_callback;

  details::setup_variable_value_from_env_or_compile_opt(
      &m_socket, "MYSQLX_UNIX_PORT", MYSQLX_UNIX_ADDR);

  if (fetch) {
    fetch_plugin_variables();
  }
}

void Plugin_system_variables::cleanup() {
  m_sys_var = nullptr;
  m_update_callback = nullptr;
  m_client_callback = nullptr;
}

}  // namespace xpl
