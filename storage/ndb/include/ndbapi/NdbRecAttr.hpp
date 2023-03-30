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

#ifndef NdbRecAttr_H
#define NdbRecAttr_H

#include "NdbDictionary.hpp"
#include "Ndb.hpp"

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
 *   if (MyTransaction->execute(Commit) == -1) goto error;
 *
 *   ndbout << MyRecAttr->u_32_value();
 * @endcode
 * For more examples, see 
 * @ref ndbapi_simple.cpp.
 *
 * @note The NdbRecAttr object is instantiated with its value when 
 *       NdbTransaction::execute is called.  Before this, the value is 
 *       undefined.  (NdbRecAttr::isNULL can be used to check 
 *       if the value is defined or not.)
 *       This means that an NdbRecAttr object only has valid information
 *       between the time of calling NdbTransaction::execute and
 *       the time of Ndb::closeTransaction.
 *       The value of the null indicator is -1 until the
 *       NdbTransaction::execute method have been called.
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
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexScanOperation;
  friend class NdbEventOperationImpl;
  friend class NdbReceiver;
  friend class Ndb;
  friend class NdbQueryOperationImpl;
  friend class NdbOut& operator<<(class NdbOut&, const class AttributeS&);
#endif

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
   */
  Uint32 get_size_in_bytes() const { return m_size_in_bytes; }

  /** @} *********************************************************************/
  /** 
   * @name Getting stored value
   * @{
   */

  /** 
   * Check if attribute value is NULL.
   *
   * @return -1 = Not defined (Failure or 
   *              NdbTransaction::execute not yet called).<br>
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
   * @return  Medium value.
   */
  Int32 medium_value() const;

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
   * @return  Int8 value.
   */           
  Int8  int8_value() const;           

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
   * @return  Unsigned medium value.
   */
  Uint32 u_medium_value() const;

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
   * @return  Uint8 value.
   */   
  Uint8 u_8_value() const;

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

public:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  const NdbRecAttr* next() const;
#endif
private:

  Uint32 attrId() const;              /* Get attribute id                     */
  bool setNULL();                     /* Set NULL indicator                   */
  void setUNDEFINED();                //
  void set_size_in_bytes(Uint32 sz);  /* Set attribute (element) size in bytes. */

  bool receive_data(const Uint32*, Uint32);

  void release();               /* Release memory if allocated          */
  void init();                  /* Initialise object when allocated     */

  NdbRecAttr(Ndb*);
  void next(NdbRecAttr* aRecAttr);
  NdbRecAttr* next();

  int setup(const class NdbDictionary::Column* col, char* aValue);
  int setup(const class NdbColumnImpl* anAttrInfo, char* aValue);
  int setup(Uint32 byteSize, char* aValue);
                                /* Set up attributes and buffers        */
  bool copyoutRequired() const; /* Need to copy data to application     */

  Uint64        theStorage[4];  /* The data storage here if <= 32 bytes */
  Uint64*       theStorageX;    /* The data storage here if >  32 bytes */
  char*         theValue;       /* The data storage in the application  */
  void*         theRef;         /* Pointer to one of above              */

  NdbRecAttr*   theNext;        /* Next pointer                         */
  Uint32        theAttrId;      /* The attribute id                     */
  
  Int32 m_size_in_bytes;
  const NdbDictionary::Column* m_column;

  // not-NULL means skip length bytes and store their value here
  Uint16* m_getVarValue;

  friend struct Ndb_free_list_t<NdbRecAttr>;

  NdbRecAttr(const NdbRecAttr&); // Not impl.
  NdbRecAttr&operator=(const NdbRecAttr&);
};

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

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
Int8
NdbRecAttr::int8_value() const
{
  return *(Int8*)theRef;
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
Uint8
NdbRecAttr::u_8_value() const
{
  return *(Uint8*)theRef;
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
  m_getVarValue = 0;
}

inline
void
NdbRecAttr::next(NdbRecAttr* aRecAttr)
{
  theNext = aRecAttr;
}

inline
NdbRecAttr*
NdbRecAttr::next()
{
  return theNext;
}

inline
const NdbRecAttr*
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
  m_size_in_bytes= 0;
  return true;
}

inline
int
NdbRecAttr::isNULL() const
{
  return m_size_in_bytes == 0 ? 1 : (m_size_in_bytes > 0 ? 0 : -1);
}

inline
void
NdbRecAttr::setUNDEFINED()
{
  m_size_in_bytes= -1;
}

inline
void
NdbRecAttr::set_size_in_bytes(Uint32 sz)
{
  m_size_in_bytes = sz;
}

class NdbOut& operator <<(class NdbOut&, const NdbRecAttr &);

class NdbRecordPrintFormat : public NdbDictionary::NdbDataPrintFormat
{
public:
  NdbRecordPrintFormat() : NdbDataPrintFormat() {}
  ~NdbRecordPrintFormat() override {}
};

/* See also NdbDictionary::printFormattedValue() */

NdbOut&
ndbrecattr_print_formatted(NdbOut& out, const NdbRecAttr &r,
                           const NdbRecordPrintFormat &f);

#endif // ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

#endif

