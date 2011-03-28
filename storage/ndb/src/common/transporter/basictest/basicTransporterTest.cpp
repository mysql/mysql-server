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

#include "TransporterRegistry.hpp"
#include "TransporterDefinitions.hpp"
#include "TransporterCallback.hpp"
#include <RefConvert.hpp>

#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>

int basePortTCP = 17000;

SCI_TransporterConfiguration sciTemplate = {
  8000, 
       // Packet size
  2500000,      // Buffer size
  2,           // number of adapters
  1,           // remote node id SCI 
  2,           // Remote node Id SCI
  0,           // local ndb node id (server)
  0,           // remote ndb node id (client)
  0,              // byteOrder;
  false,          // compression;
  true,           // checksum;
  true            // signalId;
};

TCP_TransporterConfiguration tcpTemplate = {
  17000,          // port; 
  "",             // remoteHostName;
  "",             // localhostname
  2,              // remoteNodeId;
  1,              // localNodeId;
  10000,          // sendBufferSize - Size of SendBuffer of priority B 
  10000,          // maxReceiveSize - Maximum no of bytes to receive
  0,              // byteOrder;
  false,          // compression;
  true,           // checksum;
  true            // signalId;
};

SHM_TransporterConfiguration shmTemplate = {
  0,      //remoteNodeId
  0,      //localNodeId;
  false,  //compression
  true,   //checksum;
  true,   //signalId;
  0,      //byteOrder;
  123,    //shmKey;
  2500000 //shmSize;
};

TransporterRegistry *tReg = 0;

#include <signal.h>

extern "C"
void
signalHandler(int signo){
  ::signal(13, signalHandler);
  char buf[255];
  sprintf(buf,"Signal: %d\n", signo);
  ndbout << buf << endl;
}

void 
usage(const char * progName){
  ndbout << "Usage: " << progName << " <type> localNodeId localHostName" 
	 << " remoteHostName1 remoteHostName2" << endl;
  ndbout << "  type = shm tcp ose sci" << endl;
  ndbout << "  localNodeId - 1 to 3" << endl;
}

typedef void (* CreateTransporterFunc)(void * conf, 
				       NodeId localNodeId,
				       NodeId remoteNodeId,
				       const char * localHostName,
				       const char * remoteHostName);

void createSCITransporter(void *, NodeId, NodeId, const char *, const char *);
void createTCPTransporter(void *, NodeId, NodeId, const char *, const char *);
void createSHMTransporter(void *, NodeId, NodeId, const char *, const char *);

int signalReceived[4];

