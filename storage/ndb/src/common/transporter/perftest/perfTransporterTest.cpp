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
  2000, 
       // Packet size
  2000000,      // Buffer size
  2,           // number of adapters
  1,           // remote node id SCI 
  2,           // Remote node Id SCI
  0,           // local ndb node id (server)
  0,           // remote ndb node id (client)
  0,              // byteOrder;
  false,          // compression;
  true,          // checksum;
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
  25000000 //shmSize;
};


TCP_TransporterConfiguration tcpTemplate = {
  17000,          // port; 
  "",             // remoteHostName;
  "",             // localhostname
  2,              // remoteNodeId;
  1,              // localNodeId;
  25000000,        // sendBufferSize - Size of SendBuffer of priority B 
  5000000,         // maxReceiveSize - Maximum no of bytes to receive
  0,              // byteOrder;
  false,          // compression;
  true,           // checksum;
  true            // signalId;
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
	 << " remoteHostName"
	 << " [<loop count>] [<send buf size>] [<recv buf size>]" << endl;
  ndbout << "  type = shm tcp ose sci" << endl;
  ndbout << "  localNodeId - {1,2}" << endl;
}

typedef void (* CreateTransporterFunc)(void * conf, 
				       NodeId localNodeId,
				       NodeId remoteNodeId,
				       const char * localHostName,
				       const char * remoteHostName,
				       int sendBuf,
				       int recvBuf);

void
createTCPTransporter(void*, NodeId, NodeId, const char*, const char*, int, int);
void
createSHMTransporter(void*, NodeId, NodeId, const char*, const char*, int, int);
void
createSCITransporter(void*, NodeId, NodeId, const char*, const char*, int, int);

struct TestPhase {
  int signalSize;
  int noOfSignals;
  int noOfSignalSent;
  int noOfSignalReceived;
  NDB_TICKS startTime;
  NDB_TICKS stopTime;
  NDB_TICKS accTime;
  int loopCount;
  Uint64 sendLenBytes, sendCount;
  Uint64 recvLenBytes, recvCount;
};

