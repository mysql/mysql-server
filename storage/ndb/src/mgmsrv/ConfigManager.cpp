/* Copyright (C) 2008 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "ConfigManager.hpp"
#include "MgmtSrvr.hpp"
#include "DirIterator.hpp"

#include <NdbConfig.h>

#include <SignalSender.hpp>
#include <NdbApiSignal.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <ndb_version.h>


#ifdef VM_TRACE
#define require(v)  assert(v)
#else
static void
require(bool v)
{
  if (!v)
    abort();
}
#endif

extern "C" const char* opt_connect_str;

ConfigManager::ConfigManager(const MgmtSrvr::MgmtOpts& opts,
                             const char* configdir) :
  MgmtThread("ConfigManager"),
  m_opts(opts),
  m_facade(NULL),
  m_config_mutex(NULL),
  m_config(NULL),
  m_new_config(NULL),
  m_config_retriever(opt_connect_str,
                     NDB_VERSION,
                     NDB_MGM_NODE_TYPE_MGM,
                     opts.bind_address),
  m_config_change_state(CCS_IDLE),
  m_config_state(CS_UNINITIALIZED),
  m_previous_state(CS_UNINITIALIZED),
  m_config_change_error(ConfigChangeRef::OK),
  m_client_ref(RNIL),
  m_prepared_config(NULL),
  m_node_id(0),
  m_configdir(configdir)
{
}


ConfigManager::~ConfigManager()
{
  delete m_config;
  delete m_new_config;
  delete m_prepared_config;
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
      continue;

    Uint32 nodeid;
    if(iter.get(CFG_NODE_ID, &nodeid) ||
       nodeid == own_nodeid)
      continue;

    const char * hostname;
    if(iter.get(CFG_NODE_HOST, &hostname))
      continue;

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
  DirIterator iter;

  if (iter.open(m_configdir) != 0)
    return 0;

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
          return 0; // Found more than one nodeid
        found_nodeid= nodeid;
      }

      if (version > max_version)
        max_version = version;
    }
  }

  if (max_version == 0)
    return 0;

  config_name.assfmt("%s%sndb_%u_config.bin.%u",
                     m_configdir, DIR_SEPARATOR, found_nodeid, max_version);

  Config* conf;
  if (!(conf = load_saved_config(config_name)))
    return 0;

  if (!m_config_retriever.verifyConfig(conf->m_configValues,
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
  for (iter.first(); iter.valid(); iter.next())
  {
    Uint32 type;
    if(iter.get(CFG_TYPE_OF_SECTION, &type) ||
       type != NDB_MGM_NODE_TYPE_MGM)
      continue;

    Uint32 nodeid;
    require(iter.get(CFG_NODE_ID, &nodeid) == 0);

    const char * hostname;
    if(iter.get(CFG_NODE_HOST, &hostname))
      continue;

    if (SocketServer::tryBind(0,hostname))
    {
      // This node is setup to run on this host
      if (found_nodeid == 0)
        found_nodeid = nodeid;
      else
        return 0; // More than one host on this node
    }
  }
  return found_nodeid;
}


NodeId
ConfigManager::find_nodeid_from_config(void)
{
  if (!m_opts.mycnf &&
      !m_opts.config_filename)
    return 0;

  Config* conf = load_config();
  if (conf == NULL)
    return 0;

  NodeId found_nodeid = find_own_nodeid(conf);
  if (found_nodeid == 0 ||
      !m_config_retriever.verifyConfig(conf->m_configValues, found_nodeid))
  {
    delete conf;
    return 0;
  }

  return found_nodeid;
}


bool
ConfigManager::init_nodeid(void)
{
  DBUG_ENTER("ConfigManager::init_nodeid");

  NodeId nodeid = m_config_retriever.get_configuration_nodeid();
  if (nodeid)
  {
    // Nodeid was specifed on command line or in NDB_CONNECTSTRING
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
    g_eventLogger->error(m_config_retriever.getErrorString());
    DBUG_RETURN(false);
  }

  if (!init_nodeid())
    DBUG_RETURN(false);

  if (m_opts.initial && !delete_saved_configs())
    DBUG_RETURN(false);

  if (failed_config_change_exists())
    DBUG_RETURN(false);

  BaseString config_bin_name;
  if (saved_config_exists(config_bin_name))
  {
    Config* conf = NULL;
    if (!(conf = load_saved_config(config_bin_name)))
      DBUG_RETURN(false);
    g_eventLogger->info("Loaded config from '%s'", config_bin_name.c_str());

    if (!config_ok(conf))
      DBUG_RETURN(false);

    set_config(conf);
    m_config_state = CS_CONFIRMED;

    if (m_opts.reload && // --reload
        (m_opts.mycnf || m_opts.config_filename))
    {
      Config* new_conf = load_config();
      if (new_conf == NULL)
        DBUG_RETURN(false);


      /* Copy the necessary values from old to new config */
      if (!new_conf->setGeneration(m_config->getGeneration()))
      {
        g_eventLogger->error("Failed to copy generation from old config");
        DBUG_RETURN(false);
      }

      if (!new_conf->setName(m_config->getName()))
      {
        g_eventLogger->error("Failed to copy name from old config");
        DBUG_RETURN(false);
      }

      if (!new_conf->setPrimaryMgmNode(m_config->getPrimaryMgmNode()))
      {
        g_eventLogger->error("Failed to copy primary mgm node from old config");
        DBUG_RETURN(false);
      }

      /* Check if config has changed */
      if (!m_config->equal(new_conf))
      {
        /* Loaded config is different */
        BaseString buf;
        g_eventLogger->info("Detected change of %s on disk, will try to " \
                            "set it when all ndb_mgmd(s) started. "     \
                            "This is the actual diff:\n%s",
                            m_opts.mycnf ? "my.cnf" : m_opts.config_filename,
                            m_config->diff2str(new_conf, buf));
        m_new_config= new_conf;
      }
      else
      {
        /* Loaded config was equal to current */
        g_eventLogger->info("Config equal!");
        delete new_conf;
      }
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
      m_new_config = new Config(conf); // Copy config
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

      if (!config_ok(conf))
        DBUG_RETURN(false);

      /* Use the fetched config for now */
      set_config(conf);
      m_new_config = new Config(conf); // Copy config

      if (m_config->getGeneration() == 0)
      {
        g_eventLogger->info("Fetched initial configuration, " \
                            "generation: %d, name: '%s'. "\
                            "Will try to set it when all ndb_mgmd(s) started",
                            m_config->getGeneration(), m_config->getName());
        m_config_state= CS_INITIAL;
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
  if(!config->pack(buf))
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

#ifdef __WIN__
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
  require(m_prepared_config);

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


void
ConfigManager::set_config(Config* new_config)
{
  delete m_config;
  m_config = new_config;

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
  if (!m_config_retriever.verifyConfig(conf->m_configValues, m_node_id))
  {
    g_eventLogger->error(m_config_retriever.getErrorString());
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

  Guard g(m_config_mutex);

  switch(req->requestType){
  case ConfigChangeImplReq::Prepare:{
    if (sig->header.m_noOfSections != 1)
    {
      sendConfigChangeImplRef(ss, nodeId, ConfigChangeRef::NoConfigData);
      return;
    }

    ConfigValuesFactory cf;
    if (!cf.unpack(sig->ptr[0].p, req->length))
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
      {
        Config new_config_copy(&new_config);
        require(new_config_copy.setName(new_name));
        unsigned exclude[]= {CFG_SECTION_SYSTEM, 0};
        if (!new_config_copy.equal(m_new_config, exclude))
        {
          BaseString buf;
          g_eventLogger->warning("Refusing to start initial config "    \
                                 "change when nodes have different "    \
                                 "config\n"                             \
                                 "This is the actual diff:\n%s",
                                 new_config_copy.diff2str(m_new_config, buf));
          sendConfigChangeImplRef(ss, nodeId,
                                  ConfigChangeRef::DifferentInitial);
          return;
        }
      }

      /*
         Scrap the m_new_config, it's been used to check that other node
         started from equal initial config, now it's not needed anymore
      */
      delete m_new_config;
      m_new_config = NULL;

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
  if (m_config_change_error == 0)
    m_config_change_error = (ConfigChangeRef::ErrorCode)ref->errorCode;

  switch(m_config_change_state){

  case CCS_PREPARING:{
    /* Got ref while preparing */
    m_config_change_state = CCS_ABORT;
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    /* Abort all other nodes */
    SimpleSignal ssig;
    ConfigChangeImplReq* const req =
      CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());
    req->requestType = ConfigChangeImplReq::Abort;

    g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(abort) to node %d",
                         nodeId);
    require(m_waiting_for.isclear());
    m_waiting_for = ss.broadcastSignal(m_all_mgm, ssig,
                                  MGM_CONFIG_MAN,
                                  GSN_CONFIG_CHANGE_IMPL_REQ,
                                  ConfigChangeImplReq::SignalLength);
    if (m_waiting_for.isclear())
      m_config_change_state = CCS_IDLE;
    else
      m_config_change_state = CCS_ABORTING;
    break;
  }

  case CCS_COMITTING:
    /* Got ref while comitting, impossible */
    abort();
    break;

  case CCS_ABORT:{
    /* Got ref(another) while already decided to abort */
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    /* Abort all other nodes */
    SimpleSignal ssig;
    ConfigChangeImplReq* const req =
      CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());
    req->requestType = ConfigChangeImplReq::Abort;

    g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(abort) to node %d",
                         nodeId);
    require(m_waiting_for.isclear());
    m_waiting_for = ss.broadcastSignal(m_all_mgm, ssig,
                                  MGM_CONFIG_MAN,
                                  GSN_CONFIG_CHANGE_IMPL_REQ,
                                  ConfigChangeImplReq::SignalLength);
    if (m_waiting_for.isclear())
      m_config_change_state = CCS_IDLE;
    else
      m_config_change_state = CCS_ABORTING;
    break;
  }

  case CCS_ABORTING:
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

  switch(m_config_change_state){
  case CCS_PREPARING:{
    require(conf->requestType == ConfigChangeImplReq::Prepare);
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    /* Send commit to all nodes */
    SimpleSignal ssig;
    ConfigChangeImplReq* const req =
      CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());

    req->requestType = ConfigChangeImplReq::Commit;

    g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(commit)");
    require(m_waiting_for.isclear());
    m_waiting_for = ss.broadcastSignal(m_all_mgm, ssig,
                                  MGM_CONFIG_MAN,
                                  GSN_CONFIG_CHANGE_IMPL_REQ,
                                  ConfigChangeImplReq::SignalLength);
    if (m_waiting_for.isclear())
      m_config_change_state = CCS_IDLE;
    else
      m_config_change_state = CCS_COMITTING;
    break;
  }

  case CCS_COMITTING:{
    require(conf->requestType == ConfigChangeImplReq::Commit);

    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    require(m_client_ref != RNIL);
    require(m_config_change_error == 0);
    if (m_client_ref == ss.getOwnRef())
    {
      g_eventLogger->info("Config change completed! New generation: %d",
                          m_config->getGeneration());
    }
    else
    {
      /* Send CONF to requestor */
      sendConfigChangeConf(ss, m_client_ref);
    }
    m_client_ref = RNIL;
    m_config_change_state = CCS_IDLE;
    break;
  }

  case CCS_ABORT:{
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    /* Abort all other nodes */
    SimpleSignal ssig;
    ConfigChangeImplReq* const req =
      CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());
    req->requestType = ConfigChangeImplReq::Abort;

    g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(abort)");
    require(m_waiting_for.isclear());
    m_waiting_for = ss.broadcastSignal(m_all_mgm, ssig,
                                  MGM_CONFIG_MAN,
                                  GSN_CONFIG_CHANGE_IMPL_REQ,
                                  ConfigChangeImplReq::SignalLength);
    if (m_waiting_for.isclear())
        m_config_change_state = CCS_IDLE;
    else
      m_config_change_state = CCS_ABORTING;
    break;
  }

  case CCS_ABORTING:{
    m_waiting_for.clear(nodeId);
    if (!m_waiting_for.isclear())
      return;

    require(m_client_ref != RNIL);
    require(m_config_change_error);
    if (m_client_ref == ss.getOwnRef())
    {
      g_eventLogger->error("Config change failed!");
      exit(1);
    }
    else
    {
      /* Send ref to the requestor */
      sendConfigChangeRef(ss, m_client_ref,
                          m_config_change_error);
    }
    m_config_change_error= ConfigChangeRef::OK;
    m_client_ref = RNIL;
    m_config_change_state = CCS_IDLE;
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
ConfigManager::startInitConfigChange(SignalSender& ss)
{
  require(m_config_state == CS_INITIAL);
  g_eventLogger->info("Starting initial configuration change");
  m_client_ref = ss.getOwnRef();
  sendConfigChangeImplReq(ss, m_new_config);
}


void
ConfigManager::startNewConfigChange(SignalSender& ss)
{
  require(m_config_state == CS_CONFIRMED);
  g_eventLogger->info("Starting configuration change, generation: %d",
                      m_new_config->getGeneration());
  m_client_ref = ss.getOwnRef();
  sendConfigChangeImplReq(ss, m_new_config);

  /* The new config has been sent and can now be discarded */
  delete m_new_config;
  m_new_config = NULL;
}


void
ConfigManager::sendConfigChangeImplReq(SignalSender& ss, const Config* conf)
{
  require(m_client_ref != RNIL);

  /* Send prepare to all MGM nodes */
  SimpleSignal ssig;

  UtilBuffer buf;
  conf->pack(buf);
  ssig.ptr[0].p = (Uint32*)buf.get_data();
  ssig.ptr[0].sz = (buf.length() + 3) / 4;
  ssig.header.m_noOfSections = 1;

  ConfigChangeImplReq* const req =
    CAST_PTR(ConfigChangeImplReq, ssig.getDataPtrSend());
  req->requestType = ConfigChangeImplReq::Prepare;
  req->initial = (m_config_state == CS_INITIAL);
  req->length = buf.length();

  g_eventLogger->debug("Sending CONFIG_CHANGE_IMPL_REQ(prepare)");

  require(m_waiting_for.isclear());
  m_waiting_for = ss.broadcastSignal(m_all_mgm, ssig,
                                MGM_CONFIG_MAN,
                                GSN_CONFIG_CHANGE_IMPL_REQ,
                                ConfigChangeImplReq::SignalLength);
  if (!m_waiting_for.isclear())
    m_config_change_state = CCS_ABORT;
  else
    require(m_config_change_state == CCS_IDLE);
  m_config_change_state = CCS_PREPARING;
}


void
ConfigManager::execCONFIG_CHANGE_REQ(SignalSender& ss, SimpleSignal* sig)
{
  BlockReference from = sig->header.theSendersBlockRef;
  const ConfigChangeReq * const req =
    CAST_CONSTPTR(ConfigChangeReq, sig->getDataPtr());

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

  if (m_config_change_state != CCS_IDLE)
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::ConfigChangeOnGoing);
    return;
  }
  require(m_config_change_error == ConfigChangeRef::OK);

  if (sig->header.m_noOfSections != 1)
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::NoConfigData);
    return;
  }

  ConfigValuesFactory cf;
  if (!cf.unpack(sig->ptr[0].p, req->length))
  {
    sendConfigChangeRef(ss, from, ConfigChangeRef::FailedToUnpack);
    return;
  }

  Config new_config(cf.getConfigValues());
  if (!config_ok(&new_config))
  {
    g_eventLogger->warning("Refusing to start config change, the config "\
                           "is not ok");
    sendConfigChangeRef(ss, from, ConfigChangeRef::ConfigNotOk);
    return;
  }

  m_client_ref = from;
  sendConfigChangeImplReq(ss, &new_config);
  return;
}


