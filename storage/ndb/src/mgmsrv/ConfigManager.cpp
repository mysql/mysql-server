/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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


#include "util/require.h"
#include <time.h>

#include "ConfigManager.hpp"
#include "MgmtSrvr.hpp"
#include <NdbDir.hpp>

#include <NdbConfig.h>
#include <NdbSleep.h>
#include <kernel/GlobalSignalNumbers.h>
#include <SignalSender.hpp>
#include <NdbApiSignal.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <ndb_version.h>

#include <EventLogger.hpp>

extern const char* opt_ndb_connectstring;
extern int opt_ndb_nodeid;

#if defined VM_TRACE || defined ERROR_INSERT
extern int g_errorInsert;
#define ERROR_INSERTED(x) (g_errorInsert == x)
#else
#define ERROR_INSERTED(x) false
#endif

ConfigManager::ConfigManager(const MgmtSrvr::MgmtOpts& opts,
                             const char* configdir) :
  MgmtThread("ConfigManager"),
  m_opts(opts),
  m_facade(NULL),
  m_ss(NULL),
  m_config_mutex(NULL),
  m_config(NULL),
  m_config_retriever(opt_ndb_connectstring,
                     opt_ndb_nodeid,
                     NDB_VERSION,
                     NDB_MGM_NODE_TYPE_MGM,
                     opts.bind_address),
  m_config_state(CS_UNINITIALIZED),
  m_previous_state(CS_UNINITIALIZED),
  m_prepared_config(NULL),
  m_node_id(0),
  m_configdir(configdir)
{
}


ConfigManager::~ConfigManager()
{
  delete m_config;
  delete m_prepared_config;
  if (m_ss)
    delete m_ss;
  NdbMutex_Destroy(m_config_mutex);
}


/**
   alone_on_host

   Check if this is the only node of "type" on
   this host

*/

static bool
alone_on_host(Config* conf,
              Uint32 own_type,
              Uint32 own_nodeid)
{
  ConfigIter iter(conf, CFG_SECTION_NODE);
  for (iter.first(); iter.valid(); iter.next())
  {
    Uint32 type;
    if(iter.get(CFG_TYPE_OF_SECTION, &type) ||
       type != own_type)
    {
      continue;
    }

    Uint32 nodeid;
    if(iter.get(CFG_NODE_ID, &nodeid) ||
       nodeid == own_nodeid)
    {
      continue;
    }

    const char * hostname;
    if(iter.get(CFG_NODE_HOST, &hostname))
    {
      continue;
    }

    if (SocketServer::tryBind(0,hostname))
    {
      // Another MGM node was also setup on this host
      g_eventLogger->debug("Not alone on host %s, node %d "     \
                           "will also run here",
                           hostname, nodeid);
      return false;
    }
  }
  return true;
}


/**
   find_nodeid_from_configdir

   Check if configdir only contains config files
   with one nodeid -> read the latest and confirm
   there should only be one mgm node on this host
*/

NodeId
ConfigManager::find_nodeid_from_configdir(void)
{
  BaseString config_name;
  NdbDir::Iterator iter;

  if (!m_configdir ||
      iter.open(m_configdir) != 0)
  {
    return 0;
  }

  const char* name;
  unsigned found_nodeid= 0;
  unsigned nodeid;
  char extra; // Avoid matching ndb_2_config.bin.2.tmp
  unsigned version, max_version = 0;
  while ((name = iter.next_file()) != NULL)
  {
    if (sscanf(name,
               "ndb_%u_config.bin.%u%c",
               &nodeid, &version, &extra) == 2)
    {
      // ndbout_c("match: %s", name);

      if (nodeid != found_nodeid)
      {
        if (found_nodeid != 0)
        {
          return 0; // Found more than one nodeid
        }
        found_nodeid= nodeid;
      }

      if (version > max_version)
        max_version = version;
    }
  }

  if (max_version == 0)
  {
    return 0;
  }

  config_name.assfmt("%s%sndb_%u_config.bin.%u",
                     m_configdir, DIR_SEPARATOR, found_nodeid, max_version);

  Config* conf;
  if (!(conf = load_saved_config(config_name)))
  {
    return 0;
  }

  if (!m_config_retriever.verifyConfig(conf->m_configuration,
                                       found_nodeid) ||
      !alone_on_host(conf, NDB_MGM_NODE_TYPE_MGM, found_nodeid))
  {
    delete conf;
    return 0;
  }

  delete conf;
  return found_nodeid;
}


/**
   find_own_nodeid

   Return the nodeid of the MGM node
   defined to run on this host

   Return 0 if more than one node is defined
*/

static NodeId
find_own_nodeid(Config* conf)
{
  NodeId found_nodeid= 0;
  ConfigIter iter(conf, CFG_SECTION_NODE);
  int unmatched_host_count = 0;
  std::string unmatched_hostname;
  const char *separator = "";
  for (iter.first(); iter.valid(); iter.next())
  {
    Uint32 type;
    if(iter.get(CFG_TYPE_OF_SECTION, &type) ||
       type != NDB_MGM_NODE_TYPE_MGM)
    {
      continue;
    }

    Uint32 nodeid;
    require(iter.get(CFG_NODE_ID, &nodeid) == 0);

    const char * hostname;
    if(iter.get(CFG_NODE_HOST, &hostname))
    {
      continue;
    }

    if (SocketServer::tryBind(0,hostname))
    {
      // This node is setup to run on this host
      if (found_nodeid == 0)
        found_nodeid = nodeid;
      else {
        g_eventLogger->error(
            "More than one hostname matches a local interface, including node "
            "ids %d and %d.",
            found_nodeid, nodeid);
        return 0;
      }
    } else {
      unmatched_host_count++;
      // Append the hostname to the list of unmatched host
      unmatched_hostname += separator + std::string(hostname);
      separator = ",";
    }
  }
  if (found_nodeid == 0 && unmatched_host_count > 0) {
    g_eventLogger->error(
        "At least one hostname in the configuration does not match a local "
        "interface. Failed to bind on %s",
        unmatched_hostname.c_str());
  }
  return found_nodeid;
}

NodeId
ConfigManager::find_nodeid_from_config(void)
{
  if (!m_opts.mycnf &&
      !m_opts.config_filename)
  {
    return 0;
  }

  Config* conf = load_config();
  if (conf == NULL)
  {
    return 0;
  }

  NodeId found_nodeid = find_own_nodeid(conf);
  if (found_nodeid == 0 ||
      !m_config_retriever.verifyConfig(conf->m_configuration, found_nodeid))
  {
    delete conf;
    return 0;
  }

  delete conf;
  return found_nodeid;
}


bool
ConfigManager::init_nodeid(void)
{
  DBUG_ENTER("ConfigManager::init_nodeid");

  NodeId nodeid = m_config_retriever.get_configuration_nodeid();
  if (nodeid)
  {
    // Nodeid was specified on command line or in NDB_CONNECTSTRING
    g_eventLogger->debug("Got nodeid: %d from command line "    \
                         "or NDB_CONNECTSTRING", nodeid);
    m_node_id = nodeid;
    DBUG_RETURN(true);
  }

  nodeid = find_nodeid_from_configdir();
  if (nodeid)
  {
    // Found nodeid by searching in configdir
    g_eventLogger->debug("Got nodeid: %d from searching in configdir",
                         nodeid);
    m_node_id = nodeid;
    DBUG_RETURN(true);
  }

  nodeid = find_nodeid_from_config();
  if (nodeid)
  {
    // Found nodeid by looking in the config given on command line
    g_eventLogger->debug("Got nodeid: %d from config file given "       \
                         "on command line",
                         nodeid);
    m_node_id = nodeid;
    DBUG_RETURN(true);
  }

  if (m_config_retriever.hasError())
  {
    g_eventLogger->error("%s", m_config_retriever.getErrorString());
  }

  // We _could_ try connecting to other running mgmd(s)
  // and fetch our nodeid. But, that introduces a dependency
  // that is not beneficial for a shared nothing cluster, since
  // it might only work when other mgmd(s) are started. If all
  // mgmd(s) is down it would require manual intervention.
  // Better to require the node id to always be specified
  // on the command line(or the above _local_ magic)

  g_eventLogger->error("Could not determine which nodeid to use for "\
                       "this node. Specify it with --ndb-nodeid=<nodeid> "\
                       "on command line");
  DBUG_RETURN(false);
}


static void
reset_dynamic_ports_in_config(const Config* config)
{
  ConfigIter iter(config, CFG_SECTION_CONNECTION);

  for(;iter.valid();iter.next()) {
    Uint32 port;
    require(iter.get(CFG_CONNECTION_SERVER_PORT, &port) == 0);

    if ((int)port < 0)
    {
      port = 0;
      ConfigValues::Iterator i2(config->m_configuration->m_config_values,
                                iter.m_config);
      require(i2.set(CFG_CONNECTION_SERVER_PORT, port));
    }
  }
}