TestPhase testSpec[] = {
   {  1,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of size 1  word
  ,{  1,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals of size 1  word
  ,{  1,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of size 1  word
  ,{  1, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals of size 1  word

  ,{  8,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of size 1  word
  ,{  8,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals of size 1  word
  ,{  8,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of size 1  word
  ,{  8, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals of size 1  word

  ,{ 16,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of size 1  word
  ,{ 16,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals of size 1  word
  ,{ 16,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of size 1  word
  ,{ 16, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals of size 1  word

  ,{ 24,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of size 1  word
  ,{ 24,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals of size 1  word
  ,{ 24,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of size 1  word
  ,{ 24, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals of size 1  word

  ,{  0,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of random size
  ,{  0,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals of random size
  ,{  0,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of random size
  ,{  0, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals of random size

  ,{ 100,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals 
  ,{ 100,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals 
  ,{ 100,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals 
  ,{ 100, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals 

  ,{ 500,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals 
  ,{ 500,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals 
  ,{ 500,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals 
  ,{ 500, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals 

  ,{ 1000,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals 
  ,{ 1000,   100, 0,0, 0,0,0,0,0,0,0 } //   100 signals 
  ,{ 1000,  1000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals 
  ,{ 1000, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals 
};

const int noOfTests = sizeof(testSpec)/sizeof(TestPhase);

Uint32 StaticBuffer[1000];

SendStatus
sendSignalTo(NodeId nodeId, int signalSize, Uint32 count){
  if(signalSize == 0)
    signalSize = (rand() % 25) + 1;

  SignalHeader sh;
  sh.theLength               = (signalSize > 25 ? 25 : signalSize);
  sh.theVerId_signalNumber   = count;
  sh.theReceiversBlockNumber = rand();   
  sh.theSendersBlockRef      = rand(); 
  sh.theSendersSignalId      = rand(); 
  sh.theSignalId             = rand(); 
  sh.theTrace                = rand(); 
  
  Uint32 theData[25];
  for(int i = 0; i<25; i++)
    theData[i] = (i+1) * (Uint32)(&theData[i]);
  
  theData[0] = count;
  LinearSectionPtr ptr[3];
  
  if(signalSize <= 25){
    sh.m_noOfSections = 0;
  } else {
    sh.m_noOfSections = 1;
    ptr[0].sz = signalSize - 25;
    ptr[0].p = &StaticBuffer[0];
  }

  return tReg->prepareSend(&sh, 1, theData, nodeId, ptr);
}

void
reportHeader(){
  ndbout << "#Sigs\tSz\tTime\tSig/sec\tBps\tBps-tot\t"
	 << "s len\tr len" << endl;
}

void
print(char * dst, int i){
  if(i > 1000000){
    const int d = i / 1000000;
    const int r = (i - (d * 1000000)) / 100000;
    if(d < 100)
      sprintf(dst, "%d.%dM", d, r);
    else
      sprintf(dst, "%dM", d);
  } else if(i > 1000){
    const int d = i / 1000;
    const int r = (i - (d * 1000)) / 100;
    if(d < 100)
      sprintf(dst, "%d.%dk", d, r);
    else
      sprintf(dst, "%dk", d);
  } else {
    sprintf(dst, "%d", i);
  }
}

void
printReport(TestPhase & p){
  if(p.accTime > 0) {
    Uint32 secs = (p.accTime/p.loopCount)/1000;
    Uint32 mill = (p.accTime/p.loopCount)%1000;
    char st[255];
    if(secs > 0){
      sprintf(st, "%d.%.2ds", secs, (mill/10));
    } else {
      sprintf(st, "%dms", mill);
    }
  
    Uint32 sps = (1000*p.noOfSignals*p.loopCount)/p.accTime;
    Uint32 dps = ((4000*p.noOfSignals)/p.accTime)*(p.loopCount*p.signalSize);
    Uint32 bps = ((4000*p.noOfSignals)/p.accTime)*(p.loopCount*(p.signalSize+3));
    if(p.signalSize == 0){
      dps = ((4000*p.noOfSignals)/p.accTime)*(p.loopCount*(13));
      bps = ((4000*p.noOfSignals)/p.accTime)*(p.loopCount*(13+3));
    }
    char ssps[255];
    char sbps[255];
    char sdps[255];

    print(ssps, sps);
    print(sbps, bps);
    print(sdps, dps);
    
    
    char buf[255];
    if(p.signalSize != 0){
      BaseString::snprintf(buf, 255,
	       "%d\t%d\t%s\t%s\t%s\t%s\t%d\t%d",
	       p.noOfSignals,
	       4*p.signalSize,
	       st,
	       ssps,
	       sdps,
	       sbps,
	       (int)(p.sendLenBytes / (p.sendCount == 0 ? 1 : p.sendCount)),
	       (int)(p.recvLenBytes / (p.recvCount == 0 ? 1 : p.recvCount)));
    } else {
      BaseString::snprintf(buf, 255,
	       "%d\trand\t%s\t%s\t%s\t%s\t%d\t%d",
	       p.noOfSignals,
	       st,
	       ssps,
	       sdps,
	       sbps,
	       (int)(p.sendLenBytes / (p.sendCount == 0 ? 1 : p.sendCount)),
	       (int)(p.recvLenBytes / (p.recvCount == 0 ? 1 : p.recvCount)));
      
    }
    ndbout << buf << endl;
  }
}

int loopCount = 1;
int sendBufSz = -1;
int recvBufSz = -1;

bool      isClient     = false;
bool      isConnected  = false;
bool      isStarted    = false;
int       currentPhase = 0;
TestPhase allPhases[noOfTests];
Uint32    signalToEcho;
Uint32    signalsEchoed;
NDB_TICKS startTime, stopTime;

void
client(NodeId remoteNodeId){
  isClient = true;

  currentPhase = 0;
  memcpy(allPhases, testSpec, sizeof(testSpec));

  int counter = 0;
  int sigCounter = 0;

  while(true){
    TestPhase * current = &allPhases[currentPhase];
    if(current->noOfSignals == current->noOfSignalSent &&
       current->noOfSignals == current->noOfSignalReceived){

      /**
       * Test phase done
       */
      current->stopTime  = NdbTick_CurrentMillisecond();
      current->accTime  += (current->stopTime - current->startTime);

      NdbSleep_MilliSleep(500 / loopCount);
      
      current->startTime = NdbTick_CurrentMillisecond();
      
      current->noOfSignalSent     = 0;
      current->noOfSignalReceived = 0;

      current->loopCount ++;
      if(current->loopCount == loopCount){

	printReport(allPhases[currentPhase]);

	currentPhase ++;
	if(currentPhase == noOfTests){
	  /**
	   * Now we are done
	   */
	  break;
	}
	NdbSleep_MilliSleep(500);
	current = &allPhases[currentPhase];
	current->startTime = NdbTick_CurrentMillisecond();
      }
    } 
    
    int signalsLeft = current->noOfSignals - current->noOfSignalSent;
    if(signalsLeft > 0){
      for(; signalsLeft > 0; signalsLeft--){
	if(sendSignalTo(remoteNodeId,current->signalSize,sigCounter)== SEND_OK){
	  current->noOfSignalSent++;
	  sigCounter++;
	} else {
	  ndbout << "Failed to send: " << sigCounter << endl;
	  tReg->external_IO(10);
	  break;
	}
      }
    }
    if(counter % 10 == 0)
      tReg->checkConnections();
    tReg->external_IO(0);
    counter++;
  }
}

void 
server(){
  isClient = false;
  
  signalToEcho = 0;
  signalsEchoed = 0;
  for(int i = 0; i<noOfTests; i++)
    signalToEcho += testSpec[i].noOfSignals;
  
  signalToEcho *= loopCount;

  while(signalToEcho > signalsEchoed){
    tReg->checkConnections();
    for(int i = 0; i<10; i++)
      tReg->external_IO(10);
  }
}

int
main(int argc, const char **argv){
  
  const char * progName = argv[0];

  loopCount = 100;
  sendBufSz = -1;
  recvBufSz = -1;
  
  isClient     = false;
  isConnected  = false;
  isStarted    = false;
  currentPhase = 0;

  signalHandler(0);
  
  if(argc < 5){
    usage(progName);
    return 0;
  }
  
  const char * type = argv[1];
  const NodeId localNodeId   = atoi(argv[2]);
  const char * localHostName = argv[3];
  const char * remoteHost1   = argv[4];
  
  if(argc >= 6)
    loopCount = atoi(argv[5]);
  if(argc >= 7)
    sendBufSz = atoi(argv[6]);
  if(argc >= 8)
    recvBufSz = atoi(argv[7]);

  if(localNodeId < 1 || localNodeId > 2){
    ndbout << "localNodeId = " << localNodeId << endl << endl;
    usage(progName);
    return 0;
  }
  
  if(localNodeId == 1)
    ndbout << "-- ECHO CLIENT --" << endl;
  else
    ndbout << "-- ECHO SERVER --" << endl;

  ndbout << "localNodeId:           " << localNodeId << endl;
  ndbout << "localHostName:         " << localHostName << endl;
  ndbout << "remoteHost1 (node " << (localNodeId == 1?2:1) << "): " 
	 << remoteHost1 << endl;
  ndbout << "Loop count: " << loopCount << endl;
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
    (* func)(confTemplate, 1, 2, localHostName, remoteHost1, 
	     sendBufSz, recvBufSz);
    break;
  case 2:
    (* func)(confTemplate, 2, 1, localHostName, remoteHost1,
	     sendBufSz, recvBufSz);
    break;
  }
  
  ndbout << "Doing startSending/startReceiving" << endl;
  tReg->startSending();
  tReg->startReceiving();
  
  ndbout << "Connecting" << endl;
  tReg->setPerformState(PerformConnect);
  tReg->checkConnections();

  if(localNodeId == 1)
    client(2);
  else
    server();
    
  isStarted = false;
  
  ndbout << "Sleep 3 secs" << endl;
  NdbSleep_SecSleep(3);

  ndbout << "Doing setPerformState(Disconnect)" << endl;
  tReg->setPerformState(PerformDisconnect);
  
  ndbout << "Doing checkConnections()" << endl;
  tReg->checkConnections();
    
  ndbout << "Deleting transporter registry" << endl;
  delete tReg; tReg = 0;
  
  return 0;
}

void
execute(void* callbackObj, SignalHeader * const header, Uint8 prio, 
	Uint32 * const theData,
	LinearSectionPtr ptr[3]){
  const NodeId nodeId = refToNode(header->theSendersBlockRef);
 
  if(isClient){
    allPhases[currentPhase].noOfSignalReceived++;
  } else {
    int sleepTime = 10;
    if(theData[0] != signalsEchoed){
      ndbout << "Missing signal theData[0] = " << theData[0] 
	     << " signalsEchoed = " << signalsEchoed << endl;
      ndbout << (* header) << endl;
      abort();
    }
    while(tReg->prepareSend(header, prio, theData, nodeId, ptr) != SEND_OK){
      ndbout << "Failed to echo " << theData[0] << endl;
      NdbSleep_MilliSleep(sleepTime);
      // sleepTime += 10;
    }
    signalsEchoed++;
  }
}

void 
copy(Uint32 * & insertPtr, 
     class SectionSegmentPool & thePool, const SegmentedSectionPtr & _ptr){
  abort();
}

void
reportError(void* callbackObj, NodeId nodeId, TransporterError errorCode){
  char buf[255];
  sprintf(buf, "reportError (%d, %x) in perfTest", nodeId, errorCode);
  ndbout << buf << endl;
  if(errorCode & 0x8000 && errorCode != 0x8014){
    abort(); //tReg->setPerformState(nodeId, PerformDisconnect);
  }
}

/**
 * Report average send theLength in bytes (4096 last sends)
 */
void
reportSendLen(void* callbackObj, NodeId nodeId, Uint32 count, Uint64 bytes){
  allPhases[currentPhase].sendCount    += count;
  allPhases[currentPhase].sendLenBytes += bytes;

  if(!isClient){
    ndbout << "reportSendLen(" << nodeId << ", " 
	   << (bytes/count) << ")" << endl;
  }
}

/**
 * Report average receive theLength in bytes (4096 last receives)
 */
void
reportReceiveLen(void* callbackObj, NodeId nodeId, Uint32 count, Uint64 bytes){
  allPhases[currentPhase].recvCount    += count;
  allPhases[currentPhase].recvLenBytes += bytes;

  if(!isClient){
    ndbout << "reportReceiveLen(" << nodeId << ", " 
	   << (bytes/count) << ")" << endl;
  }
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
  
  if(!isStarted){
    isStarted = true;
    startTime = NdbTick_CurrentMillisecond();
    if(isClient){
      reportHeader();
      allPhases[0].startTime = startTime;
    }
  }
  else{
    // Resend signals that were lost when connection failed
    TestPhase * current = &allPhases[currentPhase];
    current->noOfSignalSent = current->noOfSignalReceived;
  }
}

/**
 * Report connection broken
 */
void
reportDisconnect(void* callbackObj, NodeId nodeId, Uint32 errNo){
  char buf[255];
  sprintf(buf, "reportDisconnect(%d)", nodeId);
  ndbout << buf << endl;
  
  if(isStarted)
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
createSCITransporter(void * _conf,
		     NodeId localNodeId,
		     NodeId remoteNodeId,
		     const char * localHostName,
		     const char * remoteHostName,
		     int sendbuf,
		     int recvbuf) {


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
		     const char * remoteHostName,
		     int sendbuf,
		     int recvbuf) {


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


void
createTCPTransporter(void * _conf,
		     NodeId localNodeId,
		     NodeId remoteNodeId,
		     const char * localHostName,
		     const char * remoteHostName,
		     int sendBuf,
		     int recvBuf){
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

  if(sendBuf != -1){
    conf->sendBufferSize = sendBuf;
  }
  if(recvBuf != -1){
    conf->maxReceiveSize = recvBuf;
  }

  ndbout << "\tSendBufferSize:    " << conf->sendBufferSize << endl;
  ndbout << "\tReceiveBufferSize: " << conf->maxReceiveSize << endl;

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
