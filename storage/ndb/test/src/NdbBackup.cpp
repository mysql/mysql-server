/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/DumpStateOrd.hpp>
#include <NdbBackup.hpp>
#include <NdbOut.hpp>
#include <NDBT_Output.hpp>
#include <NdbConfig.h>
#include <ndb_version.h>
#include <NDBT.hpp>
#include <NdbSleep.h>
#include <random.h>
#include <NdbTick.h>
#include <util/File.hpp>

#define AUTOTEST_MYSQL_PATH_ENV "MYSQL_BASE_DIR"
#define MTR_MYSQL_PATH_ENV "MYSQL_BINDIR"

#define CHECK(b, m) { int _xx = b; if (!(_xx)) { \
  ndbout << "ERR: "<< m \
           << "   " << "File: " << __FILE__ \
           << " (Line: " << __LINE__ << ")" << "- " << _xx << endl; \
  return NDBT_FAILED; } }

#include <ConfigRetriever.hpp>
#include <mgmapi.h>
#include <mgmapi_config_parameters.h>
#include <mgmapi_configuration.hpp>

static bool isHostLocal(const char* hostName)
{
  /* Examples assuming that hostname served indicates locality... */
  return ((strcmp(hostName, "localhost") == 0) ||
          (strcmp(hostName, "127.0.0.1") == 0));
}

int
NdbBackup::clearOldBackups()
{
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  int retCode = 0;

#ifndef _WIN32
  for(size_t i = 0; i < ndbNodes.size(); i++)
  {
    int nodeId = ndbNodes[i].node_id;
    const std::string path = getBackupDataDirForNode(nodeId);
    if (path.empty())
      return -1;  
    
    const char *host;
    if (!getHostName(nodeId, &host))
      return -1;

    /* 
     * Clear old backup files
     */ 
    BaseString tmp1, tmp2;
    if (!isHostLocal(host))
    {
      // clean up backup from BackupDataDir
      tmp1.assfmt("ssh %s rm -rf %s/BACKUP", host, path.c_str());
      // clean up local copy created by scp
      tmp2.assfmt("rm -rf ./BACKUP*");
    }
    else
    {
      // clean up backup from BackupDataDir
      tmp1.assfmt("rm -rf %s/BACKUP", path.c_str());
      // clean up copy created by cp
      tmp2.assfmt("rm -rf ./BACKUP*");
    }

    ndbout << "buf: "<< tmp1.c_str() <<endl;
    int res = system(tmp1.c_str());
    ndbout << "res: " << res << endl;

    ndbout << "buf: "<< tmp2.c_str() <<endl;
    res = system(tmp2.c_str());
    ndbout << "res: " << res << endl;

    if (res && retCode == 0)
      retCode = res;
  }
#endif

  return retCode;
}

int 
NdbBackup::start(unsigned int & _backup_id,
		 int flags,
		 unsigned int user_backup_id,
		 unsigned int logtype,
		 const char* encryption_password,
		 unsigned int password_length) {

  if (!isConnected())
    return -1;

  ndb_mgm_reply reply;
  reply.return_code = 0;

  bool any = _backup_id == 0;

  if (encryption_password == NULL)
  {
    encryption_password = m_default_encryption_password;
    password_length = m_default_encryption_password_length;
  }
loop:
  if (ndb_mgm_start_backup4(handle,
			   flags,
			   &_backup_id,
			   &reply,
			   user_backup_id,
			   logtype,
			   encryption_password,
			   password_length) == -1) {

    if (ndb_mgm_get_latest_error(handle) == NDB_MGM_COULD_NOT_START_BACKUP &&
        strstr(ndb_mgm_get_latest_error_desc(handle), "file already exists") &&
        any == true)
    {
      NdbSleep_SecSleep(3);
      _backup_id += 100;
      user_backup_id += 100;
      goto loop;
    }
    
    g_err << "Error: " << ndb_mgm_get_latest_error(handle) << endl;
    g_err << "Error msg: " << ndb_mgm_get_latest_error_msg(handle) << endl;
    g_err << "Error desc: " << ndb_mgm_get_latest_error_desc(handle) << endl;
    return -1;
  }

  if(reply.return_code != 0){
    g_err  << "PLEASE CHECK CODE NdbBackup.cpp line=" << __LINE__ << endl;
    g_err << "Error: " << ndb_mgm_get_latest_error(handle) << endl;
    g_err << "Error msg: " << ndb_mgm_get_latest_error_msg(handle) << endl;
    g_err << "Error desc: " << ndb_mgm_get_latest_error_desc(handle) << endl;
    return reply.return_code;
  }
  return 0;
}

