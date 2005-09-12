/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/TestOrd.hpp>
#include <OutputStream.hpp>

#include "MgmtSrvr.hpp"
#include "SignalQueue.hpp"
#include <InitConfigFileParser.hpp>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>

#if 0 // code must be rewritten to use SignalSender

void
MgmtSrvr::handle_MGM_LOCK_CONFIG_REQ(NdbApiSignal *signal) {
  NodeId sender = refToNode(signal->theSendersBlockRef);
  const MgmLockConfigReq * const req = CAST_CONSTPTR(MgmLockConfigReq, signal->getDataPtr());

  NdbApiSignal *reply = getSignal();
  if(signal == NULL)
    return; /** @todo handle allocation failure */

  reply->set(TestOrd::TraceAPI,
	      MGMSRV,
	      GSN_MGM_LOCK_CONFIG_REP,
	      MgmLockConfigRep::SignalLength);

  MgmLockConfigRep *lockRep = CAST_PTR(MgmLockConfigRep, reply->getDataPtrSend());

  lockRep->errorCode = MgmLockConfigRep::UNKNOWN_ERROR;

  if(req->newConfigGeneration < m_nextConfigGenerationNumber) {
    lockRep->errorCode = MgmLockConfigRep::GENERATION_MISMATCH;
    goto done;
  }
  NdbMutex_Lock(m_configMutex);

  m_nextConfigGenerationNumber = req->newConfigGeneration+1;

  lockRep->errorCode = MgmLockConfigRep::OK;

 done:
  sendSignal(sender, NO_WAIT, reply, true);
  NdbMutex_Unlock(m_configMutex);
  return;
}

void
MgmtSrvr::handle_MGM_UNLOCK_CONFIG_REQ(NdbApiSignal *signal) {
  NodeId sender = refToNode(signal->theSendersBlockRef);
  const MgmUnlockConfigReq * const req = CAST_CONSTPTR(MgmUnlockConfigReq, signal->getDataPtr());
  MgmUnlockConfigRep *unlockRep;

  NdbApiSignal *reply = getSignal();
  if(signal == NULL)
    goto error; /** @todo handle allocation failure */

  reply->set(TestOrd::TraceAPI,
	     MGMSRV,
	     GSN_MGM_UNLOCK_CONFIG_REP,
	     MgmUnlockConfigRep::SignalLength);

  unlockRep = CAST_PTR(MgmUnlockConfigRep,  reply->getDataPtrSend());

  unlockRep->errorCode = MgmUnlockConfigRep::UNKNOWN_ERROR;


  NdbMutex_Lock(m_configMutex);

  if(req->commitConfig == 1) {
    m_newConfig = fetchConfig();
    commitConfig();
  } else
    rollbackConfig();
  
  unlockRep->errorCode = MgmUnlockConfigRep::OK;

  sendSignal(sender, NO_WAIT, reply, true);
 error:
  NdbMutex_Unlock(m_configMutex);
  return;
}


/**
 * Prepare all MGM nodes for configuration changes
 * 
 * @returns 0 on success, or -1 on failure
 */
int
MgmtSrvr::lockConf() {
  int result = -1;
  MgmLockConfigReq* lockReq;
  NodeId node = 0;

  /* Check if this is the master node */
  if(getPrimaryNode() != _ownNodeId)
    goto done;

  if(NdbMutex_Trylock(m_configMutex) != 0)
    return -1;

  m_newConfig = new Config(*_config); /* copy the existing config */
  _config = m_newConfig;
  
  m_newConfig = new Config(*_config);

  m_nextConfigGenerationNumber++;

  /* Make sure the new configuration _always_ is at least one step older */
  if(m_nextConfigGenerationNumber < m_newConfig->getGenerationNumber()+1)
    m_nextConfigGenerationNumber = _config->getGenerationNumber()+1;

  m_newConfig->setGenerationNumber(m_nextConfigGenerationNumber);

  node = 0;
  while(getNextNodeId(&node, NDB_MGM_NODE_TYPE_MGM)) {
    if(node != _ownNodeId) {
      NdbApiSignal* signal = getSignal();
      if (signal == NULL) {
	result = COULD_NOT_ALLOCATE_MEMORY;
	goto done;
      }
      
      lockReq = CAST_PTR(MgmLockConfigReq, signal->getDataPtrSend());
      signal->set(TestOrd::TraceAPI,
		  MGMSRV,
		  GSN_MGM_LOCK_CONFIG_REQ,
		  MgmLockConfigReq::SignalLength);
      
      lockReq->newConfigGeneration = m_nextConfigGenerationNumber;
      
      result = sendSignal(node, NO_WAIT, signal, true);

      NdbApiSignal *reply = 
	m_signalRecvQueue.waitFor(GSN_MGM_LOCK_CONFIG_REP, 0);

      if(reply == NULL) {
	/** @todo handle timeout/error */
	ndbout << __FILE__ << ":" << __LINE__ << endl;
	result = -1;
	goto done;
      }

    }
  }

 done:
  NdbMutex_Unlock(m_configMutex);
  return result;
}

