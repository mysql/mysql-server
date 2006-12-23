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

#include "MemoryChannel.hpp"
#include "NdbThread.h"
#include "NdbSleep.h"
#include "NdbOut.hpp"
#include "NdbMain.h"



MemoryChannel<int>* theMemoryChannel;


extern "C" void* runProducer(void*arg)
{
  // The producer will items into the MemoryChannel
  int count = *(int*)arg;
  int* p;
  int i = 0;
  while (i <= count)
    {
      p = new int(i);
      ndbout << "P: " << *p << endl;
      theMemoryChannel->writeChannel(p);
      if (i%5==0)
        NdbSleep_MilliSleep(i);
      i++;
    }
  return NULL;
}

extern "C" void* runConsumer(void* arg)
{
  // The producer will read items from MemoryChannel and print on screen
  int count = *(int*)arg;
  int* p;
  int i = 0;
  while (i < count)
    {
      p = theMemoryChannel->readChannel();
      ndbout << "C: " << *p << endl;
      i = *p;
      delete p;
      
    }
  return NULL;
}



class ArgStruct 
{
public:
  ArgStruct(int _items, int _no){
    items=_items; 
    no=_no;
  };
  int items;
  int no;
};

MemoryChannelMultipleWriter<ArgStruct>* theMemoryChannel2;

extern "C" void* runProducer2(void*arg)
{
  // The producer will items into the MemoryChannel
  ArgStruct* pArg = (ArgStruct*)arg;
  int count = pArg->items;
  ArgStruct* p;
  int i = 0;
  while (i < count)
    {
      p = new ArgStruct(i, pArg->no);
      ndbout << "P"<<pArg->no<<": " << i << endl;
      theMemoryChannel2->writeChannel(p);
      NdbSleep_MilliSleep(i);
      i++;
    }
  return NULL;
}

extern "C" void* runConsumer2(void* arg)
{
  // The producer will read items from MemoryChannel and print on screen
  ArgStruct* pArg = (ArgStruct*)arg;
  int count =  pArg->items * pArg->no;
  ArgStruct* p;
  int i = 0;
  while (i < count)
    {
      p = theMemoryChannel2->readChannel();
      ndbout << "C: "<< p->no << ", " << p->items << endl;
      i++;
      delete p;
    }
  ndbout << "Consumer2: " << count << " received" << endl;
  return NULL;
}




//#if defined MEMORYCHANNELTEST

//int main(int argc, char **argv)
NDB_COMMAND(mctest, "mctest", "mctest", "Test the memory channel used in Ndb", 32768)
{

  ndbout << "==== testing MemoryChannel ====" << endl;

  theMemoryChannel = new MemoryChannel<int>;
  theMemoryChannel2 = new MemoryChannelMultipleWriter<ArgStruct>;

  NdbThread* consumerThread;
  NdbThread* producerThread;

  NdbThread_SetConcurrencyLevel(2);

  int numItems = 100;
  producerThread = NdbThread_Create(runProducer, 
				    (void**)&numItems,
				    4096,
				    (char*)"producer");

  consumerThread = NdbThread_Create(runConsumer, 
				    (void**)&numItems,
				    4096,
				    (char*)"consumer");


  void *status;
  NdbThread_WaitFor(consumerThread, &status);
  NdbThread_WaitFor(producerThread, &status);

  ndbout << "==== testing MemoryChannelMultipleWriter ====" << endl;
#define NUM_THREADS2 5
  NdbThread_SetConcurrencyLevel(NUM_THREADS2+2);
  NdbThread* producerThreads[NUM_THREADS2];

  ArgStruct *pArg;
  for (int j = 0; j < NUM_THREADS2; j++)
    {
      char buf[25];
      sprintf((char*)&buf, "producer%d", j);
      pArg = new ArgStruct(numItems, j);
      producerThreads[j] = NdbThread_Create(runProducer2, 
				    (void**)pArg,
				    4096,
				    (char*)&buf);
    }

  pArg = new ArgStruct(numItems, NUM_THREADS2);
  consumerThread = NdbThread_Create(runConsumer2, 
				    (void**)pArg,
				    4096,
				    (char*)"consumer");


  NdbThread_WaitFor(consumerThread, &status);
  for (int j = 0; j < NUM_THREADS2; j++)
  {
    NdbThread_WaitFor(producerThreads[j], &status);
  }

				    
  return 0;

}

void ErrorReporter::handleError(ErrorCategory type, int messageID,
                                const char* problemData, const char* objRef,
				NdbShutdownType nst)
{

  ndbout << "ErrorReporter::handleError activated"  << endl;
  exit(1);
}

//#endif