bool
ConfigManager::init(void)
{
  DBUG_ENTER("ConfigManager::init");

  m_config_mutex = NdbMutex_Create();
  if (!m_config_mutex)
  {
    g_eventLogger->error("Failed to create mutex in ConfigManager!");
    DBUG_RETURN(false);
  }

  require(m_config_state == CS_UNINITIALIZED);

  if (m_config_retriever.hasError())
  {
    g_eventLogger->error("%s", m_config_retriever.getErrorString());
    DBUG_RETURN(false);
  }

  if (!init_nodeid())
    DBUG_RETURN(false);

  if (m_opts.initial)
  {
    /**
     * Verify valid -f before delete_saved_configs()
     */
    Config* conf = load_config();
    if (conf == NULL)
      DBUG_RETURN(false);

    delete conf;

    if (!delete_saved_configs())
      DBUG_RETURN(false);
  }

  if (failed_config_change_exists())
    DBUG_RETURN(false);

  BaseString config_bin_name;
  if (saved_config_exists(config_bin_name))
  {
    /**
     * ndb-connectstring is ignored when mgmd is started from binary
     * config
     */
    if (!(m_opts.config_filename || m_opts.mycnf) && opt_ndb_connectstring) {
      g_eventLogger->warning(
          "--ndb-connectstring is ignored when mgmd is started from binary "
          "config.");
    }

    Config* conf = NULL;
    if (!(conf = load_saved_config(config_bin_name)))
      DBUG_RETURN(false);

    if (!config_ok(conf))
      DBUG_RETURN(false);

    set_config(conf);
    m_config_state = CS_CONFIRMED;

    g_eventLogger->info("Loaded config from '%s'", config_bin_name.c_str());

    if (m_opts.reload && // --reload
        (m_opts.mycnf || m_opts.config_filename))
    {
      Config* new_conf = load_config();
      if (new_conf == NULL)
        DBUG_RETURN(false);

      /**
       * Add config to set once ConfigManager is fully started
       */
      m_config_change.config_loaded(new_conf);
      g_eventLogger->info("Loaded configuration from '%s', will try "   \
                          "to set it once started",
                          m_opts.mycnf ? "my.cnf" : m_opts.config_filename);
    }
  }
  else
  {
    if (m_opts.mycnf || m_opts.config_filename)
    {
      Config* conf = load_config();
      if (conf == NULL)
        DBUG_RETURN(false);

      if (!config_ok(conf))
        DBUG_RETURN(false);

      /*
        Set this node as primary node for config.ini/my.cnf
        in order to make it possible that make sure an old
        config.ini is only loaded with --force
      */
      if (!conf->setPrimaryMgmNode(m_node_id))
      {
        g_eventLogger->error("Failed to set primary MGM node");
        DBUG_RETURN(false);
      }

      /* Use the initial config for now */
      set_config(conf);

      g_eventLogger->info("Got initial configuration from '%s', will try " \
                          "to set it when all ndb_mgmd(s) started",
                          m_opts.mycnf ? "my.cnf" : m_opts.config_filename);
      m_config_change.m_initial_config = new Config(conf); // Copy config
      m_config_state = CS_INITIAL;
    }
    else
    {
      Config* conf = NULL;
      if (!(conf = fetch_config()))
      {
        g_eventLogger->error("Could not fetch config!");
        DBUG_RETURN(false);
      }

      /*
        The fetched config may contain dynamic ports for
        ndbd(s) which have to be reset to 0 before using
        the config
      */
      reset_dynamic_ports_in_config(conf);

      if (!config_ok(conf))
        DBUG_RETURN(false);

      /* Use the fetched config for now */
      set_config(conf);

      if (!m_opts.config_cache)
      {
        assert(!m_configdir); // Running without configdir
        g_eventLogger->info("Fetched configuration, " \
                            "generation: %d, name: '%s'. ",
                            m_config->getGeneration(), m_config->getName());
        DBUG_RETURN(true);
      }

      if (m_config->getGeneration() == 0)
      {
        g_eventLogger->info("Fetched initial configuration, " \
                            "generation: %d, name: '%s'. "\
                            "Will try to set it when all ndb_mgmd(s) started",
                            m_config->getGeneration(), m_config->getName());
        m_config_state= CS_INITIAL;
        m_config_change.m_initial_config = new Config(conf); // Copy config
      }
      else
      {
        g_eventLogger->info("Fetched confirmed configuration, " \
                            "generation: %d, name: '%s'. " \
                            "Trying to write it to disk...",
                            m_config->getGeneration(), m_config->getName());
        if (!prepareConfigChange(m_config))
        {
          abortConfigChange();
          g_eventLogger->error("Failed to write the fetched config to disk");
          DBUG_RETURN(false);
        }
        commitConfigChange();
        m_config_state = CS_CONFIRMED;
        g_eventLogger->info("The fetched configuration has been saved!");
      }
    }
  }

  require(m_config_state != CS_UNINITIALIZED);
  DBUG_RETURN(true);
}


bool
ConfigManager::prepareConfigChange(const Config* config)
{
  if (m_prepared_config)
  {
    g_eventLogger->error("Can't prepare configuration change " \
                         "when already prepared");
    return false;
  }

  Uint32 generation= config->getGeneration();
  if (generation == 0)
  {
    g_eventLogger->error("Can't prepare configuration change for "\
                         "configuration with generation 0");
    return false;
  }

  assert(m_node_id);
  m_config_name.assfmt("%s%sndb_%u_config.bin.%u",
                       m_configdir, DIR_SEPARATOR, m_node_id, generation);
  g_eventLogger->debug("Preparing configuration, generation: %d name: %s",
                       generation, m_config_name.c_str());

  /* Check file name is free */
  if (access(m_config_name.c_str(), F_OK) == 0)
  {
    g_eventLogger->error("The file '%s' already exist while preparing",
                         m_config_name.c_str());
    return false;
  }

  /* Pack the config */
  UtilBuffer buf;
  if(!config->pack(buf, OUR_V2_VERSION))
  {
    /* Failed to pack config */
    g_eventLogger->error("Failed to pack configuration while preparing");
    return false;
  }

  /* Write config to temporary file */
  BaseString prep_config_name(m_config_name);
  prep_config_name.append(".tmp");
  FILE * f = fopen(prep_config_name.c_str(), IF_WIN("wbc", "w"));
  if(f == NULL)
  {
    g_eventLogger->error("Failed to open file '%s' while preparing, errno: %d",
                         prep_config_name.c_str(), errno);
    return false;
  }

  if(fwrite(buf.get_data(), 1, buf.length(), f) != (size_t)buf.length())
  {
    g_eventLogger->error("Failed to write file '%s' while preparing, errno: %d",
                         prep_config_name.c_str(), errno);
    fclose(f);
    unlink(prep_config_name.c_str());
    return false;
  }

  if (fflush(f))
  {
    g_eventLogger->error("Failed to flush file '%s' while preparing, errno: %d",
                         prep_config_name.c_str(), errno);
    fclose(f);
    unlink(prep_config_name.c_str());
    return false;
  }

#ifdef _WIN32
  /*
	File is opened with the commit flag "c" so
	that the contents of the file buffer are written
	directly to disk when fflush is called
  */
#else
  if (fsync(fileno(f)))
  {
    g_eventLogger->error("Failed to sync file '%s' while preparing, errno: %d",
                         prep_config_name.c_str(), errno);
    fclose(f);
    unlink(prep_config_name.c_str());
    return false;
  }
#endif
  fclose(f);

  m_prepared_config = new Config(config); // Copy
  g_eventLogger->debug("Configuration prepared");

  return true;
}


void
ConfigManager::commitConfigChange(void)
{
  require(m_prepared_config != 0);

  /* Set new config locally and in all subscribers */
  set_config(m_prepared_config);
  m_prepared_config= NULL;

  /* Rename file to real name */
  require(m_config_name.length());
  BaseString prep_config_name(m_config_name);
  prep_config_name.append(".tmp");
  if(rename(prep_config_name.c_str(), m_config_name.c_str()))
  {
    g_eventLogger->error("rename from '%s' to '%s' failed while committing, " \
                         "errno: %d",
                         prep_config_name.c_str(), m_config_name.c_str(),
                         errno);
    // Crash and leave the prepared config file in place
    abort();
  }
  m_config_name.clear();

  g_eventLogger->info("Configuration %d commited", m_config->getGeneration());
}


static void
check_no_dynamic_ports_in_config(const Config* config)
{
  bool ok = true;
  ConfigIter iter(config, CFG_SECTION_CONNECTION);

  for(;iter.valid();iter.next()) {
    Uint32 n1 = 0;
    Uint32 n2 = 0;
    require(iter.get(CFG_CONNECTION_NODE_1, &n1) == 0 &&
            iter.get(CFG_CONNECTION_NODE_2, &n2) == 0);

    Uint32 port_value;
    require(iter.get(CFG_CONNECTION_SERVER_PORT, &port_value) == 0);

    int port = (int)port_value;
    if (port < 0)
    {
      g_eventLogger->error("INTERNAL ERROR: Found dynamic ports with "
                           "value in config, n1: %d, n2: %d, port: %u",
                           n1, n2, port);
      ok = false;
    }
  }
  require(ok);
}


void
ConfigManager::set_config(Config* new_config)
{
  // Check that config does not contain any dynamic ports
  check_no_dynamic_ports_in_config(new_config);

  delete m_config;
  m_config = new_config;

  // Removed cache of packed config
  m_packed_config_v1.clear();
  m_packed_config_v2.clear();

  for (unsigned i = 0; i < m_subscribers.size(); i++)
    m_subscribers[i]->config_changed(m_node_id, new_config);
}


int
ConfigManager::add_config_change_subscriber(ConfigSubscriber* subscriber)
{
  return m_subscribers.push_back(subscriber);
}