/**
 * Unlocks configuration
 * 
 * @returns 0 on success, ! 0 on error
 */
int
MgmtSrvr::unlockConf(bool commit) {
  int result = -1;
  MgmUnlockConfigReq* unlockReq;
  NodeId node = 0;

  /* Check if this is the master node */
  if(getPrimaryNode() != _ownNodeId)
    goto done;

  errno = 0;
  if(NdbMutex_Lock(m_configMutex) != 0)
    return -1;

  if(commit)
    commitConfig();
  else
    rollbackConfig();

  node = 0;
  while(getNextNodeId(&node, NDB_MGM_NODE_TYPE_MGM)) {
    if(node != _ownNodeId) {
      NdbApiSignal* signal = getSignal();
      if (signal == NULL) {
	result = COULD_NOT_ALLOCATE_MEMORY;
	goto done;
      }
      
      unlockReq = CAST_PTR(MgmUnlockConfigReq, signal->getDataPtrSend());
      signal->set(TestOrd::TraceAPI,
		  MGMSRV,
		  GSN_MGM_UNLOCK_CONFIG_REQ,
		  MgmUnlockConfigReq::SignalLength);
      unlockReq->commitConfig = commit;
      
      result = sendSignal(node, NO_WAIT, signal, true);

      NdbApiSignal *reply = 
	m_signalRecvQueue.waitFor(GSN_MGM_UNLOCK_CONFIG_REP, 0);

      if(reply == NULL) {
	/** @todo handle timeout/error */
	result = -1;
	goto done;
      }

    }
  }

 done:
  NdbMutex_Unlock(m_configMutex);
  return result;
}

#endif // code must be rewritten to use SignalSender

/**
 * Commit the new configuration
 */
int
MgmtSrvr::commitConfig() {
  int ret = saveConfig(m_newConfig);
  delete _config;
  _config = m_newConfig;
  m_newConfig = NULL;
  ndbout << "commit " << ret << endl;
  return ret;
}

/**
 * Rollback to the old configuration
 */
int
MgmtSrvr::rollbackConfig() {
  delete m_newConfig;
  m_newConfig = NULL;
  ndbout << "rollback" << endl;
  return saveConfig(_config);
}

/**
 * Save a configuration to the running configuration file
 */
int
MgmtSrvr::saveConfig(const Config *conf) {
  BaseString newfile;
  newfile.appfmt("%s.new", m_configFilename.c_str());
  
  /* Open and write to the new config file */
  FILE *f = fopen(newfile.c_str(), "w");
  if(f == NULL) {
    /** @todo Send something apropriate to the log */
    return -1;
  }
  FileOutputStream stream(f);
  conf->printConfigFile(stream);

  fclose(f);

  /* Rename file to real name */
  rename(newfile.c_str(), m_configFilename.c_str());

  return 0;
}

Config *
MgmtSrvr::readConfig() {
  Config *conf;
  InitConfigFileParser parser;
  conf = parser.parseConfig(m_configFilename.c_str());
  return conf;
}

Config *
MgmtSrvr::fetchConfig() {
  struct ndb_mgm_configuration * tmp = m_config_retriever->getConfig();
  if(tmp != 0){
    Config * conf = new Config();
    conf->m_configValues = tmp;
    return conf;
  }
  return 0;
}

bool
MgmtSrvr::changeConfig(const BaseString &section,
		       const BaseString &param,
		       const BaseString &value) {
  if(m_newConfig == NULL)
    return false;
  return m_newConfig->change(section, param, value);
}
