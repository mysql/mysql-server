/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/**
@file clone/src/clone_plugin.cc
Clone Plugin: Plugin interface

*/

#include <mysql/plugin.h>
#include <mysql/plugin_clone.h>

#include "plugin/clone/include/clone_client.h"
#include "plugin/clone/include/clone_local.h"
#include "plugin/clone/include/clone_server.h"
#include "plugin/clone/include/clone_status.h"

#include <algorithm>
#include <cctype>
#include <string>

#define CLONE_PLUGIN_VERSION 0x0100

/** Clone plugin name */
const char *clone_plugin_name = "clone";

/** Clone system variable: buffer size for data transfer */
uint clone_buffer_size;

/** Clone system variable: If clone should block concurrent DDL */
bool clone_block_ddl;

/** Clone system variable: timeout for DDL lock */
uint clone_ddl_timeout;

/** Clone system variable: If concurrency is automatically tuned */
bool clone_autotune_concurrency;

/** Clone system variable: Maximum concurrent threads */
uint clone_max_concurrency;

/** Clone system variable: Maximum network bandwidth in MiB/sec */
uint clone_max_network_bandwidth;

/** Clone system variable: Maximum IO bandwidth in MiB/sec */
uint clone_max_io_bandwidth;

/** Clone system variable: If network compression is enabled */
bool clone_enable_compression;

/** Clone system variable: valid list of donor addresses. */
static char *clone_valid_donor_list;

/** Clone system variable: SSL private key */
static char *clone_ssl_key;

/** Clone system variable: SSL Certificate */
static char *clone_ssl_cert;

/** Clone system variable: SSL Certificate authority */
static char *clone_ssl_ca;

/** Clone system variable: timeout for clone restart after n/w failure */
uint clone_restart_timeout;

/** Clone system variable: time delay after removing data */
uint clone_delay_after_data_drop;

/** Key for registering clone allocations with performance schema */
PSI_memory_key clone_mem_key;

/** Key for registering clone local worker threads */
PSI_thread_key clone_local_thd_key;

/** Key for registering clone client worker threads */
PSI_thread_key clone_client_thd_key;

/** Clone Local statement */
PSI_statement_key clone_stmt_local_key;

/** Clone Remote client statement */
PSI_statement_key clone_stmt_client_key;

/** Clone Remote server statement */
PSI_statement_key clone_stmt_server_key;

#ifdef HAVE_PSI_INTERFACE
/** Clone memory key for performance schema */
static PSI_memory_info clone_memory[] = {

    {&clone_mem_key, "data", 0, 0, PSI_DOCUMENT_ME}};

/** Clone thread key for performance schema */
static PSI_thread_info clone_threads[] = {
    {&clone_local_thd_key, "local-task", "clone_local", 0, 0, PSI_DOCUMENT_ME},
    {&clone_client_thd_key, "client-task", "clone_client", 0, 0,
     PSI_DOCUMENT_ME}};

static PSI_statement_info clone_stmts[] = {{0, "local", 0, PSI_DOCUMENT_ME},
                                           {0, "client", 0, PSI_DOCUMENT_ME},
                                           {0, "server", 0, PSI_DOCUMENT_ME}};
#endif /* HAVE_PSI_INTERFACE */

/* Use backup lock service from mysql server */
SERVICE_TYPE(registry) * mysql_service_registry;
SERVICE_TYPE(mysql_backup_lock) * mysql_service_mysql_backup_lock;
SERVICE_TYPE(clone_protocol) * mysql_service_clone_protocol;

/* Use mysql logging service */
SERVICE_TYPE(registry) * reg_srv;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

/* Namespace for all clone data types */
namespace myclone {

int validate_local_params(THD *thd) {
  /* Check if network packet size is enough. */
  Key_Values local_configs = {{"max_allowed_packet", ""}};

  int err =
      mysql_service_clone_protocol->mysql_clone_get_configs(thd, local_configs);

  if (err != 0) {
    return (err);
  }

  const std::string &val_str = local_configs[0].second;

  long long val = 0;
  bool is_exception = false;

  try {
    val = std::stoll(val_str);
  } catch (...) {
    is_exception = true; /* purecov: inspected */
  }

  if (is_exception || val <= 0) {
    /* purecov: begin deadcode */
    assert(false);
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Error extracting integer value for"
             "'max_allowed_packet' configuration");
    return (ER_INTERNAL_ERROR);
    /* purecov: end */
  }

