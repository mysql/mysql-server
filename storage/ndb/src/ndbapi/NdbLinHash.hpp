/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbLinHash_H
#define NdbLinHash_H

#include <ndb_types.h>

#define SEGMENTSIZE 64
#define SEGMENTLOGSIZE 6
#define DIRECTORYSIZE 64
#define DIRINDEX(adress) ((adress) >> SEGMENTLOGSIZE)
#define SEGINDEX(adress) ((adress) & (SEGMENTSIZE-1))

#if     !defined(MAXLOADFCTR)
#define MAXLOADFCTR 2
#endif
#if     !defined(MINLOADFCTR)
#define MINLOADFCTR (MAXLOADFCTR/2)
#endif

template<class C> 
class NdbElement_t {
public:
  NdbElement_t();
  ~NdbElement_t();
  
  Uint32 len;
  Uint32 hash;
  Uint32 localkey1;
  Uint32 *str;
  NdbElement_t<C> *next;
  C* theData;
private:
  NdbElement_t(const NdbElement_t<C> & aElement_t);
  NdbElement_t & operator = (const NdbElement_t<C> & aElement_t);
};


template <class C> 
class NdbLinHash {
public:
  NdbLinHash();
  ~NdbLinHash();
  void createHashTable(void);
  void releaseHashTable(void);
  
  int insertKey(const char * str, Uint32 len, Uint32 lkey1, C* data);
  C *deleteKey(const char * str, Uint32 len);

  C* getData(const char *, Uint32);
  Uint32* getKey(const char *, Uint32);
  
  void shrinkTable(void);
  void expandHashTable(void);
  
  Uint32 Hash(const char *str, Uint32 len);
  Uint32 Hash(Uint32 h);
  
  NdbElement_t<C> * getNext(NdbElement_t<C> * curr);
  
private:
  void getBucket(Uint32 hash, int * dirindex, int * segindex);
  
  struct Segment_t {
    NdbElement_t<C> * elements[SEGMENTSIZE];
  };

  Uint32 p;	/*bucket to be split*/
  Uint32 max;	/*max is the upper bound*/
  Int32  slack;	/*number of insertions before splits*/
  Segment_t * directory[DIRECTORYSIZE];
  
  NdbLinHash(const NdbLinHash<C> & aLinHash);
  NdbLinHash<C> & operator = (const NdbLinHash<C> & aLinHash);
};

// All template methods must be inline

template <class C>
inline
NdbLinHash<C>::NdbLinHash() { 
}

template <class C>
inline
NdbLinHash<C>::NdbLinHash(const NdbLinHash<C>& aLinHash) 
{
}

template <class C>
inline
NdbLinHash<C>::~NdbLinHash()
{
}

template <class C>
inline
Uint32
NdbLinHash<C>::Hash( const char* str, Uint32 len )
{
  Uint32 h = 0;
  while(len >= 4){
    h = (h << 5) + h + str[0];
    h = (h << 5) + h + str[1];
    h = (h << 5) + h + str[2];
    h = (h << 5) + h + str[3];
    len -= 4;
    str += 4;
  }
  
  while(len > 0){
    h = (h << 5) + h + *str++;
    len--;
  }
  return h;
}

template <class C>
inline
Uint32
NdbLinHash<C>::Hash( Uint32 h ){
  return h;
}

template <class C>
inline
NdbElement_t<C>::NdbElement_t() :
  len(0),
  hash(0),
  localkey1(0),
  str(NULL),
  next(NULL),
  theData(NULL)
{ 
}

template <class C>
inline
NdbElement_t<C>::~NdbElement_t()
{
  delete []str;
}


