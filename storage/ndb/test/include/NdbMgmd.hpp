/* Copyright (C) 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDB_MGMD_HPP
#define NDB_MGMD_HPP

#include <mgmapi.h>
#include "../../src/mgmapi/mgmapi_internal.h"

#include <BaseString.hpp>
#include <Properties.hpp>

#include <OutputStream.hpp>
#include <SocketInputStream2.hpp>

#include "../../src/mgmsrv/Config.hpp"

#include <InputStream.hpp>

class NdbMgmd {
  BaseString m_connect_str;
  NdbMgmHandle m_handle;
  Uint32 m_nodeid;
  bool m_verbose;
  unsigned int m_timeout;
  NDB_SOCKET_TYPE m_event_socket;
  
  void error(const char* msg, ...) ATTRIBUTE_FORMAT(printf, 2, 3)
  {
    if (!m_verbose)
      return;

    va_list args;
    printf("NdbMgmd::");
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");

    if (m_handle){
      ndbout_c(" error: %d, line: %d, desc: %s",
               ndb_mgm_get_latest_error(m_handle),
               ndb_mgm_get_latest_error_line(m_handle),
               ndb_mgm_get_latest_error_desc(m_handle));
    }
  }
public:
  NdbMgmd() :
    m_handle(NULL), m_nodeid(0), m_verbose(true), m_timeout(0)
    {
      const char* connect_string= getenv("NDB_CONNECTSTRING");
      if (connect_string)
        m_connect_str.assign(connect_string);
    }

  ~NdbMgmd()
  {
    close();
  }

  void close(void)
  {
    if (m_handle)
    {
      ndb_mgm_disconnect_quiet(m_handle);
      ndb_mgm_destroy_handle(&m_handle);
      m_handle = NULL;
    }
  }

  NdbMgmHandle handle(void) const {
    return m_handle;
  }

  NDB_SOCKET_TYPE socket(void) const {
    return _ndb_mgm_get_socket(m_handle);
  }

  NodeId nodeid(void) const {
    return m_nodeid;
  }

  const char* getConnectString() const {
    return m_connect_str.c_str();
  }

  void setConnectString(const char* connect_str) {
    m_connect_str.assign(connect_str);
  }

  bool set_timeout(unsigned int timeout) {
    m_timeout = timeout;
    if (m_handle &&
        ndb_mgm_set_timeout(m_handle, timeout) != 0)
    {
      error("set_timeout: failed to set timeout on handle");
      return false;
    }
    return true;
  }

  void verbose(bool yes = true){
    m_verbose= yes;
  }

  int last_error(void) const {
    return ndb_mgm_get_latest_error(m_handle);
  }

  const char* last_error_message(void) const {
    return ndb_mgm_get_latest_error_desc(m_handle);
  }

  bool connect(const char* connect_string = NULL,
               int num_retries = 0, int retry_delay_in_seconds = 0) {
    assert(m_handle == NULL);
    m_handle= ndb_mgm_create_handle();
    if (!m_handle){
      error("connect: ndb_mgm_create_handle failed");
      return false;
    }

    if (ndb_mgm_set_connectstring(m_handle,
                                  connect_string ?
                                  connect_string : getConnectString()) != 0){
      error("connect: ndb_mgm_set_connectstring failed");
      return false;
    }

    if (m_timeout > 0 &&
        ndb_mgm_set_timeout(m_handle, m_timeout) != 0){
      error("connect: ndb_mgm_set_timeout failed");
      return false;
    }

    if (ndb_mgm_connect(m_handle,num_retries,retry_delay_in_seconds,0) != 0){
      error("connect: ndb_mgm_connect failed");
      return false;
    }

  // Handshake with the server to make sure it's really there
    int major, minor, build;
    char buf[16];
    if (ndb_mgm_get_version(m_handle, &major, &minor, &build,
                            sizeof(buf), buf) != 1)
    {
        error("connect: ndb_get_version failed");
        return false;
    }
    //printf("connected to ndb_mgmd version %d.%d.%d\n",
    //        major, minor, build);

    if ((m_nodeid = ndb_mgm_get_mgmd_nodeid(m_handle)) == 0){
      error("connect: could not get nodeid of connected mgmd");
      return false;
    }

    return true;
  }

  bool is_connected(void) {
    if (!m_handle){
      error("is_connected: no handle");
      return false;
    }
    if (!ndb_mgm_is_connected(m_handle)){
      error("is_connected: not connected");
      return false;
    }
    return true;
  }

  bool disconnect(void) {
    if (ndb_mgm_disconnect(m_handle) != 0){
      error("disconnect: ndb_mgm_disconnect failed");
      return false;
    }

    ndb_mgm_destroy_handle(&m_handle);
    m_handle = NULL;

    return true;
  }

  bool restart(bool abort = false) {
    if (!is_connected()){
      error("restart: not connected");
      return false;
    }

    int disconnect= 0;
    int node_list= m_nodeid;
    int restarted= ndb_mgm_restart3(m_handle,
                                    1,
                                    &node_list,
                                    false, /* initial */
                                    false, /* nostart */
                                    abort,
                                    &disconnect);

    if (restarted != 1){
      error("restart: failed to restart node %d, restarted: %d",
            m_nodeid, restarted);
      return false;
    }
    return true;
  }

  bool call(const char* cmd, const Properties& args,
            const char* cmd_reply, Properties& reply,
            const char* bulk = NULL,
            bool name_value_pairs = true){

    if (!is_connected()){
      error("call: not connected");
      return false;
    }

    SocketOutputStream out(socket());

    if (out.println("%s", cmd)){
      error("call: println failed at line %d", __LINE__);
      return false;
    }

    Properties::Iterator iter(&args);
    const char *name;
    while((name = iter.next()) != NULL) {
      PropertiesType t;
      Uint32 val_i;
      Uint64 val_64;
      BaseString val_s;

      args.getTypeOf(name, &t);
      switch(t) {
      case PropertiesType_Uint32:
	args.get(name, &val_i);
	if (out.println("%s: %d", name, val_i)){
          error("call: println failed at line %d", __LINE__);
          return false;
        }
	break;
      case PropertiesType_Uint64:
	args.get(name, &val_64);
	if (out.println("%s: %Ld", name, val_64)){
          error("call: println failed at line %d", __LINE__);
          return false;
        }
	break;
      case PropertiesType_char:
	args.get(name, val_s);
	if (out.println("%s: %s", name, val_s.c_str())){
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

    // Emtpy line terminates argument list
    if (out.print("\n")){
      error("call: print('\n') failed at line %d", __LINE__);
      return false;
    }

    // Send any bulk data
    if (bulk)
    {
      if (out.write(bulk, strlen(bulk)) >= 0)
      {
        if (out.write("\n", 1) < 0)
        {
          error("call: print('<bulk>') failed at line %d", __LINE__);
          return false;
        }
      }
    }

    BaseString buf;
    SocketInputStream2 in(socket());
    if (cmd_reply)
    {
      // Read the reply header and compare against "cmd_reply"
      if (!in.gets(buf)){
        error("call: could not read reply command");
        return false;
      }

      // 1. Check correct reply header
      if (buf != cmd_reply){
        error("call: unexpected reply command, expected: '%s', got '%s'",
              cmd_reply, buf.c_str());
        return false;
      }
    }

    // 2. Read lines until empty line
    int line = 1;
    while(in.gets(buf)){

      // empty line -> end of reply
      if (buf == "")
        return true;

      if (name_value_pairs)
      {
        // 3a. Read colon separated name value pair, split
        // the name value pair on first ':'
        Vector<BaseString> name_value_pair;
        if (buf.split(name_value_pair, ":", 2) != 2){
          error("call: illegal name value pair '%s' received", buf.c_str());
          return false;
        }

        reply.put(name_value_pair[0].trim(" ").c_str(),
                  name_value_pair[1].trim(" ").c_str());
      }
      else
      {
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


  bool get_config(Config& config){

    if (!is_connected()){
      error("get_config: not connected");
      return false;
    }

    struct ndb_mgm_configuration* conf =
      ndb_mgm_get_configuration(m_handle,0);
    if (!conf) {
      error("get_config: ndb_mgm_get_configuration failed");
      return false;
    }

    config.m_configValues= conf;
    return true;
  }

  bool set_config(Config& config){

    if (!is_connected()){
      error("set_config: not connected");
      return false;
    }

    if (ndb_mgm_set_configuration(m_handle,
                                  config.values()) != 0)
    {
      error("set_config: ndb_mgm_set_configuration failed");
      return false;
    }
    return true;
  }

  bool end_session(void){
    if (!is_connected()){
      error("end_session: not connected");
      return false;
    }

    if (ndb_mgm_end_session(m_handle) != 0){
      error("end_session: ndb_mgm_end_session failed");
      return false;
    }
    return true;
  }

  bool subscribe_to_events(void)
  {
    if (!is_connected())
    {
      error("subscribe_to_events: not connected");
      return false;
    }
    
    int filter[] = 
    {
      15, NDB_MGM_EVENT_CATEGORY_STARTUP,
      15, NDB_MGM_EVENT_CATEGORY_SHUTDOWN,
      15, NDB_MGM_EVENT_CATEGORY_STATISTIC,
      15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT,
      15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
      15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
      15, NDB_MGM_EVENT_CATEGORY_BACKUP,
      15, NDB_MGM_EVENT_CATEGORY_CONGESTION,
      15, NDB_MGM_EVENT_CATEGORY_DEBUG,
      15, NDB_MGM_EVENT_CATEGORY_INFO,
      0
    };

#ifdef NDB_WIN
    m_event_socket.s = ndb_mgm_listen_event(m_handle, filter); 
#else
    m_event_socket.fd = ndb_mgm_listen_event(m_handle, filter);
#endif
    
    return my_socket_valid(m_event_socket);
  }

  bool get_next_event_line(char* buff, int bufflen,
                          int timeout_millis)
  {
    if (!is_connected())
    {
      error("get_next_event_line: not connected");
      return false;
    }
    
    if (!my_socket_valid(m_event_socket))
    {
      error("get_next_event_line: not subscribed");
      return false;
    }

    SocketInputStream stream(m_event_socket, timeout_millis);
    
    const char* result = stream.gets(buff, bufflen);
    if (result && strlen(result))
    {
      return true;
    }
    else
    {
      if (stream.timedout())
      {
        error("get_next_event_line: stream.gets timed out");
        return false;
      }
    }
    
    error("get_next_event_line: error from stream.gets()");
    return false;
  }
  

  // Pretty printer for 'ndb_mgm_node_type'
  class NodeType {
    BaseString m_str;
  public:
    NodeType(Uint32 node_type) {
      const char* str= NULL;
      const char* alias=
        ndb_mgm_get_node_type_alias_string((ndb_mgm_node_type)node_type, &str);
      m_str.assfmt("%s(%s)", alias, str);
    }

    const char* c_str() { return m_str.c_str(); }
  };
};

#endif
