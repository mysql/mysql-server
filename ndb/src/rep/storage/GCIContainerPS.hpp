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

#ifndef GCI_CONTAINER_PS_HPP
#define GCI_CONTAINER_PS_HPP

#include <Vector.hpp>
#include <TransporterDefinitions.hpp>

#include "NodeGroupInfo.hpp"
#include <rep/storage/GCIContainer.hpp>

#include <list>
#include <iterator>

/**
 * @class GCIContainerPS
 * @brief Interface to GCIContainer that takes node groups into account
 */
class GCIContainerPS 
{
public:
  GCIContainerPS(Uint32 maxNoOfNodeGrps);
  ~GCIContainerPS();
  
  void setNodeGroupInfo(NodeGroupInfo * info);
  NodeGroupInfo * getNodeGroupInfo() {return m_nodeGroupInfo;};
  
  void createGCIBuffer(Uint32 gci, Uint32 id);
  void getAvailableGCIBuffers(Uint32 id /*nodegrp */, 
			      Uint32 * first, Uint32 * last);

  /***************************************************************************
   * Record interface
   ***************************************************************************/
  void insertLogRecord(Uint32 grpId, Uint32 tableId, Uint32 operation,
		       class LinearSectionPtr ptr[3], Uint32 gci);
  
  void insertMetaRecord(Uint32 grpId, Uint32 tableId, 
			class LinearSectionPtr ptr[3], Uint32 gci);

  /**
   *  Destroy all buffers with GCI strictly less than gci.
   */
  void destroyGCIBuffersBeforeGCI(Uint32 gci);

  /**
   * Set that buffer is completed, i.e. no more records are to be inserted
   */
  void setCompleted(Uint32 gci, Uint32 id);

  /**
   * Fetch buffer
   * @return    GCIBuffer for gci, or NULL if no buffer found
   */
  GCIBuffer * getGCIBuffer(Uint32 gci, Uint32 id);

  /**
   *  Destroy all buffers with GCI gci.
   *  @return   true if buffer was deleted, false if no buffer exists
   */
  bool destroyGCIBuffer(Uint32 gci, Uint32 id);


  /**
   * Resets the gcicontainer to its original state (initial state and empty)
   * @return   true if reset was ok
   */
  bool reset();

private:
  GCIContainer *         m_container;
  NodeGroupInfo *        m_nodeGroupInfo;
};


#endif