void
ConfigManager::execCONFIG_CHECK_REQ(SignalSender& ss, SimpleSignal* sig)
{
  Guard g(m_config_mutex);
  BlockReference from = sig->header.theSendersBlockRef;
  NodeId nodeId = refToNode(from);
  const ConfigCheckReq * const req =
    CAST_CONSTPTR(ConfigCheckReq, sig->getDataPtr());

  Uint32 other_generation = req->generation;
  ConfigState other_state = (ConfigState)req->state;

  Uint32 generation = m_config->getGeneration();

  g_eventLogger->debug("Got CONFIG_CHECK_REQ from node: %d. "   \
                       "generation: %d, other_generation: %d, " \
                       "our state: %d, other state: %d",
                       generation, other_generation,
                       m_config_state, other_state, nodeId);

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
      g_eventLogger->error("Other node are not in INITIAL");
      exit(1);
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
      ;// OK
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

  BaseString buf("Sending CONFIG_CHECK_REQ to node(s) ");
  unsigned i = 0;
  while((i = to.find(i+1)) != NodeBitmask::NotFound)
    buf.appfmt("%d ", i);
  g_eventLogger->debug(buf);

  require(m_waiting_for.isclear());
  m_waiting_for = ss.broadcastSignal(to, ssig, MGM_CONFIG_MAN,
                                GSN_CONFIG_CHECK_REQ,
                                ConfigCheckReq::SignalLength);
}


