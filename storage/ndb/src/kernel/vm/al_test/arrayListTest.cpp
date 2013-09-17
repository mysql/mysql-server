/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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



#include <ndb_global.hpp>

#include <ArrayList.hpp>
#include <NdbOut.hpp>
#include <NdbTick.h>

#define JAM_FILE_ID 319


struct A_Listable_Object {
  Uint32 next;
  Uint32 prev;
  char somedata[12];

  void print (NdbOut & out) {
    out << "ALO: next = " << next
	<< " prev = " << prev << endl;
  }
};

extern const int x_AL_Next = offsetof(A_Listable_Object, next);
extern const int x_AL_Prev = offsetof(A_Listable_Object, prev);

NdbOut &
operator<<(NdbOut & o, A_Listable_Object & a){
  a.print(o);
  return o;
}

typedef Ptr<A_Listable_Object> A_Listable_ObjectPtr;

#define APool ArrayPool<A_Listable_Object>
#define AList ArrayList<A_Listable_Object>

APool aGPool;
AList aGList(aGPool);

class ArrayListTest {
public:
  static void tryList0(int listSize){
    APool aPool;
    AList aList(aPool);

    if(!aPool.setSize(listSize)){
      ndbout << "Failed to do aPool.setSize(" << listSize << ")" << endl;
      return;
    }
    
    int * anArray = new int[listSize];
    
    for(int i = 1; i<listSize; i++){
      int arrayElements = 0;

      
      for(int j = 0; j<i; j++){
	A_Listable_ObjectPtr p;
	const int ret = aList.seize(p);
	if(ret == RNIL){
	  ndbout << "Failed to seize!!"  << endl;
	  ndbout << "Have seized " << j
		 << " out of " << listSize << endl;
	  ndbout << "Terminating..." << endl;
	  abort();
	}
	anArray[arrayElements] = ret;
	arrayElements++;
      }
      assert(aList.noOfElements() == i);
      assert(aPool.noOfFree() == (listSize - i));
      assert(arrayElements == i);

      for(int j = 0; j<i; j++){
	aList.release(anArray[j]);
      }
      
      assert(aList.noOfElements() == 0);
      assert(aPool.noOfFree() == listSize);
    }
  }

  static void tryList1(int listSize, int iterations){
    APool aPool;
    AList aList(aPool);
    
    if(!aPool.setSize(listSize)){
      ndbout << "Failed to do aPool.setSize(" << listSize << ")" << endl;
      return;
    }
    
    ndbout << "Seizing/Releaseing " << iterations 
	   << " times over list with " << listSize << " elements" << endl;
    
    int * anArray = new int[listSize];
    int arrayElements = 0;
    
    int noOfSeize = 0;
    int noFailSeize = 0;
    int noOfRelease = 0;
    
    for(int i = 0; i<iterations; i++){
      assert(arrayElements <= listSize);
      const int r = rand() % (10 * listSize);
      if(r < (arrayElements - 1)){
	/**
	 * Release an element
	 */
	noOfRelease++;
	aList.release(anArray[r]);
	arrayElements--;
	for(int j = r; j<arrayElements; j++)
	  anArray[j] = anArray[j+1];
	
      } else {
	/**
	 * Seize an element
	 */
	A_Listable_ObjectPtr p;
	const int ret = aList.seize(p);
	if(ret == RNIL && arrayElements != listSize){
	  ndbout << "Failed to seize!!" 
		 << " iteration=" << i << endl;
	  ndbout << "Have seized " << arrayElements 
		 << " out of " << listSize << endl;
	  ndbout << "Terminating..." << endl;
	  abort();
	}
	if(arrayElements >= listSize && ret != RNIL){
	  ndbout << "Seize did not fail when it should have"
		 << " iteration=" << i << endl;
	  ndbout << "Have seized " << arrayElements 
		 << " out of " << listSize << endl;
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
  
    ndbout << "Seized: " << noOfSeize 
	   << " Seized with buffer full: " << noFailSeize 
	   << " Release: " << noOfRelease << " --- ";
    ndbout << "(" << noOfSeize << " + " << noFailSeize << " + " << noOfRelease 
	   << " = " << (noOfSeize + noFailSeize + noOfRelease) << ")" << endl;
  }

  static void tryList2(int size, int iter, int fail){
    APool aPool;
    AList aList(aPool);
  
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
      assert(arrayElements <= size);
      const int r = rand() % (10 * size);

      if((i + 1)%(iter/fail) == 0){
	aList.getPtr(size + r);
	continue;
      }
    
      if(r < (arrayElements - 1)){
	/**
	 * Release an element
	 */
	noOfRelease++;
	aList.release(anArray[r]);
	arrayElements--;
	for(int j = r; j<arrayElements; j++)
	  anArray[j] = anArray[j+1];

      } else {
	/**
	 * Seize an element
	 */
	A_Listable_ObjectPtr p;
	const int ret = aList.seize(p);
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
	} else {
	  noFailSeize++;
	}
      }
    }
    delete []anArray;
  }

  static void
  tryList3(int size, int fail){
    ndbout << "Failing " << fail << " times " << endl;

    for(int i = 0; i<fail; i++){
      APool aPool;
      AList aList(aPool);
    
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
	  aList.release(anArray[arrayElements]);
	  ndbout << "++ Inbetween these lines" << endl << endl;
	  break;
	}
	const int r = rand() % (10 * size);
	if(r < (arrayElements - 1)){
	  /**
	   * Release an element
	   */
	  noOfRelease++;
	  aList.release(anArray[r]);
	  arrayElements--;
	  for(int j = r; j<arrayElements; j++)
	    anArray[j] = anArray[j+1];
	
	} else {
	  /**
	   * Seize an element
	   */
	  A_Listable_ObjectPtr p;
	  const int ret = aList.seize(p);
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