int
main(int argc, const char **argv){
  
  signalHandler(0);
  
  for(int i = 0; i<4; i++)
    signalReceived[i] = 0;
  
  if(argc < 5){
    usage(argv[0]);
    return 0;
  }

  Uint32 noOfConnections     = 0;
  const char * progName      = argv[0];
  const char * type          = argv[1];
  const NodeId localNodeId   = atoi(argv[2]);
  const char * localHostName = argv[3];
  const char * remoteHost1   = argv[4];
  const char * remoteHost2   = NULL;
  
  if(argc == 5)
    noOfConnections = 1;
  else {
    noOfConnections = 2;
    remoteHost2 = argv[5];
  }
  
  if(localNodeId < 1 || localNodeId > 3){
    ndbout << "localNodeId = " << localNodeId << endl << endl;
    usage(progName);
    return 0;
  }
  
  ndbout << "-----------------" << endl;
  ndbout << "localNodeId:           " << localNodeId << endl;
  ndbout << "localHostName:         " << localHostName << endl;
  ndbout << "remoteHost1 (node " << (localNodeId == 1?2:1) << "): " 
	 << remoteHost1 << endl;
  if(noOfConnections == 2){
    ndbout << "remoteHost2 (node " << (localNodeId == 3?2:3) << "): " 
	   << remoteHost2 << endl;
  }
  ndbout << "-----------------" << endl;
  
  void * confTemplate = 0;
  CreateTransporterFunc func = 0;

  if(strcasecmp(type, "tcp") == 0){
    func = createTCPTransporter;
    confTemplate = &tcpTemplate;
  } else if(strcasecmp(type, "sci") == 0){
    func = createSCITransporter;
    confTemplate = &sciTemplate;
  } else if(strcasecmp(type, "shm") == 0){
    func = createSHMTransporter;
    confTemplate = &shmTemplate;
  } else {
    ndbout << "Unsupported transporter type" << endl;
    return 0;
  }
  
  ndbout << "Creating transporter registry" << endl;
  tReg = new TransporterRegistry;
  tReg->init(localNodeId);
  
  switch(localNodeId){
  case 1:
    (* func)(confTemplate, 1, 2, localHostName, remoteHost1);
    if(noOfConnections == 2)
      (* func)(confTemplate, 1, 3, localHostName, remoteHost2);
    break;
  case 2:
    (* func)(confTemplate, 2, 1, localHostName, remoteHost1);
    if(noOfConnections == 2)
      (* func)(confTemplate, 2, 3, localHostName, remoteHost2);
    break;
  case 3:
    (* func)(confTemplate, 3, 1, localHostName, remoteHost1);
    if(noOfConnections == 2)
      (* func)(confTemplate, 3, 2, localHostName, remoteHost2);
    break;
  }
  
  ndbout << "Doing startSending/startReceiving" << endl;
  tReg->startSending();
  tReg->startReceiving();
  
  ndbout << "Connecting" << endl;
  tReg->setPerformState(PerformConnect);
  tReg->checkConnections();
  
  unsigned sum = 0;
  do {
    sum = 0;
    for(int i = 0; i<4; i++)
      sum += signalReceived[i];
    
    tReg->checkConnections();
    
    tReg->external_IO(500);
    NdbSleep_MilliSleep(500);

    ndbout << "In main loop" << endl;
  } while(sum != 2*noOfConnections);
  
  ndbout << "Doing setPerformState(Disconnect)" << endl;
  tReg->setPerformState(PerformDisconnect);
  
  ndbout << "Doing checkConnections()" << endl;
  tReg->checkConnections();
  
  ndbout << "Sleeping 3 secs" << endl;
  NdbSleep_SecSleep(3);
  
  ndbout << "Deleting transporter registry" << endl;
  delete tReg; tReg = 0;
  
  return 0;
}

void
checkData(SignalHeader * const header, Uint8 prio, Uint32 * const theData,
	  LinearSectionPtr ptr[3]){
  Uint32 expectedLength = 0;
  if(prio == 0)
    expectedLength = 17;
  else
    expectedLength = 19;

  if(header->theLength != expectedLength){
    ndbout << "Unexpected signal length: " << header->theLength 
	   << " expected: " << expectedLength << endl;
    abort();
  }

  if(header->theVerId_signalNumber != expectedLength + 1)
    abort();

  if(header->theReceiversBlockNumber != expectedLength + 2)
    abort();

  if(refToBlock(header->theSendersBlockRef) != expectedLength + 3)
    abort();

  if(header->theSendersSignalId != expectedLength + 5)
    abort();

  if(header->theTrace != expectedLength + 6)
    abort();

  if(header->m_noOfSections != (prio == 0 ? 0 : 1))
    abort();

  if(header->m_fragmentInfo != (prio + 1))
    abort();
  
  Uint32 dataWordStart = header->theLength ;
  for(unsigned i = 0; i<header->theLength; i++){
    if(theData[i] != i){ //dataWordStart){
      ndbout << "data corrupt!\n" << endl;
      abort();
    }
    dataWordStart ^= (~i*i);
  }

  if(prio != 0){
    ndbout_c("Found section");
    if(ptr[0].sz != header->theLength)
      abort();

    if(memcmp(ptr[0].p, theData, (ptr[0].sz * 4)) != 0)
      abort();
  }
}