int
NdbBackup::startLogEvent(){

  if (!isConnected())
    return -1;
  log_handle= NULL;
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0, 0 };
  log_handle = ndb_mgm_create_logevent_handle(handle, filter);
  if (!log_handle) {
    g_err << "Can't create log event" << endl;
    return -1;
  }
  return 0;
}

int
NdbBackup::checkBackupStatus(){

  struct ndb_logevent log_event;
  int result = 0;
  int res;
  if(!log_handle) {
    return -1;
  }
  if ((res= ndb_logevent_get_next(log_handle, &log_event, 3000)) > 0)
  {
    switch (log_event.type) {
      case NDB_LE_BackupStarted:
	result = 1;
	break;
      case NDB_LE_BackupCompleted:
	result = 2;
        break;
      case NDB_LE_BackupAborted:
	result = 3;
        break;
      default:
        break;
    }
  }
  ndb_mgm_destroy_logevent_handle(&log_handle);
  return result;
}


std::string
NdbBackup::getBackupDataDirForNode(int node_id)
{
  if (connect())
    return "";

  // Fetch configuration from management server
  ndb_mgm::config_ptr conf(ndb_mgm_get_configuration(handle, 0));
  if (!conf)
  {
    const char * err_msg = ndb_mgm_get_latest_error_msg(handle);
    if(!err_msg)
      err_msg = "No error given!";
      
    ndbout << "Could not fetch configuration" << endl;
    ndbout << "error: " << err_msg << endl;
    return "";
  }
  
  // Find section for node with given node id
  ndb_mgm_configuration_iterator iter(conf.get(), CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, node_id)) {
    ndbout << "Invalid configuration fetched, no section for nodeid: "
           << node_id << endl;
    return "";
  }

  // Check that the found section is for a DB node
  unsigned int type;
  if(iter.get(CFG_TYPE_OF_SECTION, &type) || type != NODE_TYPE_DB){
    ndbout << "type = " << type << endl;
    ndbout << "Invalid configuration fetched, expected DB node" << endl;
    return "";
  }  
  
  // Extract the backup path
  const char * path;
  if (iter.get(CFG_DB_BACKUP_DATADIR, &path)){
    ndbout << "BackupDataDir not found" << endl;
    return "";
  }

  return path;
}

BaseString
NdbBackup::getNdbRestoreBinaryPath(){

  const char* mysql_install_path;
  if ((mysql_install_path = getenv(AUTOTEST_MYSQL_PATH_ENV)) != NULL) {
  } else if ((mysql_install_path = getenv(MTR_MYSQL_PATH_ENV)) != NULL) {
  } else {
    g_err << "Either MYSQL_BASE_DIR or MYSQL_BINDIR environment variables"
          << "must be defined as search path for ndb_restore binary" << endl;
    return "";
  }

  BaseString ndb_restore_bin_path;
  ndb_restore_bin_path.assfmt("%s/bin/ndb_restore", mysql_install_path);
  if (!File_class::exists(ndb_restore_bin_path.c_str()))
  {
    ndb_restore_bin_path.assfmt("%s/storage/ndb/tools/ndb_restore", mysql_install_path);
    if (!File_class::exists(ndb_restore_bin_path.c_str()))
    {
      g_err << "Failed to find ndb_restore in either $MYSQL_BASE_DIR "
            << "or $MYSQL_BINDIR paths " <<  ndb_restore_bin_path.c_str() << endl;
      return "";
    }
    else
      return ndb_restore_bin_path;
  }
  return ndb_restore_bin_path;
}