bool
ConfigManager::config_ok(const Config* conf)
{
  assert(m_node_id);

  /**
   * Validation of Port number for management nodes happens only if its not
   * started. validate_port is set to true when node is not started and set
   * to false when node is started.
   * Validation is also skipped when printing full config.
   */
  bool validate_port = false;
  if (!(m_started.get(m_node_id) || m_opts.print_full_config))
    validate_port = true;
  if (!m_config_retriever.verifyConfig(conf->m_configuration, m_node_id,
                                       validate_port))
  {
    g_eventLogger->error("%s", m_config_retriever.getErrorString());
    return false;
  }

  // Check DataDir exist
  ConfigIter iter(conf, CFG_SECTION_NODE);
  require(iter.find(CFG_NODE_ID, m_node_id) == 0);

  const char *datadir;
  require(iter.get(CFG_NODE_DATADIR, &datadir) == 0);

  if (strcmp(datadir, "") != 0 && // datadir != ""
      access(datadir, F_OK))                 // dir exists
  {
    g_eventLogger->error("Directory '%s' specified with DataDir "  \
                         "in configuration does not exist.",       \
                         datadir);
    return false;
  }

  /**
   * Gives information to users to start all the management nodes for multiple
   * mgmd node configuration.
   */
  if (!(m_started.get(m_node_id) || m_opts.print_full_config)) {
    Uint32 num_mgm_nodes = 0;
    ConfigIter it(conf, CFG_SECTION_NODE);
    for (it.first(); it.valid(); it.next()) {
      unsigned int type;
      require(it.get(CFG_TYPE_OF_SECTION, &type) == 0);

      if (type == NODE_TYPE_MGM) num_mgm_nodes++;
      if (num_mgm_nodes > 1) {
        g_eventLogger->info("Cluster configuration has multiple "
                            "Management nodes. Please start the other "
                            "mgmd nodes if not started yet.");
        break;
      }
    }
  }

  return true;
}


void
ConfigManager::abortConfigChange(void)
{
  /* Should always succeed */

  /* Remove the prepared file */
  BaseString prep_config_name(m_config_name);
  prep_config_name.append(".tmp");
  unlink(prep_config_name.c_str());
  m_config_name.clear();

  delete m_prepared_config;
  m_prepared_config= NULL;
}



void
ConfigManager::sendConfigChangeImplRef(SignalSender& ss, NodeId nodeId,
                                       ConfigChangeRef::ErrorCode error) const
{
  SimpleSignal ssig;
  ConfigChangeImplRef* const ref =
    CAST_PTR(ConfigChangeImplRef, ssig.getDataPtrSend());
  ref->errorCode = error;

  g_eventLogger->debug("Send CONFIG_CHANGE_IMPL_REF to node: %d, error: %d",
                       nodeId, error);

  ss.sendSignal(nodeId, ssig,
                MGM_CONFIG_MAN, GSN_CONFIG_CHANGE_IMPL_REF,
                ConfigChangeImplRef::SignalLength);
}



void
ConfigManager::execCONFIG_CHANGE_IMPL_REQ(SignalSender& ss, SimpleSignal* sig)
{
  NodeId nodeId = refToNode(sig->header.theSendersBlockRef);
  const ConfigChangeImplReq * const req =
    CAST_CONSTPTR(ConfigChangeImplReq, sig->getDataPtr());

  g_eventLogger->debug("Got CONFIG_CHANGE_IMPL_REQ from node: %d, "\
                       "requestType: %d",
                       nodeId, req->requestType);

  if (!m_defragger.defragment(sig))
    return; // More fragments to come

  const Uint32 version_sending = ss.getNodeInfo(nodeId).m_info.m_version;
  bool v2 = ndb_config_version_v2(version_sending);
  Guard g(m_config_mutex);

  switch(req->requestType){
  case ConfigChangeImplReq::Prepare:{
    if (sig->header.m_noOfSections != 1)
    {
      sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::NoConfigData);
      return;
    }

    ConfigValuesFactory cf;
    bool ret = v2 ?
      cf.unpack_v2(sig->ptr[0].p, req->length) :
      cf.unpack_v1(sig->ptr[0].p, req->length);

    if (!ret)
    {
      sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::FailedToUnpack);
      return;
    }

    Config new_config(cf.getConfigValues());
    Uint32 new_generation = new_config.getGeneration();
    Uint32 curr_generation = m_config->getGeneration();
    const char* new_name = new_config.getName();
    const char* curr_name = m_config->getName();

    if (m_config->illegal_change(&new_config))
    {
      sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::IllegalConfigChange);
      return;
    }

    if (req->initial)
    {
      // Check own state
      if (m_config_state != CS_INITIAL)
      {
        g_eventLogger->warning("Refusing to start initial "             \
                               "configuration change since this node "  \
                               "is not in INITIAL state");
        sendConfigChangeImplRef(ss, nodeId,
                                ConfigChangeRef::IllegalInitialState);
        return;
      }

      // Check generation
      if (new_generation != 0)
      {
        g_eventLogger->warning("Refusing to start initial "             \
                               "configuration change since new "        \
                               "generation is not 0 (new_generation: %d)",
                               new_generation);
        sendConfigChangeImplRef(ss, nodeId,
                                ConfigChangeRef::IllegalInitialGeneration);
        return;
      }
      new_generation = 1;

      // Check config is equal to our initial config
      // but skip check if message is from self...
      if (nodeId != refToNode(ss.getOwnRef()))
      {
        Config new_config_copy(&new_config);
        require(new_config_copy.setName(new_name));
        unsigned exclude[]= {CFG_SECTION_SYSTEM, 0};
        if (!new_config_copy.equal(m_config_change.m_initial_config, exclude))
        {
          BaseString buf;
          g_eventLogger->warning
            ("Refusing to start initial config "                        \
             "change when nodes have different "                        \
             "config\n"                                                 \
             "This is the actual diff:\n%s",
             new_config_copy.diff2str(m_config_change.m_initial_config, buf));
          sendConfigChangeImplRef(ss, nodeId,
                                  ConfigChangeRef::DifferentInitial);
          return;
        }

        /*
          Scrap the new_config, it's been used to check that other node
          started from equal initial config, now it's not needed anymore
        */
        delete m_config_change.m_initial_config;
        m_config_change.m_initial_config = NULL;
      }
    }
    else
    {

      // Check that new config has same primary mgm node as current
      Uint32 curr_primary = m_config->getPrimaryMgmNode();
      Uint32 new_primary = new_config.getPrimaryMgmNode();
      if (new_primary != curr_primary)
      {
        g_eventLogger->warning("Refusing to start configuration change " \
                               "requested by node %d, the new config uses " \
                               "different primary mgm node %d. "      \
                               "Current primary mmgm node is %d.",
                               nodeId, new_primary, curr_primary);
        sendConfigChangeImplRef(ss, nodeId,
                                ConfigChangeRef::NotPrimaryMgmNode);
        return;
      }

      if (new_generation == 0 ||
          new_generation != curr_generation)
      {
        BaseString buf;
        g_eventLogger->warning("Refusing to start config change "     \
                               "requested by node with different "    \
                               "generation: %d. Our generation: %d\n" \
                               "This is the actual diff:\n%s",
                               new_generation, curr_generation,
                               new_config.diff2str(m_config, buf));
        sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::InvalidGeneration);
        return;
      }
      new_generation++;

      // Check same cluster name
      if (strcmp(new_name, curr_name))
      {
        BaseString buf;
        g_eventLogger->warning("Refusing to start config change "       \
                               "requested by node with different "      \
                               "name: '%s'. Our name: '%s'\n"           \
                               "This is the actual diff:\n%s",
                               new_name, curr_name,
                               new_config.diff2str(m_config, buf));
        sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::InvalidConfigName);
        return;
      }
    }

    // Set new generation
    if(!new_config.setGeneration(new_generation))
    {
      g_eventLogger->error("Failed to set new generation to %d",
                           new_generation);
      sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::InternalError);
      return;
    }

    if (!prepareConfigChange(&new_config))
    {
      sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::PrepareFailed);
      return;
    }
    break;
  }

  case ConfigChangeImplReq::Commit:
    commitConfigChange();

    // All nodes has agreed on config -> CONFIRMED
    m_config_state = CS_CONFIRMED;

    break;

  case ConfigChangeImplReq::Abort:
    abortConfigChange();
    break;

  default:
    g_eventLogger->error("execCONFIG_CHANGE_IMPL_REQ: unhandled state");
    abort();
    break;
  }

  /* Send CONF */
  SimpleSignal ssig;
  ConfigChangeImplConf* const conf =
    CAST_PTR(ConfigChangeImplConf, ssig.getDataPtrSend());
  conf->requestType = req->requestType;

  g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_CONF to node: %d",
                       nodeId);

  ss.sendSignal(nodeId, ssig,
                MGM_CONFIG_MAN,
                GSN_CONFIG_CHANGE_IMPL_CONF,
                ConfigChangeImplConf::SignalLength);
}


void ConfigManager::set_config_change_state(ConfigChangeState::States state)
{
  if (state == ConfigChangeState::IDLE)
  {
    // Rebuild m_all_mgm so that each node in config is included
    // new mgm nodes might have been added
    assert(m_config_change.m_error == ConfigChangeRef::OK);
    m_config->get_nodemask(m_all_mgm, NDB_MGM_NODE_TYPE_MGM);
  }

  m_config_change.m_state.m_current_state = state;
}


void
ConfigManager::execCONFIG_CHANGE_IMPL_REF(SignalSender& ss, SimpleSignal* sig)
{
  NodeId nodeId = refToNode(sig->header.theSendersBlockRef);
  g_eventLogger->debug("Got CONFIG_CHANGE_IMPL_REF from node: %d", nodeId);

  const ConfigChangeImplRef * const ref =
    CAST_CONSTPTR(ConfigChangeImplRef, sig->getDataPtr());
  g_eventLogger->warning("Node %d refused configuration change, error: %d",
                         nodeId, ref->errorCode);

  /* Remember the original error code */
  if (m_config_change.m_error == 0)
    m_config_change.m_error = (ConfigChangeRef::ErrorCode)ref->errorCode;

  switch(m_config_change.m_state){
  case ConfigChangeState::ABORT:
  case ConfigChangeState::PREPARING:{
    /* Got ref while preparing (or already decided to abort) */
    m_config_change.m_contacted_nodes.clear(nodeId);
    set_config_change_state(ConfigChangeState::ABORT);

    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    startAbortConfigChange(ss);
    break;
  }
  case ConfigChangeState::COMITTING:
    /* Got ref while committing, impossible */
    abort();
    break;

  case ConfigChangeState::ABORTING:
    /* Got ref while aborting, impossible */
    abort();
    break;

  default:
    g_eventLogger->error("execCONFIG_CHANGE_IMPL_REF: unhandled state");
    abort();
    break;
  }
}


