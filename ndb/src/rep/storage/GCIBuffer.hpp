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

#ifndef GCI_BUFFER_HPP
#define GCI_BUFFER_HPP

#include "GCIPage.hpp"
#include <Vector.hpp>
#include <TransporterDefinitions.hpp>

#include <signaldata/RepImpl.hpp>

/**
 * @class GCIBuffer
 * @brief A GCIBuffer contains pages containing log records for ONE gci.
 *
 * @todo Load and save to disk
 */

class GCIBuffer 
{
public: 
  GCIBuffer(Uint32 gci, Uint32 id);
  ~GCIBuffer();
 
  /**
   * @fn        insertLogRecord
   * @param     tableId    Table this will be LogRecord applies to.
   * @param     operation  Operation this LogRecord represents
   * @param     ptr        Ptr of type LinearSectionPtr that contains the data.
   * @return    A full page or 0, if the insert didn't generate a full page.  
   */ 
  void insertLogRecord(Uint32 tableId, Uint32 operation, 
		       class LinearSectionPtr ptr[3]);

  void insertMetaRecord(Uint32 tableId, class LinearSectionPtr ptr[3]);

  /**
   * @fn	inserts a page, containing Records into a GCI Buffer.
   * @param     gci - the gci of the page.
   * @param	dataPtr - Pointer originating from Page::m_page.
   * @param     dataBLen - length of dataptr in bytes
   * @note      Page must NOT be deallocated after being inserted!
   */
  void insertPage(Uint32 gci, char * dataPtr, Uint32 dataBLen);

  /**
   * @fn        isComplete 
   * @return    True if this GCI Buffer is done (gci is completed).
   */
  bool isComplete()   { return m_complete; };
  void setComplete()  { m_complete = true; };

  /**
   * @fn	getReceivedBytes
   * @returns   the total number of bytes that this buffer has received.
   */
  Uint32 getReceivedBytes() const { return m_receivedBytes;} ;

  /**
   * Iterator for pages
   */
  class iterator {
  public:
    iterator(const GCIBuffer* gciBuffer);
    GCIPage * first();  ///< @return First page (or NULL if no page exists)
    GCIPage * next();   ///< @return Next page (or NULL if no more page exists)
    bool exists();      ///< @return true if another page exists (for next())
  private:
    Uint32             m_iterator;
    const GCIBuffer *  m_gciBuffer;
  };
  friend class GCIBuffer::iterator;

  /***************************************************************************
   * GCI Buffer meta information
   ***************************************************************************/
  void    setGCI(Uint32 gci)      { m_gci = gci; };
  Uint32  getGCI()                { return m_gci; };
  
  void    setId(Uint32 id)        { m_id = id; };
  Uint32  getId()                 { return m_id; };

  bool	  m_force;	    // if true, ignore "execute" errors when
	      		    // restoring buffer (PUBLIC) during phase
			    // starting.
private: 
  /***************************************************************************
   * Private Variables
   ***************************************************************************/
  Uint32              m_gci;           ///< GCI of this buffer
  Uint32              m_id;            ///< <m_gci, id> names GCIBuffer 
  bool                m_complete;      ///< GCI complete; buffer contains 
                                       ///< everything
  Vector <GCIPage *>  m_pageList;      ///< Storage for data/log record pages.
  Uint32	      m_receivedBytes; ///< Received bytes in this buffer
};

#endif
