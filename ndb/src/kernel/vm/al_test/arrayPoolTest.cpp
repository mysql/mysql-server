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

#include <ArrayList.hpp>
#include <NdbOut.hpp>
#include <NdbTick.h>

struct A_Poolable_Object {
  Uint32 next;
  char somedata[12];

  void print (NdbOut & out) {
    out << "A_Poolable_Object: next = " << next << endl;
  }

};


NdbOut &
operator<<(NdbOut & o, A_Poolable_Object & a){
  a.print(o);
  return o;
}

typedef Ptr<A_Poolable_Object> A_Poolable_ObjectPtr;
#if 1
#define BPool ArrayPool<A_Poolable_Object>
#else
#define BPool ArrayPool(A_Poolable_Object, next)
#endif

class ArrayPoolTest {
public:
  static void tryPool1(int poolSize, int iterations){
    BPool aPool;
    
    if(!aPool.setSize(poolSize)){
      ndbout << "Failed to do aPool.setSize(" << poolSize << ")" << endl;
      return;
    }
    
    ndbout << "Seizing/Releaseing " << iterations 
	   << " times over pool with " << poolSize << " elements" << endl;
    
    int * anArray = new int[poolSize];
    int arrayElements = 0;
    
    int noOfSeize = 0;
    int noFailSeize = 0;
    int noOfRelease = 0;
    
    for(int i = 0; i<iterations; i++){
      if(!((arrayElements    <= poolSize) &&
	   (aPool.noOfFree() == aPool.noOfFree2()) &&
	   (aPool.noOfFree() == (poolSize - arrayElements)))){
	ndbout << "Assertion!!" 
	       << " iteration=" << i << endl;
	const int f1 = aPool.noOfFree();
	const int f2 = aPool.noOfFree2();
	ndbout << "noOfFree()  = " << f1 << endl;
	ndbout << "noOfFree2() = " << f2 << endl;
	ndbout << "poolSize    = " << poolSize << endl;
	ndbout << "arrayElemts = " << arrayElements << endl;
	aPool.print(ndbout);
	assert(0);
      }

      const int r = rand() % (10 * poolSize);
      if(r < (arrayElements - 1)){
	/**
	 * Release an element
	 */
	noOfRelease++;
	aPool.release(anArray[r]);
	arrayElements--;
	for(int j = r; j<arrayElements; j++)
	  anArray[j] = anArray[j+1];
	
      } else {
	/**
	 * Seize an element
	 */
	A_Poolable_ObjectPtr p;
	const int ret = aPool.seize(p);
	if(ret == RNIL && arrayElements != poolSize){
	  ndbout << "Failed to seize!!" 
		 << " iteration=" << i << endl;
	  ndbout << "Have seized " << arrayElements 
		 << " out of " << poolSize << endl;
	  ndbout << "Terminating..." << endl;
	  abort();
	}
	if(arrayElements >= poolSize && ret != RNIL){
	  ndbout << "Seize did not fail when it should have"
		 << " iteration=" << i << endl;
	  ndbout << "Have seized " << arrayElements 
		 << " out of " << poolSize << endl;
	  ndbout << "Terminating..." << endl;
	  abort();
	}
	if(ret != RNIL){
	  noOfSeize++;
	  anArray[arrayElements] = ret;
	  arrayElements++;
	  memset(p.p, i, sizeof(p.p->somedata));
	} else {
	  noFailSeize++;
	}
      }
    }
    delete []anArray;
  
    ndbout << "Seized: " << noOfSeize 
	   << " Seized with buffer full: " << noFailSeize 
	   << " Release: " << noOfRelease << " --- ";
    ndbout << "(" << noOfSeize << " + " << noFailSeize << " + " << noOfRelease 
	   << " = " << (noOfSeize + noFailSeize + noOfRelease) << ")" << endl;
  }

