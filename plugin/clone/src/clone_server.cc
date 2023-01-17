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
@file clone/src/clone_server.cc
Clone Plugin: Server implementation

*/

#include "plugin/clone/include/clone_server.h"
#include "plugin/clone/include/clone_status.h"

#include "my_byteorder.h"

/* Namespace for all clone data types */
namespace myclone {

Server::Server(THD *thd, MYSQL_SOCKET socket)
    : m_server_thd(thd),
      m_is_master(false),
      m_storage_initialized(false),
      m_pfs_initialized(false),
      m_acquired_backup_lock(false),
      m_protocol_version(CLONE_PROTOCOL_VERSION),
      m_client_ddl_timeout(),
      m_backup_lock(true) {
  m_ext_link.set_socket(socket);
  m_storage_vec.reserve(MAX_CLONE_STORAGE_ENGINE);

  m_tasks.reserve(MAX_CLONE_STORAGE_ENGINE);

  m_copy_buff.init();
  m_res_buff.init();
}

Server::~Server() {
  assert(!m_storage_initialized);
  m_copy_buff.free();
  m_res_buff.free();
}

int Server::clone() {
  int err = 0;

  while (true) {
    uchar command;
    uchar *com_buf;
    size_t com_len;

    err = mysql_service_clone_protocol->mysql_clone_get_command(
        get_thd(), &command, &com_buf, &com_len);

    bool done = true;

    if (err == 0) {
      err = parse_command_buffer(command, com_buf, com_len, done);
    }

    if (err == 0 && thd_killed(get_thd())) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      err = ER_QUERY_INTERRUPTED;
    }

    /* Send status to client */
    err = send_status(err);

    if (done || err != 0) {
      if (m_storage_initialized) {
        assert(err != 0);
        /* Don't abort clone if worker thread fails during attach. */
        int in_err = (command == COM_ATTACH) ? 0 : err;

        hton_clone_end(get_thd(), get_storage_vector(), m_tasks, in_err);
        m_storage_initialized = false;
      }
      /* Release if we have acquired backup lock */
      if (m_acquired_backup_lock) {
        assert(m_is_master);
        mysql_service_mysql_backup_lock->release(get_thd());
      }
      break;
    }
  }

  log_error(get_thd(), false, err, "Exiting clone protocol");
  return (err);
}

int Server::send_status(int err) {
  uchar res_cmd;
  char info_mesg[128];

  if (err == 0) {
    /* Send complete response */
    res_cmd = static_cast<uchar>(COM_RES_COMPLETE);

    err = mysql_service_clone_protocol->mysql_clone_send_response(
        get_thd(), false, &res_cmd, sizeof(res_cmd));
    log_error(get_thd(), false, err, "COM_RES_COMPLETE");

  } else {
    /* Send Error Response */
    res_cmd = static_cast<uchar>(COM_RES_ERROR);

    snprintf(info_mesg, 128, "Before sending COM_RES_ERROR: %s",
             is_network_error(err) ? "network " : " ");
    log_error(get_thd(), false, err, &info_mesg[0]);

    err = mysql_service_clone_protocol->mysql_clone_send_error(
        get_thd(), res_cmd, is_network_error(err));
    log_error(get_thd(), false, err, "After sending COM_RES_ERROR");
  }

  return (err);
}

