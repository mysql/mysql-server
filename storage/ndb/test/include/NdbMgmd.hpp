/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_MGMD_HPP
#define NDB_MGMD_HPP

#include <mgmapi.h>
#include "../../src/mgmapi/mgmapi_internal.h"
#include "mgmcommon/NdbMgm.hpp"
#include "portlib/ndb_compiler.h"
#include "util/require.h"

#include <BaseString.hpp>
#include <Properties.hpp>
#include "NDBT_Output.hpp"

#include <OutputStream.hpp>
#include <SocketInputStream2.hpp>

#include "mgmcommon/Config.hpp"

#include <InputStream.hpp>

#include "NdbSleep.h"
#include "util/TlsKeyManager.hpp"

class NdbMgmd {
  BaseString m_connect_str;
  NdbMgmHandle m_handle;
  Uint32 m_nodeid;
  bool m_verbose;
  unsigned int m_timeout;
  unsigned int m_version;
  const char *m_tls_path;
  unsigned long long m_tls_level;
  NdbSocket m_event_socket;

  void error(const char *msg, ...) ATTRIBUTE_FORMAT(printf, 2, 3) {
    if (!m_verbose) return;

    va_list args;
    printf("NdbMgmd::");
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");

    if (m_handle) {
      ndbout_c(" error: %d, line: %d, desc: %s",
               ndb_mgm_get_latest_error(m_handle),
               ndb_mgm_get_latest_error_line(m_handle),
               ndb_mgm_get_latest_error_desc(m_handle));
    }
  }

 public:
  NdbMgmd()
      : m_handle(NULL),
        m_nodeid(0),
        m_verbose(true),
        m_timeout(0),
        m_version(NDB_VERSION),
        m_tls_path(nullptr),
        m_tls_level(0) {
    const char *connect_string = getenv("NDB_CONNECTSTRING");
    if (connect_string) m_connect_str.assign(connect_string);
  }

  ~NdbMgmd() { close(); }

  unsigned int get_version() { return m_version; }
  void close(void) {
    if (m_handle) {
      ndb_mgm_disconnect_quiet(m_handle);
      ndb_mgm_destroy_handle(&m_handle);
      m_handle = NULL;
    }
    if (m_event_socket.is_valid()) m_event_socket.close();
  }

  NdbMgmHandle handle(void) const { return m_handle; }

  NdbSocket convert_to_transporter() {
    return ndb_mgm_convert_to_transporter(&m_handle);
  }

  const NdbSocket &socket(void) const { return _ndb_mgm_get_socket(m_handle); }

  NodeId nodeid(void) const { return m_nodeid; }

  const char *getConnectString() const { return m_connect_str.c_str(); }

  void setConnectString(const char *connect_str) {
    m_connect_str.assign(connect_str);
  }

  void use_tls(const char *path, unsigned long long level) {
    m_tls_path = path;
    m_tls_level = level;
  }

  bool set_timeout(unsigned int timeout) {
    m_timeout = timeout;
    if (m_handle && ndb_mgm_set_timeout(m_handle, timeout) != 0) {
      error("set_timeout: failed to set timeout on handle");
      return false;
    }
    return true;
  }

  void verbose(bool yes = true) { m_verbose = yes; }

  int last_error(void) const { return ndb_mgm_get_latest_error(m_handle); }

  const char *last_error_message(void) const {
    return ndb_mgm_get_latest_error_desc(m_handle);
  }

