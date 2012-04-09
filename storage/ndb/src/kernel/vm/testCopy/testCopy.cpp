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
#include <NdbTick.h>

#ifdef __NDB_FORTE6
#define HAND
bool hand = true;
#else
bool hand = false;
#endif 

struct Data7 {
  Uint32 data[7];

#ifdef HAND
  inline Data7& operator=(const Data7 & o){
    Uint32 t0 = o.data[0];
    Uint32 t1 = o.data[1];
    Uint32 t2 = o.data[2];
    Uint32 t3 = o.data[3];
    Uint32 t4 = o.data[4];
    Uint32 t5 = o.data[5];
    Uint32 t6 = o.data[6];
    data[0] = t0;
    data[1] = t1;
    data[2] = t2;
    data[3] = t3;
    data[4] = t4;
    data[5] = t5;
    data[6] = t6;
    return * this;
  }
#endif
};

struct Data25 {
  Uint32 data[25];
};

struct TestSignal {
  
  Data7  head;
  Data25 data;
};

Uint32 g_time = 3000;
Uint32 g_count = 8*2048;

TestSignal g_signal;
TestSignal * g_jobBuffer;

template<Uint32 LEN>
inline
void
MEMCOPY(Uint32 * to, const Uint32 * from){
  Uint32 t0 ;
  Uint32 t1 ;
  Uint32 t2 ;
  Uint32 t3 ;
  Uint32 len = LEN;
  while(len > 4){
    t0 = from[0];
    t1 = from[1];
    t2 = from[2];
    t3 = from[3];
    
    to[0] = t0;
    to[1] = t1;
    to[2] = t2;
    to[3] = t3;

    
    to += 4;
    from += 4;
    len -= 4;
  }
  
  //ndbout_c("len = %d", len);

  t0 = from[0];
  t1 = from[1];
  t2 = from[2];
  switch(len & 3){
  case 3:
    //ndbout_c("3");
    to[2] = t2;
  case 2:
    //ndbout_c("2");
    to[1] = t1;
  case 1:
    //ndbout_c("1");
    to[0] = t0;
  }
  
}

inline
void
MEMCOPY_NO_WORDS(Uint32 * to, const Uint32 * from, Uint32 len){
  Uint32 t0 ;
  Uint32 t1 ;
  Uint32 t2 ;
  Uint32 t3 ;
  while(len > 4){
    t0 = from[0];
    t1 = from[1];
    t2 = from[2];
    t3 = from[3];
    
    to[0] = t0;
    to[1] = t1;
    to[2] = t2;
    to[3] = t3;

    
    to += 4;
    from += 4;
    len -= 4;
  }
  
  //ndbout_c("len = %d", len);

  t0 = from[0];
  t1 = from[1];
  t2 = from[2];
  switch(len & 3){
  case 3:
    //ndbout_c("3");
    to[2] = t2;
  case 2:
    //ndbout_c("2");
    to[1] = t1;
  case 1:
    //ndbout_c("1");
    to[0] = t0;
  }
}

inline
void 
copy1(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

  Uint32 t0 = ts.head.data[0];
  Uint32 t1 = ts.head.data[1];
  Uint32 t2 = ts.head.data[2];
  Uint32 t3 = ts.head.data[3];
  Uint32 t4 = ts.head.data[4];
  Uint32 t5 = ts.head.data[5];
  Uint32 t6 = ts.head.data[6];

  dst.head.data[0] = t0;
  dst.head.data[1] = t1;
  dst.head.data[2] = t2;
  dst.head.data[3] = t3;
  dst.head.data[4] = t4;
  dst.head.data[5] = t5;
  dst.head.data[6] = t6;
} 



inline
void 
copy2(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

  Uint32 t0 = ts.head.data[0];
  Uint32 t1 = ts.head.data[1];
  Uint32 t2 = ts.head.data[2];
  Uint32 t3 = ts.head.data[3];

  dst.head.data[0] = t0;
  dst.head.data[1] = t1;
  dst.head.data[2] = t2;
  dst.head.data[3] = t3;

  Uint32 t4 = ts.head.data[4];
  Uint32 t5 = ts.head.data[5];
  Uint32 t6 = ts.head.data[6];

  dst.head.data[4] = t4;
  dst.head.data[5] = t5;
  dst.head.data[6] = t6;
} 