int Server::init_storage(Ha_clone_mode mode, uchar *com_buf, size_t com_len) {
  auto thd = get_thd();

  assert(thd != nullptr);
  assert(!m_pfs_initialized);

  auto err = deserialize_init_buffer(com_buf, com_len);

  if (err != 0) {
    return (err);
  }

  if (m_is_master) {
    /* Set statement type for master thread */
    mysql_service_clone_protocol->mysql_clone_start_statement(
        thd, PSI_NOT_INSTRUMENTED, clone_stmt_server_key);

    /* Acquire backup lock */
    if (block_ddl()) {
      auto failed = mysql_service_mysql_backup_lock->acquire(
          thd, BACKUP_LOCK_SERVICE_DEFAULT, m_client_ddl_timeout);

      if (failed) {
        return (ER_LOCK_WAIT_TIMEOUT);
      }
      m_acquired_backup_lock = true;
      log_error(get_thd(), false, 0, "Acquired backup lock");
    }
  }
  m_pfs_initialized = true;

  /* Work around to use client DDL timeout while waiting for backup
  lock in clone_init_tablespaces if required. */
  auto saved_donor_timeout = clone_ddl_timeout;
  clone_ddl_timeout = m_client_ddl_timeout;

  /* Get server locators */
  err = hton_clone_begin(get_thd(), get_storage_vector(), m_tasks,
                         HA_CLONE_HYBRID, mode);
  if (err != 0) {
    clone_ddl_timeout = saved_donor_timeout;
    return (err);
  }
  m_storage_initialized = true;
  clone_ddl_timeout = saved_donor_timeout;

  if (m_is_master && mode == HA_CLONE_MODE_START) {
    /* Validate local configurations. */
    err = validate_local_params(get_thd());

    if (err == 0) {
      /* Send current server parameters for validation. */
      err = send_params();
    }

    if (err != 0) {
      return (err);
    }
  }

  /* Send locators back to client */
  err = send_locators();

  return (err);
}

int Server::parse_command_buffer(uchar command, uchar *com_buf, size_t com_len,
                                 bool &done) {
  int err = 0;
  auto com = static_cast<Command_RPC>(command);
  done = false;

  switch (com) {
    case COM_REINIT:
      m_is_master = true;
      err = init_storage(HA_CLONE_MODE_RESTART, com_buf, com_len);
      log_error(get_thd(), false, err, "COM_REINIT: Storage Initialize");
      break;

    case COM_INIT:
      m_is_master = true;

      /* Initialize storage, send locators and validating configurations.  */
      err = init_storage(HA_CLONE_MODE_START, com_buf, com_len);

      log_error(get_thd(), false, err, "COM_INIT: Storage Initialize");
      break;

    case COM_ATTACH:
      m_is_master = false;
      err = init_storage(HA_CLONE_MODE_ADD_TASK, com_buf, com_len);
      log_error(get_thd(), false, err, "COM_ATTACH: Storage Attach");
      break;

    case COM_EXECUTE: {
      if (!m_storage_initialized) {
        /* purecov: begin deadcode */
        err = ER_CLONE_PROTOCOL;
        my_error(err, MYF(0), "Wrong Clone RPC: Execute request before Init");
        log_error(get_thd(), false, err, "COM_EXECUTE : Storage ninitialized");
        break;
        /* purecov: end */
      }

      Server_Cbk clone_callback(this);

      err = hton_clone_copy(get_thd(), get_storage_vector(), m_tasks,
                            &clone_callback);
      log_error(get_thd(), false, err, "COM_EXECUTE: Storage Execute");
      break;
    }
    case COM_ACK: {
      m_pfs_initialized = true;
      int err_code = 0;
      Locator loc = {nullptr, nullptr, 0};

      Server_Cbk clone_callback(this);

      err = deserialize_ack_buffer(com_buf, com_len, &clone_callback, err_code,
                                   &loc);

      if (err == 0) {
        auto hton = loc.m_hton;

        err = hton->clone_interface.clone_ack(hton, get_thd(), loc.m_loc,
                                              loc.m_loc_len, 0, err_code,
                                              &clone_callback);
      }
      log_error(get_thd(), false, err, "COM_ACK: Storage Ack");
      break;
    }

    case COM_EXIT:
      if (m_storage_initialized) {
        hton_clone_end(get_thd(), get_storage_vector(), m_tasks, 0);
        m_storage_initialized = false;
      }
      done = true;
      log_error(get_thd(), false, err, "COM_EXIT: Storage End");
      break;

    case COM_MAX:
      [[fallthrough]];
    default:
      /* purecov: begin deadcode */
      err = ER_CLONE_PROTOCOL;
      my_error(err, MYF(0), "Wrong Clone RPC: Invalid request");
      break;
      /* purecov: end */
  }
  return (err);
}