  bool connect(const char *connect_string = NULL, int num_retries = 12,
               int retry_delay_in_seconds = 5, bool use_tls = true) {
    require(m_handle == NULL);

    m_handle = ndb_mgm_create_handle();
    if (!m_handle) {
      error("connect: ndb_mgm_create_handle failed");
      return false;
    }

    if (ndb_mgm_set_connectstring(
            m_handle, connect_string ? connect_string : getConnectString()) !=
        0) {
      error("connect: ndb_mgm_set_connectstring failed");
      return false;
    }

    if (m_timeout > 0 && ndb_mgm_set_timeout(m_handle, m_timeout) != 0) {
      error("connect: ndb_mgm_set_timeout failed");
      return false;
    }

    int connect_status = -1;
    if (m_tls_path) {
      TlsKeyManager tlsKeyManager;
      tlsKeyManager.init_mgm_client(m_tls_path);
      ndb_mgm_set_ssl_ctx(m_handle, tlsKeyManager.ctx());
      connect_status = ndb_mgm_connect_tls(
          m_handle, num_retries, retry_delay_in_seconds, 0, m_tls_level);
    } else {
      connect_status =
          ndb_mgm_connect(m_handle, num_retries, retry_delay_in_seconds, 0);
    }

    if (connect_status != 0) {
      error("connect: ndb_mgm_connect failed");
      return false;
    }

    // Handshake with the server to make sure it's really there
    int major, minor, build;
    char buf[16];
    if (ndb_mgm_get_version(m_handle, &major, &minor, &build, sizeof(buf),
                            buf) != 1) {
      error("connect: ndb_get_version failed");
      return false;
    }
    m_version = (major << 16) + (minor << 8) + build;
    // printf("connected to ndb_mgmd version %d.%d.%d\n",
    //        major, minor, build);

    if ((m_nodeid = ndb_mgm_get_mgmd_nodeid(m_handle)) == 0) {
      error("connect: could not get nodeid of connected mgmd");
      return false;
    }

    return true;
  }

  int start_tls(struct ssl_ctx_st *ctx) {
    ndb_mgm_set_ssl_ctx(m_handle, ctx);
    return ndb_mgm_start_tls(m_handle);
  }

  bool is_connected(void) {
    if (!m_handle) {
      error("is_connected: no handle");
      return false;
    }
    if (!ndb_mgm_is_connected(m_handle)) {
      error("is_connected: not connected");
      return false;
    }
    return true;
  }

  bool disconnect(void) {
    if (ndb_mgm_disconnect(m_handle) != 0) {
      error("disconnect: ndb_mgm_disconnect failed");
      return false;
    }

    ndb_mgm_destroy_handle(&m_handle);
    m_handle = NULL;

    return true;
  }

  bool restart(bool abort = false) {
    if (!is_connected()) {
      error("restart: not connected");
      return false;
    }

    int disconnect = 0;
    int node_list = m_nodeid;
    int restarted =
        ndb_mgm_restart3(m_handle, 1, &node_list, false, /* initial */
                         false,                          /* nostart */
                         abort, &disconnect);

    if (restarted != 1) {
      error("restart: failed to restart node %d, restarted: %d", m_nodeid,
            restarted);
      return false;
    }
    return true;
  }