  if (val < longlong{CLONE_MIN_NET_BLOCK}) {
    err = ER_CLONE_NETWORK_PACKET;
    my_error(err, MYF(0), CLONE_MIN_NET_BLOCK, val);
  }
  return (err);
}

}  // namespace myclone

using Donor_Callback = std::function<bool(std::string &, uint32_t)>;

/** Scan through donor list and call back after extracting host and port.
@param[in]	donor_list	all donor server list
@param[in]	callback	callback function
@return true, if scan is successful or match is found. */
static bool scan_donor_list(const std::string &donor_list,
                            Donor_Callback callback) {
  size_t comma_pos = 0;
  size_t begin_pos = 0;

  try {
    /* Don't allow space in donor list. */
    auto space_pos = donor_list.find(" ");
    if (space_pos != std::string::npos) {
      return (false);
    }
    /* Scan through all entries. */
    while (comma_pos != std::string::npos) {
      comma_pos = donor_list.find(",", begin_pos);
      auto entry_len = comma_pos;

      if (entry_len != std::string::npos) {
        if (comma_pos <= begin_pos) {
          return (false);
        }
        /* Exclude the comma separator. */
        entry_len = comma_pos - begin_pos;
      }

      std::string entry = donor_list.substr(begin_pos, entry_len);
      auto colon_pos = entry.find(":");

      /* Bad entry if no separator is found or found in beginning. */
      if (colon_pos == std::string::npos || colon_pos == 0) {
        return (false);
      }

      auto port_str = entry.substr(colon_pos + 1);
      /* Allow only decimal digit in PORT. */
      for (char &digit : port_str) {
        if (std::isdigit(digit) == 0) {
          return (false);
        }
      }
      auto valid_port = static_cast<uint32_t>(std::stoi(port_str));
      auto valid_host = entry.substr(0, colon_pos);

      bool match = callback(valid_host, valid_port);

      if (match) {
        return (true);
      }
      /* Set next begin position. */
      begin_pos = comma_pos + 1;
    }
  } catch (...) { /* purecov: inspected */
    /* If entry format is bad, return. */
    return (false); /* purecov: inspected */
  }
  return (true);
}

/** Validate the <HOST> and <PORT are configured in valid_donor_list
@param[in,out]	thd	user session THD
@param[in]	host	host name of donor
@param[in]	port	port number of donor
@return error code */
static int match_valid_donor_address(MYSQL_THD thd, const char *host,
                                     uint port) {
  myclone::Key_Values configs = {{"clone_valid_donor_list", ""}};

  /* Get Clone configuration parameter value safely. */
  auto err =
      mysql_service_clone_protocol->mysql_clone_get_configs(thd, configs);
  if (err != 0) {
    return (err);
  }

  auto &valid_str = configs[0].second;
  bool found = false;

  Donor_Callback callback = [&](std::string &valid_host, uint32_t valid_port) {
    /* Host in MySQL is case insensitive and converted to lower case. */
    auto transform_lower = [](unsigned char c) {
      return static_cast<unsigned char>(std::tolower(c));
    };
    std::transform(valid_host.begin(), valid_host.end(), valid_host.begin(),
                   transform_lower);

    /* Check if input matches with configured host and port. */
    if (0 == valid_host.compare(host) && port == valid_port) {
      found = true;
    }
    return (found);
  };

  static_cast<void>(scan_donor_list(valid_str, callback));

  if (found) {
    return (0);
  }

  char err_buf[MYSYS_ERRMSG_SIZE];

  snprintf(err_buf, sizeof(err_buf),
           "%s:%u is not found in "
           "clone_valid_donor_list: %s",
           host, port, valid_str.c_str());

  my_error(ER_CLONE_SYS_CONFIG, MYF(0), err_buf);

  return (ER_CLONE_SYS_CONFIG);
}

