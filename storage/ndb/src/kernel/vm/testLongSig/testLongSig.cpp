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


#include <ndb_global.h>
#include <NdbApi.hpp>
#include <readline/readline.h>
#include <../../ndbapi/SignalSender.hpp>

void
print_help(){
  ndbout << "The test menu" << endl;
  ndbout << "1 - Sending of long signals w/ segmented sections" << endl;
  ndbout << "2 - As 1 but using receiver group" << endl;
  ndbout << "3 - Sending of long signals w/ linear sections" << endl;
  ndbout << "4 - As 3 but using receiver group" << endl;
  ndbout << "5 - Sending of manually fragmented signals w/ segmented sections" 
	 << endl;
  ndbout << "6 - As 5 but using receiver group" << endl;
  ndbout << "7 - Sending of manually fragmented signals w/ linear sections" 
	 << endl;
  ndbout << "8 - As but using receiver group" << endl;
  
  ndbout << "9 - Sending of CONTINUEB fragmented signals w/ segmented sections" 
	 << endl;
  ndbout << "10 - As 9 but using receiver group" << endl;
  ndbout << "11 - Sending of CONTINUEB fragmented signals w/ linear sections" 
	 << endl;
  ndbout << "12 - As but using receiver group" << endl;
  ndbout << "13 - As 5 but with no release" << endl;
  ndbout << "14 - As 13 but using receiver group" << endl;
  ndbout << "15 - Send 100 * 1000 25 len signals wo/ sections" << endl;
  ndbout << "r - Recive signal from anyone" << endl;
  ndbout << "a - Run tests 1 - 14 with variable sizes - 10 loops" << endl;
  ndbout << "b - Run tests 1 - 14 with variable sizes - 100 loops" << endl;
  ndbout << "c - Run tests 1 - 14 with variable sizes - 1000k loops" << endl;
}

void runTest(SignalSender &, Uint32 i, bool verbose);

static
int
randRange(Uint32 min, Uint32 max){
  float r = rand();
  float f = (max - min + 1);
  float d = (float)RAND_MAX + 1.0;
  return min + (int)(f * r / d);
}

static
int
randRange(const Uint32 odds[], Uint32 count){
  Uint32 val = randRange((Uint32)0, 100);
  
  Uint32 i = 0;
  Uint32 sum = 0;
  while(sum <= val && i < count){
    sum += odds[i];
    i++;
  }
  return i - 1;
}

/**
 * testLongSignals
 * To run this code :
 *   cd storage/ndb/src/kernel
 *   make testLongSignals
 *   ./testLongSignals <connectstring>
 *
 */