  static void tryPool2(int size, int iter, int fail){
    BPool aPool;
  
    if(!aPool.setSize(size)){
      ndbout << "Failed to do aPool.setSize(" << size << ")" << endl;
      return;
    }
  
    ndbout << "doing getPtr(i) where i > size(" << size << ") " 
	   << fail << " times mixed with " << iter 
	   << " ordinary seize/release" << endl;

    int * anArray = new int[size];
    int arrayElements = 0;
  
    int noOfSeize = 0;
    int noFailSeize = 0;
    int noOfRelease = 0;

    for(int i = 0; i<iter; i++){
      if(!((arrayElements    <= size) &&
	   (aPool.noOfFree() == aPool.noOfFree2()) &&
	   (aPool.noOfFree() == (size - arrayElements)))){
	ndbout << "Assertion!!" 
	       << " iteration=" << i << endl;
	const int f1 = aPool.noOfFree();
	const int f2 = aPool.noOfFree2();
	ndbout << "noOfFree()  = " << f1 << endl;
	ndbout << "noOfFree2() = " << f2 << endl;
	ndbout << "poolSize    = " << size << endl;
	ndbout << "arrayElemts = " << arrayElements << endl;
	aPool.print(ndbout);
	assert(0);
      }
      const int r = rand() % (10 * size);

      if((i + 1)%(iter/fail) == 0){
	aPool.getPtr(size + r);
	continue;
      }
    
      if(r < (arrayElements - 1)){
	/**
	 * Release an element
	 */
	noOfRelease++;
	aPool.release(anArray[r]);
	arrayElements--;
	for(int j = r; j<arrayElements; j++)
	  anArray[j] = anArray[j+1];

      } else {
	/**
	 * Seize an element
	 */
	A_Poolable_ObjectPtr p;
	const int ret = aPool.seize(p);
	if(ret == RNIL && arrayElements != size){
	  ndbout << "Failed to seize!!" 
		 << " iteration=" << i << endl;
	  ndbout << "Have seized " << arrayElements 
		 << " out of " << size << endl;
	  ndbout << "Terminating..." << endl;
	  abort();
	}
	if(arrayElements >= size && ret != RNIL){
	  ndbout << "Seize did not fail when it should have"
		 << " iteration=" << i << endl;
	  ndbout << "Have seized " << arrayElements 
		 << " out of " << size << endl;
	  ndbout << "Terminating..." << endl;
	  abort();
	}
	if(ret != RNIL){
	  noOfSeize++;
	  anArray[arrayElements] = ret;
	  arrayElements++;
	  memset(p.p, p.i, sizeof(p.p->somedata));
	} else {
	  noFailSeize++;
	}
      }
    }
    delete []anArray;
  }

  static void
  tryPool3(int size, int fail){
    ndbout << "Failing " << fail << " times " << endl;

    for(int i = 0; i<fail; i++){
      BPool aPool;
      if(!aPool.setSize(size)){
	ndbout << "Failed to do aPool.setSize(" << size << ")" << endl;
	return;
      }

      const int noOfElementsInBufferWhenFail = (i + 1) * (size /(fail + 1));

      int * anArray = new int[size];
      for(int i = 0; i<size; i++)
	anArray[i] = i;
      int arrayElements = 0;
    
      int noOfSeize = 0;
      int noFailSeize = 0;
      int noOfRelease = 0;
    
      while(true){
	assert(arrayElements <= size);
	if(arrayElements == noOfElementsInBufferWhenFail){
	  ndbout << "++ You should get a ErrorReporter::handle... " << endl;
	  aPool.release(anArray[arrayElements]);
	  ndbout << "++ Inbetween these lines" << endl << endl;
	  break;
	}
	const int r = rand() % (10 * size);
	if(r < (arrayElements - 1)){
	  /**
	   * Release an element
	   */
	  noOfRelease++;
	  aPool.release(anArray[r]);
	  arrayElements--;
	  for(int j = r; j<arrayElements; j++)
	    anArray[j] = anArray[j+1];
	
	} else {
	  /**
	   * Seize an element
	   */
	  A_Poolable_ObjectPtr p;
	  const int ret = aPool.seize(p);
	  if(ret == RNIL && arrayElements != size){
	    ndbout << "Failed to seize!!" << endl;
	    ndbout << "Have seized " << arrayElements 
		   << " out of " << size << endl;
	    ndbout << "Terminating..." << endl;
	    abort();
	  }
	  if(arrayElements >= size && ret != RNIL){
	    ndbout << "Seize did not fail when it should have" << endl;
	    ndbout << "Have seized " << arrayElements 
		   << " out of " << size << endl;
	    ndbout << "Terminating..." << endl;
	    abort();
	  }
	  if(ret != RNIL){
	    noOfSeize++;
	    anArray[arrayElements] = ret;
	    arrayElements++;
	  } else {
	    noFailSeize++;
	  }
	}
      }
      delete []anArray;
    }
  
  }
};