int Server::deserialize_ack_buffer(const uchar *ack_buf, size_t ack_len,
                                   Ha_clone_cbk *cbk, int &err_code,
                                   Locator *loc) {
  size_t serialized_length = 0;

  const uchar *desc_ptr = nullptr;
  uint desc_len = 0;

  /*  Should not deserialize if less than the base length */
  if (ack_len < (4 + loc->serlialized_length())) {
    goto err_end;
  }

  /* Extract error code */
  err_code = uint4korr(ack_buf);
  ack_buf += 4;
  ack_len -= 4;

  /* Extract Locator */
  serialized_length = loc->deserialize(get_thd(), ack_buf);

  if (ack_len < serialized_length) {
    goto err_end;
  }
  ack_buf += serialized_length;
  ack_len -= serialized_length;

  /* Extract descriptor */
  if (ack_len < 4) {
    goto err_end;
  }

  desc_len = uint4korr(ack_buf);
  ack_buf += 4;
  ack_len -= 4;

  if (desc_len > 0) {
    desc_ptr = ack_buf;
  }

  cbk->set_data_desc(desc_ptr, desc_len);

  ack_len -= desc_len;

  if (ack_len == 0) {
    return (0);
  }

err_end:
  /* purecov: begin deadcode */
  my_error(ER_CLONE_PROTOCOL, MYF(0), "Wrong Clone RPC: Init ACK length");
  return (ER_CLONE_PROTOCOL);
  /* purecov: end */
}

int Server::deserialize_init_buffer(const uchar *init_buf, size_t init_len) {
  if (init_len < 8) {
    goto err_end;
  }

  /* Extract protocol version */
  m_protocol_version = uint4korr(init_buf);
  if (m_protocol_version > CLONE_PROTOCOL_VERSION) {
    m_protocol_version = CLONE_PROTOCOL_VERSION;
  }
  init_buf += 4;
  init_len -= 4;

  /* Extract DDL timeout */
  {
    uint32_t client_ddl_timeout = uint4korr(init_buf);
    init_buf += 4;
    init_len -= 4;

    set_client_timeout(client_ddl_timeout);
  }

  /* Initialize locators */
  while (init_len > 0) {
    Locator loc = {nullptr, nullptr, 0};

    /*  Should not deserialize if less than the base length */
    if (init_len < loc.serlialized_length()) {
      goto err_end;
    }

    auto serialized_length = loc.deserialize(get_thd(), init_buf);

    init_buf += serialized_length;

    if (init_len < serialized_length) {
      goto err_end;
    }

    m_storage_vec.push_back(loc);

    init_len -= serialized_length;
  }

  if (init_len == 0) {
    return (0);
  }

err_end:
  my_error(ER_CLONE_PROTOCOL, MYF(0), "Wrong Clone RPC: Init buffer length");

  return (ER_CLONE_PROTOCOL);
}

int Server::send_key_value(Command_Response rcmd, String_Key &key_str,
                           String_Key &val_str) {
  /* Add length for key. */
  auto buf_len = key_str.length();
  buf_len += 4;

  bool send_value = (rcmd == COM_RES_CONFIG || rcmd == COM_RES_PLUGIN_V2 ||
                     rcmd == COM_RES_CONFIG_V3);

  /** Add length for value. */
  if (send_value) {
    buf_len += val_str.length();
    buf_len += 4;
  }
  /* Add length for response type. */
  ++buf_len;

  /* Allocate for response buffer */
  auto err = m_res_buff.allocate(buf_len);
  auto buf_ptr = m_res_buff.m_buffer;
  if (err != 0) {
    return (true);
  }

  /* Store response command */
  *buf_ptr = static_cast<uchar>(rcmd);
  ++buf_ptr;

  /* Store key */
  int4store(buf_ptr, key_str.length());
  buf_ptr += 4;
  memcpy(buf_ptr, key_str.c_str(), key_str.length());
  buf_ptr += key_str.length();

  /* Store Value */
  if (send_value) {
    int4store(buf_ptr, val_str.length());
    buf_ptr += 4;
    memcpy(buf_ptr, val_str.c_str(), val_str.length());
  }
  err = mysql_service_clone_protocol->mysql_clone_send_response(
      get_thd(), false, m_res_buff.m_buffer, buf_len);

  return (err);
}