void
ConfigManager::execCONFIG_CHANGE_IMPL_CONF(SignalSender& ss, SimpleSignal* sig)
{
  NodeId nodeId = refToNode(sig->header.theSendersBlockRef);
  const ConfigChangeImplConf * const conf =
    CAST_CONSTPTR(ConfigChangeImplConf, sig->getDataPtr());
  g_eventLogger->debug("Got CONFIG_CHANGE_IMPL_CONF from node %d", nodeId);

  switch(m_config_change.m_state){
  case ConfigChangeState::PREPARING:{
    require(conf->requestType == ConfigChangeImplReq::Prepare);
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    // send to next
    int res = sendConfigChangeImplReq(ss, m_config_change.m_new_config);
    if (res > 0)
    {
      // sent to new node...
      return;
    }
    else if (res < 0)
    {
      // send failed, start abort
      startAbortConfigChange(ss);
      return;
    }

    /**
     * All node has received new config..
     *   ok to delete it...
     */
    delete m_config_change.m_new_config;
    m_config_change.m_new_config = 0;

    /* Send commit to all nodes */
    SimpleSignal ssig;
    ConfigChangeImplReq* const req =
      CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());

    req->requestType = ConfigChangeImplReq::Commit;

    g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(commit)");
    require(m_waiting_for.isclear());
    m_waiting_for = ss.broadcastSignal(m_config_change.m_contacted_nodes, ssig,
                                       MGM_CONFIG_MAN,
                                       GSN_CONFIG_CHANGE_IMPL_REQ,
                                       ConfigChangeImplReq::SignalLength);
    if (m_waiting_for.isclear())
      set_config_change_state(ConfigChangeState::IDLE);
    else
      set_config_change_state(ConfigChangeState::COMITTING);
    break;
  }

  case ConfigChangeState::COMITTING:{
    require(conf->requestType == ConfigChangeImplReq::Commit);

    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    require(m_config_change.m_client_ref != RNIL);
    require(m_config_change.m_error == 0);
    if (m_config_change.m_client_ref == ss.getOwnRef())
    {
      g_eventLogger->info("Config change completed! New generation: %d",
                          m_config->getGeneration());
    }
    else
    {
      /* Send CONF to requestor */
      sendConfigChangeConf(ss, m_config_change.m_client_ref);
    }
    m_config_change.m_client_ref = RNIL;
    set_config_change_state(ConfigChangeState::IDLE);
    break;
  }

  case ConfigChangeState::ABORT:{
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    startAbortConfigChange(ss);
    break;
  }

  case ConfigChangeState::ABORTING:{
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    require(m_config_change.m_client_ref != RNIL);
    require(m_config_change.m_error);
    if (m_config_change.m_client_ref == ss.getOwnRef())
    {
      g_eventLogger->
        error("Configuration change failed! error: %d '%s'",
              m_config_change.m_error,
              ConfigChangeRef::errorMessage(m_config_change.m_error));
      exit(1);
    }
    else
    {
      /* Send ref to the requestor */
      sendConfigChangeRef(ss, m_config_change.m_client_ref,
                          m_config_change.m_error);
    }
    m_config_change.m_error= ConfigChangeRef::OK;
    m_config_change.m_client_ref = RNIL;
    set_config_change_state(ConfigChangeState::IDLE);
    break;
  }

  default:
    g_eventLogger->error("execCONFIG_CHANGE_IMPL_CONF: unhandled state");
    abort();
    break;
  }
}


void
ConfigManager::sendConfigChangeRef(SignalSender& ss, BlockReference to,
                                   ConfigChangeRef::ErrorCode error) const
{
  NodeId nodeId = refToNode(to);
  SimpleSignal ssig;
  ConfigChangeRef* const ref =
    CAST_PTR(ConfigChangeRef, ssig.getDataPtrSend());
  ref->errorCode = error;

  g_eventLogger->debug("Send CONFIG_CHANGE_REF to node: %d, error: %d",
                       nodeId, error);

  ss.sendSignal(nodeId, ssig, refToBlock(to),
                GSN_CONFIG_CHANGE_REF, ConfigChangeRef::SignalLength);
}


void
ConfigManager::sendConfigChangeConf(SignalSender& ss, BlockReference to) const
{
  NodeId nodeId = refToNode(to);
  SimpleSignal ssig;

  g_eventLogger->debug("Send CONFIG_CHANGE_CONF to node: %d", nodeId);

  ss.sendSignal(nodeId, ssig, refToBlock(to),
                GSN_CONFIG_CHANGE_CONF, ConfigChangeConf::SignalLength);
}


void
ConfigManager::startConfigChange(SignalSender& ss, Uint32 ref)
{
  if (m_config_state == CS_INITIAL)
  {
    g_eventLogger->info("Starting initial configuration change");
  }
  else
  {
    require(m_config_state == CS_CONFIRMED);
    g_eventLogger->info("Starting configuration change, generation: %d",
                        m_config_change.m_new_config->getGeneration());
  }
  m_config_change.m_contacted_nodes.clear();
  m_config_change.m_client_ref = ref;
  if (sendConfigChangeImplReq(ss, m_config_change.m_new_config) <= 0)
  {
    g_eventLogger->error("Failed to start configuration change!");
    exit(1);
  }
}

void
ConfigManager::startAbortConfigChange(SignalSender& ss)
{
  /* Abort all other nodes */
  SimpleSignal ssig;
  ConfigChangeImplReq* const req =
    CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());
  req->requestType = ConfigChangeImplReq::Abort;

  g_eventLogger->debug
    ("Sending CONFIG_CHANGE_IMPL_REQ(abort) to %s",
     BaseString::getPrettyText(m_config_change.m_contacted_nodes).c_str());

  require(m_waiting_for.isclear());
  m_waiting_for = ss.broadcastSignal(m_config_change.m_contacted_nodes, ssig,
                                     MGM_CONFIG_MAN,
                                     GSN_CONFIG_CHANGE_IMPL_REQ,
                                     ConfigChangeImplReq::SignalLength);

  if (m_config_change.m_new_config)
  {
    delete m_config_change.m_new_config;
    m_config_change.m_new_config = 0;
  }

  if (m_waiting_for.isclear())
  {
    /**
     * Send CONFIG_CHANGE_IMPL_CONF (aborting) to self
     */
    m_waiting_for.set(ss.getOwnNodeId());
    ConfigChangeImplConf* const conf =
      CAST_PTR(ConfigChangeImplConf, ssig.getDataPtrSend());
    conf->requestType = ConfigChangeImplReq::Abort;

    ss.sendSignal(ss.getOwnNodeId(), ssig,
                  MGM_CONFIG_MAN,
                  GSN_CONFIG_CHANGE_IMPL_CONF,
                  ConfigChangeImplConf::SignalLength);
  }

  set_config_change_state(ConfigChangeState::ABORTING);
}

int
ConfigManager::sendConfigChangeImplReq(SignalSender& ss, const Config* conf)
{
  require(m_waiting_for.isclear());
  require(m_config_change.m_client_ref != RNIL);

  if (m_config_change.m_contacted_nodes.isclear())
  {
    require(m_config_change.m_state == ConfigChangeState::IDLE);
  }
  else
  {
    require(m_config_change.m_state == ConfigChangeState::PREPARING);
  }

  set_config_change_state(ConfigChangeState::PREPARING);

  NodeBitmask nodes = m_all_mgm;
  nodes.bitANDC(m_config_change.m_contacted_nodes);
  if (nodes.isclear())
  {
    return 0; // all done
  }

  /**
   * Send prepare to all MGM nodes 1 by 1
   *   keep track of which I sent to in m_contacted_nodes
   */
  SimpleSignal ssig;
  Uint32 nodeId = nodes.find(0);

  const Uint32 version_receiving = ss.getNodeInfo(nodeId).m_info.m_version;
  bool v2 = ndb_config_version_v2(version_receiving);

  UtilBuffer buf;
  conf->pack(buf, v2);
  ssig.ptr[0].p = (Uint32*)buf.get_data();
  ssig.ptr[0].sz = (buf.length() + 3) / 4;
  ssig.header.m_noOfSections = 1;

  ConfigChangeImplReq* const req =
    CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());
  req->requestType = ConfigChangeImplReq::Prepare;
  req->initial = (m_config_state == CS_INITIAL);
  req->length = buf.length();

  g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(prepare) to %u", nodeId);
  int result = ss.sendFragmentedSignal(nodeId, ssig, MGM_CONFIG_MAN,
                                       GSN_CONFIG_CHANGE_IMPL_REQ,
                                       ConfigChangeImplReq::SignalLength);
  if (result != 0)
  {
    g_eventLogger->warning("Failed to send configuration change "
                           "prepare to node: %d, result: %d",
                           nodeId, result);
    return -1;
  }

  m_waiting_for.set(nodeId);
  m_config_change.m_contacted_nodes.set(nodeId);

  return 1;
}

