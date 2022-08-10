/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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


#include <ndb_global.h>
#include <NdbOut.hpp>

#include "APIService.hpp"
#include "CPCD.hpp"
#include <NdbMutex.h> 

#include "common.hpp"
#ifdef _WIN32
#include <sys/types.h>
#include <direct.h>
#endif

extern const ParserRow<CPCDAPISession> commands[];


CPCD::CPCD() {
  loadingProcessList = false;
  m_processes.clear();
  m_monitor = NULL;
  m_monitor = new Monitor(this);
  m_procfile = "ndb_cpcd.db";
}

CPCD::~CPCD() {
  if(m_monitor != NULL) {
    delete m_monitor;
    m_monitor = NULL;
  }
}

int
CPCD::findUniqueId() {
  int id = 0;
  bool ok = false;
  m_processes.lock();
  
  while(!ok) {
    ok = true;
    id = rand() % 8192; /* Don't want so big numbers */

    if(id == 0)
    {
      ok = false;
      continue;
    }

    for(unsigned i = 0; i<m_processes.size(); i++)
    {
      if(m_processes[i]->m_id == id)
      {
	ok = false;
        break;
      }
    }
  }
  m_processes.unlock();
  return id;
}

bool
CPCD::defineProcess(const class Properties &args, const uintptr_t sessionid,
                    RequestStatus * rs, int *id) {
  CPCD::Process *proc = new CPCD::Process(args, this, sessionid);

  if(proc->m_id == -1)
    proc->m_id = findUniqueId();

  *id = proc->m_id;

  Guard tmp(m_processes);

  for(unsigned i = 0; i<m_processes.size(); i++) {
    Process * existentProc = m_processes[i];
    
    if((strcmp(proc->m_name.c_str(), existentProc->m_name.c_str()) == 0) &&
       (strcmp(proc->m_group.c_str(), existentProc->m_group.c_str()) == 0)) {
      delete proc;

      /* Identical names in the same group */
      rs->err(AlreadyExists, "Name already exists");
      return false;
    }

    if(proc->m_id == existentProc->m_id) {
      delete proc;

      /* Identical ID numbers */
      rs->err(AlreadyExists, "Id already exists");
      return false;
    }
  }
  
  m_processes.push_back(proc, false);
  logger.debug("Process %s:%s:%d defined",
                proc->m_group.c_str(), proc->m_name.c_str(), proc->m_id);

  notifyChanges();
  return true;
}

bool
CPCD::undefineProcess(const int id, const uintptr_t sessionid,
                      CPCD::RequestStatus *rs) {
  Guard tmp(m_processes);

  Process * proc = 0;
  unsigned i;
  for(i = 0; i < m_processes.size(); i++) {
    if(m_processes[i]->m_id == id) {
      proc = m_processes[i];
      break;
    }
  }

  if(proc == 0){
    rs->err(NotExists, "No such process");
    return false;
  }

  if (!proc->allowsChangeFromSession(sessionid)) {
    logger.error("Process %s:%s:%d undefine attempt from invalid session",
                 proc->m_group.c_str(), proc->m_name.c_str(), proc->m_id);
    rs->err(Error, "Undefine attempt from invalid session");
    return false;
  }

  switch (proc->m_status) {
    case STARTING:
    case RUNNING:
      logger.error("Process %s:%s:%d undefine attempt without stop",
                   proc->m_group.c_str(), proc->m_name.c_str(), proc->m_id);
      rs->err(Error, "Undefine attempt for a non-stopped process");
      return false;
    case STOPPING:
    case STOPPED:
      break;
  }

  if (proc->m_remove_on_stopped)
  {
    rs->err(Error, "Undefine already in progress");
    return false;
  }

  proc->m_remove_on_stopped = true;
  logger.debug("Process %s:%s:%d undefined",
                proc->m_group.c_str(), proc->m_name.c_str(), proc->m_id);

  notifyChanges();
  return true;
}

