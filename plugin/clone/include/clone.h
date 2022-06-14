/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
@file clone/include/clone.h
Clone Plugin: Common objects and interfaces

*/

#ifndef CLONE_H
#define CLONE_H

#include "mysqld_error.h"
#include "plugin/clone/include/clone_hton.h"

#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql/psi/mysql_thread.h"

#include "mysql/components/services/backup_lock_service.h"
#include "mysql/components/services/clone_protocol_service.h"
#include "mysql/components/services/log_builtins.h"
#include "violite.h"

extern SERVICE_TYPE(registry) * mysql_service_registry;
extern SERVICE_TYPE(mysql_backup_lock) * mysql_service_mysql_backup_lock;
extern SERVICE_TYPE(clone_protocol) * mysql_service_clone_protocol;
extern SERVICE_TYPE(log_builtins) * log_bi;
extern SERVICE_TYPE(log_builtins_string) * log_bs;

/** Clone memory key for performance schema */
extern PSI_memory_key clone_mem_key;

/** Key for registering clone local worker threads */
extern PSI_thread_key clone_local_thd_key;

/** Key for registering clone client worker threads */
extern PSI_thread_key clone_client_thd_key;

/** Clone Local statement */
extern PSI_statement_key clone_stmt_local_key;

/** Clone Remote client statement */
extern PSI_statement_key clone_stmt_client_key;

/** Clone Remote server statement */
extern PSI_statement_key clone_stmt_server_key;

/** Size of intermediate buffer for transferring data from source
file to network or destination file. */
extern uint clone_buffer_size;

/** Clone system variable: If clone should block concurrent DDL */
extern bool clone_block_ddl;

/** Clone system variable: timeout for DDL lock */
extern uint clone_ddl_timeout;

/** Clone system variable: If concurrency is automatically tuned */
extern bool clone_autotune_concurrency;

/** Clone system variable: Maximum concurrent threads */
extern uint clone_max_concurrency;

/** Clone system variable: Maximum network bandwidth in MiB/sec */
extern uint clone_max_network_bandwidth;

/** Clone system variable: Maximum IO bandwidth in MiB/sec */
extern uint clone_max_io_bandwidth;

/** Clone system variable: If network compression is enabled */
extern bool clone_enable_compression;

/** Clone system variable: SSL private key */
extern char *clone_client_ssl_private_key;

/** Clone system variable: SSL Certificate */
extern char *clone_client_ssl_certificate;

/** Clone system variable: SSL Certificate authority */
extern char *clone_client_ssl_certficate_authority;

/** Clone system variable: time delay after removing data */
extern uint clone_delay_after_data_drop;

/** Number of storage engines supporting clone. */
const uint MAX_CLONE_STORAGE_ENGINE = 16;

/** Maximum number of restart attempts */
const uint CLONE_MAX_RESTART = 100;

/** Minimum block size of clone data. */
const uint CLONE_MIN_BLOCK = 1024 * 1024;

/** Minimum network packet. Safe margin for meta information */
const uint CLONE_MIN_NET_BLOCK = 2 * CLONE_MIN_BLOCK;