void
ConfigManager::sendConfigCheckRef(SignalSender& ss, BlockReference to,
                                  ConfigCheckRef::ErrorCode error,
                                  Uint32 generation,
                                  Uint32 other_generation,
                                  ConfigState state,
                                  ConfigState other_state) const
{
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

  ss.sendSignal(nodeId, ssig, MGM_CONFIG_MAN,
                GSN_CONFIG_CHECK_REF, ConfigCheckReq::SignalLength);
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
                GSN_CONFIG_CHECK_CONF, ConfigCheckReq::SignalLength);
}


void
ConfigManager::execCONFIG_CHECK_CONF(SignalSender& ss, SimpleSignal* sig)
{
  BlockReference from = sig->header.theSendersBlockRef;
  NodeId nodeId = refToNode(from);
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

  const ConfigCheckRef* const ref =
    CAST_CONSTPTR(ConfigCheckRef, sig->getDataPtr());

  g_eventLogger->error("Got CONFIG_CHECK_REF from node %d, "   \
                       "error: %d, message: '%s'\n"            \
                       "generation: %d, expected generation: %d\n"\
                       "state: %d, expected state: %d",
                       nodeId, ref->error,
                       ConfigCheckRef::errorMessage(ref->error),
                       ref->generation, ref->expected_generation,
                       ref->state, ref->expected_state);
  exit(1);
}