  bool call(const char *cmd, const Properties &args, const char *cmd_reply,
            Properties &reply, const char *bulk = NULL,
            bool name_value_pairs = true) {
    if (!is_connected()) {
      error("call: not connected");
      return false;
    }

    SocketOutputStream out(socket());

    if (out.println("%s", cmd)) {
      error("call: println failed at line %d", __LINE__);
      return false;
    }

    Properties::Iterator iter(&args);
    const char *name;
    while ((name = iter.next()) != NULL) {
      PropertiesType t;
      Uint32 val_i;
      Uint64 val_64;
      BaseString val_s;

      args.getTypeOf(name, &t);
      switch (t) {
        case PropertiesType_Uint32:
          args.get(name, &val_i);
          if (out.println("%s: %d", name, val_i)) {
            error("call: println failed at line %d", __LINE__);
            return false;
          }
          break;
        case PropertiesType_Uint64:
          args.get(name, &val_64);
          if (out.println("%s: %lld", name, val_64)) {
            error("call: println failed at line %d", __LINE__);
            return false;
          }
          break;
        case PropertiesType_char:
          args.get(name, val_s);
          if (out.println("%s: %s", name, val_s.c_str())) {
            error("call: println failed at line %d", __LINE__);
            return false;
          }
          break;
        default:
        case PropertiesType_Properties:
          /* Illegal */
          abort();
          break;
      }
    }

    // Empty line terminates argument list
    if (out.print("\n")) {
      error("call: print('\n') failed at line %d", __LINE__);
      return false;
    }

    // Send any bulk data
    if (bulk) {
      if (out.write(bulk, strlen(bulk)) >= 0) {
        if (out.write("\n", 1) < 0) {
          error("call: print('<bulk>') failed at line %d", __LINE__);
          return false;
        }
      }
    }

    BaseString buf;
    SocketInputStream2 in(socket());
    if (cmd_reply) {
      // Read the reply header and compare against "cmd_reply"
      if (!in.gets(buf)) {
        error("call: could not read reply command");
        return false;
      }

      // 1. Check correct reply header
      if (buf != cmd_reply) {
        error("call: unexpected reply command, expected: '%s', got '%s'",
              cmd_reply, buf.c_str());
        return false;
      }
    }

    // 2. Read lines until empty line
    int line = 1;
    while (in.gets(buf)) {
      // empty line -> end of reply
      if (buf == "") return true;

      if (name_value_pairs) {
        // 3a. Read colon separated name value pair, split
        // the name value pair on first ':'
        Vector<BaseString> name_value_pair;
        if (buf.split(name_value_pair, ":", 2) != 2) {
          error("call: illegal name value pair '%s' received", buf.c_str());
          return false;
        }

        reply.put(name_value_pair[0].trim(" ").c_str(),
                  name_value_pair[1].trim(" ").c_str());
      } else {
        // 3b. Not name value pair, save the line into "reply"
        // using unique key
        reply.put("line", line++, buf.c_str());
      }
    }

    error("call: should never come here");
    reply.print();
    abort();
    return false;
  }

  bool get_config(Config &config) {
    if (!is_connected()) {
      error("get_config: not connected");
      return false;
    }

    ndb_mgm::config_ptr conf(ndb_mgm_get_configuration(m_handle, 0));
    if (!conf) {
      error("get_config: ndb_mgm_get_configuration failed");
      return false;
    }

    config.m_configuration = conf.release();
    return true;
  }

  bool set_config(Config &config) {
    if (!is_connected()) {
      error("set_config: not connected");
      return false;
    }

    if (ndb_mgm_set_configuration(m_handle, config.get_configuration()) != 0) {
      error("set_config: ndb_mgm_set_configuration failed");
      return false;
    }
    return true;
  }

  bool end_session(void) {
    if (!is_connected()) {
      error("end_session: not connected");
      return false;
    }

    if (ndb_mgm_end_session(m_handle) != 0) {
      error("end_session: ndb_mgm_end_session failed");
      return false;
    }
    return true;
  }

  bool subscribe_to_events(void) {
    if (!is_connected()) {
      error("subscribe_to_events: not connected");
      return false;
    }

    int filter[] = {15, NDB_MGM_EVENT_CATEGORY_STARTUP,
                    15, NDB_MGM_EVENT_CATEGORY_SHUTDOWN,
                    15, NDB_MGM_EVENT_CATEGORY_STATISTIC,
                    15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT,
                    15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
                    15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
                    15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                    15, NDB_MGM_EVENT_CATEGORY_CONGESTION,
                    15, NDB_MGM_EVENT_CATEGORY_DEBUG,
                    15, NDB_MGM_EVENT_CATEGORY_INFO,
                    0};

    m_event_socket = ndb_mgm_listen_event_internal(m_handle, filter, 0, true);

    return m_event_socket.is_valid();
  }

