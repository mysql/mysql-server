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

#ifndef NdbRecAttr_H
#define NdbRecAttr_H

#include <NdbDictionary.hpp>

class NdbOperation;

/**
 * @class NdbRecAttr
 * @brief Contains value of an attribute.
 *
 * NdbRecAttr objects are used to store the attribute value 
 * after retrieving the value from the NDB Cluster using the method 
 * NdbOperation::getValue.  The objects are allocated by the NDB API.
 * An example application program follows:
 *
 * @code
 *   MyRecAttr = MyOperation->getValue("ATTR2", NULL);
 *   if (MyRecAttr == NULL) goto error;
 *
 *   if (MyConnection->execute(Commit) == -1) goto error;
 *
 *   ndbout << MyRecAttr->u_32_value();
 * @endcode
 * For more examples, see 
 * @ref ndbapi_example1.cpp and 
 * @ref ndbapi_example2.cpp.
 *
 * @note The NdbRecAttr object is instantiated with its value when 
 *       NdbConnection::execute is called.  Before this, the value is 
 *       undefined.  (NdbRecAttr::isNULL can be used to check 
 *       if the value is defined or not.)
 *       This means that an NdbRecAttr object only has valid information
 *       between the time of calling NdbConnection::execute and
 *       the time of Ndb::closeTransaction.
 *       The value of the null indicator is -1 until the
 *       NdbConnection::execute method have been called.
 *
 * For simple types, there are methods which directly getting the value
 * from the NdbRecAttr object.
 *
 * To get a reference to the value, there are two methods:
 * NdbRecAttr::aRef (memory is released by NDB API) and 
 * NdbRecAttr::getAttributeObject (memory must be released 
 * by application program).
 * The two methods may return different pointers.
 *
 * There are also methods to check attribute type, attribute size and
 * array size.  
 * The method NdbRecAttr::arraySize returns the number of elements in the
 * array (where each element is of size given by NdbRecAttr::attrSize). 
 * The NdbRecAttr::arraySize method is needed when reading variable-sized
 * attributes.
 *
 * @note Variable-sized attributes are not yet supported.
 */
class NdbRecAttr
{
  friend class NdbOperation;
  friend class NdbIndexScanOperation;
  friend class NdbEventOperationImpl;
  friend class NdbReceiver;
  friend class Ndb;
  friend class NdbOut& operator<<(class NdbOut&, const class AttributeS&);

public:
  /** 
   * @name Getting meta information
   * @{
   */
  const NdbDictionary::Column * getColumn() const;

  /**
   * Get type of column
   * @return Data type of the column
   */
  NdbDictionary::Column::Type getType() const;
  
  /**
   * Get attribute (element) size in bytes. 
   * 
   * @note For arrays, the method only gives the size of an element.
   *       The total attribute size is calculated by 
   *       multiplying this value with the value 
   *       returned by NdbRecAttr::arraySize.
   *
   * @return Attribute size in 32 bit unsigned int. 
   */
  Uint32 attrSize() const ;             

  /**
   * Get array size of attribute. 
   * For variable-sized arrays this method returns the 
   * size of the attribute read.
   *
   * @return array size in 32 unsigned int.
   */
  Uint32 arraySize() const ;            
  Uint32 getLength() const ;
  
  /** @} *********************************************************************/
  /** 
   * @name Getting stored value
   * @{
   */

  /** 
   * Check if attribute value is NULL.
   *
   * @return -1 = Not defined (Failure or 
   *              NdbConnection::execute not yet called).<br>
   *          0 = Attribute value is defined, but not equal to NULL.<br>
   *          1 = Attribute value is defined and equal to NULL.
   */
  int isNULL() const; 

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  64 bit long value.
   */
  Int64 int64_value() const;  

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  32 bit int value.
   */   
  Int32 int32_value() const;  

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  Short value.
   */
  short short_value() const;

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  Char value.
   */           
  char  char_value() const;           

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  64 bit unsigned value.
   */
  Uint64 u_64_value() const;          

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  32 bit unsigned value.
   */
  Uint32 u_32_value() const;          

  /**
   * Get value stored in NdbRecAttr object.
   * 
   * @return  Unsigned short value.
   */
  Uint16 u_short_value() const;

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  Unsigned char value.
   */   
  Uint8 u_char_value() const;        

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  Float value.
   */
  float float_value() const;         

  /**
   * Get value stored in NdbRecAttr object.
   *
   * @return  Double value.
   */
  double double_value() const;        
  
  /** @} *********************************************************************/
  /** 
   * @name Getting reference to stored value
   * @{
   */

