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

#include "GCIContainer.hpp"
#include <NdbOut.hpp>
#include <NdbMem.h>
#include <new>

#include <rep/rep_version.hpp>

//#define GCICONTAINER_DEBUG

/*****************************************************************************
 * Constructors / Destructors
 *****************************************************************************/

GCIContainer::GCIContainer(Uint32 maxNoOfIds) 
{
  m_maxNoOfIds = maxNoOfIds;

  gciRange = new GCIRange[maxNoOfIds * sizeof(GCIRange)];
  
  for(Uint32 i = 0; i < maxNoOfIds; i++) {
    gciRange[i].m_firstGCI = 1;  // The empty interval = [1,0]
    gciRange[i].m_lastGCI = 0;
  }
  theMutexPtr = NdbMutex_Create();
}

GCIContainer::~GCIContainer() 
{
  for(Uint32 i=0; i < m_bufferList.size(); i++) {
    delete m_bufferList[i];
    m_bufferList[i] = 0;
  }

  m_bufferList=0;
  delete [] gciRange;
  NdbMutex_Destroy(theMutexPtr);
}

/*****************************************************************************
 * GCIBuffer Create / Destroy
 *****************************************************************************/

GCIBuffer * 
GCIContainer::createGCIBuffer(Uint32 gci, Uint32 id) 
{
  GCIBuffer * buf = new GCIBuffer(gci, id);
  if (buf == NULL) REPABORT("Could not allocate new buffer");
  
  m_bufferList.push_back(buf, true);
  
#ifdef GCICONTAINER_DEBUG
  ndbout_c("GCIContainer: New buffer created (GCI: %d, Id: %d)", gci, id);
#endif
  return buf;
}

/**
 * Delete all GCI buffers strictly less than "gci"
 */
void
GCIContainer::destroyGCIBuffersBeforeGCI(Uint32 gci, Uint32 id) 
{
  for(Uint32 i = 0 ; i < m_bufferList.size(); i++) {
    if(m_bufferList[i]->getGCI() < gci) {
#ifdef GCICONTAINER_DEBUG
      ndbout_c("GCIContainer: Destroying buffer (GCI: %d, id: %d)",
	       m_bufferList[i]->getGCI(), id);
#endif
      destroyGCIBuffer(i, id);
    }
  }
}

/**
 * Delete one GCI Buffer
 */
bool
GCIContainer::destroyGCIBuffer(Uint32 gci, Uint32 id) 
{
  m_bufferList.lock();
  for(Uint32 i = 0 ; i < m_bufferList.size(); i++) {
    if((m_bufferList[i]->getGCI() == gci) && 
       (m_bufferList[i]->getId() == id)) {

      /**
       * Delete the GCI Buffer
       */      
      delete m_bufferList[i];
      m_bufferList[i] = 0;

      /**
       * Remove from the list of buffers stored in GCIContainer
       */
      m_bufferList.erase(i,false); 
      m_bufferList.unlock();

      /**
       * Set info
       */
      NdbMutex_Lock(theMutexPtr);
      if(gciRange[id].m_firstGCI != gci)
	RLOG(("WARNING! Buffer %d deleted from [%d-%d]",
	      gci, gciRange[id].m_firstGCI, gciRange[id].m_lastGCI));
      
      gciRange[id].m_firstGCI++;

      /**
       * Normalize empty interval to [1,0]
       */
      if (gciRange[id].m_firstGCI > gciRange[id].m_lastGCI){
	gciRange[id].m_firstGCI = 1;
	gciRange[id].m_lastGCI = 0;
      }
      NdbMutex_Unlock(theMutexPtr);
      return true;
    }
  }
  m_bufferList.unlock();
  return false;
}

/*****************************************************************************
 * GCIBuffer interface
 *****************************************************************************/

GCIBuffer * 
GCIContainer::getGCIBuffer(Uint32 gci, Uint32 id) 
{
  GCIBuffer * gciBuffer = 0;

  m_bufferList.lock();
  for(Uint32 i=0; i < m_bufferList.size(); i++) {
    gciBuffer = m_bufferList[i];
    if((gciBuffer->getGCI() == gci) && (gciBuffer->getId() == id)) {
      m_bufferList.unlock();
      return gciBuffer;
    }
  }
  m_bufferList.unlock();
  return 0;
}