void
ConfigManager::run()
{
  assert(m_facade);
  SignalSender ss(m_facade, MGM_CONFIG_MAN);
  ss.lock();

  ss.getNodes(m_all_mgm, NodeInfo::MGM);

  m_started.set(m_facade->ownId());

  while (!is_stopped())
  {

    if (m_config_change_state == CCS_IDLE)
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

        if (m_new_config &&                     // Updated config.ini was found
            m_started.equal(m_all_mgm) &&       // All mgmd started
            m_checked.equal(m_started) &&       // All nodes checked
            m_all_mgm.find(0) == m_facade->ownId()) // Lowest nodeid
        {
            startInitConfigChange(ss);
        }
        break;

      case CS_CONFIRMED:
        if (print_state)
          ndbout_c("==CONFIRMED==");

        if (m_new_config &&                 // Updated config.ini was found
            m_started.equal(m_all_mgm) &&   // All mgmd started
            m_checked.equal(m_started))     // All nodes checked
        {
          startNewConfigChange(ss);
        }

        break;

      default:
        break;
      }

      // Send CHECK_CONFIG to all nodes not yet checked
      if (m_waiting_for.isclear() &&    // Nothing else ongoing
          !m_checked.equal(m_started))  // Some nodes have not been checked
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
      const NFCompleteRep * const rep =
        CAST_CONSTPTR(NFCompleteRep, sig->getDataPtr());
      NodeId nodeId= rep->failedNodeId;

      if (m_all_mgm.get(nodeId)) // Not mgm node
        break;

      ndbout_c("Node %d failed", nodeId);
      m_started.clear(nodeId);
      m_checked.clear(nodeId);

      if (m_config_change_state != CCS_IDLE)
      {
        g_eventLogger->info("Node %d failed during config change!!",
                            nodeId);
        // TODO start take over of config change protocol
      }
      break;
    }

    case GSN_NODE_FAILREP:
      // ignore, NF_COMPLETEREP will come
      break;

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
}


