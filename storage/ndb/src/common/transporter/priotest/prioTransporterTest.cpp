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

#include "TransporterRegistry.hpp"
#include "TransporterDefinitions.hpp"
#include "TransporterCallback.hpp"
#include <RefConvert.hpp>

#include "prioTransporterTest.hpp"

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
  100000, // shmSize
  0,      // shmKey
  1,           // local ndb node id (server)
  2,           // remote ndb node id (client)
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
  2000000,          // sendBufferSize - Size of SendBuffer of priority B 
  2000,           // maxReceiveSize - Maximum no of bytes to receive
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
  ndbout << "Usage: " << progName << " localNodeId localHostName" 
	 << " remoteHostName"
	 << " [<loop count>] [<send buf size>] [<recv buf size>]" << endl;
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

struct TestPhase {
  int signalSize;
  int noOfSignals;
  int noOfSignalSent;
  int noOfSignalReceived;
  NDB_TICKS startTime;
  NDB_TICKS stopTime;

  NDB_TICKS startTimePrioA;
  NDB_TICKS stopTimePrioA;
  NDB_TICKS totTimePrioA;
  int bytesSentBeforePrioA;
  NDB_TICKS accTime;
  int loopCount;
  Uint64 sendLenBytes, sendCount;
  Uint64 recvLenBytes, recvCount;
};