int  
NdbBackup::execRestore(bool _restore_data,
		       bool _restore_meta,
		       bool _restore_epoch,
                       int _node_id,
		       unsigned _backup_id,
                       unsigned _error_insert,
                       const char * encryption_password,
                       int password_length)
{
  ndbout << "getBackupDataDir "<< _node_id <<endl;

  const std::string path = getBackupDataDirForNode(_node_id);
  if (path.empty())
    return -1;  

  BaseString ndb_restore_bin_path = getNdbRestoreBinaryPath();
  if (ndb_restore_bin_path == ""){
    return -1;
  }

  ndbout << "getHostName "<< _node_id <<endl;
  const char *host;
  if (!getHostName(_node_id, &host)){
    return -1;
  }

  if (encryption_password == nullptr)
  {
    encryption_password = m_default_encryption_password;
    password_length = m_default_encryption_password_length;
  }
  else if (password_length == -1)
  {
    password_length = strlen(encryption_password);
  }
  else
  {
    const void* p = memchr(encryption_password, 0, password_length);
    if (p != encryption_password + password_length)
    {
      // Only NUL-terminated strings are allowed as password.
      return -1;
    }
  }
  if (encryption_password != nullptr &&
      strcspn(encryption_password, "!\"$%'\\^") != (size_t)password_length)
  {
    // Do not allow hard to handle on command line characters.
    return -1;
  }

  /* 
   * Copy  backup files to local dir
   */ 
  BaseString tmp;
  if (!isHostLocal(host))
  {
    tmp.assfmt("scp -r %s:%s/BACKUP/BACKUP-%d/* .",
               host, path.c_str(),
               _backup_id);
  }
  else
  {
    tmp.assfmt("scp -r %s/BACKUP/BACKUP-%d/* .",
               path.c_str(),
               _backup_id);
  }

  ndbout << "buf: "<< tmp.c_str() <<endl;
  int res = system(tmp.c_str());  
  
  ndbout << "scp res: " << res << endl;

  BaseString cmd;
#if 1
#else
  cmd = "valgrind --leak-check=yes -v "
#endif
  cmd.append(ndb_restore_bin_path.c_str());
  cmd.append(" --no-defaults");

#ifdef ERROR_INSERT
  if(_error_insert > 0)
  {
    cmd.appfmt(" --error-insert=%u", _error_insert);
  }
#endif

  if (encryption_password != NULL)
  {
    cmd.appfmt(" --decrypt --backup-password=\"%s\"", encryption_password);
  }

  cmd.appfmt(" -c \"%s:%d\" -n %d -b %d",
             ndb_mgm_get_connected_host(handle),
             ndb_mgm_get_connected_port(handle),
             _node_id, 
             _backup_id);

  if(res == 0 && !_restore_meta && !_restore_data && !_restore_epoch)
  {
    // ndb_restore connects to cluster, prints backup info
    // and exits without restoring anything
    tmp = cmd;
    ndbout << "buf: "<< tmp.c_str() <<endl;
    res = system(tmp.c_str());
  }

  if (res == 0 && _restore_meta)
  {
    /** don't restore DD objects */
    tmp.assfmt("%s -m -d .", cmd.c_str());
    
    ndbout << "buf: "<< tmp.c_str() <<endl;
    res = system(tmp.c_str());
  }
  
  if (res == 0 && _restore_data)
  {

    tmp.assfmt("%s -r .", cmd.c_str());
    
    ndbout << "buf: "<< tmp.c_str() <<endl;
    res = system(tmp.c_str());
  }

  if (res == 0 && _restore_epoch)
  {
    tmp.assfmt("%s -e .", cmd.c_str());

    ndbout << "buf: "<< tmp.c_str() <<endl;
    res = system(tmp.c_str());
  }
  
  ndbout << "ndb_restore res: " << res << endl;

  return res;
}

int 
NdbBackup::restore(unsigned _backup_id,
                   bool restore_meta,
                   bool restore_data,
                   unsigned error_insert,
                   bool restore_epoch)
{
  
  if (!isConnected())
    return -1;

  if (getStatus() != 0)
    return -1;

  if(!restore_meta && !restore_data && !restore_epoch &&
    (execRestore(false, false, false, ndbNodes[0].node_id, _backup_id, error_insert) !=0))
    return -1;

  if(restore_meta && // if metadata restore enabled 
    // restore metadata for first node
    (execRestore(false, true, false, ndbNodes[0].node_id, _backup_id, error_insert) !=0))
    return -1;

  // Restore data once for each node
  if(restore_data)
  {
    for(unsigned i = 0; i < ndbNodes.size(); i++)
    {
      if(execRestore(true, false, false, ndbNodes[i].node_id, _backup_id, error_insert) != 0)
        return -1;
    }
  }

  // Restore epoch from first node
  if (restore_epoch)
  {
    if (execRestore(false, false, true, ndbNodes[0].node_id, _backup_id, error_insert) !=0)
    {
      return -1;
    }
  }
  return 0;
}

