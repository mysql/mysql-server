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

#include <ndb_global.h>
#include "GCIBuffer.hpp"

/*****************************************************************************
 * Constructor / Destructor
 *****************************************************************************/

GCIBuffer::GCIBuffer(Uint32 gci, Uint32 id) 
{
  m_gci = gci; 
  m_id = id;
  m_complete = false;
  m_receivedBytes = 0;
}

GCIBuffer::~GCIBuffer() 
{
  /**
   *  Loop through all pages and delete them
   */
  for(Uint32 i=0; i<m_pageList.size(); i++) {
    delete m_pageList[i];
    m_pageList[i] = 0;
  } 
  m_pageList.clear();
  //  m_pageList = 0;
}

/*****************************************************************************
 * Inserts
 *****************************************************************************/

void
GCIBuffer::insertLogRecord(Uint32 tableId, Uint32 operation,
			   class LinearSectionPtr ptr[3]) 
{
  GCIPage * p;
  if(m_pageList.size() == 0) {
    p = new GCIPage(m_gci);
    assert(p != NULL);
    m_pageList.push_back(p);
  }

  p = m_pageList.back();
  if (!p->insertLogRecord(tableId, operation, ptr)) {
    /**
     * GCIPage is full.
     */
    GCIPage * newPage = new GCIPage(m_gci);
    assert(newPage != NULL);
    m_pageList.push_back(newPage);
    bool res = newPage->insertLogRecord(tableId, operation, ptr);
    
    if(!res) {
      ndbout << "GCIBuffer: gci : " << m_gci << endl;
      assert(res); 
    }
  }
}

/**
 * @todo: We must be able to distinguish between Scan meta 
 *	  data and log meta data. 
 *        Currently only scan meta data is considered.
 */ 
void
GCIBuffer::insertMetaRecord(Uint32 tableId, class LinearSectionPtr ptr[3]) 
{
  GCIPage * p;
  if(m_pageList.size()==0) {                          
    p = new GCIPage(m_gci);
    assert(p != NULL);
    m_pageList.push_back(p);
  }
  
  p = m_pageList.back();
  
  if (!p->insertMetaRecord(tableId, ptr)) {           
    /**
     * Page is full.
     */
    GCIPage * newPage = new GCIPage(m_gci);
    assert(newPage != NULL);
    m_pageList.push_back(newPage);
    
    bool res = newPage->insertMetaRecord(tableId, ptr);
    assert(res);
  }
}

void
GCIBuffer::insertPage(Uint32 gci, char * dataPtr, Uint32 dataBLen) 
{
  /**
   * allocate a new GCIPage
   */
  GCIPage *  page = new GCIPage(gci);
  assert(page != 0);

  /**
   * copy data into page
   */
  page->copyDataToPage(dataPtr, dataBLen);
  
  /**
   * put page on pagelist.
   */
  m_pageList.push_back(page);
  
  /**
   * Update GCI Buffer received bytes
   */
  m_receivedBytes += dataBLen;
}


/*****************************************************************************
 * Iterator
 *****************************************************************************/

GCIBuffer::iterator::iterator(const GCIBuffer* gciBuffer) 
{
  m_gciBuffer = gciBuffer;
  m_iterator=0;

}

GCIPage * 
GCIBuffer::iterator::first() 
{
  m_iterator = 0;
  if(m_gciBuffer->m_pageList.size() == 0) return NULL;
  return (m_gciBuffer->m_pageList)[m_iterator];
}


GCIPage * 
GCIBuffer::iterator::next() 
{
  m_iterator++;
  if(m_gciBuffer->m_pageList.size() == 0) return NULL;

  if((m_iterator<m_gciBuffer->m_pageList.size())) 
    return (m_gciBuffer->m_pageList)[m_iterator];
  else
    return NULL;
}


bool 
GCIBuffer::iterator::exists() 
{
  return (m_iterator < m_gciBuffer->m_pageList.size());
}