TestPhase testSpec[] = {
   {  1,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of size 1  word
  ,{  1,   10000, 0,0, 0,0,0,0,0,0,0 } //   100 signals of size 1  word
  ,{  1,  10000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of size 1  word
  ,{  1, 10000, 0,0, 0,0,0,0,0,0,0 } // 10000 signals of size 1  word

  ,{  8,    10, 0,0, 0,0,0,0,0,0,0 } //    10 signals of size 1  word
  ,{  8,   10000, 0,0, 0,0,0,0,0,0,0 } //   100 signals of size 1  word
  ,{  8,  10000, 0,0, 0,0,0,0,0,0,0 } //  1000 signals of size 1  word
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
};

const int noOfTests = sizeof(testSpec)/sizeof(TestPhase);

SendStatus
sendSignalTo(NodeId nodeId, int signalSize, int prio){
  if(signalSize == 0)
    signalSize = (rand() % 25) + 1;

  SignalHeader sh;
  sh.theLength               = signalSize;
  sh.theVerId_signalNumber   = rand();
  sh.theReceiversBlockNumber = rand();   
  sh.theSendersBlockRef      = rand(); 
  sh.theSendersSignalId      = rand(); 
  sh.theSignalId             = rand(); 
  sh.theTrace                = rand(); 
  
  Uint32 theData[25];
  for(int i = 0; i<signalSize; i++)
    theData[i] = (i+1) * (Uint32)(&theData[i]);
  
  return tReg->prepareSend(&sh, prio, theData, nodeId);
}

void
reportHeader(){
  ndbout << "#Sigs\tSz\tPayload\tTime\tSig/sec\tBps\t"
	 << "s len\tr len\tprioAtime\tbytesb4pA" << endl;
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
    Uint32 bps = ((4000*p.noOfSignals)/p.accTime)*(p.loopCount*(p.signalSize+3));
    if(p.signalSize == 0)
      ((4000*p.noOfSignals)/p.accTime)*(p.loopCount*(13+3));
    
    char ssps[255];
    if(sps > 1000000){
      sps /= 1000000;
      sprintf(ssps, "%dM", (int)sps);
    } else if(sps > 1000){
    sps /= 1000;
    sprintf(ssps, "%dk", (int)sps);
    } else {
      sprintf(ssps, "%d", (int)sps);
    }
    
    char sbps[255];
    if(bps > 1000000){
      bps /= 1000000;
      sprintf(sbps, "%dM", bps);
    } else if(bps>1000){
      bps /= 1000;
      sprintf(sbps, "%dk", bps);
    } else {
      sprintf(sbps, "%d", bps);
  }
    
    char buf[255];
    if(p.signalSize != 0){
      BaseString::snprintf(buf, 255,
	       "%d\t%d\t%d\t%s\t%s\t%s\t%d\t%d\t%d\t%d",
	       p.noOfSignals,
	       p.signalSize,
	       (4*p.signalSize),
	       st,
	       ssps,
	       sbps,
	       (int)(p.sendLenBytes / (p.sendCount == 0 ? 1 : p.sendCount)),
	       (int)(p.recvLenBytes / (p.recvCount == 0 ? 1 : p.recvCount)),
	       (int)(p.totTimePrioA / p.loopCount),
	       (int)(p.bytesSentBeforePrioA));
    } else {
      BaseString::snprintf(buf, 255,
	       "%d\trand\t4*rand\t%s\t%s\t%s\t%d\t%d\t%d\t%d",
	       p.noOfSignals,
	       st,
	       ssps,
	       sbps,
	       (int)(p.sendLenBytes / (p.sendCount == 0 ? 1 : p.sendCount)),
	       (int)(p.recvLenBytes / (p.recvCount == 0 ? 1 : p.recvCount)),
	       (int)(p.totTimePrioA / p.loopCount),
	       (int)(p.bytesSentBeforePrioA));
      
    }
    ndbout << buf << endl;
  }
}

int loopCount = 1;
int sendBufSz = -1;
int recvBufSz = -1;

NDB_TICKS startSec=0;
NDB_TICKS stopSec=0;
Uint32 startMicro=0;
Uint32 stopMicro=0;
int timerStarted;
int timerStopped;

bool      isClient     = false;
bool      isConnected  = false;
bool      isStarted    = false;
int       currentPhase = 0;
TestPhase allPhases[noOfTests];
Uint32    signalToEcho;
NDB_TICKS startTime, stopTime;

void
client(NodeId remoteNodeId){
  isClient = true;

  currentPhase = 0;
  memcpy(allPhases, testSpec, sizeof(testSpec));

  int counter = 0;

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
      for(; signalsLeft > 1; signalsLeft--){
	if(sendSignalTo(remoteNodeId, current->signalSize, 1) == SEND_OK) {
	  current->noOfSignalSent++;
	  //	  ndbout << "sent prio b" << endl;
	  current->bytesSentBeforePrioA += (current->signalSize << 2);
	}
	else {
	  tReg->external_IO(10);
	  break;
	}
      }
      //prio A
      if(signalsLeft==1) {
	NDB_TICKS sec = 0;
	Uint32 micro=0;
	int ret = NdbTick_CurrentMicrosecond(&sec,&micro);
	if(ret==0)
	  current->startTimePrioA  = micro + sec*1000000;
	if(sendSignalTo(remoteNodeId, current->signalSize, 0) == SEND_OK) {
	  current->noOfSignalSent++;
	  signalsLeft--;
	}	
	else {
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
  for(int i = 0; i<noOfTests; i++)
    signalToEcho += testSpec[i].noOfSignals;
  
  signalToEcho *= loopCount;
  
  while(signalToEcho > 0){
    tReg->checkConnections();
    for(int i = 0; i<10; i++)
      tReg->external_IO(10);
  }
}

int
prioTransporterTest(TestType tt, const char * progName, 
		    int argc, const char **argv){
  
  loopCount = 100;
  sendBufSz = -1;
  recvBufSz = -1;
  
  isClient     = false;
  isConnected  = false;
  isStarted    = false;
  currentPhase = 0;

  signalHandler(0);
  
  if(argc < 4){
    usage(progName);
    return 0;
  }
  
  const NodeId localNodeId   = atoi(argv[1]);
  const char * localHostName = argv[2];
  const char * remoteHost1   = argv[3];
  
  if(argc >= 5)
    loopCount = atoi(argv[4]);
  if(argc >= 6)
    sendBufSz = atoi(argv[5]);
  if(argc >= 7)
    recvBufSz = atoi(argv[6]);

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
  switch(tt){
  case TestTCP:
    func = createTCPTransporter;
    confTemplate = &tcpTemplate;
    break;
  case TestSCI:
    func = createSCITransporter;
    confTemplate = &sciTemplate;
    break;
  case TestSHM:
    func = createSHMTransporter;
    confTemplate = &shmTemplate;
    break;
  default:
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

NdbOut & operator <<(NdbOut & out, SignalHeader & sh){
  out << "-- Signal Header --" << endl;
  out << "theLength:    " << sh.theLength << endl;
  out << "gsn:          " << sh.theVerId_signalNumber << endl;
  out << "recBlockNo:   " << sh.theReceiversBlockNumber << endl;
  out << "sendBlockRef: " << sh.theSendersBlockRef << endl;
  out << "sendersSig:   " << sh.theSendersSignalId << endl;
  out << "theSignalId:  " << sh.theSignalId << endl;
  out << "trace:        " << (int)sh.theTrace << endl;
  return out;
} 

void
execute(SignalHeader * const header, Uint8 prio, Uint32 * const theData){
  const NodeId nodeId = refToNode(header->theSendersBlockRef);
  NDB_TICKS sec = 0;
  Uint32 micro=0;
  int ret = NdbTick_CurrentMicrosecond(&sec,&micro);
  if(prio == 0 && isClient && ret == 0) {
    allPhases[currentPhase].stopTimePrioA  = micro + sec*1000000;
    allPhases[currentPhase].totTimePrioA += 
      allPhases[currentPhase].stopTimePrioA -  
      allPhases[currentPhase].startTimePrioA;
  }
  if(ret!=0)
    allPhases[currentPhase].totTimePrioA = -1;

  if(isClient){
    allPhases[currentPhase].noOfSignalReceived++;
  } else {
    int sleepTime = 10;
    while(tReg->prepareSend(header, prio, theData, nodeId) != SEND_OK){
      ndbout << "Failed to echo" << sleepTime << endl;
      NdbSleep_MilliSleep(sleepTime);
      // sleepTime += 10;
    }
    
    signalToEcho--;
  }
}

void
reportError(NodeId nodeId, TransporterError errorCode){
  char buf[255];
  sprintf(buf, "reportError (%d, %x) in perfTest", nodeId, errorCode);
  ndbout << buf << endl;
  if(errorCode & 0x8000){
    tReg->setPerformState(nodeId, PerformDisconnect);
  }
}

/**
 * Report average send theLength in bytes (4096 last sends)
 */
void
reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes){
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
reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes){
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
reportConnect(NodeId nodeId){
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
reportDisconnect(NodeId nodeId, Uint32 errNo){
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