void
sendSignalTo(NodeId nodeId, int prio){
  SignalHeader sh;
  sh.theLength = (prio == 0 ? 17 : 19); 
  sh.theVerId_signalNumber   = sh.theLength + 1;
  sh.theReceiversBlockNumber = sh.theLength + 2;
  sh.theSendersBlockRef      = sh.theLength + 3;
  sh.theSendersSignalId      = sh.theLength + 4;
  sh.theSignalId             = sh.theLength + 5;
  sh.theTrace                = sh.theLength + 6;
  sh.m_noOfSections          = (prio == 0 ? 0 : 1);
  sh.m_fragmentInfo          = prio + 1;
  
  Uint32 theData[25];

  Uint32 dataWordStart = sh.theLength;
  for(unsigned i = 0; i<sh.theLength; i++){
    theData[i] = i;
    dataWordStart ^= (~i*i);
  }
  ndbout << "Sending prio " << (int)prio << " signal to node: " 
	 << nodeId 
	 << " gsn = " << sh.theVerId_signalNumber << endl;
  
  LinearSectionPtr ptr[3];
  ptr[0].p = &theData[0];
  ptr[0].sz = sh.theLength;

  SendStatus s = tReg->prepareSend(&sh, prio, theData, nodeId, ptr);
  if(s != SEND_OK){
    ndbout << "Send was not ok. Send was: " << s << endl;
  }
}

void
execute(void* callbackObj, 
	SignalHeader * const header, Uint8 prio, Uint32 * const theData, 
	LinearSectionPtr ptr[3]){
  const NodeId nodeId = refToNode(header->theSendersBlockRef);
  
  ndbout << "Recieved prio " << (int)prio << " signal from node: " 
	 << nodeId
	 << " gsn = " << header->theVerId_signalNumber << endl;
  checkData(header, prio, theData, ptr);
  ndbout << " Data is ok!\n" << endl;
  
  signalReceived[nodeId]++;
  
  if(prio == 0)
    sendSignalTo(nodeId, 1);
  else
    tReg->setPerformState(nodeId, PerformDisconnect);
}

void 
copy(Uint32 * & insertPtr, 
     class SectionSegmentPool & thePool, const SegmentedSectionPtr & _ptr){
  abort();
}

void
reportError(void* callbackObj, NodeId nodeId, TransporterError errorCode){
  char buf[255];
  sprintf(buf, "reportError (%d, %x)", nodeId, errorCode);
  ndbout << buf << endl;
  if(errorCode & 0x8000){
    tReg->setPerformState(nodeId, PerformDisconnect);
    abort();
  }
}

/**
 * Report average send theLength in bytes (4096 last sends)
 */
void
reportSendLen(void* callbackObj, NodeId nodeId, Uint32 count, Uint64 bytes){
  char buf[255];
  sprintf(buf, "reportSendLen(%d, %d)", nodeId, (Uint32)(bytes/count));
  ndbout << buf << endl;
}

/**
 * Report average receive theLength in bytes (4096 last receives)
 */
void
reportReceiveLen(void* callbackObj, NodeId nodeId, Uint32 count, Uint64 bytes){
  char buf[255];
  sprintf(buf, "reportReceiveLen(%d, %d)", nodeId, (Uint32)(bytes/count));
  ndbout << buf << endl;
}

/**
 * Report connection established
 */
void
reportConnect(void* callbackObj, NodeId nodeId){
  char buf[255];
  sprintf(buf, "reportConnect(%d)", nodeId);
  ndbout << buf << endl;
  tReg->setPerformState(nodeId, PerformIO);
  
  sendSignalTo(nodeId, 0);
}

/**
 * Report connection broken
 */
void
reportDisconnect(void* callbackObj, NodeId nodeId, Uint32 errNo){
  char buf[255];
  sprintf(buf, "reportDisconnect(%d)", nodeId);
  ndbout << buf << endl;
  if(signalReceived[nodeId] < 2)
    tReg->setPerformState(nodeId, PerformConnect);
}

int
checkJobBuffer() {
  /** 
   * Check to see if jobbbuffers are starting to get full
   * and if so call doJob
   */
  return 0;
}