/** Check valid_donor_list format "<HOST1>:<PORT1>,<HOST2:PORT2,..."
@param[in]	thd	user session THD
@param[in]	var	system variable
@param[out]	save	possibly updated variable value
@param[in]	value	current variable value
@return error code */
static int check_donor_addr_format(MYSQL_THD thd, SYS_VAR *var [[maybe_unused]],
                                   void *save, struct st_mysql_value *value) {
  char temp_buffer[STRING_BUFFER_USUAL_SIZE];
  auto buf_len = static_cast<int>(sizeof(temp_buffer));

  auto addrs_cstring = value->val_str(value, temp_buffer, &buf_len);

  if (addrs_cstring && (addrs_cstring == temp_buffer)) {
    addrs_cstring = thd_strmake(thd, addrs_cstring, buf_len);
  }

  if (addrs_cstring == nullptr) {
    /* purecov: begin deadcode */
    (*(const char **)save) = nullptr;
    /* NULL is a valid value */
    return (0);
    /* purecov: end */
  }

  std::string addrs(addrs_cstring);

  Donor_Callback callback = [](std::string, uint32_t) { return (false); };

  bool success = scan_donor_list(addrs_cstring, callback);

  if (!success) {
    (*(const char **)save) = nullptr;
    my_error(ER_CLONE_SYS_CONFIG, MYF(0),
             "Invalid Format. Please enter "
             "\"<hostname1>:<port1>,...\"' with no extra space");
    return (ER_CLONE_SYS_CONFIG);
  }
  *(const char **)save = addrs_cstring;
  return (0);
}

/** Check if it is safe to uninstall plugin.
@param[in]	plugin_info	server plugin handle
@return error code */
static int plugin_clone_check(MYSQL_PLUGIN plugin_info) {
  return clone_handle_check_drop(plugin_info);
}

/** Initialize clone plugin
@param[in]	plugin_info	server plugin handle
@return error code */
static int plugin_clone_init(MYSQL_PLUGIN plugin_info [[maybe_unused]]) {
  /* Acquire registry and log service handles. */
  if (init_logging_service_for_plugin(&mysql_service_registry, &log_bi,
                                      &log_bs)) {
    return (-1);
  }

  my_h_service service;

  /* Acquire backup lock service handle. */
  if (mysql_service_registry->acquire("mysql_backup_lock", &service)) {
    return (-1);
  }
  mysql_service_mysql_backup_lock =
      reinterpret_cast<SERVICE_TYPE(mysql_backup_lock) *>(service);

  /* Acquire clone protocol service handle. */
  if (mysql_service_registry->acquire("clone_protocol", &service)) {
    return (-1);
  }
  mysql_service_clone_protocol =
      reinterpret_cast<SERVICE_TYPE(clone_protocol) *>(service);

  auto error = clone_handle_create(clone_plugin_name);

  /* During DB creation skip PFS dynamic tables. PFS is not fully initialized
  at this point. */
  bool skip_pfs_tables = false;
  if (error == ER_INIT_BOOTSTRAP_COMPLETE) {
    skip_pfs_tables = true;
  } else if (error != 0) {
    return (error);
  }

  if (!skip_pfs_tables && myclone::Table_pfs::acquire_services()) {
    LogPluginErr(ERROR_LEVEL, ER_CLONE_CLIENT_TRACE,
                 "PFS table creation failed");
    return (-1);
  }

#ifdef HAVE_PSI_INTERFACE
  /* Register memory key */
  int count = static_cast<int>(sizeof(clone_memory) / sizeof(clone_memory[0]));

  mysql_memory_register(clone_plugin_name, clone_memory, count);

  /* Register thread keys */
  count = static_cast<int>(sizeof(clone_threads) / sizeof(clone_threads[0]));

  mysql_thread_register(clone_plugin_name, clone_threads, count);

  /* Register statement keys */
  count = static_cast<int>(sizeof(clone_stmts) / sizeof(clone_stmts[0]));

  mysql_statement_register(clone_plugin_name, clone_stmts, count);

  /* Set the statement key values */
  assert(count >= 3);
  clone_stmt_local_key = clone_stmts[0].m_key;
  clone_stmt_client_key = clone_stmts[1].m_key;
  clone_stmt_server_key = clone_stmts[2].m_key;
#endif

  return (0);
}