void
ConfigManager::execCONFIG_CHANGE_REQ(SignalSender& ss, SimpleSignal* sig)
{
  BlockReference from = sig->header.theSendersBlockRef;
  const ConfigChangeReq * const req =
    CAST_CONSTPTR(ConfigChangeReq, sig->getDataPtr());

  if (!m_defragger.defragment(sig))
    return; // More fragments to come

  if (!m_started.equal(m_all_mgm)) // Not all started
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::NotAllStarted);
    return;
  }

  if (m_all_mgm.find(0) != m_facade->ownId()) // Not the master
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::NotMaster);
    return;
  }

  if (m_config_change.m_state != ConfigChangeState::IDLE)
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::ConfigChangeOnGoing);
    return;
  }
  require(m_config_change.m_error == ConfigChangeRef::OK);

  if (sig->header.m_noOfSections != 1)
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::NoConfigData);
    return;
  }

  NodeId senderNodeId = refToNode(sig->header.theSendersBlockRef);
  const Uint32 version_sending = ss.getNodeInfo(senderNodeId).m_info.m_version;
  bool v2 = ndb_config_version_v2(version_sending);
  ConfigValuesFactory cf;
  bool ret = v2 ?
    cf.unpack_v2(sig->ptr[0].p, req->length) :
    cf.unpack_v1(sig->ptr[0].p, req->length);

  if (!ret)
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::FailedToUnpack);
    return;
  }

  Config * new_config = new Config(cf.getConfigValues());
  if (!config_ok(new_config))
  {
    g_eventLogger->warning("Refusing to start config change, the config "\
                           "is not ok");
    sendConfigChangeRef(ss, from, ConfigChangeRef::ConfigNotOk);
    delete new_config;
    return;
  }

  m_config_change.m_new_config = new_config;
  startConfigChange(ss, from);

  return;
}


static Uint32
config_check_checksum(const Config* config, bool v2)
{
  Config copy(config);

  // Make constants of a few values in SYSTEM section that are
  // not part of the  checksum used for "config check"
  copy.setName("CHECKSUM");
  copy.setPrimaryMgmNode(0);

  Uint32 checksum = copy.checksum(v2);

  return checksum;
}


void
ConfigManager::execCONFIG_CHECK_REQ(SignalSender& ss, SimpleSignal* sig)
{
  Guard g(m_config_mutex);
  BlockReference from = sig->header.theSendersBlockRef;
  NodeId nodeId = refToNode(from);
  const ConfigCheckReq * const req =
    CAST_CONSTPTR(ConfigCheckReq, sig->getDataPtr());

  const Uint32 version_sending = ss.getNodeInfo(nodeId).m_info.m_version;
  bool v2 = ndb_config_version_v2(version_sending);

  Uint32 other_generation = req->generation;
  ConfigState other_state = (ConfigState)req->state;

  Uint32 generation = m_config->getGeneration();

  if (ERROR_INSERTED(100) && nodeId != ss.getOwnNodeId())
  {
    g_eventLogger->debug("execCONFIG_CHECK_REQ() ERROR_INSERTED(100) => exit()");
    exit(0);
  }

  // checksum
  Uint32 checksum = config_check_checksum(m_config, v2);
  Uint32 other_checksum = req->checksum;
  if (sig->header.theLength == ConfigCheckReq::SignalLengthBeforeChecksum)
  {
    // Other side uses old version without checksum, use our checksum to
    // bypass the checks
    g_eventLogger->debug("Other mgmd does not have checksum, using own");
    other_checksum = checksum;
  }

  if (m_prepared_config || m_config_change.m_new_config)
  {
    g_eventLogger->debug("Got CONFIG_CHECK_REQ from node: %d while "
                         "config change in progress (m_prepared_config). "
                         "Returning incorrect state, causing it to be retried",
                         nodeId);
    sendConfigCheckRef(ss, from, ConfigCheckRef::WrongState,
                       generation, other_generation,
                       m_config_state, CS_UNINITIALIZED);
    return;
  }

  if (m_config_change.m_loaded_config && ss.getOwnNodeId() < nodeId)
  {
    g_eventLogger->debug("Got CONFIG_CHECK_REQ from node: %d while "
                         "having a loaded config (and my node is lower: %d). "
                         "Returning incorrect state, causing it to be retried",
                         nodeId,
                         ss.getOwnNodeId());
    sendConfigCheckRef(ss, from, ConfigCheckRef::WrongState,
                       generation, other_generation,
                       m_config_state, CS_UNINITIALIZED);
    return;
  }

  g_eventLogger->debug("Got CONFIG_CHECK_REQ from node: %d. "
                       "Our generation: %d, other generation: %d, "
                       "our state: %d, other state: %d, "
                       "our checksum: 0x%.8x, other checksum: 0x%.8x",
                       nodeId, generation, other_generation,
                       m_config_state, other_state,
                       checksum, other_checksum);

  switch (m_config_state)
  {
  default:
  case CS_UNINITIALIZED:
    g_eventLogger->error("execCONFIG_CHECK_REQ: unhandled state");
    abort();
    break;

  case CS_INITIAL:
    if (other_state != CS_INITIAL)
    {
      g_eventLogger->warning("Refusing CONFIG_CHECK_REQ from %u, "
                             "  it's not CS_INITIAL (I am). "
                             " Waiting for my check",
                             nodeId);
      sendConfigCheckRef(ss, from, ConfigCheckRef::WrongState,
                         generation, other_generation,
                         m_config_state, other_state);
      return;
    }

    require(generation == 0);
    if (other_generation != generation)
    {
      g_eventLogger->warning("Refusing other node, it has different "   \
                             "generation: %d, expected: %d",
                             other_generation, generation);
      sendConfigCheckRef(ss, from, ConfigCheckRef::WrongGeneration,
                         generation, other_generation,
                         m_config_state, other_state);
      return;
    }

    if (other_checksum != checksum)
    {
      g_eventLogger->warning("Refusing other node, it has different "
                             "checksum: 0x%.8x, expected: 0x%.8x",
                             other_checksum, checksum);
      sendConfigCheckRef(ss, from, ConfigCheckRef::WrongChecksum,
                         generation, other_generation,
                         m_config_state, other_state);
      return;
    }
    break;

  case CS_CONFIRMED:

    if (other_state != CS_CONFIRMED)
    {
      g_eventLogger->warning("Refusing other node, it's in different "  \
                             "state: %d, expected: %d",
                             other_state, m_config_state);
      sendConfigCheckRef(ss, from, ConfigCheckRef::WrongState,
                         generation, other_generation,
                         m_config_state, other_state);
      return;
    }

    if (other_generation == generation)
    {
      // Same generation, make sure it has same checksum
      if (other_checksum != checksum)
      {
        g_eventLogger->warning("Refusing other node, it has different "
                               "checksum: 0x%.8x, expected: 0x%.8x",
                               other_checksum, checksum);
        sendConfigCheckRef(ss, from, ConfigCheckRef::WrongChecksum,
                           generation, other_generation,
                           m_config_state, other_state);
        return;
      }
      // OK!
    }
    else if (other_generation < generation)
    {
      g_eventLogger->warning("Refusing other node, it has lower "       \
                             " generation: %d, expected: %d",
                             other_generation, generation);
      sendConfigCheckRef(ss, from, ConfigCheckRef::WrongGeneration,
                         generation, other_generation,
                         m_config_state, other_state);
      return;
    }
    else
    {
      g_eventLogger->error("Other node has higher generation: %d, this " \
                           "node is out of sync with generation: %d",
                           other_generation, generation);
      exit(1);
    }

    break;
  }

  sendConfigCheckConf(ss, from);
  return;
}


void
ConfigManager::sendConfigCheckReq(SignalSender& ss, NodeBitmask to)
{
  SimpleSignal ssig;
  ConfigCheckReq* const req =
    CAST_PTR(ConfigCheckReq, ssig.getDataPtrSend());

  req->state =        m_config_state;
  req->generation =   m_config->getGeneration();

  g_eventLogger->debug("Sending CONFIG_CHECK_REQ to %s",
                       BaseString::getPrettyText(to).c_str());

  require(m_waiting_for.isclear());

  Uint32 nodeId = to.find(0);
  while (nodeId != to.NotFound)
  {
    const Uint32 version_receiving = ss.getNodeInfo(nodeId).m_info.m_version;
    bool v2 = ndb_config_version_v2(version_receiving);
    req->checksum = config_check_checksum(m_config, v2);
    m_waiting_for.set(nodeId);
    ss.sendSignal(nodeId,
                  ssig,
                  MGM_CONFIG_MAN,
                  GSN_CONFIG_CHECK_REQ,
                  ConfigCheckReq::SignalLength);
    nodeId = to.find(nodeId + 1);
  }
}

static bool
send_config_in_check_ref(Uint32 x)
{
  if (x >= NDB_MAKE_VERSION(7,0,8))
    return true;
  return false;
}

void
ConfigManager::sendConfigCheckRef(SignalSender& ss, BlockReference to,
                                  ConfigCheckRef::ErrorCode error,
                                  Uint32 generation,
                                  Uint32 other_generation,
                                  ConfigState state,
                                  ConfigState other_state) const
{
  int result;
  NodeId nodeId = refToNode(to);
  SimpleSignal ssig;
  ConfigCheckRef* const ref =
    CAST_PTR(ConfigCheckRef, ssig.getDataPtrSend());
  ref->error = error;
  ref->generation = other_generation;
  ref->expected_generation = generation;
  ref->state = other_state;
  ref->expected_state = state;

  g_eventLogger->debug("Send CONFIG_CHECK_REF with error: %d to node: %d",
                       error, nodeId);

  if (!send_config_in_check_ref(ss.getNodeInfo(nodeId).m_info.m_version))
  {
    result = ss.sendSignal(nodeId, ssig, MGM_CONFIG_MAN,
                           GSN_CONFIG_CHECK_REF, ConfigCheckRef::SignalLength);
  }
  else
  {
    const Uint32 version_receiving = ss.getNodeInfo(nodeId).m_info.m_version;
    bool v2 = ndb_config_version_v2(version_receiving);
    UtilBuffer buf;
    m_config->pack(buf, v2);
    ssig.ptr[0].p = (Uint32*)buf.get_data();
    ssig.ptr[0].sz = (buf.length() + 3) / 4;
    ssig.header.m_noOfSections = 1;

    ref->length = buf.length();

    g_eventLogger->debug("Sending CONFIG_CHECK_REF with config");

    result = ss.sendFragmentedSignal(nodeId, ssig, MGM_CONFIG_MAN,
                                    GSN_CONFIG_CHECK_REF,
                                    ConfigCheckRef::SignalLengthWithConfig);
  }

  if (result != 0)
  {
    g_eventLogger->warning("Failed to send CONFIG_CHECK_REF "
                           "to node: %d, result: %d",
                           nodeId, result);
  }
}