/* Namespace for all clone data types */
namespace myclone {

/**  Clone protocol oldest version */
const uint32_t CLONE_PROTOCOL_VERSION_V1 = 0x0100;

/** Send also SO names along with plugin name */
const uint32_t CLONE_PROTOCOL_VERSION_V2 = 0x0101;

/** Send more configurations required by recipient. */
const uint32_t CLONE_PROTOCOL_VERSION_V3 = 0x0102;

/**  Clone protocol latest version */
const uint32_t CLONE_PROTOCOL_VERSION = CLONE_PROTOCOL_VERSION_V3;

/** Flag to indicate no backup lock for DDL. This is multiplexed with
clone_ddl_timeout and sent to donor server. */
const uint32_t NO_BACKUP_LOCK_FLAG = 1ULL << 31;

/** Clone protocol commands. Please bump the protocol version before adding
new command. */
typedef enum Type_Cmmand_RPC : uchar {
  /** Initialize clone and negotiate version */
  COM_INIT = 1,

  /** Attach to current on going clone operation */
  COM_ATTACH,

  /** Re-Initialize clone network error */
  COM_REINIT,

  /** Execute command to clone remote database */
  COM_EXECUTE,

  /** Send Error or ACK data to remote database */
  COM_ACK,

  /** Exit clone protocol */
  COM_EXIT,

  /** Limit value for clone RPC */
  COM_MAX
} Command_RPC;

/** Clone protocol response. Please bump the protocol version before adding
new response. */
typedef enum Type_Command_Response : uchar {
  /** Remote Locators */
  COM_RES_LOCS = 1,

  /** Remote Data descriptor */
  COM_RES_DATA_DESC,

  /** Remote Data */
  COM_RES_DATA,

  /** Plugin */
  COM_RES_PLUGIN,

  /** Configuration */
  COM_RES_CONFIG,

  /** Character set collation */
  COM_RES_COLLATION,

  /** Plugin with shared object name : introduced in version 0x0101 */
  COM_RES_PLUGIN_V2,

  /** Additional configuration : introduced in version 0x0102 */
  COM_RES_CONFIG_V3,

  /** End of response data */
  COM_RES_COMPLETE = 99,

  /** Error in remote server operation */
  COM_RES_ERROR = 100,

  /** Limit value for clone RPC response */
  COM_RES_MAX
} Command_Response;

using String_Key = std::string;
using String_Keys = std::vector<String_Key>;

using Key_Value = std::pair<String_Key, String_Key>;
using Key_Values = std::vector<Key_Value>;

/** We transfers data between storage handle and external data link.
Storage handle is always identified by a set of locators provided by
the Storage Engines. External data link could be of type buffer or file
in case of local clone and network socket in case of remote clone. */
enum Data_Link_Type {
  CLONE_HANDLE_SOCKET = 1,
  CLONE_HANDLE_BUFFER,
  CLONE_HANDLE_FILE
};

/** Data stored in buffer */
struct Buffer {
  /** Initialize buffer */
  void init() {
    m_buffer = nullptr;
    m_length = 0;
  }

  /** Allocate or Re-Allocate buffer
  @param[in]	length	length to allocate or extend to
  @return error if allocation fails. */
  int allocate(size_t length) {
    if (m_length >= length) {
      assert(m_buffer != nullptr);
      return (0);
    }

    uchar *temp_ptr = nullptr;

    if (m_buffer == nullptr) {
      temp_ptr =
          static_cast<uchar *>(my_malloc(clone_mem_key, length, MYF(MY_WME)));

    } else {
      temp_ptr = static_cast<uchar *>(
          my_realloc(clone_mem_key, m_buffer, length, MYF(MY_WME)));
    }

    if (temp_ptr == nullptr) {
      my_error(ER_OUTOFMEMORY, MYF(0), length);
      return (ER_OUTOFMEMORY);
    }

    m_buffer = temp_ptr;
    m_length = length;

    return (0);
  }

  /** Free buffer */
  void free() {
    my_free(m_buffer);
    init();
  }

  /** Buffer pointer */
  uchar *m_buffer;

  /** Buffer length */
  size_t m_length;
};

/** Data stored in file */
struct File {
  /** Open file descriptor */
  Ha_clone_file m_file_desc;

  /** Data length */
  uint m_length;
};

/** External data link for transfer */
struct Data_Link {
  /** Get external handle type.
  @return handle type */
  Data_Link_Type get_type() { return (m_type); }

  /** Get external handle of type buffer. Caller must ensure
  correct handle type.
  @return clone buffer */
  Buffer *get_buffer() {
    assert(m_type == CLONE_HANDLE_BUFFER);
    return (&m_buffer);
  }

  /** Set external handle buffer.
  @param[in]	in_buf	buffer pointer
  @param[in]	in_len	buffer length */
  void set_buffer(uchar *in_buf, uint in_len) {
    m_type = CLONE_HANDLE_BUFFER;

    m_buffer.m_buffer = in_buf;
    m_buffer.m_length = in_len;
  }

  /** Get external handle of type file. Caller must ensure
  correct handle type.
  @return clone file */
  File *get_file() {
    assert(m_type == CLONE_HANDLE_FILE);
    return (&m_file);
  }

  /** Set external handle file.
  @param[in]	in_file	file descriptor
  @param[in]	in_len	data length */
  void set_file(Ha_clone_file in_file, uint in_len) {
    m_type = CLONE_HANDLE_FILE;

    m_file.m_file_desc = in_file;
    m_file.m_length = in_len;
  }

  /** Set external handle socket.
  @param[in]	socket	network socket */
  void set_socket(MYSQL_SOCKET socket) {
    m_type = CLONE_HANDLE_SOCKET;
    m_socket = socket;
  }

 private:
  /** external handle type */
  Data_Link_Type m_type;

  /** external handle data */
  union {
    MYSQL_SOCKET m_socket;
    Buffer m_buffer;
    File m_file;
  };
};

/** Validate all local configuration parameters.
@param[in]	thd	current session THD
@return error code */
int validate_local_params(THD *thd);
}  // namespace myclone

#endif /* CLONE_H */