  bool get_next_event_line(char *buff, int bufflen, int timeout_millis) {
    if (!is_connected()) {
      error("get_next_event_line: not connected");
      return false;
    }

    if (!m_event_socket.is_valid()) {
      error("get_next_event_line: not subscribed");
      return false;
    }

    SocketInputStream stream(m_event_socket, timeout_millis);

    const char *result = stream.gets(buff, bufflen);
    if (result && strlen(result)) {
      return true;
    } else {
      if (stream.timedout()) {
        error("get_next_event_line: stream.gets timed out");
        return false;
      }
    }

    error("get_next_event_line: error from stream.gets()");
    return false;
  }

  bool change_config(Uint64 new_value, Uint64 *saved_old_value,
                     unsigned type_of_section, unsigned config_variable) {
    if (!connect()) {
      error("Mgmd not connected");
      return false;
    }

    Config conf;
    if (!get_config(conf)) {
      error("Mgmd : get_config failed");
      return false;
    }

    Uint64 default_value = 0;
    ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
    for (int idx = 0; iter.openSection(type_of_section, idx); idx++) {
      Uint64 old_value = 0;
      if (iter.get(config_variable, &old_value)) {
        if (default_value == 0) {
          default_value = old_value;
        } else if (old_value != default_value) {
          g_err << "Config value is not consistent across sections." << endl;
          iter.closeSection();
          return false;
        }
      }
      iter.set(config_variable, new_value);
      iter.closeSection();
    }

    // Return old config value
    *saved_old_value = default_value;

    // Set the new config in mgmd
    if (!set_config(conf)) {
      error("Mgmd : set_config failed");
      return false;
    }

    // TODO: Instead of using flaky sleep, try reconnect and
    // determine whether the config is changed.
    NdbSleep_SecSleep(10);  // Give MGM server time to restart

    return true;
  }

  bool change_config32(Uint32 new_value, Uint32 *saved_old_value,
                       unsigned type_of_section, unsigned config_variable) {
    if (!is_connected()) {
      if (!connect()) {
        error("Mgmd not connected");
        return false;
      }
    }

    Config conf;
    if (!get_config(conf)) {
      error("Mgmd : get_config failed");
      return false;
    }

    Uint32 default_value = 0;
    ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
    for (int idx = 0; iter.openSection(type_of_section, idx); idx++) {
      Uint32 old_value = 0;
      if (iter.get(config_variable, &old_value)) {
        if (default_value == 0) {
          default_value = old_value;
        } else if (old_value != default_value) {
          g_err << "Config value is not consistent across sections." << endl;
          iter.closeSection();
          return false;
        }
      }
      iter.set(config_variable, new_value);
      iter.closeSection();
    }

    // Return old config value
    *saved_old_value = default_value;

    // Set the new config in mgmd
    if (!set_config(conf)) {
      error("Mgmd : set_config failed");
      return false;
    }

    // TODO: Instead of using flaky sleep, try reconnect and
    // determine whether the config is changed.
    NdbSleep_SecSleep(10);  // Give MGM server time to restart

    return true;
  }

  Uint32 get_config32(unsigned type_of_section, unsigned config_variable) {
    if (!is_connected()) {
      if (!connect()) {
        error("Mgmd not connected");
        return 0;
      }
    }

    Config conf;
    if (!get_config(conf)) {
      error("Mgmd : get_config failed");
      return 0;
    }

    ConfigValues::Iterator iter(conf.m_configuration->m_config_values);
    for (int idx = 0; iter.openSection(type_of_section, idx); idx++) {
      Uint32 current_value = 0;
      if (iter.get(config_variable, &current_value)) {
        if (current_value > 0) {
          iter.closeSection();
          return current_value;
        }
      }
      iter.closeSection();
    }
    return 0;
  }

  // Pretty printer for 'ndb_mgm_node_type'
  class NodeType {
    BaseString m_str;

   public:
    NodeType(Uint32 node_type) {
      const char *str = NULL;
      const char *alias = ndb_mgm_get_node_type_alias_string(
          (ndb_mgm_node_type)node_type, &str);
      m_str.assfmt("%s(%s)", alias, str);
    }

    const char *c_str() { return m_str.c_str(); }
  };
};

#endif