// Master failure
int
NFDuringBackupM_codes[] = {
  10003,
  10004,
  10007,
  10008,
  10009,
  10010,
  10012,
  10013
};

// Slave failure
int
NFDuringBackupS_codes[] = {
  10014,
  10015,
  10016,
  10017,
  10018,
  10020
};

// Master takeover etc...
int
NFDuringBackupSL_codes[] = {
  10001,
  10002,
  10021
};

int 
NdbBackup::NFMaster(NdbRestarter& _restarter){
  const int sz = sizeof(NFDuringBackupM_codes)/sizeof(NFDuringBackupM_codes[0]);
  return NF(_restarter, NFDuringBackupM_codes, sz, true);
}

int 
NdbBackup::NFMasterAsSlave(NdbRestarter& _restarter){
  const int sz = sizeof(NFDuringBackupS_codes)/sizeof(NFDuringBackupS_codes[0]);
  return NF(_restarter, NFDuringBackupS_codes, sz, true);
}

int 
NdbBackup::NFSlave(NdbRestarter& _restarter){
  const int sz = sizeof(NFDuringBackupS_codes)/sizeof(NFDuringBackupS_codes[0]);
  return NF(_restarter, NFDuringBackupS_codes, sz, false);
}

int 
NdbBackup::NF(NdbRestarter& _restarter, int *NFDuringBackup_codes, const int sz, bool onMaster){
  int nNodes = _restarter.getNumDbNodes();
  {
    if(nNodes == 1)
      return NDBT_OK;
    
    int nodeId = _restarter.getMasterNodeId();

    CHECK(_restarter.restartOneDbNode(nodeId, false, true, true) == 0,
	  "Could not restart node "<< nodeId);
    
    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
	  "waitNodesNoStart failed");
    
    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");
  }
  
  CHECK(_restarter.waitClusterStarted() == 0,
	"waitClusterStarted failed");
  
  myRandom48Init((long)NdbTick_CurrentMillisecond());

  for(int i = 0; i<sz; i++){

    int error = NFDuringBackup_codes[i];
    unsigned int backupId;

    const int masterNodeId = _restarter.getMasterNodeId();
    CHECK(masterNodeId > 0, "getMasterNodeId failed");
    int nodeId;

    nodeId = masterNodeId;
    if (!onMaster) {
      int randomId;
      while (nodeId == masterNodeId) {
	randomId = myRandom48(nNodes);
	nodeId = _restarter.getDbNodeId(randomId);
      }
    }

    g_err << "NdbBackup::NF node = " << nodeId 
	   << " error code = " << error << " masterNodeId = "
	   << masterNodeId << endl;


    int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    CHECK(_restarter.dumpStateOneNode(nodeId, val, 2) == 0,
	  "failed to set RestartOnErrorInsert");
    CHECK(_restarter.insertErrorInNode(nodeId, error) == 0,
	  "failed to set error insert");
   
    g_info << "error inserted"  << endl;
    NdbSleep_SecSleep(1);

    g_info << "starting backup"  << endl;
    int r = start(backupId);
    g_info << "r = " << r
	   << " (which should fail) started with id = "  << backupId << endl;
    if (r == 0) {
      g_err << "Backup should have failed on error_insertion " << error << endl
	    << "Master = " << masterNodeId << "Node = " << nodeId << endl;
      return NDBT_FAILED;
    }

    CHECK(_restarter.waitNodesNoStart(&nodeId, 1) == 0,
	  "waitNodesNoStart failed");

    g_info << "number of nodes running " << _restarter.getNumDbNodes() << endl;

    if (_restarter.getNumDbNodes() != nNodes) {
      g_err << "Failure: cluster not up" << endl;
      return NDBT_FAILED;
    }

    g_info << "starting new backup"  << endl;
    CHECK(start(backupId) == 0,
	  "failed to start backup");
    g_info << "(which should succeed) started with id = "  << backupId << endl;

    g_info << "starting node"  << endl;
    CHECK(_restarter.startNodes(&nodeId, 1) == 0,
	  "failed to start node");

    CHECK(_restarter.waitClusterStarted() == 0,
	  "waitClusterStarted failed");
    g_info << "node started"  << endl;

    int val2[] = { 24, 2424 };
    CHECK(_restarter.dumpStateAllNodes(val2, 2) == 0,
	  "failed to check backup resources RestartOnErrorInsert");
    
    CHECK(_restarter.insertErrorInNode(nodeId, 10099) == 0,
	  "failed to set error insert");

    NdbSleep_SecSleep(1);
  }

  return NDBT_OK;
}