void
ConfigManager::sendConfigCheckConf(SignalSender& ss, BlockReference to) const
{
  NodeId nodeId = refToNode(to);
  SimpleSignal ssig;
  ConfigCheckConf* const conf =
    CAST_PTR(ConfigCheckConf, ssig.getDataPtrSend());
  conf->state = m_config_state;
  conf->generation = m_config->getGeneration();

  g_eventLogger->debug("Send CONFIG_CHECK_CONF to node: %d", nodeId);

  ss.sendSignal(nodeId, ssig, MGM_CONFIG_MAN,
                GSN_CONFIG_CHECK_CONF, ConfigCheckConf::SignalLength);
}


void
ConfigManager::execCONFIG_CHECK_CONF(SignalSender& ss, SimpleSignal* sig)
{
  BlockReference from = sig->header.theSendersBlockRef;
  NodeId nodeId = refToNode(from);
  assert(m_waiting_for.get(nodeId));
  m_waiting_for.clear(nodeId);
  m_checked.set(nodeId);

  g_eventLogger->debug("Got CONFIG_CHECK_CONF from node: %d",
                       nodeId);

  return;
}


void
ConfigManager::execCONFIG_CHECK_REF(SignalSender& ss, SimpleSignal* sig)
{
  BlockReference from = sig->header.theSendersBlockRef;
  NodeId nodeId = refToNode(from);
  assert(m_waiting_for.get(nodeId));

  const ConfigCheckRef* const ref =
    CAST_CONSTPTR(ConfigCheckRef, sig->getDataPtr());

  if (!m_defragger.defragment(sig))
    return; // More fragments to come

  g_eventLogger->debug("Got CONFIG_CHECK_REF from node %d, "
                      "error: %d, message: '%s', "
                      "generation: %d, expected generation: %d, "
                      "state: %d, expected state: %d own-state: %u",
                      nodeId, ref->error,
                      ConfigCheckRef::errorMessage(ref->error),
                      ref->generation, ref->expected_generation,
                      ref->state, ref->expected_state,
                      m_config_state);

  assert(ref->generation != ref->expected_generation ||
         ref->state != ref->expected_state ||
         ref->error == ConfigCheckRef::WrongChecksum);
  if((Uint32)m_config_state != ref->state)
  {
    // The config state changed while this check was in the air
    // drop the signal and thus cause it to run again later
    require(!m_checked.get(nodeId));
    m_waiting_for.clear(nodeId);
    return;
  }

  switch(m_config_state)
  {
  default:
  case CS_UNINITIALIZED:
    g_eventLogger->error("execCONFIG_CHECK_REF: unhandled state");
    abort();
    break;

  case CS_INITIAL:
    if (ref->expected_state == CS_CONFIRMED)
    {
      if (sig->header.theLength != ConfigCheckRef::SignalLengthWithConfig)
        break; // No config in the REF -> no action

      // The other node has sent it's config in the signal, use it if equal
      assert(sig->header.m_noOfSections == 1);

      const Uint32 version_sending = ss.getNodeInfo(nodeId).m_info.m_version;
      bool v2 = ndb_config_version_v2(version_sending);
      ConfigValuesFactory cf;
      bool ret = v2 ?
        cf.unpack_v2(sig->ptr[0].p, ref->length) :
        cf.unpack_v1(sig->ptr[0].p, ref->length);
      require(ret);

      Config other_config(cf.getConfigValues());
      assert(other_config.getGeneration() > 0);

      unsigned exclude[]= {CFG_SECTION_SYSTEM, 0};
      if (!other_config.equal(m_config, exclude))
      {
        BaseString buf;
        g_eventLogger->error("This node was started --initial with "
                             "a config which is _not_ equal to the one "
                             "node %d is using. Refusing to start with "
                             "different configurations, diff: \n%s",
                             nodeId,
                             other_config.diff2str(m_config, buf, exclude));
        exit(1);
      }

      g_eventLogger->info("This node was started --inital with "
                          "a config equal to the one node %d is using. "
                          "Will use the config with generation %d "
                          "from node %d!",
                          nodeId, other_config.getGeneration(), nodeId);

      if (! prepareConfigChange(&other_config))
      {
        abortConfigChange();
        g_eventLogger->error("Failed to write the fetched config to disk");
        exit(1);
      }
      commitConfigChange();
      m_config_state = CS_CONFIRMED;
      g_eventLogger->info("The fetched configuration has been saved!");
      m_waiting_for.clear(nodeId);
      m_checked.set(nodeId);
      delete m_config_change.m_initial_config;
      m_config_change.m_initial_config = NULL;
      return;
    }
    break;

  case CS_CONFIRMED:
    if (ref->expected_state == CS_INITIAL)
    {
      g_eventLogger->info("Waiting for peer");
      m_waiting_for.clear(nodeId);
      return;
    }
    break;
  }

  if (ref->error == ConfigCheckRef::WrongChecksum &&
      m_node_id < nodeId)
  {
    g_eventLogger->warning("Ignoring CONFIG_CHECK_REF for wrong checksum "
                           "other node has higher node id and should "
                           "shutdown");
    return;
  }

  g_eventLogger->error("Terminating");
  exit(1);
}

void
ConfigManager::set_facade(TransporterFacade * f)
{
  m_facade = f;
  m_ss = new SignalSender(f,
                          MGM_CONFIG_MAN,  // blockNum
                          true);           // deliverAll
  require(m_ss != 0);
}

bool
ConfigManager::ConfigChange::config_loaded(Config* config)
{
  if (m_loaded_config != 0)
    return false;
  m_loaded_config = config;
  return true;
}

Config*
ConfigManager::prepareLoadedConfig(Config * new_conf)
{
  /* Copy the necessary values from old to new config */
  if (!new_conf->setGeneration(m_config->getGeneration()))
  {
    g_eventLogger->error("Failed to copy generation from old config");
    delete new_conf;
    return 0;
  }

  if (!new_conf->setName(m_config->getName()))
  {
    g_eventLogger->error("Failed to copy name from old config");
    delete new_conf;
    return 0;
  }

  if (!new_conf->setPrimaryMgmNode(m_config->getPrimaryMgmNode()))
  {
    g_eventLogger->error("Failed to copy primary mgm node from old config");
    delete new_conf;
    return 0;
  }

  /* Check if config has changed */
  if (!m_config->equal(new_conf))
  {
    /* Loaded config is different */
    BaseString buf;
    g_eventLogger->info("Detected change of %s on disk, will try to "
                        "set it. "
                        "This is the actual diff:\n%s",
                        m_opts.mycnf ? "my.cnf" : m_opts.config_filename,
                        m_config->diff2str(new_conf, buf));

    return new_conf;
  }
  else
  {
    /* Loaded config was equal to current */
    g_eventLogger->info("Config equal!");
    delete new_conf;
  }
  return 0;
}