int Server::send_params() {
  int err = 0;

  /* Send plugins */
  auto plugin_cbk = [](THD *, plugin_ref plugin, void *ctx) {
    auto server = static_cast<Server *>(ctx);

    if (plugin == nullptr) {
      return false;
    }
    /* Send plugin name string */
    String_Key pstring(plugin_name(plugin)->str, plugin_name(plugin)->length);

    if (server->send_only_plugin_name()) {
      auto err = server->send_key_value(COM_RES_PLUGIN, pstring, pstring);
      return (err != 0);
    }

    /* Send plugin dynamic library name. */
    String_Key dstring;

    auto plugin_dl = plugin_dlib(plugin);
    if (plugin_dl != nullptr) {
      dstring.assign(plugin_dl->dl.str, plugin_dl->dl.length);
    }

    auto err = server->send_key_value(COM_RES_PLUGIN_V2, pstring, dstring);
    return (err != 0);
  };

  /* Check only for plugins in active state - PLUGIN_IS_READY. We already have
  backup lock here and no new plugins can be installed or uninstalled at this
  point. However, there could be some left over plugins in PLUGIN_IS_DELETED
  state which are uninstalled but not removed yet. */
  auto result = plugin_foreach(get_thd(), plugin_cbk, MYSQL_ANY_PLUGIN, this);

  if (result) {
    err = ER_INTERNAL_ERROR;
    my_error(err, MYF(0), "Clone error sending plugin information");
    return err;
  }

  /* Send character sets and collations */
  String_Keys char_sets;

  err = mysql_service_clone_protocol->mysql_clone_get_charsets(get_thd(),
                                                               char_sets);
  if (err != 0) {
    return err;
  }

  for (auto &element : char_sets) {
    err = send_key_value(COM_RES_COLLATION, element, element);
    if (err != 0) {
      return err;
    }
  }

  /* Send configurations for validation. */
  err = send_configs(COM_RES_CONFIG);

  if (err != 0 || skip_other_configs()) {
    return err;
  }

  /* Send other configurations required by recipient. */
  err = send_configs(COM_RES_CONFIG_V3);

  return err;
}

int Server::send_configs(Command_Response rcmd) {
  /** All configuration parameters to be validated. */
  Key_Values all_configs = {{"version", ""},
                            {"version_compile_machine", ""},
                            {"version_compile_os", ""},
                            {"character_set_server", ""},
                            {"character_set_filesystem", ""},
                            {"collation_server", ""},
                            {"innodb_page_size", ""}};

  /** All other configuration required by recipient. */
  Key_Values other_configs = {
      {"clone_donor_timeout_after_network_failure", ""}};

  auto &configs = (rcmd == COM_RES_CONFIG_V3) ? other_configs : all_configs;

  auto err =
      mysql_service_clone_protocol->mysql_clone_get_configs(get_thd(), configs);

  if (err != 0) {
    return err;
  }

  for (auto &key_val : configs) {
    err = send_key_value(rcmd, key_val.first, key_val.second);
    if (err != 0) {
      break;
    }
  }
  return err;
}

int Server::send_locators() {
  /* Add length of protocol Version */
  auto buf_len = sizeof(m_protocol_version);

  /* Add length for response type */
  ++buf_len;

  /* Add SE and locator length */
  for (auto &loc : m_storage_vec) {
    buf_len += loc.serlialized_length();
  }

  /* Allocate for response buffer */
  auto err = m_res_buff.allocate(buf_len);
  auto buf_ptr = m_res_buff.m_buffer;

  if (err != 0) {
    return (err);
  }

  /* Store response command */
  *buf_ptr = static_cast<uchar>(COM_RES_LOCS);
  ++buf_ptr;

  /* Store version */
  int4store(buf_ptr, m_protocol_version);
  buf_ptr += 4;

  /* Store SE information and Locators */
  for (auto &loc : m_storage_vec) {
    buf_ptr += loc.serialize(buf_ptr);
  }

  err = mysql_service_clone_protocol->mysql_clone_send_response(
      get_thd(), false, m_res_buff.m_buffer, buf_len);

  return (err);
}