void
GCIContainer::setCompleted(Uint32 gci, Uint32 id) 
{
  GCIBuffer * gciBuffer = getGCIBuffer(gci, id);
  if(gciBuffer == 0) gciBuffer = createGCIBuffer(gci, id);

  gciBuffer->setComplete();
  
#ifdef GCICONTAINER_DEBUG
  ndbout_c("GCIContainer: Buffer completely stored in GCIContainer (GCI: %d)",
	   gci);
#endif
  
  NdbMutex_Lock(theMutexPtr);
  
  /**
   * If this is the first GCI Buffer to be completed
   * then both first and last must be updated.
   * Subsequently, only the last value must be updated.
   */
  if(gciRange[id].m_firstGCI == 1 && gciRange[id].m_lastGCI == 0) {
    gciRange[id].m_firstGCI =  gci;
    gciRange[id].m_lastGCI =  gci;
  } else {
    if (gci != gciRange[id].m_lastGCI + 1) {
      RLOG(("WARNING! Non-consequtive buffer %u completed [%u-%u])",
	    gci, gciRange[id].m_firstGCI, gciRange[id].m_lastGCI));
    }
    gciRange[id].m_lastGCI = gci;
  }  
  NdbMutex_Unlock(theMutexPtr);
}

void
GCIContainer::getAvailableGCIBuffers(Uint32 id,	Uint32 * first, Uint32 * last) 
{
  NdbMutex_Lock(theMutexPtr);
  *first = gciRange[id].m_firstGCI;
  *last = gciRange[id].m_lastGCI;
  NdbMutex_Unlock(theMutexPtr);
}

/*****************************************************************************
 * Inserts
 *****************************************************************************/
void
GCIContainer::insertMetaRecord(Uint32 id, Uint32 tableId, 
			       class LinearSectionPtr ptr[3], Uint32 gci) 
{
  /**********************************************************
   * 1. Find correct GCI Buffer (Doesn't exist?  Create one)
   **********************************************************/
  GCIBuffer * gciBuffer = getGCIBuffer(gci, id);
  if(gciBuffer == 0) gciBuffer = createGCIBuffer(gci, id);

  /**********************************
   * 2. Insert record into GCIBuffer
   **********************************/
  gciBuffer->insertMetaRecord(tableId, ptr);
}

void
GCIContainer::insertLogRecord(Uint32 id, Uint32 tableId, Uint32 operation,
			      class LinearSectionPtr ptr[3], Uint32 gci) 
{
  /*********************************************************
   * 1. Find correct GCI Buffer (doesn't exist? create one)
   *********************************************************/
  GCIBuffer * gciBuffer = getGCIBuffer(gci, id);
  if(gciBuffer == 0) gciBuffer = createGCIBuffer(gci, id);
  /**********************************
   * 2. Insert record into GCIBuffer
   **********************************/
  gciBuffer->insertLogRecord(tableId, operation, ptr);
}

void
GCIContainer::insertPage(Uint32 gci, Uint32 id,
			 char * dataPtr, Uint32 dataBLen) 
{
  /*********************************************************
   * 1. Find correct GCI Buffer (doesn't exist? create one)
   *********************************************************/
  GCIBuffer * gciBuffer = getGCIBuffer(gci, id);
  if(gciBuffer == 0) gciBuffer = createGCIBuffer(gci, id);

  /********************************
   * 2. Insert page into GCIBuffer
   ********************************/
  gciBuffer->insertPage(gci, dataPtr, dataBLen);
}

bool
GCIContainer::reset() 
{
  /**
   * Clear the intervals
   */ 
  for(Uint32 i = 0; i < m_maxNoOfIds; i++) {
    gciRange[i].m_firstGCI = 1;  // The empty interval = [1,0]
    gciRange[i].m_lastGCI = 0;
  }

  /**
   * Destroy ALL gci buffers for ALL ids
   */
  for(Uint32 i=0; i < m_bufferList.size(); i++) {
    delete m_bufferList[i];
    m_bufferList[i] = 0;
  }
  m_bufferList.clear();

  return true;
}