void
ConfigManager::run()
{
  assert(m_facade);
  SignalSender & ss = * m_ss;

  if (!m_opts.config_cache)
  {
    /* Stop receiving signals by closing ConfigManager's
       block in TransporterFacade */
    delete m_ss;
    m_ss = NULL;

    /* Confirm the present config, free the space that was allocated for a
       new one, and terminate the manager thread */
    m_config_change.release();
    m_config_state = CS_CONFIRMED;
    ndbout_c("== ConfigManager disabled -- manager thread will exit ==");
    return;
  }

  ss.lock();

  // Build bitmaks of all mgm nodes in config
  m_config->get_nodemask(m_all_mgm, NDB_MGM_NODE_TYPE_MGM);

  // exclude nowait-nodes from config change protocol
  m_all_mgm.bitANDC(m_opts.nowait_nodes);
  m_all_mgm.set(m_facade->ownId()); // Never exclude own node

  while (!is_stopped())
  {

    if (m_config_change.m_state == ConfigChangeState::IDLE)
    {
      bool print_state = false;
      if (m_previous_state != m_config_state)
      {
        print_state = true;
        m_previous_state = m_config_state;
      }

      /*
        Check if it's necessary to start something to get
        out of the current state
      */
      switch (m_config_state){

      case CS_UNINITIALIZED:
        abort();
        break;

      case CS_INITIAL:
        /*
          INITIAL => CONFIRMED
          When all mgm nodes has been started and checked that they
          are also in INITIAL, the node with the lowest node id
          will start an initial config change. When completed
          all nodes will be in CONFIRMED
        */

        if (print_state)
          ndbout_c("==INITIAL==");

        if (m_config_change.m_initial_config && // Updated config.ini was found
            m_started.equal(m_all_mgm) &&       // All mgmd started
            m_checked.equal(m_started) &&       // All nodes checked
            m_all_mgm.find(0) == m_facade->ownId()) // Lowest nodeid
        {
          Config* new_conf = m_config_change.m_initial_config;
          m_config_change.m_initial_config = 0;
          m_config_change.m_new_config = new_conf;
          startConfigChange(ss, ss.getOwnRef());
        }
        break;

      case CS_CONFIRMED:
        if (print_state)
          ndbout_c("==CONFIRMED==");

        if (m_config_change.m_loaded_config != 0 &&
            m_config_change.m_new_config == 0    &&
            m_started.equal(m_all_mgm)           &&
            m_checked.equal(m_started))
        {
          Config* new_conf = m_config_change.m_loaded_config;
          m_config_change.m_loaded_config = 0;
          m_config_change.m_new_config = prepareLoadedConfig(new_conf);
        }

        if (m_config_change.m_new_config && // Updated config.ini was found
            m_started.equal(m_all_mgm) &&   // All mgmd started
            m_checked.equal(m_started))     // All nodes checked
        {
          startConfigChange(ss, ss.getOwnRef());
        }

        break;

      default:
        break;
      }

      // Send CHECK_CONFIG to all nodes not yet checked
      if (m_waiting_for.isclear() &&   // Nothing outstanding
          m_prepared_config == 0 &&    //   and no config change ongoing
          !m_checked.equal(m_started)) // Some nodes have not been checked
      {
        NodeBitmask not_checked;
        not_checked.assign(m_started);
        not_checked.bitANDC(m_checked);
        sendConfigCheckReq(ss, not_checked);
      }
    }

    SimpleSignal *sig = ss.waitFor((Uint32)1000);
    if (!sig)
      continue;

    switch (sig->readSignalNumber()) {

    case GSN_CONFIG_CHANGE_REQ:
      execCONFIG_CHANGE_REQ(ss, sig);
      break;

    case GSN_CONFIG_CHANGE_IMPL_REQ:
      execCONFIG_CHANGE_IMPL_REQ(ss, sig);
      break;

    case GSN_CONFIG_CHANGE_IMPL_REF:
      execCONFIG_CHANGE_IMPL_REF(ss, sig);
      break;

    case GSN_CONFIG_CHANGE_IMPL_CONF:
      execCONFIG_CHANGE_IMPL_CONF(ss, sig);
      break;

    case GSN_NF_COMPLETEREP:{
       break;
    }

    case GSN_NODE_FAILREP: {
      const NodeFailRep * rep =
              CAST_CONSTPTR(NodeFailRep, sig->getDataPtr());
      assert(sig->getLength() >= NodeFailRep::SignalLengthLong);

      NodeBitmask nodeMap;
      Uint32 len = NodeFailRep::getNodeMaskLength(sig->getLength());
      if (sig->header.m_noOfSections >= 1)
      {
        assert (len == 0);
        nodeMap.assign(sig->ptr[0].sz, sig->ptr[0].p);
      }
      else{
        nodeMap.assign(len, rep->theAllNodes);
      }
      assert(rep->noOfNodes == nodeMap.count());
      nodeMap.bitAND(m_all_mgm);

      Uint32 nodeId = 0;
      for (nodeId = nodeMap.find_first();
           nodeId != NodeBitmask::NotFound;
           nodeId = nodeMap.find_next(nodeId+1))
      {
        m_started.clear(nodeId);
        m_checked.clear(nodeId);
        m_defragger.node_failed(nodeId);

        if (m_config_change.m_state != ConfigChangeState::IDLE)
        {
          g_eventLogger->info("Node %d failed during config change!!",
                              nodeId);
          g_eventLogger->warning("Node failure handling of config "
                                 "change protocol not yet implemented!! "
                                 "No more configuration changes can occur, "
                                 "but the node will continue to serve the "
                                 "last good configuration");
          // TODO start take over of config change protocol
        }
      }
      break;
    }

    case GSN_API_REGCONF:{
      NodeId nodeId = refToNode(sig->header.theSendersBlockRef);
      if (m_all_mgm.get(nodeId) &&      // Is a mgm node
          !m_started.get(nodeId))       // Not already marked as started
      {
        g_eventLogger->info("Node %d connected", nodeId);
        m_started.set(nodeId);
      }
      break;
    }

    case GSN_CONFIG_CHECK_REQ:
      execCONFIG_CHECK_REQ(ss, sig);
      break;

    case GSN_CONFIG_CHECK_REF:
      execCONFIG_CHECK_REF(ss, sig);
      break;

    case GSN_CONFIG_CHECK_CONF:
      execCONFIG_CHECK_CONF(ss, sig);
      break;

    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      break;

    default:
      sig->print();
      g_eventLogger->error("Unknown signal received. SignalNumber: "
                           "%i from (%d, 0x%x)",
                           sig->readSignalNumber(),
                           refToNode(sig->header.theSendersBlockRef),
                           refToBlock(sig->header.theSendersBlockRef));
      abort();
      break;
    }
  }
  ss.unlock();
}


#include "InitConfigFileParser.hpp"

Config*
ConfigManager::load_init_config(const char* config_filename)
{
  InitConfigFileParser parser;
  return parser.parseConfig(config_filename);
}


Config*
ConfigManager::load_init_mycnf(const char* cluster_config_suffix)
{
  InitConfigFileParser parser;
  return parser.parse_mycnf(cluster_config_suffix);
}


Config*
ConfigManager::load_config(const char* config_filename, bool mycnf,
                           BaseString& msg, const char* cluster_config_suffix)
{
  Config* new_conf = nullptr;
  if (mycnf && (new_conf = load_init_mycnf(cluster_config_suffix)) == nullptr)
  {
    msg.assign("Could not load configuration from 'my.cnf'");
    return nullptr;
  }
  else if (config_filename &&
           (new_conf = load_init_config(config_filename)) == NULL)
  {
    msg.assfmt("Could not load configuration from '%s'",
               config_filename);
    return nullptr;
  }

  return new_conf;
}


Config*
ConfigManager::load_config(void) const
{
  BaseString msg;
  Config* new_conf = load_config(m_opts.config_filename,
                                 m_opts.mycnf, msg,
                                 m_opts.cluster_config_suffix);
  if (new_conf == nullptr)
  {
    g_eventLogger->error(msg);
    return nullptr;
  }
  return new_conf;
}


Config*
ConfigManager::fetch_config(void)
{
  DBUG_ENTER("ConfigManager::fetch_config");

  while(true)
  {
    /* Loop until config loaded from other mgmd(s) */
    char buf[128];
    g_eventLogger->info("Trying to get configuration from other mgmd(s) "\
                        "using '%s'...",
                        m_config_retriever.get_connectstring(buf, sizeof(buf)));

    if (!m_config_retriever.is_connected())
    {
      int ret = m_config_retriever.do_connect(30 /* retry */,
        1 /* delay */,
        0 /* verbose */);
      if (ret == 0)
      {
        //connection success
        g_eventLogger->info("Connected to '%s:%d'...",
          m_config_retriever.get_mgmd_host(),
          m_config_retriever.get_mgmd_port());
        break;
      }
      else if (ret == -2)
      {
        //premanent error, return without re-try
        g_eventLogger->error("%s", m_config_retriever.getErrorString());
        DBUG_RETURN(NULL);
      }
    }
    else
    {
      g_eventLogger->info("Connected to '%s:%d'...",
        m_config_retriever.get_mgmd_host(),
        m_config_retriever.get_mgmd_port());
      break;
    }
  }
  // read config from other management server
  ndb_mgm::config_ptr conf =
    m_config_retriever.getConfig(m_config_retriever.get_mgmHandle());

  // Disconnect from other mgmd
  m_config_retriever.disconnect();

  if (!conf) {
    g_eventLogger->error("%s", m_config_retriever.getErrorString());
    DBUG_RETURN(NULL);
  }

  DBUG_RETURN(new Config(conf.release()));
}


static bool
delete_file(const char* file_name)
{
#ifdef _WIN32
  if (DeleteFile(file_name) == 0)
  {
    g_eventLogger->error("Failed to delete file '%s', error: %d",
                         file_name, GetLastError());
    return false;
  }
#else
  if (unlink(file_name) == -1)
  {
    g_eventLogger->error("Failed to delete file '%s', error: %d",
                         file_name, errno);
    return false;
  }
#endif
  return true;
}


bool
ConfigManager::delete_saved_configs(void) const
{
  NdbDir::Iterator iter;

  if (!m_configdir)
  {
    // No configdir -> no files to delete
    return true;
  }

  if (iter.open(m_configdir) != 0)
    return false;

  bool result = true;
  const char* name;
  unsigned nodeid;
  char extra; // Avoid matching ndb_2_config.bin.2.tmp
  BaseString full_name;
  unsigned version;
  while ((name= iter.next_file()) != NULL)
  {
    if (sscanf(name,
               "ndb_%u_config.bin.%u%c",
               &nodeid, &version, &extra) == 2)
    {
      // ndbout_c("match: %s", name);

      if (nodeid != m_node_id)
        continue;

      // Delete the file
      full_name.assfmt("%s%s%s", m_configdir, DIR_SEPARATOR, name);
      g_eventLogger->debug("Deleting binary config file '%s'",
                           full_name.c_str());
      if (!delete_file(full_name.c_str()))
      {
        // Make function return false, but continue and try
        // to delete other files
        result = false;
      }
    }
  }

  return result;
}


bool
ConfigManager::saved_config_exists(BaseString& config_name) const
{
  NdbDir::Iterator iter;

  if (!m_configdir ||
      iter.open(m_configdir) != 0)
    return 0;

  const char* name;
  unsigned nodeid;
  char extra; // Avoid matching ndb_2_config.bin.2.tmp
  unsigned version, max_version= 0;
  while ((name= iter.next_file()) != NULL)
  {
    if (sscanf(name,
               "ndb_%u_config.bin.%u%c",
               &nodeid, &version, &extra) == 2)
    {
      // ndbout_c("match: %s", name);

      if (nodeid != m_node_id)
        continue;

      if (version>max_version)
        max_version= version;
    }
  }

  if (max_version == 0)
    return false;

  config_name.assfmt("%s%sndb_%u_config.bin.%u",
                     m_configdir, DIR_SEPARATOR, m_node_id, max_version);
  return true;
}