bool
CPCD::startProcess(const int id, const uintptr_t sessionid,
   CPCD::RequestStatus *rs) {
  Process * proc = 0;
  {

    Guard tmp(m_processes);
    
    for(unsigned i = 0; i < m_processes.size(); i++) {
      if(m_processes[i]->m_id == id) {
	proc = m_processes[i];
	break;
      }
    }
    
    if(proc == 0){
      rs->err(NotExists, "No such process");
      return false;
    }

    if (!proc->allowsChangeFromSession(sessionid)) {
      logger.error("Process %s:%s:%d start attempt from invalid session",
                  proc->m_group.c_str(), proc->m_name.c_str(), proc->m_id);
      rs->err(Error, "Start attempt from invalid session");
      return false;
    }
    
    if (proc->m_remove_on_stopped)
    {
      rs->err(Error, "Undefine in progress, start not allowed.");
      return false;
    }

    switch(proc->m_status){
    case STOPPED:
      proc->m_status = STARTING;
      logger.debug("Process %s:%s:%d with pid %d starting",
                    proc->m_group.c_str(), proc->m_name.c_str(),
                    proc->m_id, proc->getPid());
      if(proc->start() != 0){
	rs->err(Error, "Failed to start");
	return false;
      }
      break;
    case STARTING:
      rs->err(Error, "Already starting");
      return false;
    case RUNNING:
      rs->err(Error, "Already started");
      return false;
    case STOPPING:
      rs->err(Error, "Currently stopping");
      return false;
    }
    
    notifyChanges();
  }

  return true;
}

bool
CPCD::stopProcess(const int id, uintptr_t sessionid, CPCD::RequestStatus *rs) {

  Guard tmp(m_processes);

  Process * proc = 0;
  for(unsigned i = 0; i < m_processes.size(); i++) {
    if(m_processes[i]->m_id == id) {
      proc = m_processes[i];
      break;
    }
  }

  if(proc == 0){
    rs->err(NotExists, "No such process");
    return false;
  }

  if (!proc->allowsChangeFromSession(sessionid)) {
    logger.error("Process %s:%s:%d undefine attempt from invalid session",
                 proc->m_group.c_str(), proc->m_name.c_str(), proc->m_id);
    rs->err(Error, "Undefine attempt from invalid session");
    return false;
  }

  switch(proc->m_status){
  case STARTING:
  case RUNNING:
    logger.debug("Process %s:%s:%d with pid %d STOPPING",
                  proc->m_group.c_str(), proc->m_name.c_str(),
                  proc->m_id, proc->getPid());
    proc->stop();
    break;
  case STOPPED:
    rs->err(AlreadyStopped, "Already stopped");
    return false;
    break;
  case STOPPING:
    rs->err(AlreadyStopped, "Already stopping");
    return false;
  }
  
  notifyChanges();

  return true;
}

bool
CPCD::notifyChanges() {
  bool ret = true;
  if(!loadingProcessList)
    ret = saveProcessList();

  m_monitor->signal();

  return ret;
}


#ifdef _WIN32
static int link(const char* from_file, const char* to_file)
{
  BOOL fail_if_exists = true;
  if (CopyFile(from_file, to_file, fail_if_exists) == 0)
  {
    /* "On error, -1 is returned" */
    return -1;
  }
  /* "On success, zero is returned" */
  return 0;
}
#endif