int
FailS_codes[] = {
  10025,
  10027,
  10033,
  10035,
  10036
};

int
FailM_codes[] = {
  10023,
  10024,
  10025,
  10026,
  10027,
  10028,
  10031,
  10033,
  10035,
  10037,
  10038
};

int 
NdbBackup::FailMaster(NdbRestarter& _restarter){
  const int sz = sizeof(FailM_codes)/sizeof(FailM_codes[0]);
  return Fail(_restarter, FailM_codes, sz, true);
}

int 
NdbBackup::FailMasterAsSlave(NdbRestarter& _restarter){
  const int sz = sizeof(FailS_codes)/sizeof(FailS_codes[0]);
  return Fail(_restarter, FailS_codes, sz, true);
}

int 
NdbBackup::FailSlave(NdbRestarter& _restarter){
  const int sz = sizeof(FailS_codes)/sizeof(FailS_codes[0]);
  return Fail(_restarter, FailS_codes, sz, false);
}

int 
NdbBackup::Fail(NdbRestarter& _restarter, int *Fail_codes, const int sz, bool onMaster){

  CHECK(_restarter.waitClusterStarted() == 0,
	"waitClusterStarted failed");

  int nNodes = _restarter.getNumDbNodes();

  myRandom48Init((long)NdbTick_CurrentMillisecond());

  for(int i = 0; i<sz; i++){
    int error = Fail_codes[i];
    unsigned int backupId;

    const int masterNodeId = _restarter.getMasterNodeId();
    CHECK(masterNodeId > 0, "getMasterNodeId failed");
    int nodeId;

    nodeId = masterNodeId;
    if (!onMaster) {
      int randomId;
      while (nodeId == masterNodeId) {
	randomId = myRandom48(nNodes);
	nodeId = _restarter.getDbNodeId(randomId);
      }
    }

    g_err << "NdbBackup::Fail node = " << nodeId 
	   << " error code = " << error << " masterNodeId = "
	   << masterNodeId << endl;

    CHECK(_restarter.insertErrorInNode(nodeId, error) == 0,
	  "failed to set error insert");
   
    g_info << "error inserted"  << endl;
    g_info << "waiting some before starting backup"  << endl;

    g_info << "starting backup"  << endl;
    int r = start(backupId);
    g_info << "r = " << r
	   << " (which should fail) started with id = "  << backupId << endl;
    if (r == 0) {
      g_err << "Backup should have failed on error_insertion " << error << endl
	    << "Master = " << masterNodeId << "Node = " << nodeId << endl;
      return NDBT_FAILED;
    }
    
    CHECK(_restarter.waitClusterStarted() == 0,
	  "waitClusterStarted failed");

    CHECK(_restarter.insertErrorInNode(nodeId, 10099) == 0,
	  "failed to set error insert");

    NdbSleep_SecSleep(5);
    
    int val2[] = { 24, 2424 };
    CHECK(_restarter.dumpStateAllNodes(val2, 2) == 0,
	  "failed to check backup resources RestartOnErrorInsert");
    
  }

  return NDBT_OK;
}

int
NdbBackup::abort(unsigned int _backup_id)
{
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_abort_backup(handle, _backup_id, &reply);
  if (result != 0)
  {
    g_err << "Failed to abort backup" << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;

}

int
NdbBackup::set_default_encryption_password(const char* pwd, int len)
{
  free(m_default_encryption_password);
  if (pwd == NULL)
  {
    m_default_encryption_password = NULL;
    m_default_encryption_password_length = 0;
    return NDBT_OK;
  }
  if (len == -1)
  {
    len = strlen(pwd);
  }
  m_default_encryption_password = (char*)malloc(len + 1);
  m_default_encryption_password_length = len;
  memcpy(m_default_encryption_password, pwd, len);
  m_default_encryption_password[len] = 0;
  return NDBT_OK;
}