#include "InitConfigFileParser.hpp"

Config*
ConfigManager::load_init_config(const char* config_filename)
{
   InitConfigFileParser parser;
   g_eventLogger->info("Reading cluster configuration from '%s'",
                       config_filename);
  return parser.parseConfig(config_filename);
}


Config*
ConfigManager::load_init_mycnf(void)
{
  InitConfigFileParser parser;
  g_eventLogger->info("Reading cluster configuration using my.cnf");
  return parser.parse_mycnf();
}


Config*
ConfigManager::load_config(const char* config_filename, bool mycnf,
                           BaseString& msg)
{
  Config* new_conf = NULL;
  if (mycnf && (new_conf = load_init_mycnf()) == NULL)
  {
    msg.assign("Could not load configuration from 'my.cnf'");
    return NULL;
  }
  else if (config_filename &&
           (new_conf = load_init_config(config_filename)) == NULL)
  {
    msg.assfmt("Could not load configuration from '%s'",
               config_filename);
    return NULL;
  }

  return new_conf;
}


Config*
ConfigManager::load_config(void) const
{
  BaseString msg;
  Config* new_conf = NULL;
  if ((new_conf = load_config(m_opts.config_filename,
                              m_opts.mycnf, msg)) == NULL)
  {
    g_eventLogger->error(msg);
    return NULL;
  }
  return new_conf;
}


