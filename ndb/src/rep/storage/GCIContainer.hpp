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

#ifndef GCI_CONTAINER_HPP
#define GCI_CONTAINER_HPP

#include <Vector.hpp>

#include "LogRecord.hpp"
#include "GCIBuffer.hpp"

#undef swap
#include <list>
#include <iterator>

/**
 * @class GCIContainer
 * @brief Responsible for storing LogRecord:s in GCIBuffer:s
 *
 * Each GCIBuffer stored in the GCIContainer is named by a pair <GCI, id>.
 * (On PS REP the id is the nodeId, on SS REP the id is the node group).
 */
class GCIContainer {
public:
  GCIContainer(Uint32 maxNoOfIds);
  ~GCIContainer();
  
  /***************************************************************************
   * GCIBuffer interface
   ***************************************************************************/
  /**
   * @return    GCIBuffer if success, NULL otherwise
   */
  GCIBuffer * createGCIBuffer(Uint32 gci, Uint32 id);

  /**
   *  Destroy all buffers with GCI strictly less than gci.
   */
  void destroyGCIBuffersBeforeGCI(Uint32 gci, Uint32 id);

  /**
   *  Destroy all buffers with GCI gci.
   *  @return   true if buffer was deleted, false if no buffer exists
   */
  bool destroyGCIBuffer(Uint32 gci, Uint32 id);

  /**
   * Fetch buffer
   * @return    GCIBuffer for gci, or NULL if no buffer found
   */
  GCIBuffer * getGCIBuffer(Uint32 gci, Uint32 id);

  /**
   * Set that buffer is completed, i.e. no more records are to be inserted
   */
  void setCompleted(Uint32 gci, Uint32 id);


  /**
   * @fn        insertPage
   * @param	gci        GCI this page belongs to.
   * @param	id         Id this page belongs to.
   * @param	dataPtr    Pointer originating from Page::m_page
   * @param     dataBLen   Length in bytes of data following dataptr.
   */
  void insertPage(Uint32 gci, Uint32 id, char * dataPtr, Uint32 dataBLen);


  /***************************************************************************
   * Record interface
   ***************************************************************************/
  void insertLogRecord(Uint32 id, Uint32 tableId, Uint32 operation,
		       class LinearSectionPtr ptr[3], Uint32 gci);
  
  void insertMetaRecord(Uint32 id, Uint32 tableId, 
			class LinearSectionPtr ptr[3], Uint32 gci);

  /** 
   * Get available (complete) GCI Buffers that exists in the container.
   * first == last means that there is one complete buffer
   * @param id     Id for which to as for available gci buffers.
   * @param first  First complete gci buffer
   * @param last   Last complete gci buffer
   */
  void getAvailableGCIBuffers(Uint32 id, Uint32 * first, Uint32 * last);

  /**
   * Resets the gcicontainer to its original state (initial state and empty)
   * I.e., same state as when the object was first constructed.
   * @return   true if reset was ok
   */
  bool reset();

private: 
  NdbMutex*                  theMutexPtr;
  MutexVector <GCIBuffer *>  m_bufferList;   ///< All GCIBuffers stored

  typedef struct GCIRange {    
    Uint32 m_firstGCI;
    Uint32 m_lastGCI;
  };
  
  Uint32                     m_maxNoOfIds;

  GCIRange *  gciRange;                   ///< Array of GCI ranges for each id
};

#endif