void
createOSETransporter(void * _conf,
		     NodeId localNodeId,
		     NodeId remoteNodeId,
		     const char * localHostName,
		     const char * remoteHostName){
  ndbout << "Creating OSE transporter from node " 
	 << localNodeId << "(" << localHostName << ") to "
	 << remoteNodeId << "(" << remoteHostName << ")..." << endl;;
  
  OSE_TransporterConfiguration * conf = (OSE_TransporterConfiguration*)_conf;
  
  conf->localNodeId    = localNodeId;
  conf->localHostName  = localHostName;
  conf->remoteNodeId   = remoteNodeId;
  conf->remoteHostName = remoteHostName;
  bool res = tReg->createTransporter(conf);
  if(res)
    ndbout << "... -- Success " << endl;
  else
    ndbout << "... -- Failure " << endl;
}

void
createTCPTransporter(void * _conf,
		     NodeId localNodeId,
		     NodeId remoteNodeId,
		     const char * localHostName,
		     const char * remoteHostName){
  ndbout << "Creating TCP transporter from node " 
	 << localNodeId << "(" << localHostName << ") to "
	 << remoteNodeId << "(" << remoteHostName << ")..." << endl;;
  
  TCP_TransporterConfiguration * conf = (TCP_TransporterConfiguration*)_conf;
  
  int port;
  if(localNodeId == 1 && remoteNodeId == 2) port = basePortTCP + 0;
  if(localNodeId == 1 && remoteNodeId == 3) port = basePortTCP + 1;
  if(localNodeId == 2 && remoteNodeId == 1) port = basePortTCP + 0;
  if(localNodeId == 2 && remoteNodeId == 3) port = basePortTCP + 2;
  if(localNodeId == 3 && remoteNodeId == 1) port = basePortTCP + 1;
  if(localNodeId == 3 && remoteNodeId == 2) port = basePortTCP + 2;
  
  conf->localNodeId    = localNodeId;
  conf->localHostName  = localHostName;
  conf->remoteNodeId   = remoteNodeId;
  conf->remoteHostName = remoteHostName;
  conf->port           = port;
  bool res = tReg->createTransporter(conf);
  if(res)
    ndbout << "... -- Success " << endl;
  else
    ndbout << "... -- Failure " << endl;
}

void
createSCITransporter(void * _conf,
		     NodeId localNodeId,
		     NodeId remoteNodeId,
		     const char * localHostName,
		     const char * remoteHostName){


  ndbout << "Creating SCI transporter from node " 
	 << localNodeId << "(" << localHostName << ") to "
	 << remoteNodeId << "(" << remoteHostName << ")..." << endl;;
  
  
  SCI_TransporterConfiguration * conf = (SCI_TransporterConfiguration*)_conf;

  conf->remoteSciNodeId0= (Uint16)atoi(localHostName);
  conf->remoteSciNodeId1= (Uint16)atoi(remoteHostName);


  conf->localNodeId    = localNodeId;
  conf->remoteNodeId   = remoteNodeId;

  bool res = tReg->createTransporter(conf);
  if(res)
    ndbout << "... -- Success " << endl;
  else
    ndbout << "... -- Failure " << endl;
}

void
createSHMTransporter(void * _conf,
		     NodeId localNodeId,
		     NodeId remoteNodeId,
		     const char * localHostName,
		     const char * remoteHostName){


  ndbout << "Creating SHM transporter from node " 
	 << localNodeId << "(" << localHostName << ") to "
	 << remoteNodeId << "(" << remoteHostName << ")..." << endl;;
  
  
  SHM_TransporterConfiguration * conf = (SHM_TransporterConfiguration*)_conf;

  conf->localNodeId    = localNodeId;
  conf->remoteNodeId   = remoteNodeId;
  
  bool res = tReg->createTransporter(conf);
  if(res)
    ndbout << "... -- Success " << endl;
  else
    ndbout << "... -- Failure " << endl;
}
