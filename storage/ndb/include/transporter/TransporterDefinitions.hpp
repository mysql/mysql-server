/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TransporterDefinitions_H
#define TransporterDefinitions_H

#include <ndb_global.h> 
#include <kernel_types.h> 
#include <NdbOut.hpp>

/**
 * The maximum number of transporters allowed
 * A maximum is needed to be able to allocate the array of transporters
 */
const int MAX_NTRANSPORTERS = 256;

/**
 * The sendbuffer limit after which the contents of the buffer is sent
 */
const int TCP_SEND_LIMIT = 64000;

enum SendStatus { 
  SEND_OK = 0, 
  SEND_BLOCKED = 1, 
  SEND_DISCONNECTED = 2, 
  SEND_BUFFER_FULL = 3,
  SEND_MESSAGE_TOO_BIG = 4,
  SEND_UNKNOWN_NODE = 5
};

enum TransporterType {
  tt_TCP_TRANSPORTER = 1,
  tt_SCI_TRANSPORTER = 2,
  tt_SHM_TRANSPORTER = 3
};

/**
 * Maximum message sizes
 * ---------------------
 * Maximum byte sizes for sent and received messages.
 * The maximum send message size is temporarily smaller than 
 * the maximum receive message size to support online
 * upgrade
 * Maximum received size increased in :
 *   mysql-5.1-telco-6.3.18 from 16516 bytes to 32768
 * Maximum send size increased in :
 *   mysql-5.1-telco-6.4.0 from 16516 bytes to 32768
 *
 * Therefore mysql-5.1-telco-6.4.0 cannot safely communicate 
 * with nodes at versions lower than mysql-5.1-telco-6.3.18 
 * 
 */
const Uint32 MAX_RECV_MESSAGE_BYTESIZE = 32768;
const Uint32 MAX_SEND_MESSAGE_BYTESIZE = 32768;

/**
 * TransporterConfiguration
 *
 * used for setting up a transporter. the union member specific is for
 * information specific to a transporter type.
 */
struct TransporterConfiguration {
  Int32 s_port; // negative port number implies dynamic port
  const char *remoteHostName;
  const char *localHostName;
  NodeId remoteNodeId;
  NodeId localNodeId;
  NodeId serverNodeId;
  bool checksum;
  bool signalId;
  bool isMgmConnection; // is a mgm connection, requires transforming
  TransporterType type;

  union { // Transporter specific configuration information

    struct {
      Uint32 sendBufferSize;     // Size of SendBuffer of priority B 
      Uint32 maxReceiveSize;     // Maximum no of bytes to receive
      Uint32 tcpSndBufSize;
      Uint32 tcpRcvBufSize;
      Uint32 tcpMaxsegSize;
      Uint32 tcpOverloadLimit;
    } tcp;
    
    struct {
      Uint32 shmKey;
      Uint32 shmSize;
      int    signum;
    } shm;

    struct {
      Uint32 sendLimit;        // Packet size
      Uint32 bufferSize;       // Buffer size
      
      Uint32 nLocalAdapters;   // 1 or 2, the number of adapters on local host
      
      Uint32 remoteSciNodeId0; // SCInodeId for adapter 1
      Uint32 remoteSciNodeId1; // SCInodeId for adapter 2
    } sci;
  };
};

struct SignalHeader {	
  Uint32 theVerId_signalNumber;    // 4 bit ver id - 16 bit gsn
  Uint32 theReceiversBlockNumber;  // Only 16 bit blocknum  
  Uint32 theSendersBlockRef;
  Uint32 theLength;
  Uint32 theSendersSignalId;
  Uint32 theSignalId;
  Uint16 theTrace;
  Uint8  m_noOfSections;
  Uint8  m_fragmentInfo;
}; /** 7x4 = 28 Bytes */

struct LinearSectionPtr {
  Uint32 sz;
  Uint32 * p;
};

struct SegmentedSectionPtrPOD
{
  Uint32 sz;
  Uint32 i;
  struct SectionSegment * p;

  void setNull() { p = 0;}
  bool isNull() const { return p == 0;}
  inline SegmentedSectionPtrPOD& assign(struct SegmentedSectionPtr&);
};

struct SegmentedSectionPtr {
  Uint32 sz;
  Uint32 i;
  struct SectionSegment * p;

  SegmentedSectionPtr() {}
  SegmentedSectionPtr(Uint32 sz_arg, Uint32 i_arg,
                      struct SectionSegment *p_arg)
    :sz(sz_arg), i(i_arg), p(p_arg)
  {}
  SegmentedSectionPtr(const SegmentedSectionPtrPOD & src)
    :sz(src.sz), i(src.i), p(src.p)
  {}

  void setNull() { p = 0;}
  bool isNull() const { return p == 0;}
};

inline
SegmentedSectionPtrPOD&
SegmentedSectionPtrPOD::assign(struct SegmentedSectionPtr& src)
{
  this->i = src.i;
  this->p = src.p;
  this->sz = src.sz;
  return *this;
}

/* Abstract interface for iterating over
 * words in a section
 */
struct GenericSectionIterator {
  virtual ~GenericSectionIterator() {};
  virtual void reset()=0;
  virtual const Uint32* getNextWords(Uint32& sz)=0;
};

struct GenericSectionPtr {
  Uint32 sz;
  GenericSectionIterator* sectionIter;
};

class NdbOut & operator <<(class NdbOut & out, SignalHeader & sh);

#endif // Define of TransporterDefinitions_H
