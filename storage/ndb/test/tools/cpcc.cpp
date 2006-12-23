/* Copyright (C) 2003 MySQL AB

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


#include <ndb_global.h>
#include <getarg.h>
#include "CpcClient.hpp"
#include <NdbEnv.h>

#define DEFAULT_PORT 1234
#define ENV_HOSTS "NDB_CPCC_HOSTS"

struct settings {
  int m_longl;
  short m_port;
} g_settings = { 0 , DEFAULT_PORT };

Vector<SimpleCpcClient*> g_hosts;
int connect(Vector<SimpleCpcClient*>&);

class Expression {
public:
  virtual bool evaluate(SimpleCpcClient*, const SimpleCpcClient::Process &)= 0;
};

int for_each(Vector<SimpleCpcClient*>& list, Expression &);
int start_stop(const char * cmd, Vector<SimpleCpcClient*>& list, 
	       Vector<Vector<Uint32> >& procs);

class True : public Expression {
public:
  virtual bool evaluate(SimpleCpcClient*, const SimpleCpcClient::Process & p){
    return true;
  }
};

class FieldEQ : public Expression {
  BaseString m_field;
  BaseString m_value;
public:
  FieldEQ(const BaseString & field, const BaseString & value){
    m_field = field;
    m_value = value;
  }
  virtual ~FieldEQ(){}

  virtual bool evaluate(SimpleCpcClient*, const SimpleCpcClient::Process & p){
    BaseString v;
    if(m_field == "name") v = p.m_name;
  
    if(m_field == "type") v = p.m_type;
    if(m_field == "status") v = p.m_status;
    if(m_field == "owner") v = p.m_owner;
    if(m_field == "group") v = p.m_group;
    if(m_field == "path") v = p.m_path;
    if(m_field == "args") v = p.m_args;
    if(m_field == "env") v = p.m_env;
    if(m_field == "cwd") v = p.m_cwd;
    
    if(m_field == "stdin") v = p.m_stdin;
    if(m_field == "stdout") v = p.m_stdout;
    if(m_field == "stderr") v = p.m_stderr;
    
    return v == m_value;
  }
};

class Match : public Expression {
  Expression & m_cond;
  Expression & m_apply;
public:
  Match(Expression& condition, Expression & rule)
    : m_cond(condition), m_apply(rule) {
  }
  virtual ~Match(){}

  virtual bool evaluate(SimpleCpcClient* c,const SimpleCpcClient::Process & p){
    if(m_cond.evaluate(c, p))
      return m_apply.evaluate(c, p);
    return false;
  }
};

class Operate : public Expression {
  const char * cmd;
  SimpleCpcClient * host;
  settings & sets;
public:
  Operate(const char * c, settings & s) : sets(s) {
    cmd = c;
    host = 0;
  }
  
  virtual bool evaluate(SimpleCpcClient*, const SimpleCpcClient::Process & p);
};

class ProcEQ : public Expression {
  SimpleCpcClient * host;
  Uint32 id;
public:
  ProcEQ(SimpleCpcClient* h, Uint32 i){
    host = h; id = i;
  }

  virtual bool evaluate(SimpleCpcClient* c,const SimpleCpcClient::Process & p){
    return p.m_id == (int)id && c == host;
  }
};

class OrExpr : public Expression {
  Expression * m_rule;
  Vector<Expression *> m_cond;
  bool on_empty;
public:
  OrExpr(Expression * rule, bool onEmp = true){
    m_rule = rule;
    on_empty = onEmp;
  }

  virtual ~OrExpr(){}

  virtual bool evaluate(SimpleCpcClient* c, const SimpleCpcClient::Process & p){
    bool run = on_empty;
    for(size_t i = 0; i<m_cond.size(); i++){
      if(m_cond[i]->evaluate(c, p)){
	run = true;
	break;
      }
    }
    if(run)
      return m_rule->evaluate(c, p);
    return false;
  }

  void push_back(Expression * expr){
    m_cond.push_back(expr);
  }
};

void
add_host(Vector<SimpleCpcClient*> & hosts, BaseString tmp){
  Vector<BaseString> split;
  tmp.split(split, ":");
  
  short port = g_settings.m_port;
  if(split.size() > 1)
    port = atoi(split[1].c_str());
  
  hosts.push_back(new SimpleCpcClient(split[0].c_str(), port));
}

void
add_hosts(Vector<SimpleCpcClient*> & hosts, BaseString list){
  Vector<BaseString> split;
  list.split(split);
  for(size_t i = 0; i<split.size(); i++){
    add_host(hosts, split[i]);
  }
}

int 
main(int argc, const char** argv){
  ndb_init();
  int help = 0;
  const char *cmd=0, *name=0, *group=0, *owner=0;
  int list = 0, start = 0, stop = 0, rm = 0;
  struct getargs args[] = {
    { "cmd", 'c', arg_string, &cmd, "command", "command to run (default ls)" }
    ,{ "name", 'n', arg_string, &name, 
       "apply command for all processes with name" }
    ,{ "group", 'g', arg_string, &group, 
       "apply command for all processes in group" }
    ,{ "owner", 'g', arg_string, &owner,
       "apply command for all processes with owner" }
    ,{ "long", 'l', arg_flag, &g_settings.m_longl, "long", "long listing"}
    ,{ "usage", '?', arg_flag, &help, "Print help", "" }
    ,{ "ls",  0, arg_flag, &list, "-c list", "list process(es)" }
    ,{ "start", 0, arg_flag, &start, "-c start", "start process(es)" }
    ,{ "stop",  0, arg_flag, &stop, "-c stop", "stop process(es)" }
    ,{ "rm",    0, arg_flag, &rm, "-c rm", "undefine process(es)" }
  };
  const int num_args = 10;
  int i; 
  int optind = 0;
  char desc[] = "[host:[port]]\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || help) {
    arg_printusage(args, num_args, argv[0], desc);
    return 1;
  }

  if(list + start + stop + rm > 1){
    ndbout_c("Can only specify one command");
    arg_printusage(args, num_args, argv[0], desc);
    return 1;
  }
  
  if(list) cmd = "list";
  if(start) cmd = "start";
  if(stop) cmd = "stop";
  if(rm) cmd = "rm";
  if(!cmd) cmd = "list";
  
  Expression * m_expr = 0;

  for(i = optind; i<argc; i++){
    add_host(g_hosts, argv[i]);
  }

  OrExpr * orE = new OrExpr(new Operate(cmd, g_settings), true);
  m_expr = orE;
  for(i = optind; i<argc; i++){
    BaseString tmp(argv[i]);
    Vector<BaseString> split;
    tmp.split(split, ":");
    
    if(split.size() > 2){
      Uint32 id = atoi(split[2].c_str());
      orE->push_back(new ProcEQ(g_hosts[i-optind], id));	
    }
  }

  if(g_hosts.size() == 0){
    char buf[1024];
    if(NdbEnv_GetEnv(ENV_HOSTS, buf, sizeof(buf))){
      add_hosts(g_hosts, BaseString(buf));
    }
  }
  
  if(g_hosts.size() == 0){
    g_hosts.push_back(new SimpleCpcClient("localhost", g_settings.m_port));
  }
  
  if(group != 0){
    Expression * tmp = new FieldEQ("group", group);
    m_expr = new Match(* tmp, * m_expr);
  }
  
  if(name != 0){
    Expression * tmp = new FieldEQ("name", name);
    m_expr = new Match(* tmp, * m_expr);
  }

  if(owner != 0){
    Expression * tmp = new FieldEQ("owner", owner);
    m_expr = new Match(* tmp, * m_expr);
  }

  connect(g_hosts);
  for_each(g_hosts, * m_expr);
  
  return 0;
}

int
connect(Vector<SimpleCpcClient*>& list){
  for(size_t i = 0; i<list.size(); i++){
    if(list[i]->connect() != 0){
      ndbout_c("Failed to connect to %s:%d", 
	       list[i]->getHost(), list[i]->getPort());
      delete list[i]; list[i] = 0;
    }
  }
  return 0;
}

int
for_each(Vector<SimpleCpcClient*>& list, Expression & expr){
  for(size_t i = 0; i<list.size(); i++){
    if(list[i] == 0)
      continue;
    Properties p;
    Vector<SimpleCpcClient::Process> procs;
    if(list[i]->list_processes(procs, p) != 0){
      ndbout << "Failed to list processes on " 
	     << list[i]->getHost() << ":" << list[i]->getPort() << endl;
    }
    for(size_t j = 0; j<procs.size(); j++)
      expr.evaluate(list[i], procs[j]);
  }
  return 0;
}

bool
Operate::evaluate(SimpleCpcClient* c, const SimpleCpcClient::Process & pp){
  Uint32 id = pp.m_id;
  Properties p;
  int res;

  if(strcasecmp(cmd, "start") == 0)
    res = c->start_process(id, p);
  else if(strcasecmp(cmd, "stop") == 0)
    res = c->stop_process(id, p);
  else if(strcasecmp(cmd, "rm") == 0)
    res = c->undefine_process(id, p);
  else if(strcasecmp(cmd, "list") == 0){
    if(!sets.m_longl){
      if(host != c){
	ndbout_c("--- %s:%d", c->getHost(), c->getPort());
	host = c;
      }
    }
    
    char s = 0;
    const char * status = pp.m_status.c_str();
    if(strcmp(status, "stopped") == 0)  s = '-';
    if(strcmp(status, "starting") == 0) s = 's';
    if(strcmp(status, "running") == 0)  s = 'r';
    if(strcmp(status, "stopping") == 0)  s = 'k';    
    if(s == 0) s = '?';

    if(!sets.m_longl){
      ndbout_c("%c%c\t%d\t%s\t%s\t%s(%s)", 
	       s, 
	       pp.m_type.c_str()[0], id, pp.m_owner.c_str(), 
	       pp.m_group.c_str(), pp.m_name.c_str(), pp.m_path.c_str());
    } else {
      ndbout_c("%c%c %s:%d:%d %s %s %s(%s)", 
	       s, pp.m_type.c_str()[0], c->getHost(), c->getPort(),
	       id, pp.m_owner.c_str(), pp.m_group.c_str(), 
	       pp.m_name.c_str(), pp.m_path.c_str());
    }
    return true;
  }
  
  if(res != 0){
    BaseString msg;
    p.get("errormessage", msg);
    ndbout_c("Failed to %s %d on %s:%d - %s",
	     cmd, id, 
	     c->getHost(), c->getPort(), msg.c_str());
    return false;
  }
  
  return true;
}

template class Vector<Expression*>;
template class Vector<SimpleCpcClient*>;