Config*
ConfigManager::fetch_config(void)
{
  DBUG_ENTER("ConfigManager::fetch_config");

  /* Loop until config loaded from other mgmd(s) */
  char buf[128];
  g_eventLogger->info("Trying to get configuration from other mgmd(s) "\
                      "using '%s'...",
                      m_config_retriever.get_connectstring(buf, sizeof(buf)));

  if (!m_config_retriever.is_connected())
  {

    if (m_config_retriever.do_connect(-1 /* retry */,
                                      1 /* delay */,
                                      0 /* verbose */) != 0)
    {
      g_eventLogger->error("INTERNAL ERROR: Inifinite wait for connect " \
                           "returned!");
      abort();
    }
    g_eventLogger->info("Connected...");

  }

  // read config from other management server
  ndb_mgm_configuration * tmp =
    m_config_retriever.getConfig(m_config_retriever.get_mgmHandle());
  if (tmp == NULL) {
    g_eventLogger->error(m_config_retriever.getErrorString());
    DBUG_RETURN(false);
  }

  DBUG_RETURN(new Config(tmp));
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
  DirIterator iter;

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
  DirIterator iter;

  if (iter.open(m_configdir) != 0)
    return false;

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
  DirIterator iter;

  if (iter.open(m_configdir) != 0)
    return false;

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
  struct ndb_mgm_configuration * tmp =
    m_config_retriever.getConfig(config_name.c_str());
  if(tmp == NULL)
  {
    g_eventLogger->error("Failed to load config from '%s', error: '%s'",
                         config_name.c_str(),
                         m_config_retriever.getErrorString());
    return NULL;
  }

  Config* conf = new Config(tmp);
  if (conf == NULL)
    g_eventLogger->error("Failed to load config, out of memory");
  return conf;
}


bool
ConfigManager::get_packed_config(UtilBuffer& pack_buf)
{
  Guard g(m_config_mutex);

  /* Only allow the config to be exported if it's been confirmed */
  if (m_config_state != CS_CONFIRMED)
    return false;

  require(m_config);
  return m_config->pack(pack_buf);
}


template class Vector<ConfigSubscriber*>;