/** Uninitialize clone plugin
@param[in]	plugin_info	server plugin handle
@return error code */
static int plugin_clone_deinit(MYSQL_PLUGIN plugin_info [[maybe_unused]]) {
  /* If service registry is uninitialized, return. */
  if (mysql_service_registry == nullptr) {
    return (0);
  }

  auto error = clone_handle_drop();

  if (error != ER_INIT_BOOTSTRAP_COMPLETE) {
    myclone::Table_pfs::release_services();
  }

  using backup_lock_t = SERVICE_TYPE_NO_CONST(mysql_backup_lock);
  mysql_service_registry->release(reinterpret_cast<my_h_service>(
      const_cast<backup_lock_t *>(mysql_service_mysql_backup_lock)));
  mysql_service_mysql_backup_lock = nullptr;

  using clone_protocol_t = SERVICE_TYPE_NO_CONST(clone_protocol);
  mysql_service_registry->release(reinterpret_cast<my_h_service>(
      const_cast<clone_protocol_t *>(mysql_service_clone_protocol)));
  mysql_service_clone_protocol = nullptr;

  deinit_logging_service_for_plugin(&mysql_service_registry, &log_bi, &log_bs);

  return (0);
}

/** Clone database from local server.
@param[in,out]	thd		server thread handle
@param[in]	data_dir	cloned data directory
@return error code */
static int plugin_clone_local(THD *thd, const char *data_dir) {
  myclone::Client_Share client_share(nullptr, 0, nullptr, nullptr, data_dir, 0);

  myclone::Server server(thd, MYSQL_INVALID_SOCKET);

  /* Update session and statement PFS keys */
  assert(thd != nullptr);
  mysql_service_clone_protocol->mysql_clone_start_statement(
      thd, PSI_NOT_INSTRUMENTED, clone_stmt_local_key);

  myclone::Local clone_inst(thd, &server, &client_share, 0, true);

  auto error = clone_inst.clone();

  return (error);
}

/** Clone database from remote server.
@param[in,out]	thd		server thread handle
@param[in]	remote_host	remote host IP address
@param[in]	remote_port	remote server port
@param[in]	remote_user	remote user name
@param[in]	remote_passwd	remote user's password
@param[in]	data_dir	cloned data directory
@param[in]	ssl_mode	ssl mode for remote connection
@return error code */
static int plugin_clone_remote_client(THD *thd, const char *remote_host,
                                      uint remote_port, const char *remote_user,
                                      const char *remote_passwd,
                                      const char *data_dir, int ssl_mode) {
  /* Validate that donor address matches with preconfigured value. */
  auto error = match_valid_donor_address(thd, remote_host, remote_port);
  if (error != 0) {
    return (error);
  }

  myclone::Client_Share client_share(remote_host, remote_port, remote_user,
                                     remote_passwd, data_dir, ssl_mode);

  /* Update session and statement PFS keys */
  assert(thd != nullptr);

  mysql_service_clone_protocol->mysql_clone_start_statement(
      thd, PSI_NOT_INSTRUMENTED, clone_stmt_client_key);

  myclone::Client clone_inst(thd, &client_share, 0, true);

  error = clone_inst.clone();

  return (error);
}

/** Clone database and send to remote clone client.
@param[in,out]	thd	server thread handle
@param[in]	socket	network socket to remote client
@return error code */
static int plugin_clone_remote_server(THD *thd, MYSQL_SOCKET socket) {
  myclone::Server clone_inst(thd, socket);

  auto err = clone_inst.clone();

  return (err);
}

