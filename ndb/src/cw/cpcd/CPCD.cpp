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


#include <ndb_global.h>
#include <NdbOut.hpp>

#include "APIService.hpp"
#include "CPCD.hpp"
#include <NdbMutex.h> 

#include "common.hpp"

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
  int id;
  bool ok = false;
  m_processes.lock();
  
  while(!ok) {
    ok = true;
    id = random() % 8192; /* Don't want so big numbers */
    
    if(id == 0)
      ok = false;

    for(size_t i = 0; i<m_processes.size(); i++) {
      if(m_processes[i]->m_id == id)
	ok = false;
    }
  }
  m_processes.unlock();
  return id;
}

bool
CPCD::defineProcess(RequestStatus * rs, Process * arg){
  if(arg->m_id == -1)
    arg->m_id = findUniqueId();

  Guard tmp(m_processes);

  for(size_t i = 0; i<m_processes.size(); i++) {
    Process * proc = m_processes[i];
    
    if((strcmp(arg->m_name.c_str(), proc->m_name.c_str()) == 0) && 
       (strcmp(arg->m_group.c_str(), proc->m_group.c_str()) == 0)) {
      /* Identical names in the same group */
      rs->err(AlreadyExists, "Name already exists");
      return false;
    }

    if(arg->m_id == proc->m_id) {
      /* Identical ID numbers */
      rs->err(AlreadyExists, "Id already exists");
      return false;
    }
  }
  
  m_processes.push_back(arg, false);

  notifyChanges();
  report(arg->m_id, CPCEvent::ET_PROC_USER_DEFINE);

  return true;
}

bool
CPCD::undefineProcess(CPCD::RequestStatus *rs, int id) {

  Guard tmp(m_processes);

  Process * proc = 0;
  size_t i;
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

  switch(proc->m_status){
  case RUNNING:
  case STOPPED:
  case STOPPING:
  case STARTING:
    proc->stop();
    m_processes.erase(i, false /* Already locked */);
  }
  
  
  notifyChanges();
  
  report(id, CPCEvent::ET_PROC_USER_UNDEFINE);

  return true;
}

bool
CPCD::startProcess(CPCD::RequestStatus *rs, int id) {

  Process * proc = 0;
  {

    Guard tmp(m_processes);
    
    for(size_t i = 0; i < m_processes.size(); i++) {
      if(m_processes[i]->m_id == id) {
	proc = m_processes[i];
	break;
      }
    }
    
    if(proc == 0){
      rs->err(NotExists, "No such process");
      return false;
    }
    
    switch(proc->m_status){
    case STOPPED:
      proc->m_status = STARTING;
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
  report(id, CPCEvent::ET_PROC_USER_START);

  return true;
}

bool
CPCD::stopProcess(CPCD::RequestStatus *rs, int id) {

  Guard tmp(m_processes);

  Process * proc = 0;
  for(size_t i = 0; i < m_processes.size(); i++) {
    if(m_processes[i]->m_id == id) {
      proc = m_processes[i];
      break;
    }
  }

  if(proc == 0){
    rs->err(NotExists, "No such process");
    return false;
  }

  switch(proc->m_status){
  case STARTING:
  case RUNNING:
    proc->stop();
    break;
  case STOPPED:
    rs->err(AlreadyStopped, "Already stopped");
    return false;
    break;
  case STOPPING:
    rs->err(Error, "Already stopping");
    return false;
  }
  
  notifyChanges();

  report(id, CPCEvent::ET_PROC_USER_START);
  
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

  for(size_t i = 0; i<m_processes.size(); i++){
    m_processes[i]->print(f);
    fprintf(f, "\n");

    if(m_processes[i]->m_processType == TEMPORARY){
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

  CPCDAPISession sess(f, *this);
  sess.loadFile();
  loadingProcessList = false;

  size_t i;
  Vector<int> temporary;
  for(i = 0; i<m_processes.size(); i++){
    Process * proc = m_processes[i];
    proc->readPid();
    if(proc->m_processType == TEMPORARY){
      temporary.push_back(proc->m_id);
    }
  }
  
  for(i = 0; i<temporary.size(); i++){
    RequestStatus rs;
    undefineProcess(&rs, temporary[i]);
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

#if 0
void
CPCD::sigchild(int pid){
  m_processes.lock(); 
  for(size_t i = 0; i<m_processes.size(); i++){
    if(m_processes[i].m_pid == pid){
    }
  }
  wait(pid, 0, 0);
}
#endif

  /** Register event subscriber */
void
CPCD::do_register(EventSubscriber * sub){
  m_subscribers.lock();
  m_subscribers.push_back(sub, false);
  m_subscribers.unlock();  
}

EventSubscriber*
CPCD::do_unregister(EventSubscriber * sub){
  m_subscribers.lock();

  for(size_t i = 0; i<m_subscribers.size(); i++){
    if(m_subscribers[i] == sub){
      m_subscribers.erase(i);
      m_subscribers.unlock();  
      return sub;
    }
  }

  m_subscribers.unlock();  
  return 0;
}

void
CPCD::report(int id, CPCEvent::EventType t){
  CPCEvent e;
  e.m_time = time(0);
  e.m_proc = id;
  e.m_type = t;
  m_subscribers.lock();
  for(size_t i = 0; i<m_subscribers.size(); i++){
    (* m_subscribers[i]).report(e);
  }
  m_subscribers.unlock();
}

template class MutexVector<EventSubscriber*>;