  /**
   * Get reference to attribute value. 
   * 
   * Returns a char*-pointer to the value.
   * The pointer is aligned appropriately for the data type.  
   * The memory is released when Ndb::closeTransaction is executed 
   * for the transaction which read the value.
   *
   * @note The memory is released by NDB API.
   * 
   * @note The pointer to the attribute value stored in an NdbRecAttr
   *       object (i.e. the pointer returned by aRef) is constant.  
   *       This means that this method can be called anytime after 
   *       NdbOperation::getValue has been called.
   * 
   * @return Pointer to attribute value.         
   */
  char* aRef() const;                 
                                
  /** @} *********************************************************************/
                             
  /**
   * Make a copy of RecAttr object including all data.
   *
   * @note  Copy needs to be deleted by application program.
   */
  NdbRecAttr * clone() const;
  
  /**
   * Destructor
   *
   * @note  You should only delete RecAttr-copies, 
   *        i.e. objects that has been cloned.
   */
  ~NdbRecAttr();    
private:
  NdbRecAttr();

  Uint32 attrId() const;        /* Get attribute id                     */
  bool setNULL();               /* Set NULL indicator                   */
  bool receive_data(const Uint32*, Uint32);

  void release();               /* Release memory if allocated          */
  void init();                  /* Initialise object when allocated     */

  void next(NdbRecAttr* aRecAttr);
  NdbRecAttr* next() const;

  int setup(const class NdbDictionary::Column* col, char* aValue);
  int setup(const class NdbColumnImpl* anAttrInfo, char* aValue);
                                /* Set up attributes and buffers        */
  bool copyoutRequired() const; /* Need to copy data to application     */
  void copyout();               /* Copy from storage to application     */

  Uint64        theStorage[4];  /* The data storage here if <= 32 bytes */
  Uint64*       theStorageX;    /* The data storage here if >  32 bytes */
  char*         theValue;       /* The data storage in the application  */
  void*         theRef;         /* Pointer to one of above              */

  NdbRecAttr*   theNext;        /* Next pointer                         */
  Uint32        theAttrId;      /* The attribute id                     */

  int theNULLind;
  bool m_nullable;
  Uint32 theAttrSize;
  Uint32 theArraySize;
  const NdbDictionary::Column* m_column;
};

inline
NdbDictionary::Column::Type
NdbRecAttr::getType() const {
  return m_column->getType();
}

inline
const NdbDictionary::Column *
NdbRecAttr::getColumn() const {
  return m_column;
}

inline
Uint32
NdbRecAttr::attrSize() const {
  return theAttrSize;
}

inline
Uint32
NdbRecAttr::arraySize() const 
{
  return theArraySize;
}

inline
Int64
NdbRecAttr::int64_value() const 
{
  return *(Int64*)theRef;
}

inline
Int32
NdbRecAttr::int32_value() const 
{
  return *(Int32*)theRef;
}

inline
short
NdbRecAttr::short_value() const
{
  return *(short*)theRef;
}

inline
char
NdbRecAttr::char_value() const
{
  return *(char*)theRef;
}

inline
Uint64
NdbRecAttr::u_64_value() const
{
  return *(Uint64*)theRef;
}

inline
Uint32
NdbRecAttr::u_32_value() const
{
  return *(Uint32*)theRef;
}

inline
Uint16
NdbRecAttr::u_short_value() const
{
  return *(Uint16*)theRef;
}

inline
Uint8
NdbRecAttr::u_char_value() const
{
  return *(Uint8*)theRef;
}

inline
float
NdbRecAttr::float_value() const
{
  return *(float*)theRef;
}

inline
double
NdbRecAttr::double_value() const
{
  return *(double*)theRef;
}

inline
void
NdbRecAttr::release()
{
  if (theStorageX != 0) {
    delete [] theStorageX;
    theStorageX = 0;
  }
}

inline
void
NdbRecAttr::init()
{
  theStorageX = 0;
  theValue = 0;
  theRef = 0;
  theNext = 0;
  theAttrId = 0xFFFF;
  theNULLind = -1;
}

inline
void
NdbRecAttr::next(NdbRecAttr* aRecAttr)
{
  theNext = aRecAttr;
}

inline
NdbRecAttr*
NdbRecAttr::next() const
{
  return theNext;
}

inline
char*
NdbRecAttr::aRef() const
{
  return (char*)theRef; 
}

inline
bool
NdbRecAttr::copyoutRequired() const
{
  return theRef != theValue && theValue != 0;
}

inline
Uint32
NdbRecAttr::attrId() const
{
  return theAttrId;
}

inline
bool
NdbRecAttr::setNULL()
{
  theNULLind = 1;
  return m_nullable;
}

inline
int
NdbRecAttr::isNULL() const
{
  return theNULLind;
}

class NdbOut& operator <<(class NdbOut&, const NdbRecAttr &);

#endif