/** clone plugin interfaces */
struct Mysql_clone clone_descriptor = {
    MYSQL_CLONE_INTERFACE_VERSION, plugin_clone_local,
    plugin_clone_remote_client, plugin_clone_remote_server};

/** Size of intermediate buffer for transferring data from source
file to network or destination file. Set to high value for faster
data transfer to/from file system. Especially for direct i/o where
disk driver can do parallel IO for transfer. */
static MYSQL_SYSVAR_UINT(buffer_size, clone_buffer_size, PLUGIN_VAR_RQCMDARG,
                         "buffer size used by clone for data transfer", nullptr,
                         nullptr, CLONE_MIN_BLOCK * 4, /* Default =   4M */
                         CLONE_MIN_BLOCK,              /* Minimum =   1M */
                         CLONE_MIN_BLOCK * 256,        /* Maximum = 256M */
                         CLONE_MIN_BLOCK);             /* Block   =   1M */

/** If clone should block concurrent DDL */
static MYSQL_SYSVAR_BOOL(block_ddl, clone_block_ddl, PLUGIN_VAR_NOCMDARG,
                         "If clone should block concurrent DDL", nullptr,
                         nullptr, false); /* Allow concurrent ddl by default */

/** Time in seconds to wait for DDL lock. Relevant for donor only when
clone_block_ddl is set to true. */
static MYSQL_SYSVAR_UINT(ddl_timeout, clone_ddl_timeout, PLUGIN_VAR_RQCMDARG,
                         "Time in seconds to wait for DDL lock", nullptr,
                         nullptr, 60 * 5,   /* Default =  5 min */
                         0,                 /* Minimum =  0 no wait */
                         60 * 60 * 24 * 30, /* Maximum =  1 month */
                         1);                /* Step    =  1 sec */

/** If concurrency is automatically tuned */
static MYSQL_SYSVAR_BOOL(autotune_concurrency, clone_autotune_concurrency,
                         PLUGIN_VAR_NOCMDARG,
                         "If concurrency is automatically tuned", nullptr,
                         nullptr, true); /* Enable auto tuning by default */

/** Maximum number of concurrent threads for clone */
static MYSQL_SYSVAR_UINT(max_concurrency, clone_max_concurrency,
                         PLUGIN_VAR_RQCMDARG,
                         "Maximum number of concurrent threads for clone",
                         nullptr, nullptr,
                         CLONE_DEF_CON, /* Default =   8 threads */
                         1,             /* Minimum =   1 thread */
                         128,           /* Maximum = 128 threads */
                         1);            /* Step    =   1 thread */

/** Maximum network bandwidth for clone */
static MYSQL_SYSVAR_UINT(max_network_bandwidth, clone_max_network_bandwidth,
                         PLUGIN_VAR_RQCMDARG,
                         "Maximum network bandwidth for clone in MiB/sec",
                         nullptr, nullptr, 0, /* Default =   0 unlimited */
                         0,                   /* Minimum =   0 unlimited */
                         1024 * 1024,         /* Maximum =   1 TiB/sec */
                         1);                  /* Step    =   1 MiB/sec */

/** Maximum IO bandwidth for clone */
static MYSQL_SYSVAR_UINT(max_data_bandwidth, clone_max_io_bandwidth,
                         PLUGIN_VAR_RQCMDARG,
                         "Maximum File data bandwidth for clone in MiB/sec",
                         nullptr, nullptr, 0, /* Default =   0 unlimited */
                         0,                   /* Minimum =   0 unlimited */
                         1024 * 1024,         /* Maximum =   1 TiB/sec */
                         1);                  /* Step    =   1 MiB/sec */

/** If data is compressed in network layer. */
static MYSQL_SYSVAR_BOOL(enable_compression, clone_enable_compression,
                         PLUGIN_VAR_NOCMDARG,
                         "If compression is done at network", nullptr, nullptr,
                         false); /* Disable compression by default */