int Server::send_descriptor(handlerton *hton, bool secure, uint loc_index,
                            const uchar *desc_buf, uint desc_len) {
  /* Add data descriptor length */
  auto buf_len = desc_len;

  /* Add length for response type */
  ++buf_len;

  /* Add length for Storage Engine type */
  ++buf_len;

  /* Add length for Locator Index */
  ++buf_len;

  /* Allocate for response buffer */
  auto err = m_res_buff.allocate(buf_len);

  if (err != 0) {
    return (err);
  }

  auto buf_ptr = m_res_buff.m_buffer;

  /* Store response command */
  *buf_ptr = static_cast<uchar>(COM_RES_DATA_DESC);
  ++buf_ptr;

  /* Store Storage Engine type */
  *buf_ptr = static_cast<uchar>(hton->db_type);
  ++buf_ptr;

  /* Store Locator Index */
  *buf_ptr = static_cast<uchar>(loc_index);
  ++buf_ptr;

  /* Store Descriptor */
  memcpy(buf_ptr, desc_buf, desc_len);

  err = mysql_service_clone_protocol->mysql_clone_send_response(
      get_thd(), secure, m_res_buff.m_buffer, buf_len);

  return (err);
}

int Server_Cbk::send_descriptor() {
  auto server = get_clone_server();

  uint desc_len = 0;
  auto desc = get_data_desc(&desc_len);

  auto err = server->send_descriptor(get_hton(), is_secure(), get_loc_index(),
                                     desc, desc_len);
  return (err);
}

int Server_Cbk::file_cbk(Ha_clone_file from_file, uint len) {
  auto server = get_clone_server();

  /* Check if session is interrupted. */
  if (thd_killed(server->get_thd())) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    return (ER_QUERY_INTERRUPTED);
  }

  /* Add one byte for descriptor type */
  auto buf_len = len + 1;
  auto buf_ptr = server->alloc_copy_buffer(buf_len + CLONE_OS_ALIGN);

  if (buf_ptr == nullptr) {
    return (ER_OUTOFMEMORY);
  }

  /* Store response command */
  auto data_ptr = buf_ptr + 1;

  /* Align buffer to CLONE_OS_ALIGN[4K] for O_DIRECT */
  data_ptr = clone_os_align(data_ptr);
  buf_ptr = data_ptr - 1;

  *buf_ptr = static_cast<uchar>(COM_RES_DATA);

  auto err =
      clone_os_copy_file_to_buf(from_file, data_ptr, len, get_source_name());
  if (err != 0) {
    return (err);
  }

  /* Step 1: Send Descriptor */
  err = send_descriptor();

  if (err != 0) {
    return (err);
  }

  /* Step 2: Send Data */
  err = mysql_service_clone_protocol->mysql_clone_send_response(
      server->get_thd(), false, buf_ptr, buf_len);

  return (err);
}

int Server_Cbk::buffer_cbk(uchar *from_buffer, uint buf_len) {
  auto server = get_clone_server();

  if (thd_killed(server->get_thd())) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    return (ER_QUERY_INTERRUPTED);
  }

  uchar *buf_ptr = nullptr;
  uint total_len = 0;

  if (buf_len > 0) {
    /* Add one byte for descriptor type */
    total_len = buf_len + 1;
    buf_ptr = server->alloc_copy_buffer(total_len);

    if (buf_ptr == nullptr) {
      return (ER_OUTOFMEMORY);
    }
  }

  /* Step 1: Send Descriptor */
  auto err = send_descriptor();

  if (err != 0 || buf_len == 0) {
    return (err);
  }

  /* Step 2: Send Data */
  *buf_ptr = static_cast<uchar>(COM_RES_DATA);
  memcpy(buf_ptr + 1, from_buffer, static_cast<size_t>(buf_len));

  err = mysql_service_clone_protocol->mysql_clone_send_response(
      server->get_thd(), false, buf_ptr, total_len);

  return (err);
}

/* purecov: begin deadcode */
int Server_Cbk::apply_file_cbk(Ha_clone_file to_file [[maybe_unused]]) {
  assert(false);
  my_error(ER_INTERNAL_ERROR, MYF(0), "Apply callback from Clone Server");
  return (ER_INTERNAL_ERROR);
}

int Server_Cbk::apply_buffer_cbk(uchar *&to_buffer [[maybe_unused]],
                                 uint &len [[maybe_unused]]) {
  assert(false);
  my_error(ER_INTERNAL_ERROR, MYF(0), "Apply callback from Clone Server");
  return (ER_INTERNAL_ERROR);
}
/* purecov: end */
}  // namespace myclone
