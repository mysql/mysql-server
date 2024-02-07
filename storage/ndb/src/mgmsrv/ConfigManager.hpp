/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#ifndef ConfigManager_H
#define ConfigManager_H

#include "Config.hpp"
#include "ConfigSubscriber.hpp"
#include "Defragger.hpp"
#include "MgmtSrvr.hpp"
#include "MgmtThread.hpp"

#include <ConfigRetriever.hpp>

#include <HashMap.hpp>
#include <NodeBitmask.hpp>
#include <SignalSender.hpp>

#include <signaldata/ConfigChange.hpp>

class ConfigManager : public MgmtThread {
  const MgmtSrvr::MgmtOpts &m_opts;
  TransporterFacade *m_facade;
  SignalSender *m_ss;

  NdbMutex *m_config_mutex;
  const Config *m_config;
  BaseString m_packed_config_v1;  // base64 packed
  BaseString m_packed_config_v2;  // base64 packed

  ConfigRetriever m_config_retriever;

  struct ConfigChangeState {
    enum States {
      IDLE = 0,
      PREPARING = 1,
      COMITTING = 2,
      ABORT = 3,
      ABORTING = 4
    } m_current_state;

    ConfigChangeState() : m_current_state(IDLE) {}

    operator int() const { return m_current_state; }
  };

  void set_config_change_state(ConfigChangeState::States state);

  enum ConfigState {
    CS_UNINITIALIZED = 0,

    CS_INITIAL = 1,  // Initial config.ini, ie. no config.bin.X found

    CS_CONFIRMED = 2,  // Started and all agreed
    CS_FORCED = 3      // Forced start
  };

  ConfigState m_config_state;
  ConfigState m_previous_state;

  struct ConfigChange {
    ConfigChange()
        : m_client_ref(RNIL),
          m_error(ConfigChangeRef::OK),
          m_new_config(0),
          m_loaded_config(0),
          m_initial_config(0) {}

    void release() {
      if (m_new_config) delete m_new_config;
      if (m_loaded_config) delete m_loaded_config;
      if (m_initial_config) delete m_initial_config;
      m_new_config = 0;
      m_loaded_config = 0;
      m_initial_config = 0;
    }

    virtual ~ConfigChange() { release(); }

    ConfigChangeState m_state;
    BlockReference m_client_ref;
    /* The original error that caused config change to be aborted */
    ConfigChangeRef::ErrorCode m_error;
    const Config *m_new_config;

    bool config_loaded(Config *config);
    Config *m_loaded_config;
    Config *m_initial_config;
    NodeBitmask m_contacted_nodes;
  } m_config_change;

  BaseString m_config_name;
  Config *m_prepared_config;

  NodeBitmask m_all_mgm;
  NodeBitmask m_started;
  NodeBitmask m_waiting_for;
  NodeBitmask m_checked;

  NodeId m_node_id;

  const char *m_configdir;

  Defragger m_defragger;

  /* Functions used from 'init' */
  static Config *load_init_config(const char *);
  static Config *load_init_mycnf(const char *cluster_config_suffix);
  Config *load_config(void) const;
  Config *fetch_config(void);
  bool save_config(const Config *conf);
  bool save_config(void);
  bool saved_config_exists(BaseString &config_name) const;
  bool delete_saved_configs(void) const;
  bool failed_config_change_exists(void) const;
  Config *load_saved_config(const BaseString &config_name);
  NodeId find_nodeid_from_configdir(void);
  NodeId find_nodeid_from_config(void);
  bool init_nodeid(void);

  /* Set the new config and inform subscribers */
  void set_config(Config *config);
  Vector<ConfigSubscriber *> m_subscribers;

  /* Check config is ok */
  bool config_ok(const Config *conf);

  /* Prepare loaded config */
  Config *prepareLoadedConfig(Config *config);

  /* Functions for writing config.bin to disk */
  bool prepareConfigChange(const Config *config);
  void commitConfigChange();
  void abortConfigChange();

  /* Functions for starting config change from ConfigManager */
  void startConfigChange(SignalSender &ss, Uint32 ref);
  void startAbortConfigChange(SignalSender &);

  /* CONFIG_CHANGE - controlling config change from other node */
  void execCONFIG_CHANGE_REQ(SignalSender &ss, SimpleSignal *sig);
  void execCONFIG_CHANGE_REF(SignalSender &ss, SimpleSignal *sig);
  void sendConfigChangeRef(SignalSender &ss, BlockReference,
                           ConfigChangeRef::ErrorCode) const;
  void sendConfigChangeConf(SignalSender &ss, BlockReference) const;

  /*
    CONFIG_CHANGE_IMPL - protocol for starting config change
    between nodes
  */
  void execCONFIG_CHANGE_IMPL_REQ(SignalSender &ss, SimpleSignal *sig);
  void execCONFIG_CHANGE_IMPL_REF(SignalSender &ss, SimpleSignal *sig);
  void execCONFIG_CHANGE_IMPL_CONF(SignalSender &ss, SimpleSignal *sig);
  void sendConfigChangeImplRef(SignalSender &ss, NodeId nodeId,
                               ConfigChangeRef::ErrorCode) const;
  int sendConfigChangeImplReq(SignalSender &ss, const Config *conf);

  /*
    CONFIG_CHECK - protocol for exchanging and checking config state
    between nodes
  */
  void execCONFIG_CHECK_REQ(SignalSender &ss, SimpleSignal *sig);
  void execCONFIG_CHECK_CONF(SignalSender &ss, SimpleSignal *sig);
  void execCONFIG_CHECK_REF(SignalSender &ss, SimpleSignal *sig);
  void sendConfigCheckReq(SignalSender &ss, NodeBitmask to);
  void sendConfigCheckRef(SignalSender &ss, BlockReference to,
                          ConfigCheckRef::ErrorCode, Uint32, Uint32,
                          ConfigState, ConfigState) const;
  void sendConfigCheckConf(SignalSender &ss, BlockReference to) const;

  class DynamicPorts {
    struct NodePair {
      int node1;
      int node2;
      NodePair(int n1, int n2) : node1(n1), node2(n2) {}
    };
    HashMap<NodePair, int> m_ports;
    bool check(int &node1, int &node2) const;

   public:
    bool set(int node1, int node2, int value);
    bool get(int node1, int node2, int *value) const;

    // Write all known dynamic ports into the config
    bool set_in_config(Config *config);

  } m_dynamic_ports;

 public:
  ConfigManager(const MgmtSrvr::MgmtOpts &, const char *configdir);
  ~ConfigManager() override;
  bool init();
  void set_facade(TransporterFacade *facade);
  void run() override;

  /*
    Installs subscriber that will be notified when
    config has changed
   */
  int add_config_change_subscriber(ConfigSubscriber *);

  /*
    Retrieve the current configuration in base64 packed format
   */
  bool get_packed_config(ndb_mgm_node_type nodetype, BaseString *buf64,
                         BaseString &error, bool v2, Uint32 node_id);

  static Config *load_config(const char *config_filename, bool mycnf,
                             BaseString &msg,
                             const char *cluster_config_suffix);

  bool set_dynamic_port(int node1, int node2, int value, BaseString &msg);
  bool set_dynamic_ports(int node, MgmtSrvr::DynPortSpec ports[],
                         unsigned num_ports, BaseString &msg);
  bool get_dynamic_port(int node1, int node2, int *value,
                        BaseString &msg) const;
};

#endif
