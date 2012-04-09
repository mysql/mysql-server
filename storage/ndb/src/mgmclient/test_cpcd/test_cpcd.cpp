/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include "../CpcClient.hpp"
#include <Vector.hpp>

SimpleCpcClient g_client("localhost", 1234);
Vector<SimpleCpcClient::Process> g_procs;

void define();
void start(SimpleCpcClient::Process & p);
void stop(SimpleCpcClient::Process & p);
void undefine(SimpleCpcClient::Process & p);
void list();
SimpleCpcClient::Process* find(int id);

#define ABORT() {ndbout_c("ABORT"); while(true); abort();}

int name = 0;

int
main(void){

  g_client.connect();

  srand(time(0));
  for(int i = 0; i<1000; i++){
    int sz = g_procs.size();
    int test = rand() % 100;
    if(sz == 0 || test < 10){
      define();
      continue;
    }

    list();

    int proc = rand() % g_procs.size();
    SimpleCpcClient::Process & p = g_procs[proc];
    if(p.m_status == "running" && test > 50){
      ndbout_c("undefine %d: %s (running)", p.m_id, p.m_name.c_str());
      undefine(p);
      g_procs.erase(proc);
      continue;
    }
    if(p.m_status == "running" && test <= 50){
      ndbout_c("stop %d: %s(running)", p.m_id, p.m_name.c_str());
      stop(p);
      continue;
    }
    if(p.m_status == "stopped" && test > 50){
      ndbout_c("undefine %d: %s(stopped)", p.m_id, p.m_name.c_str());
      undefine(p);
      g_procs.erase(proc);
      continue;
    }
    if(p.m_status == "stopped" && test <= 50){
      ndbout_c("start %d %s(stopped)", p.m_id, p.m_name.c_str());
      start(p);
      continue;
    }
    ndbout_c("Unknown: %s", p.m_status.c_str());
  }
}

void define(){
  SimpleCpcClient::Process m_proc;
  m_proc.m_id = -1;
  m_proc.m_type = "temporary";
  m_proc.m_owner = "atrt";  
  m_proc.m_group = "group";    
  //m_proc.m_cwd
  //m_proc.m_env
  //proc.m_proc.m_stdout = "log.out";
  //proc.m_proc.m_stderr = "2>&1";
  //proc.m_proc.m_runas = proc.m_host->m_user;
  m_proc.m_ulimit = "c:unlimited";
  if((rand() & 15) >= 0){
    m_proc.m_name.assfmt("%d-%d-%s", getpid(), name++, "sleep");
    m_proc.m_path.assign("/bin/sleep");
    m_proc.m_args = "600";
  } else {
    m_proc.m_name.assfmt("%d-%d-%s", getpid(), name++, "test.sh");
    m_proc.m_path.assign("/home/jonas/run/cpcd/test.sh");
    m_proc.m_args = "600";
  }
  g_procs.push_back(m_proc);
  
  Properties reply;
  if(g_client.define_process(g_procs.back(), reply) != 0){
    ndbout_c("define %s -> ERR", m_proc.m_name.c_str());
    reply.print();
    ABORT();
  }
  ndbout_c("define %s -> %d", m_proc.m_name.c_str(), m_proc.m_id);
}

void start(SimpleCpcClient::Process & p){
  Properties reply;
  if(g_client.start_process(p.m_id, reply) != 0){
    reply.print();
    ABORT();
  }
}

void stop(SimpleCpcClient::Process & p){
  Properties reply;
  if(g_client.stop_process(p.m_id, reply) != 0){
    reply.print();
    ABORT();
  }
}

void undefine(SimpleCpcClient::Process & p){
  Properties reply;
  if(g_client.undefine_process(p.m_id, reply) != 0){
    reply.print();
    ABORT();  
  }
}

void list(){
  Properties reply;
  Vector<SimpleCpcClient::Process> procs;
  if(g_client.list_processes(procs, reply) != 0){
    reply.print();
    ABORT();
  }

  for(Uint32 i = 0; i<procs.size(); i++){
    SimpleCpcClient::Process * p = find(procs[i].m_id);
    if(p != 0){
      p->m_status = procs[i].m_status;
    }
  }
}
SimpleCpcClient::Process* find(int id){
  for(Uint32 i = 0; i<g_procs.size(); i++){
    if(g_procs[i].m_id == id)
      return &g_procs[i];
  }
  return 0;
}