int
main(int argc, char** argv) {
  ndb_init();

  srand(NdbTick_CurrentMillisecond());
#if 0
  for(int i = 0; i<100; i++)
    ndbout_c("randRange(0, 3) = %d", randRange(0, 3));
  return 0;
#endif

  if (argc != 2)
  {
    ndbout << "No connectstring given, usage : " << argv[0] 
           << " <connectstring>" << endl;
    return -1;
  }
  Ndb_cluster_connection con(argv[1]);
  
  ndbout << "Connecting...";
  if (con.connect(12,5,1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return -1;
  }
  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return -1;
  }
  ndbout << "done" << endl;

  SignalSender ss(&con);

  ndbout_c("Connected as block=%d node=%d",
	   refToBlock(ss.getOwnRef()), refToNode(ss.getOwnRef()));
  
  Uint32 data[25];
  Uint32 sec0[70];
  Uint32 sec1[123];
  Uint32 sec2[10];
  
  data[0] = ss.getOwnRef();
  data[1] = 1;
  data[2] = 76; 
  data[3] = 1;
  data[4] = 1;
  data[5] = 70;
  data[6] = 123;
  data[7] = 10;
  const Uint32 theDataLen = 18;

  for(Uint32 i = 0; i<70; i++)
    sec0[i] = i;
  
  for(Uint32 i = 0; i<123; i++)
    sec1[i] = 70+i;

  for(Uint32 i = 0; i<10; i++)
    sec2[i] = (i + 1)*(i + 1);
  
  SimpleSignal signal1;
  signal1.set(ss, 0, CMVMI, GSN_TESTSIG, theDataLen + 2);  
  signal1.header.m_noOfSections = 1;
  signal1.header.m_fragmentInfo = 1;

  memcpy(&signal1.theData[0], data, 4 * theDataLen );
  signal1.theData[theDataLen + 0] = 0;
  signal1.theData[theDataLen + 1] = 7; // FragmentId
  
  signal1.ptr[0].sz = 60;
  signal1.ptr[0].p = &sec0[0];
  
  SimpleSignal signal2;
  
  Uint32 idx = 0;
  memcpy(&signal2.theData[0], data, 4 * theDataLen );
  signal2.theData[theDataLen + idx] = 0; idx++;
  signal2.theData[theDataLen + idx] = 1; idx++;
  //signal2.theData[theDataLen + idx] = 2; idx++;
  signal2.theData[theDataLen + idx] = 7; idx++; // FragmentId

  signal2.set(ss, 0, CMVMI, GSN_TESTSIG, theDataLen + idx);
  signal2.header.m_fragmentInfo = 3;
  signal2.header.m_noOfSections = idx - 1;
  
  signal2.ptr[0].sz = 10;
  signal2.ptr[0].p = &sec0[60];
  
  signal2.ptr[1].sz = 123;
  signal2.ptr[1].p = &sec1[0];
  
  signal2.ptr[2].sz = 10;
  signal2.ptr[2].p = &sec2[0];
  
  char * buf;
  while((buf = readline("Enter command: "))){
    add_history(buf);
    data[1] = atoi(buf);
    if(strcmp(buf, "r") == 0){
      SimpleSignal * ret1 = ss.waitFor();
      (* ret1).print();
      continue;
    }
    if(strcmp(buf, "a") == 0){
      runTest(ss, 10, true);
      print_help();
      continue;
    }
    if(strcmp(buf, "b") == 0){
      runTest(ss, 100, false);
      print_help();
      continue;
    }
    if(strcmp(buf, "c") == 0){
      runTest(ss, 1000000, false);
      print_help();
      continue;
    }
    
    if(data[1] >= 1 && data[1] <= 14){
      Uint32 nodeId = ss.getAliveNode();
      ndbout_c("Sending 2 fragmented to node %d", nodeId);
      ss.sendSignal(nodeId, &signal1);
      ss.sendSignal(nodeId, &signal2);

      if(data[1] >= 5){
	continue;
      }
      ndbout_c("Waiting for signal from %d", nodeId);
      
      SimpleSignal * ret1 = ss.waitFor((Uint16)nodeId);
      (* ret1).print();
      Uint32 count = ret1->theData[4] - 1;
      while(count > 0){
	ndbout << "Waiting for " << count << " signals... ";
	SimpleSignal * ret1 = ss.waitFor();
	ndbout_c("received from node %d", 
		 refToNode(ret1->header.theSendersBlockRef));
	(* ret1).print();
	count--;
      }
    } else if (data[1] == 15) {
      const Uint32 count = 3500;
      const Uint32 loop = 1000;

      signal1.set(ss, 0, CMVMI, GSN_TESTSIG, 25);
      signal1.header.m_fragmentInfo = 0;
      signal1.header.m_noOfSections = 0;
      signal1.theData[1] = 14; 
      signal1.theData[3] = 0;   // Print
      signal1.theData[8] = count;
      signal1.theData[9] = loop;
      Uint32 nodeId = ss.getAliveNode();
      ndbout_c("Sending 25 len signal to node %d", nodeId);
      ss.sendSignal(nodeId, &signal1);

      Uint32 total;
      {
	SimpleSignal * ret1 = ss.waitFor((Uint16)nodeId);
	ndbout_c("received from node %d", 
		 refToNode(ret1->header.theSendersBlockRef));
	total = ret1->theData[10] - 1;
      }

      do {
	ndbout << "Waiting for " << total << " signals... " << flush;
	SimpleSignal * ret1 = ss.waitFor((Uint16)nodeId);
	ndbout_c("received from node %d", 
		 refToNode(ret1->header.theSendersBlockRef));
	total --;
      } while(total > 0);
    } else {
      print_help();
    }
  }
  ndbout << "Exiting" << endl;
  ndb_end(0);
};