/* Must be called with m_processlist locked */
bool
CPCD::saveProcessList(){
  char newfile[PATH_MAX+4];
  char oldfile[PATH_MAX+4];
  char curfile[PATH_MAX];
  FILE *f;

  /* Create the filenames that we will use later */
  BaseString::snprintf(newfile, sizeof(newfile), "%s.new", m_procfile.c_str());
  BaseString::snprintf(oldfile, sizeof(oldfile), "%s.old", m_procfile.c_str());
  BaseString::snprintf(curfile, sizeof(curfile), "%s", m_procfile.c_str());

  f = fopen(newfile, "w");

  if(f == NULL) {
    /* XXX What should be done here? */
    logger.critical("Cannot open `%s': %s\n", newfile, strerror(errno));
    return false;
  }

  for(unsigned i = 0; i<m_processes.size(); i++){
    m_processes[i]->print(f);
    fprintf(f, "\n");

    if(m_processes[i]->m_type == ProcessType::TEMPORARY){
      /**
       * Interactive process should never be "restarted" on cpcd restart
       */
      continue;
    }
    
    if(m_processes[i]->m_status == RUNNING || 
       m_processes[i]->m_status == STARTING){
      fprintf(f, "start process\nid: %d\n\n", m_processes[i]->m_id);
    }
  }
  
  fclose(f);
  f = NULL;
  
  /* This will probably only work on reasonably Unix-like systems. You have
   * been warned...
   * 
   * The motivation behind all this link()ing is that the daemon might
   * crash right in the middle of updating the configuration file, and in
   * that case we want to be sure that the old file is around until we are
   * guaranteed that there is always at least one copy of either the old or
   * the new configuration file left.
   */

  /* Remove an old config file if it exists */
  unlink(oldfile);

  if(link(curfile, oldfile) != 0) /* make a backup of the running config */
    logger.error("Cannot rename '%s' -> '%s'", curfile, oldfile);
  else {
    if(unlink(curfile) != 0) { /* remove the running config file */
      logger.critical("Cannot remove file '%s'", curfile);
      return false;
    }
  }

  if(link(newfile, curfile) != 0) { /* put the new config file in place */
    printf("-->%d\n", __LINE__);

    logger.critical("Cannot rename '%s' -> '%s': %s", 
		    curfile, newfile, strerror(errno));
    return false;
  }

  /* XXX Ideally we would fsync() the directory here, but I'm not sure if
   * that actually works.
   */

  unlink(newfile); /* remove the temporary file */
  unlink(oldfile); /* remove the old file */

  logger.info("Process list saved as '%s'", curfile);

  return true;
}

bool
CPCD::loadProcessList(){
  BaseString secondfile;
  FILE *f;

  loadingProcessList = true;

  secondfile.assfmt("%s.new", m_procfile.c_str());

  /* Try to open the config file */
  f = fopen(m_procfile.c_str(), "r");

  /* If it did not exist, try to open the backup. See the saveProcessList()
   * method for an explanation why it is done this way.
   */
  if(f == NULL) {
    f = fopen(secondfile.c_str(), "r");
    
    if(f == NULL) {
      /* XXX What to do here? */
      logger.info("Configuration file `%s' not found",
		  m_procfile.c_str());
      logger.info("Starting with empty configuration");
      loadingProcessList = false;
      return false;
    } else {
      logger.info("Configuration file `%s' missing",
		  m_procfile.c_str());
      logger.info("Backup configuration file `%s' is used",
		  secondfile.c_str());
      /* XXX Maybe we should just rename the backup file to the official
       * name, and be done with it?
       */
    }
  }

/*
  File is ignored anyways, so don't load it,
  kept for future use of config file.

  CPCDAPISession sess(f, *this);
  sess.loadFile();
*/
  fclose(f);
  loadingProcessList = false;

  unsigned i;
  Vector<int> temporary;
  for(i = 0; i<m_processes.size(); i++){
    Process * proc = m_processes[i];
    proc->readPid();
    logger.debug("Loading Process %s:%s:%d with pid %d ",
                  proc->m_group.c_str(), proc->m_name.c_str(),
                  proc->m_id, proc->getPid());
    if(proc->m_type == ProcessType::TEMPORARY){
      temporary.push_back(proc->m_id);
    }
  }
  
  for(i = 0; i<temporary.size(); i++){
    // TODO(tiago) CPCD should not call an API method but instead an internal
    //             method to perform cleanup. This is not an issue because we
    //             this code is never reached (as we don't read the database)
    RequestStatus rs;
    undefineProcess(temporary[i], 0, &rs);
  }
  
  /* Don't call notifyChanges here, as that would save the file we just
     loaded */
  m_monitor->signal();
  return true;
}

MutexVector<CPCD::Process *> *
CPCD::getProcessList() {
  return &m_processes;
}

void
CPCD::RequestStatus::err(enum RequestStatusCode status, const char *msg) {
  m_status = status;
  BaseString::snprintf(m_errorstring, sizeof(m_errorstring), "%s", msg);
}
