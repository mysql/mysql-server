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

#include "GCIPage.hpp"
#include "assert.h"
#include <new>

GCIPage::GCIPage(Uint32 gci) 
{
  m_first = NULL;
  m_last = NULL;
  m_gci = gci;
  m_full = false;
  m_currentPagePos=m_page;
  m_usedBytes = 0;
}

/*****************************************************************************
 * Insert
 *****************************************************************************/

/**
 * Store a new log record on this page.
 * @return True if success, false otherwise
 */
bool 
GCIPage::insertLogRecord(Uint32 tableId, Uint32 operation,
			 class LinearSectionPtr ptr[3]) 
{
  /**
   * Calculate size of new logrecord in bytes
   */
  assert(m_page!=NULL);
  Uint32 size = 4*ptr[0].sz + 4*ptr[1].sz + sizeof(LogRecord);

  if(!((m_currentPagePos + size ) < (m_page + m_pageBSize))) {
    m_full = true;
    return false;  // No free space. GCIBuffer must allocate a new page
  }
  LogRecord * lr = new(m_currentPagePos) LogRecord();
  if (lr==0) REPABORT("Could not allocate new log record");
  
  lr->recordType = Record::LOG;
  lr->recordLen = size;
  lr->operation = operation; 
  lr->tableId = tableId;
  lr->attributeHeaderWSize = ptr[0].sz;
  lr->attributeDataWSize = ptr[1].sz;
  
  m_currentPagePos += sizeof(LogRecord);
  
  lr->attributeHeader = (Uint32*)m_currentPagePos;
  memcpy(lr->attributeHeader, ptr[0].p, lr->attributeHeaderWSize * 4);
  
  m_currentPagePos += lr->attributeHeaderWSize * 4;
  
  lr->attributeData = (Uint32*)m_currentPagePos;
  memcpy(lr->attributeData, ptr[1].p, lr->attributeDataWSize * 4);
  
  m_currentPagePos += lr->attributeDataWSize * 4;

  m_usedBytes+=size;  
  return true;
}

/**
 * Store a new log record on this page.
 * @return True if sucessful, false otherwise.
 */
bool 
GCIPage::insertMetaRecord(Uint32 tableId, class LinearSectionPtr ptr[3]) 
{
  /**
   * Calculate size of new logrecord in bytes
   */
  Uint32 size = 4*ptr[0].sz + sizeof(MetaRecord);
  
  if(!((m_currentPagePos + size ) < (m_page + m_pageBSize))) {
    m_full = true;
    return false;  // No free space. GCIBuffer must allocate a new page
  }
  MetaRecord *  mr = new(m_currentPagePos) MetaRecord();
  if (mr==0) REPABORT("Could not allocate new meta record");

  //  mr->operation = operation; 
  mr->recordType = Record::META;
  mr->recordLen = size;

  mr->tableId = tableId;
  mr->dataLen = ptr[0].sz;

  
  m_currentPagePos += sizeof(MetaRecord);
  
  mr->data = (Uint32*)m_currentPagePos;
  memcpy(mr->data, ptr[0].p, mr->dataLen * 4);
  
  m_currentPagePos += mr->dataLen * 4;

  m_usedBytes+=size;
  return true;
}

/**
 * copy function
 */
void 
GCIPage::copyDataToPage(char * dataPtr, Uint32 dataBLen)
{
  assert (dataBLen < m_pageBSize);
  memcpy(m_page, dataPtr, dataBLen);
  m_currentPagePos=m_page + dataBLen;
  m_usedBytes = dataBLen;
  m_full = true;
  m_first = (Record * )m_page;
  dataPtr = 0;
}

/*****************************************************************************
 * Iterator
 *****************************************************************************/

GCIPage::iterator::iterator(const GCIPage* page) 
{
  m_gciPage = page;
  m_data = m_gciPage->m_page;
  m_currentRecord = (Record*)m_data;
}

Record * 
GCIPage::iterator::first() 
{
  return m_currentRecord;
}  

Record * 
GCIPage::iterator::next() 
{
  m_currentRecord =  (Record*) 
    ((char*)(m_currentRecord)+ m_currentRecord->recordLen);
  if((char*)m_currentRecord < (char*)(m_data + m_gciPage->m_usedBytes))
    return m_currentRecord;
  else {
    return 0;
  }
}

bool 
GCIPage::iterator::exists() 
{
  return ((char*)m_currentRecord < (m_data + m_gciPage->m_usedBytes));
}
