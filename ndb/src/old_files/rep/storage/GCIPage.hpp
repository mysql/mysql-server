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

#ifndef GCI_PAGE_HPP
#define GCI_PAGE_HPP

#include "LogRecord.hpp"
#include <TransporterDefinitions.hpp>

#include <rep/rep_version.hpp>

/**
 * @class GCIPage
 * @brief A GCIPage contains a number of LogRecords for a certain GCI.
 */
class GCIPage 
{
public:
  GCIPage(Uint32 gci);
  GCIPage(Uint32 gci, char * dataPtr, Uint32 szBytes);

  /**
   *  @fn      insertLogRecord
   *  @param   tableId    the table this will be LogRecord applies to.
   *  @param   operation  the operation this LogRecord represents
   *  @param   ptr        A LinearSectionPtr p'tr that contains the data.
   *  @return             PAGE_FULL if the page is full, otherwise "true"
   */ 
  bool insertLogRecord(Uint32 tableId, Uint32 operation,
		       class LinearSectionPtr ptr[3]);

  /**
   *  @fn      insertMetaRecord
   */
  bool insertMetaRecord(Uint32 tableId, class LinearSectionPtr ptr[3]);

  /**
   *  @fn      getFirstRecord
   *  @return  First record (or NULL if no record is stored on page)
   */
  Record * getFirstRecord() { return m_first; };

  /**
   *  @fn getStorage
   */
  Uint32 *  getStoragePtr() const {return (Uint32*)m_page;} ;
  Uint32    getStorageByteSize() const {return m_usedBytes;} ;
  Uint32    getStorageWordSize() const {return m_usedBytes >> 2;};
  
  /**
   * @fn	copyDataToPage
   * @info	copy dataPtr to Page
   * @param	dataPtr - data to copy
   * @param	dataBLen - size in bytes to copy.
   */
  void copyDataToPage(char * dataPtr, Uint32 szBytes);
  
  /**
   * Iterator for records (Not yet used!  Maybe should not be used.)
   */
  class iterator {
  public:
    iterator(const GCIPage* page);
    Record *  first();   ///< @return First record (or NULL if no page exists)
    Record *  next();    ///< @return Next record (or NULL if no more records)
    bool      exists();  ///< @return true if another record exists-for next()
  private:
    Record *         m_currentRecord;
    const char *     m_data;
    const GCIPage *  m_gciPage;
  };
  friend class GCIPage::iterator;
  
  /**
   *  @fn      getGCI
   *           Get the GCI of all log records stored on this page.
   */
  Uint32 getGCI()           { return m_gci; };

  /**
   *  @fn      isFull
   *  @return  true if page is full, i.e. is one attempt to add a record
   *           has failed, false otherwise.
   */
  bool  isFull()            { return m_full; };

private:
  Uint32	       m_gci;                 ///< GCI for this page

  Record *             m_first;               ///< Pointer to first log record
  Record *             m_last;                ///< Pointer to last log record
  
  bool		       m_full;

  static const Uint32  m_pageBSize = 8192;    ///< Page size in bytes
  char                 m_page[m_pageBSize];   ///< Storage for pages
  char *               m_currentPagePos;
  Uint32               m_usedBytes;
};

#endif