/* Initialize the hashtable HASH_T  */
template <class C>
inline
void
NdbLinHash<C>::createHashTable() {
  p = 0;
  max = SEGMENTSIZE - 1;
  slack = SEGMENTSIZE * MAXLOADFCTR;
  directory[0] = new Segment_t();
  int i;
 
  /* The first segment cleared before used */
  for(i  = 0; i < SEGMENTSIZE; i++ )
    directory[0]->elements[i] = 0;
  
  /* clear the rest of the directory */
  for(i = 1; i < DIRECTORYSIZE; i++)
    directory[i] = 0;
}

template <class C>
inline
void
NdbLinHash<C>::getBucket(Uint32 hash, int * dir, int * seg){
  Uint32 adress = hash & max;
  if(adress < p)
    adress = hash & (2 * max + 1);
  
  * dir = DIRINDEX(adress);
  * seg = SEGINDEX(adress);
}

template <class C>
inline
Int32
NdbLinHash<C>::insertKey( const char* str, Uint32 len, Uint32 lkey1, C* data )
{
  const Uint32 hash = Hash(str, len);
  int dir, seg;
  getBucket(hash, &dir, &seg);
  
  NdbElement_t<C> **chainp = &directory[dir]->elements[seg];
  
  /**
   * Check if the string already are in the hash table
   * chain=chainp will copy the contents of HASH_T into chain  
   */
  NdbElement_t<C> * oldChain = 0;  
  NdbElement_t<C> * chain;
  for(chain = *chainp; chain != 0; chain = chain->next){
    if(chain->len == len && !memcmp(chain->str, str, len)) 
      return -1; /* Element already exists */
    else 
      oldChain = chain;
  }

  /* New entry */
  chain = new NdbElement_t<C>();
  chain->len = len;
  chain->hash = hash;
  chain->localkey1 = lkey1;
  chain->next = 0;
  chain->theData = data;
  chain->str = new Uint32[((len + 3) >> 2)];
  memcpy( &chain->str[0], str, len );
  if (oldChain != 0) 
    oldChain->next = chain;
  else
    *chainp =  chain; 
  
#if 0
  if(--(slack) < 0)
    expandHashTable(); 
#endif
  
  return chain->localkey1;
}


template <class C>
inline
Uint32*
NdbLinHash<C>::getKey( const char* str, Uint32 len )
{
  const Uint32 tHash = Hash(str, len);
  int dir, seg;
  getBucket(tHash, &dir, &seg);
  
  NdbElement_t<C> ** keyp = &directory[dir]->elements[seg];
  
  /*Check if the string are in the hash table*/
  for(NdbElement_t<C> * key = *keyp; key != 0; key = key->next ) {
    if(key->len == len && !memcmp(key->str, str, len)) {
      return &key->localkey1;  
    }
  }
  return NULL ; /* The key was not found */	
}

template <class C>
inline
C*
NdbLinHash<C>::getData( const char* str, Uint32 len ){
  
  const Uint32 tHash = Hash(str, len);
  int dir, seg;
  getBucket(tHash, &dir, &seg);
  
  NdbElement_t<C> ** keyp = &directory[dir]->elements[seg];
    
  /*Check if the string are in the hash table*/
  for(NdbElement_t<C> * key = *keyp; key != 0; key = key->next ) {
    if(key->len == len && !memcmp(key->str, str, len)) {
      return key->theData;  
    }
  }
  return NULL ; /* The key was not found */	
}

template <class C>
inline
C *
NdbLinHash<C>::deleteKey ( const char* str, Uint32 len){
  const Uint32 hash = Hash(str, len);
  int dir, seg;
  getBucket(hash, &dir, &seg);
  
  NdbElement_t<C> *oldChain = 0;
  NdbElement_t<C> **chainp = &directory[dir]->elements[seg];
  for(NdbElement_t<C> * chain = *chainp; chain != 0; chain = chain->next){
    if(chain->len == len && !memcmp(chain->str, str, len)){
      C *data= chain->theData;
      if (oldChain == 0) {
	* chainp = chain->next;
      } else {
	oldChain->next = chain->next;
      }
      delete chain;
      return data;
    } else {
      oldChain = chain;
    }
  }
  return 0; /* Element doesn't exist */
}

