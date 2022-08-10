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

#ifndef SIGNAL_COUNTER_HPP
#define SIGNAL_COUNTER_HPP

#include <NodeBitmask.hpp>
#include <ErrorReporter.hpp>

class SignalCounter {
  friend struct NodeReceiverGroup;
  
private:
  Uint32 m_count;
  NdbNodeBitmask m_nodes;

public:
  SignalCounter() { clearWaitingFor();}
  void clearWaitingFor();
  
  /**
   * When sending to different node
   */
  void setWaitingFor(NdbNodeBitmask nodes);
  void setWaitingFor(Uint32 nodeId);
  void clearWaitingFor(Uint32 nodeId);
  void forceClearWaitingFor(Uint32 nodeId);
  
  bool isWaitingFor(Uint32 nodeId) const;
  bool done() const;

  const char * getText() const;

  SignalCounter& operator=(const NdbNodeBitmask & bitmask);

  /**
   * When sending to same node
   */
  SignalCounter& operator=(Uint32 count);
  SignalCounter& operator--(int);
  SignalCounter& operator++(int);
  SignalCounter& operator+=(Uint32);
  Uint32 getCount() const;

  const NdbNodeBitmask& getNodeBitmask() const { return m_nodes; }
};

inline
void
SignalCounter::setWaitingFor(NdbNodeBitmask nodes)
{
  m_nodes.assign(nodes);
  m_count = nodes.count();
}

inline
void 
SignalCounter::setWaitingFor(Uint32 nodeId) {
  if(nodeId <= MAX_DATA_NODE_ID && !m_nodes.get(nodeId)){
    m_nodes.set(nodeId);
    m_count++;
    return;
  }
  ErrorReporter::handleAssert("SignalCounter::set", __FILE__, __LINE__);
}

inline
bool
SignalCounter::isWaitingFor(Uint32 nodeId) const {
  return m_nodes.get(nodeId);
}

inline
bool
SignalCounter::done() const {
  return m_count == 0;
}

inline
Uint32
SignalCounter::getCount() const {
  return m_count;
}

inline
void
SignalCounter::clearWaitingFor(Uint32 nodeId) {
  if(nodeId <= MAX_DATA_NODE_ID && m_nodes.get(nodeId) && m_count > 0){
    m_count--;
    m_nodes.clear(nodeId);
    return;
  }
  ErrorReporter::handleAssert("SignalCounter::clearWaitingFor", __FILE__, __LINE__);
}

inline
void
SignalCounter::clearWaitingFor(){
  m_count = 0;
  m_nodes.clear();
}

inline
void
SignalCounter::forceClearWaitingFor(Uint32 nodeId){
  if(isWaitingFor(nodeId)){
    clearWaitingFor(nodeId);
  }
}

inline
SignalCounter&
SignalCounter::operator=(Uint32 count){
  m_count = count;
  m_nodes.clear();
  return * this;
}

inline
SignalCounter&
SignalCounter::operator--(int){
  if(m_count > 0){
    m_count--;
    return * this;
  }
  ErrorReporter::handleAssert("SignalCounter::operator--", __FILE__, __LINE__);
  return * this;
}

inline
SignalCounter&
SignalCounter::operator++(int){
  m_count++;
  return * this;
}

inline
SignalCounter&
SignalCounter::operator+=(Uint32 n){
  m_count += n;
  return * this;
}

inline
const char *
SignalCounter::getText() const {
  static char buf[255];
  static char nodes[NdbNodeBitmask::TextLength+1];
  BaseString::snprintf(buf, sizeof(buf), "[SignalCounter: m_count=%d %s]", m_count, m_nodes.getText(nodes));
  return buf;
}

inline
SignalCounter&
SignalCounter::operator=(const NdbNodeBitmask & bitmask){
  m_nodes = bitmask;
  m_count = bitmask.count();
  return * this;
}

#endif