bool
ConfigManager::failed_config_change_exists() const
{
  NdbDir::Iterator iter;

  if (!m_configdir ||
      iter.open(m_configdir) != 0)
    return 0;

  const char* name;
  char tmp;
  unsigned nodeid;
  unsigned version;
  while ((name= iter.next_file()) != NULL)
  {
    // Check for a previously failed config
    // change, ie. ndb_<nodeid>_config.bin.X.tmp exist
    if (sscanf(name,
               "ndb_%u_config.bin.%u.tm%c",
               &nodeid, &version, &tmp) == 3 &&
        tmp == 'p')
    {
      if (nodeid != m_node_id)
        continue;

      g_eventLogger->error("Found binary configuration file '%s%s%s' from "
                           "previous failed attempt to change config. This "
                           "error must be manually resolved by removing the "
                           "file(ie. ROLLBACK) or renaming the file to it's "
                           "name without the .tmp extension(ie COMMIT). Make "
                           "sure to check the other nodes so that they all "
                           "have the same configuration generation.",
                           m_configdir, DIR_SEPARATOR, name);
      return true;
    }
  }

  return false;
}


Config*
ConfigManager::load_saved_config(const BaseString& config_name)
{
  ndb_mgm::config_ptr retrieved_config =
      m_config_retriever.getConfig(config_name.c_str());
  if(!retrieved_config)
  {
    g_eventLogger->error("Failed to load config from '%s', error: '%s'",
                         config_name.c_str(),
                         m_config_retriever.getErrorString());
    return NULL;
  }

  Config* conf = new Config(retrieved_config.release());
  if (!conf)
    g_eventLogger->error("Failed to load config, out of memory");
  return conf;
}

bool
ConfigManager::get_packed_config(ndb_mgm_node_type nodetype,
                                 BaseString* buf64,
                                 BaseString& error,
                                 bool v2,
                                 Uint32 node_id)
{
  Guard g(m_config_mutex);

  /*
    Only allow the config to be exported if it's been confirmed
    or if another mgmd is asking for it
  */
  switch(m_config_state)
  {
  case CS_INITIAL:
    if (nodetype == NDB_MGM_NODE_TYPE_MGM)
      ; // allow other mgmd to fetch initial configuration
    else
    {
      error.assign("The cluster configuration is not yet confirmed "
                   "by all defined management servers. ");
      if (m_config_change.m_state != ConfigChangeState::IDLE)
      {
        error.append("Initial configuration change is in progress.");
      }
      else
      {
        NodeBitmask not_started(m_all_mgm);
        not_started.bitANDC(m_checked);
        error.append("This management server is still waiting for node ");
        error.append(BaseString::getPrettyText(not_started));
        error.append(" to connect.");
      }
      return false;
    }
    break;

  case CS_CONFIRMED:
    // OK
    break;

  default:
    error.assign("get_packed_config, unknown config state: %d",
                 m_config_state);
     return false;
    break;

  }

  require(m_config != 0);
  if (buf64)
  {
    if (v2)
    {
      if (!m_packed_config_v2.length())
      {
        // No packed config exist, generate a new one
        Config config_copy(m_config);
        if (!m_dynamic_ports.set_in_config(&config_copy))
        {
          error.assign("get_packed_config, failed to set dynamic ports in config");
          return false;
        }
        if (!config_copy.pack64_v2(m_packed_config_v2))
        {
          error.assign("get_packed_config, failed to pack config_copy");
          return false;
        }
      }
      if (node_id != 0)
      {
        NodeBitmask all_mgm;
        m_config->get_nodemask(all_mgm, NDB_MGM_NODE_TYPE_MGM);
        if (all_mgm.get(node_id) == false)
        {
          BaseString tmp;
          Config config_copy(m_config);
          if (config_copy.pack64_v2(tmp, node_id))
          {
            buf64->assign(tmp, tmp.length());
            return true;
          }
        }
      }
      buf64->assign(m_packed_config_v2, m_packed_config_v2.length());
    }
    else
    {
      if (!m_packed_config_v1.length())
      {
        // No packed config exist, generate a new one
        Config config_copy(m_config);
        if (!m_dynamic_ports.set_in_config(&config_copy))
        {
          error.assign("get_packed_config, failed to set dynamic ports in config");
          return false;
        }
        if (!config_copy.pack64_v1(m_packed_config_v1))
        {
          error.assign("get_packed_config, failed to pack config_copy");
          return false;
        }
      }
      buf64->assign(m_packed_config_v1, m_packed_config_v1.length());
    }
  }
  return true;
}

static bool
check_dynamic_port_configured(const Config* config,
                              int node1, int node2,
                              BaseString& msg)
{
  ConfigIter iter(config, CFG_SECTION_CONNECTION);

  for(;iter.valid();iter.next()) {
    Uint32 n1, n2;
    if (iter.get(CFG_CONNECTION_NODE_1, &n1) != 0 ||
        iter.get(CFG_CONNECTION_NODE_2, &n2) != 0)
    {
      msg.assign("Could not get node1 or node2 from connection section");
      return false;
    }

    if((n1 == (Uint32)node1 && n2 == (Uint32)node2) ||
       (n1 == (Uint32)node2 && n2 == (Uint32)node1))
      break;
  }
  if(!iter.valid()) {
    msg.assfmt("Unable to find connection between nodes %d -> %d",
               node1, node2);
    return false;
  }

  Uint32 port;
  if(iter.get(CFG_CONNECTION_SERVER_PORT, &port) != 0) {
    msg.assign("Unable to get current value of CFG_CONNECTION_SERVER_PORT");
    return false;
  }

  if (port != 0)
  {
    // Dynamic ports is zero in configuration
    msg.assfmt("Server port for %d -> %d is not marked as dynamic, value: %u",
               node1, node2, port);
    return false;
  }
  return true;
}


bool
ConfigManager::set_dynamic_port(int node1, int node2, int value,
                                BaseString& msg)
{
  MgmtSrvr::DynPortSpec port = { node2, value };

  return set_dynamic_ports(node1, &port, 1, msg);
}


bool
ConfigManager::set_dynamic_ports(int node, MgmtSrvr::DynPortSpec ports[],
                                 unsigned num_ports, BaseString &msg)
{
  Guard g(m_config_mutex);

  // Check that all ports to set are configured as dynamic
  for(unsigned i = 0; i < num_ports; i++)
  {
    const int node2 = ports[i].node;
    if (!check_dynamic_port_configured(m_config,
                                       node, node2, msg))
    {
      return false;
    }
  }

  // Set the dynamic ports
  bool result = true;
  for(unsigned i = 0; i < num_ports; i++)
  {
    const int node2 = ports[i].node;
    const int value = ports[i].port;
    if (!m_dynamic_ports.set(node, node2, value))
    {
      // Failed to set one port, report problem but since it's very unlikely
      // that this step fails, continue and attempt to set remaining ports.
      msg.assfmt("Failed to set dynamic port(s)");
      result =  false;
    }
  }

  // Removed cache of packed config, need to be recreated
  // to include the new dynamic port
  m_packed_config_v1.clear();
  m_packed_config_v2.clear();

  return result;
}


bool
ConfigManager::get_dynamic_port(int node1, int node2, int *value,
                                BaseString& msg) const {

  Guard g(m_config_mutex);
  if (!check_dynamic_port_configured(m_config,
                                     node1, node2, msg))
    return false;

  if (!m_dynamic_ports.get(node1, node2, value))
  {
    msg.assfmt("Could not get dynamic port for %d -> %d", node1, node2);
    return false;
  }
  return true;
}


bool ConfigManager::DynamicPorts::check(int& node1, int& node2) const
{
  // Always use smaller node first
  if (node1 > node2)
  {
    int tmp = node1;
    node1 = node2;
    node2 = tmp;
  }

  // Only NDB nodes can be dynamic port server
  if (node1 <= 0 || node1 >= MAX_NDB_NODES)
    return false;
  if (node2 <= 0 || node2 >= MAX_NODES)
    return false;
  if (node1 == node2)
    return false;

  return true;
}


bool ConfigManager::DynamicPorts::set(int node1, int node2, int port)
{
  if (!check(node1, node2))
    return false;

  if (!m_ports.insert(NodePair(node1, node2), port, true))
    return false;

  return true;
}


bool ConfigManager::DynamicPorts::get(int node1, int node2, int* port) const
{
  if (!check(node1, node2))
    return false;

  int value = 0; // Return 0 if not found
  (void)m_ports.search(NodePair(node1, node2), value);

  *port = (int)value;
  return true;
}


bool
ConfigManager::DynamicPorts::set_in_config(Config* config)
{
  bool result = true;
  ConfigIter iter(config, CFG_SECTION_CONNECTION);

  for(;iter.valid();iter.next()) {
    Uint32 port = 0;
    if (iter.get(CFG_CONNECTION_SERVER_PORT, &port) != 0 ||
        port != 0)
      continue; // Not configured as dynamic port

    Uint32 n1, n2;
    require(iter.get(CFG_CONNECTION_NODE_1, &n1) == 0);
    require(iter.get(CFG_CONNECTION_NODE_2, &n2) == 0);

    int dyn_port;
    if (!get(n1, n2, &dyn_port) || dyn_port == 0)
      continue; // No dynamic port registered

    // Write the dynamic port to config
    port = (Uint32)dyn_port;
    ConfigValues::Iterator i2(config->m_configuration->m_config_values,
                              iter.m_config);
    if(i2.set(CFG_CONNECTION_SERVER_PORT, port) == false)
      result = false;
  }
  return result;
}


template class Vector<ConfigSubscriber*>;
