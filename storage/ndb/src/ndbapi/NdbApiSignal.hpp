/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**********************************************************************
 * Name:		NdbApiSignal.H
 * Include:	
 * Link:		
 * Author:		UABMNST Mona Natterkvist UAB/B/SD
 * Date:		97----
 * Version:	0.1
 * Description:	Interface between TIS and NDB
 * Documentation:
 * Adjust:		971204  UABMNST   First version.
 * Adjust:         000705  QABANAB   Changes in Protocol2
 * Comment:	
 *****************************************************************************/
#ifndef NdbApiSignal_H
#define NdbApiSignal_H

#include <kernel_types.h>
#include <RefConvert.hpp>
#include <TransporterDefinitions.hpp>

class Ndb;

/**
 * A NdbApiSignal : public SignalHeader
 *
 * Stores the address to theData in theSignalId
 */
class NdbApiSignal : public SignalHeader
 {
public:  
  			NdbApiSignal(Ndb* ndb);
  			NdbApiSignal(BlockReference ref);
  			NdbApiSignal(const NdbApiSignal &);
                        NdbApiSignal(const SignalHeader &header)
			  : SignalHeader(header), theNextSignal(nullptr), 
                            theRealData(nullptr) {}
  			~NdbApiSignal();

  void                  set(Uint8  trace,
			    Uint16 receiversBlockNumber,
			    Uint16 signalNumber,
			    Uint32 length);

  
  void 			setData(Uint32 aWord, Uint32 aDataNo);  
  Uint32 		readData(Uint32 aDataNo) const; // Read word in signal

  // Set signal header
  int                   setSignal(int NdbSignalType, Uint32 receiverBlockNo);
  int 			readSignalNumber() const;	// Read signal number
  Uint32             	getLength() const;
  Uint32             	getNoOfSections() const;
  void	             	setLength(Uint32 aLength);
  void 			next(NdbApiSignal* anApiSignal);  
  NdbApiSignal* 	next();
 
   const Uint32 *       getDataPtr() const;
         Uint32 *       getDataPtrSend();
   const Uint32 *       getConstDataPtrSend() const;
   static constexpr Uint32 MaxSignalWords = 25;

  NodeId                get_sender_node() const;

  /**
   * Fragmentation
   */
  bool isFragmented() const { return m_fragmentInfo != 0;}
  bool isFirstFragment() const { return m_fragmentInfo <= 1;}
  bool isLastFragment() const { 
    return m_fragmentInfo == 0 || m_fragmentInfo == 3; 
  }
  
  Uint32 getFragmentId() const { 
    return (m_fragmentInfo == 0 ? 0 : getDataPtr()[theLength - 1]); 
  }

  Uint32 getFragmentSectionNumber(Uint32 i) const
  {
    return getDataPtr()[theLength - 1 - m_noOfSections + i];
  }

  NdbApiSignal& operator=(const NdbApiSignal& src) {
    copyFrom(&src);
    return *this;
  }

private:
  void setDataPtr(Uint32 *);
  
  friend class AssembleBatchedFragments;
  friend class NdbTransaction;
  friend class NdbScanReceiver;
  friend class Table;
  friend class TransporterFacade;
  void copyFrom(const NdbApiSignal * src);

  /**
   * Only used when creating a signal in the api
   */
  Uint32 theData[MaxSignalWords];
  NdbApiSignal *theNextSignal;
  Uint32 *theRealData;
};
/**********************************************************************
NodeId get_sender_node
Remark:        Get the node id of the sender
***********************************************************************/
inline
NodeId
NdbApiSignal::get_sender_node() const
{
  return refToNode(theSendersBlockRef);
}

/**********************************************************************
void getLength
Remark:        Get the length of the signal.
******************************************************************************/
inline
Uint32
NdbApiSignal::getLength() const{
  return theLength;
}

/* Get number of sections in signal */
inline
Uint32
NdbApiSignal::getNoOfSections() const
{
  return m_noOfSections;
}

/**********************************************************************
void setLength
Parameters:    aLength: Signal length
Remark:        Set the length in the signal.
******************************************************************************/
inline
void
NdbApiSignal::setLength(Uint32 aLength){
  theLength = aLength;
}

/**********************************************************************
void next(NdbApiSignal* aSignal);

Parameters:     aSignal: Signal object.
Remark:         Insert signal rear in a linked list.   
*****************************************************************************/
inline
void 
NdbApiSignal::next(NdbApiSignal* aSignal){
  theNextSignal = aSignal;
}
/**********************************************************************
NdbApiSignal* next();

Return Value:   Return theNext signal object if the next was successful.
                Return NULL: In all other case.  
Remark:         Read the theNext in signal.   
*****************************************************************************/
inline
NdbApiSignal* 
NdbApiSignal::next(){
  return theNextSignal;
}
/**********************************************************************
int readSignalNo();

Return Value:    Return the signalNumber. 
Remark:          Read signal number 
*****************************************************************************/
inline
int		
NdbApiSignal::readSignalNumber() const
{
  return (int)theVerId_signalNumber;
}
/**********************************************************************
Uint32 readData(Uint32 aDataNo);

Return Value:   Return Data word in a signal.
                Return -1: In all other case.
                aDataNo: Data number in signal.
Remark:         Return the dataWord information in a signal for a dataNo.  
******************************************************************************/
inline
Uint32
NdbApiSignal::readData(Uint32 aDataNo) const {
  return getDataPtr()[aDataNo-1];
}
/**********************************************************************
int setData(Uint32 aWord, int aDataNo);

Return Value:   Return 0 : setData was successful.
                Return -1: In all other case.  
Parameters:     aWord: Data word.
                aDataNo: Data number in signal.
Remark:         Set Data word in signal 1 - 25  
******************************************************************************/
inline
void
NdbApiSignal::setData(Uint32 aWord, Uint32 aDataNo){
  getDataPtrSend()[aDataNo -1] = aWord;
}

/**
 * Return pointer to data structure
 */
inline
const Uint32 *
NdbApiSignal::getDataPtr() const {
  return theRealData;
}

inline Uint32* NdbApiSignal::getDataPtrSend() { return theData; }

inline
const Uint32 *
NdbApiSignal::getConstDataPtrSend() const
{
  return theData;
}

inline
void
NdbApiSignal::setDataPtr(Uint32 * ptr){
  theRealData = ptr;
}

#endif