inline
void
copy3(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

  dst.head = ts.head;
}

inline
void
copy4(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

  memcpy(&dst.head.data[0], &ts.head.data[0], sizeof(Data7));
}

inline
void
copy5(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

  MEMCOPY_NO_WORDS(&dst.head.data[0], &ts.head.data[0], 7);
}

inline
void
copy6(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

  MEMCOPY<7>(&dst.head.data[0], &ts.head.data[0]);
}

inline
void
copy7(Uint32 i, TestSignal & ts){
  TestSignal & dst = g_jobBuffer[i];

#if (__GNUC__ >= 3 ) || (__GNUC__ == 2 && __GNUC_MINOR >= 95)
  __builtin_memcpy(&dst.head.data[0], &ts.head.data[0], sizeof(Data7));
#else
  dst.head = ts.head;
#endif
}

template<void (* C)(Uint32 i, TestSignal & ts)>
int 
doTime(Uint32 ms){
  
  Uint64 ms1, ms2;
  const Uint32 count = g_count;
  for(Uint32 i = 0; i<count; i++)
    C(i, g_signal);
  for(Uint32 i = 0; i<count; i++)
    C(i, g_signal);
  
  Uint32 laps = 0;
  
  ms1 = NdbTick_CurrentMillisecond();
  do {
    for(int j = 100; j>= 0; j--)
      for(Uint32 i = 0; i<count; i++){
	C(i, g_signal);
      }
    ms2 = NdbTick_CurrentMillisecond();
    laps += 100;
  } while((ms2 - ms1) < ms);
  
  return laps;
}


template<void (* C)(Uint32 i, TestSignal & ts)>
void doCopyLap(Uint32 laps, const char * title){

  Uint64 ms1, ms2;
  const Uint32 count = g_count;
  for(Uint32 i = 0; i<count; i++)
    C(i, g_signal);
  laps--;
  for(Uint32 i = 0; i<count; i++)
    C(i, g_signal);
  laps--;
  
  Uint32 div = laps;
  
  ms1 = NdbTick_CurrentMillisecond();
  while(laps > 0){
    for(Uint32 i = 0; i<count; i++){
#if (__GNUC__ == 3 && __GNUC_MINOR >= 1)
      _builtin_prefetch(&g_jobBuffer[i], 1, 0);
#endif
      C(i, g_signal);
    }
    laps--;
  }
  ms2 = NdbTick_CurrentMillisecond();
  
  ms2 -= ms1;
  Uint32 diff = ms2;
  ndbout_c("%s : %d laps in %d millis => %d copies/sec",
	   title, div, diff, (1000*div*g_count+(diff/2))/diff);
}

int
main(int argc, const char ** argv){
  
  if(argc > 1)
    g_count = atoi(argv[1]);

  if(argc > 2)
    g_time = atoi(argv[2]);
  
  ndbout_c("Using %d entries => %d kB ", 
	   g_count,
	   g_count * sizeof(TestSignal) / 1024);
  ndbout_c("Testing for %d ms", g_time);

  ndbout_c("Using %s copy-constructor", 
	   (hand ? "hand written" : "compiler generated")); 
  
  g_jobBuffer = new TestSignal[g_count + 1];
  for(int i = 0; i<10; i++)
    memset(g_jobBuffer, 0, g_count * sizeof(TestSignal));

  int laps = doTime<copy2>(g_time);
  ndbout_c("Laps = %d", laps);

  doCopyLap<copy2>(laps, "4 t-variables");
  doCopyLap<copy3>(laps, "copy constr. ");
  doCopyLap<copy1>(laps, "7 t-variables");
  doCopyLap<copy4>(laps, "mem copy     ");
  doCopyLap<copy5>(laps, "mem copy hand");
  doCopyLap<copy6>(laps, "mem copy temp");
  doCopyLap<copy7>(laps, "mem copy gcc ");

  delete[] g_jobBuffer;
  return 0;
}