template <class C>
inline
void 
NdbLinHash<C>::releaseHashTable( void ){
  NdbElement_t<C>* tNextElement;
  NdbElement_t<C>* tElement;
  
  //Traverse the whole directory structure
  for(int countd = 0; countd < DIRECTORYSIZE;countd++ ){
    if (directory[countd] != 0) {
      //Traverse whole hashtable
      for(int counts = 0; counts < SEGMENTSIZE; counts++ )
	if (directory[countd]->elements[counts] != 0) {
	  tElement = directory[countd]->elements[counts];
	  //Delete all elements even those who is linked
	  do {
	    tNextElement = tElement->next;	       
	    delete tElement;
	    tElement = tNextElement;
	  } while (tNextElement != 0);
	}
      delete directory[countd];
    }   
  }
}

template <class C>
inline
void
NdbLinHash<C>::shrinkTable( void )
{
  Segment_t *lastseg;
  NdbElement_t<C> **chainp;
  Uint32 oldlast = p + max;

  if( oldlast == 0 )
    return;

  // Adjust the state variables.
  if( p == 0 ) {
    max >>= 1;
    p = max;
  }
  else
    --(p);
    
  // Update slack after shrink.
    
  slack -= MAXLOADFCTR;

  // Insert the chain oldlast at the end of chain p.
    
  chainp = &directory[DIRINDEX(p)]->elements[SEGINDEX(p)];
  while( *chainp != 0 ) {
    chainp = &((*chainp)->next);
    lastseg = directory[DIRINDEX(oldlast)];
    *chainp = lastseg->elements[SEGINDEX(oldlast)];

    // If necessary free last segment.
    if( SEGINDEX(oldlast) == 0)
      delete lastseg;
  }
}

template <class C>
inline
void 
NdbLinHash<C>::expandHashTable( void )
{

  NdbElement_t<C>	**oldbucketp, *chain, *headofold, *headofnew, *next;
  Uint32		maxp = max + 1;
  Uint32		newadress = maxp + p;


  // Still room in the adress space?
  if( newadress >= DIRECTORYSIZE * SEGMENTSIZE ) {
    return;
  }  
  
  // If necessary, create a new segment.
  if( SEGINDEX(newadress) == 0 )
    directory[DIRINDEX(newadress)] = new Segment_t();
    
  // Locate the old (to be split) bucket.
  oldbucketp = &directory[DIRINDEX(p)]->elements[SEGINDEX(p)];
    
  // Adjust the state variables.
  p++; 	    
  if( p > max ) {
    max = 2 *max + 1;
    p = 0;
  }
	    
  // Update slack after expandation.
  slack += MAXLOADFCTR;
    
  // Relocate records to the new bucket.
  headofold = 0;
  headofnew = 0;
    
  for( chain = *oldbucketp; chain != 0; chain = next ) {
    next = chain->next;
    if( chain->hash & maxp ) {
      chain->next = headofnew;
      headofnew = chain;
    }
    else {
      chain->next = headofold;
      headofold = chain;
    }
  }
  *oldbucketp = headofold;
  directory[DIRINDEX(newadress)]->elements[SEGINDEX(newadress)] = headofnew;
}

template <class C>
inline
NdbElement_t<C> *
NdbLinHash<C>::getNext(NdbElement_t<C> * curr){
  if(curr != 0 && curr->next != 0)
    return curr->next;
  
  int dir = 0, seg = 0;

  if(curr != 0){
    getBucket(curr->hash, &dir, &seg);
  }
  
  for(int countd = dir; countd < DIRECTORYSIZE;countd++ ){
    if (directory[countd] != 0) {
      for(int counts = seg + 1; counts < SEGMENTSIZE; counts++ ){
	if (directory[countd]->elements[counts] != 0) {
	  return directory[countd]->elements[counts];
	}   
      }
    }
  }

  return 0;
}

#endif