void
runTest(SignalSender & ss, Uint32 count, bool verbose){
  
  SimpleSignal sig;
  Uint32 sec0[256];
  Uint32 sec1[256];
  Uint32 sec2[256];
  for(Uint32 i = 0; i<256; i++){
    sec0[i] = i;
    sec1[i] = i + i;
    sec2[i] = i * i;
  }

  sig.theData[0] = ss.getOwnRef();
  sig.theData[1] = 1;   // TestType
  sig.theData[2] = 128; // FragSize
  sig.theData[3] = 0;   // Print
  sig.theData[4] = 1;   // RetCount
  
  sig.ptr[0].p = &sec0[0];
  sig.ptr[1].p = &sec1[0];
  sig.ptr[2].p = &sec2[0];

  for(unsigned loop = 0; loop < count; loop++){
    const Uint32 odds[] =  { 5, 40, 30, 25 };
    const Uint32 secs = randRange(odds, 4);
    sig.ptr[0].sz = randRange(1, 256);
    sig.ptr[1].sz = randRange(1, 256);
    sig.ptr[2].sz = randRange(1, 256);
    sig.header.m_noOfSections = secs;
    const Uint32 len = 5 + (secs > 0 ? 1 : 0) * (25 - 5 - 7);
    sig.set(ss, 0, CMVMI, GSN_TESTSIG, len);
    ndbout << "Loop " << loop << " #secs = " << secs << " sizes = [ ";
    unsigned min = 256;
    unsigned max = 0;
    unsigned sum = 0;
    for(unsigned i = 0; i<secs; i++){
      const Uint32 sz = sig.ptr[i].sz;
      ndbout << sz << " ";
      min = (min < sz ? min : sz);
      max = (max > sz ? max : sz);
      sum += sz;
      sig.theData[5+i] = sz;
    }
    ndbout_c("] len = %d", len);
    for(int test = 1; test <= 14; test++){
      sig.theData[1] = test;
      Uint32 nodeId = ss.getAliveNode();
      if(verbose){
	ndbout << "  Test " << test << " node " << nodeId << "...";
	fflush(stdout);
      }
      SendStatus r = ss.sendSignal(nodeId, &sig);
      assert(r == SEND_OK);
      if(test < 5){
	SimpleSignal * ret1 = ss.waitFor((Uint16)nodeId);
	Uint32 count = ret1->theData[4] - 1;

	while(count > 0){
	  ret1 = ss.waitFor();
	  count--;
	}
	if(verbose)
	  ndbout << "done" << endl;
      } else {
	Uint32 nodes = ss.getNoOfConnectedNodes();
	Uint32 sum2 = 0;
	if((test & 1) == 1) 
	  nodes = 1;
	while(nodes > 0){
	  SimpleSignal * ret = ss.waitFor();
	  if(ret->header.m_fragmentInfo == 0){
	    for(Uint32 i = 0; i<ret->header.m_noOfSections; i++)
	      sum2 += ret->ptr[i].sz;
	  } else {
	    for(Uint32 i = 0; i<ret->header.m_noOfSections; i++)
	      if(ret->theData[i] != 3)
		sum2 += ret->ptr[i].sz;
	  }
	  if(ret->header.m_fragmentInfo == 0 ||
	     ret->header.m_fragmentInfo == 3){
	    nodes--;
	  }
	}
	if(verbose)
	  ndbout_c("done sum=%d sum2=%d", sum, sum2);
      }
    }
  }
}