/** List of valid donor addresses allowed to clone from. */
static MYSQL_SYSVAR_STR(valid_donor_list, clone_valid_donor_list,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "List of valid donor addresses allowed to clone from"
                        " HOST1:PORT1,HOST2:PORT2,...",
                        check_donor_addr_format, nullptr, nullptr);

/** SSL path name of the SSL private key file */
static MYSQL_SYSVAR_STR(ssl_key, clone_ssl_key,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SSL path name of the SSL private key file", nullptr,
                        nullptr, nullptr);

/** SSL path name of the public key certificate file */
static MYSQL_SYSVAR_STR(ssl_cert, clone_ssl_cert,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SSL path name of the public key certificate file",
                        nullptr, nullptr, nullptr);

/** SSL path name of the Certificate Authority (CA) certificate file */
static MYSQL_SYSVAR_STR(ssl_ca, clone_ssl_ca,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SSL path name for Certificate Authority (CA) file",
                        nullptr, nullptr, nullptr);

/** Donor allows an on going clone operation to resume after short network
failures. This is the time in minutes up to which the donor allows recipient to
re-connect and resume after a network failure. After timeout, donor drops the
current snapshot and the clone operation can no longer be resumed. */
static MYSQL_SYSVAR_UINT(donor_timeout_after_network_failure,
                         clone_restart_timeout, PLUGIN_VAR_RQCMDARG,
                         "Time in minutes up to which donor allows recipient"
                         " to re-connect and restart cloning after network"
                         " failure.",
                         nullptr, nullptr, 5, /* Default =   5 min */
                         0,                   /* Minimum =   0 min: no wait */
                         30,                  /* Maximum =  30 min */
                         1);                  /* Step    =   1 min */

/* Time in seconds to wait after data drop.
In VxFS file system, it was found that the disk space is released
asynchronously after data files are successfully removed. Since it
is FS specific behavior and we could not find any generic way to wait
till the space is completely released, therefore this configuration
is introduced.*/
static MYSQL_SYSVAR_UINT(delay_after_data_drop, clone_delay_after_data_drop,
                         PLUGIN_VAR_RQCMDARG,
                         "Time in seconds to wait after removing data.",
                         nullptr, nullptr, 0, /* Default =  0 no wait */
                         0,                   /* Minimum =  0 no wait */
                         60 * 60,             /* Maximum =  1 hour */
                         1);                  /* Step    =  1 sec */

/** Clone system variables */
static SYS_VAR *clone_system_variables[] = {
    MYSQL_SYSVAR(buffer_size),
    MYSQL_SYSVAR(block_ddl),
    MYSQL_SYSVAR(ddl_timeout),
    MYSQL_SYSVAR(max_concurrency),
    MYSQL_SYSVAR(max_network_bandwidth),
    MYSQL_SYSVAR(max_data_bandwidth),
    MYSQL_SYSVAR(enable_compression),
    MYSQL_SYSVAR(autotune_concurrency),
    MYSQL_SYSVAR(valid_donor_list),
    MYSQL_SYSVAR(ssl_key),
    MYSQL_SYSVAR(ssl_cert),
    MYSQL_SYSVAR(ssl_ca),
    MYSQL_SYSVAR(donor_timeout_after_network_failure),
    MYSQL_SYSVAR(delay_after_data_drop),
    nullptr};

/** Declare clone plugin */
mysql_declare_plugin(clone_plugin){
    MYSQL_CLONE_PLUGIN,

    &clone_descriptor,
    clone_plugin_name, /* Plugin name */

    PLUGIN_AUTHOR_ORACLE,
    "CLONE PLUGIN", /* Plugin descriptive text */
    PLUGIN_LICENSE_GPL,

    plugin_clone_init,   /* Plugin Init */
    plugin_clone_check,  /* Plugin check uninstall */
    plugin_clone_deinit, /* Plugin Deinit */

    CLONE_PLUGIN_VERSION,   /* Plugin Version */
    nullptr,                /* status variables */
    clone_system_variables, /* system variables */
    nullptr,                /* config options */
    0,                      /* flags */
} /** Declare clone plugin */
mysql_declare_plugin_end;
