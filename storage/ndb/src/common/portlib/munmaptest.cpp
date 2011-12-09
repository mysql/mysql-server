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

#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbEnv.h>
#include <NdbHost.h>
#include <NdbMain.h>
#include <getarg.h>

struct ThreadData
{
  char * mapAddr;
  Uint32 mapSize;
  Uint32 chunk;
  Uint32 idx;
  
};

long long getMilli();
long long getMicro();


void* mapSegment(void * arg);
void* unmapSegment(void * arg);


void* mapSegment(void * arg) {
  
  ThreadData * threadArgs;
  long long start=0;
  int total=0;
  int id = *(int *)arg;
  threadArgs = new ThreadData [1];
  Uint32 size=5*1024*1024;
  struct NdbThread* unmapthread_var;
  void *status = 0;  
  int run = 1;
  int max=0, min =100000000, sum=0;
  while(run < 1001) {
    start=getMicro(); 
    char * ptr =(char*) mmap(0, 
			     size, 
			     PROT_READ|PROT_WRITE, 
			     MAP_PRIVATE|MAP_ANONYMOUS, 
			     0,
			     0);

    total=(int)(getMicro()-start);
    
    ndbout << "T"  << id << ": mmap took : " << total << " microsecs. " 
	   << " Run: " << run ;
    ndbout_c(" mapped @ %p \n", ptr);
    
    if(total>max)
      max = total;
    if(total<min)
      min=total;
    
    sum+=total;
    
    if(ptr<0) {
      ndbout << "failed to mmap!" << endl;
      exit(1);
    }

    
    threadArgs[0].mapAddr = (char *)ptr;
    threadArgs[0].mapSize = size;
    threadArgs[0].chunk = 4096;
    threadArgs[0].idx = 0;
    
    
    for(Uint32 j=0; j<size; j=j+4096)
      ptr[j]='1';
    
    unmapthread_var = NdbThread_Create(unmapSegment, // Function 
				       (void**)&threadArgs[0],// Arg
				       32768,        // Stacksize
				       (char*)"unmapthread",  // Thread name
				       NDB_THREAD_PRIO_MEAN); // Thread prio
    
    
    if(NdbThread_WaitFor(unmapthread_var, &status) != 0) {
      ndbout << "test failed - exitting " << endl;
      exit(1);
    }
    run++;
  }
  
  ndbout << "MAX: " << max << " MIN: " << min;
  float mean = (float) ((float)sum/(float)run);
  ndbout_c(" AVERAGE: %2.5f\n",mean);
}



void* unmapSegment(void * arg)
{
  
  char * freeAddr;
  char * mapAddr;
  ThreadData * threadData = (ThreadData*) arg;
  int start=0;
  int total=0;
  Uint32 mapSize = threadData->mapSize;
  Uint32 chunk = threadData->chunk;
  mapAddr = threadData->mapAddr;
 
  
  
  freeAddr = mapAddr+mapSize-chunk;
  NdbSleep_MilliSleep(100);  
  for(Uint32 i=0;i<mapSize; i = i+chunk) {
    start=getMicro(); 
    if(munmap(freeAddr, chunk) < 0){
      ndbout << "munmap failed" << endl;
      exit(1);
    }
    total=(int)(getMicro()-start);
    freeAddr = freeAddr - chunk;
    NdbSleep_MilliSleep(10);
    ndbout << "unmap 4096 bytes : " << total << "microsecs" << endl;
  }
  return NULL;
}


static int trash;
static int segmentsize=1;


static struct getargs args[] = {
  { "trash", 't', arg_integer, &trash,
    "trash the memory before (1 to trash 0 to not trash)", "trash"},
  { "segment", 's', arg_integer, &segmentsize,
    "segment size (in MB)", "segment"},
};


static const int num_args = sizeof(args) / sizeof(args[0]);

NDB_MAIN(munmaptest) {
   
  const char *progname = "munmaptest"; 
  int optind = 0;

  if(getarg(args, num_args, argc, argv, &optind)) {
    arg_printusage(args, num_args, progname, "");
    exit(1);
  }
  
  int size;
  char * ptr;
  if(trash) {
    for(int i=0; i<100; i++) {
      size=1+(int) (10.0*rand()/(RAND_MAX+1.0));
      NdbSleep_MilliSleep(10);
      ptr =(char*) mmap(0, 
			size*1024*1024, 
			PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_ANONYMOUS, 
			0,
			0);      
      for(int i=0;i<(size*1024*1024); i=i+4096) {
	*(ptr+i)='1';
      }
      NdbSleep_MilliSleep(10);
     
      munmap(ptr,size);

    }
    
    
  }

  int noThreads = 1;
  struct NdbThread*  mapthread_var;
  int id[noThreads];
  void *status=0;

  ThreadData * threadArgs = new ThreadData[noThreads];




  for(int i=0; i < noThreads; i++) {
    threadArgs[i].mapSize = segmentsize*1024*1024;
    threadArgs[i].idx = i;
    mapthread_var = NdbThread_Create(mapSegment, // Function
				     (void**)&threadArgs[i],// Arg
				     32768,        // Stacksize
				     (char*)"mapthread",  // Thread name
				     NDB_THREAD_PRIO_MEAN); // Thread prio
    
  }
  
  
  if(NdbThread_WaitFor(mapthread_var, &status) != 0) {
    ndbout << "test failed - exitting " << endl;
    exit(1);
  }

}

long long getMilli() {
  struct timeval tick_time;
  gettimeofday(&tick_time, 0);

  return 
    ((long long)tick_time.tv_sec)  * ((long long)1000) +
    ((long long)tick_time.tv_usec) / ((long long)1000);
}

long long getMicro(){
  struct timeval tick_time;
  int res = gettimeofday(&tick_time, 0);

  long long secs   = tick_time.tv_sec;
  long long micros = tick_time.tv_usec;
  
  micros = secs*1000000+micros;
  return micros;
}
