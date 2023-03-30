/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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


/*****************************************************************************
Name:          NdbSchemaOp.cpp
Include:
Link:
Author:        UABMNST Mona Natterkvist UAB/B/SD
               EMIKRON Mikael Ronstrom                         
Date:          040524
Version:       3.0
Description:   Interface between application and NDB
Documentation: Handles createTable and createAttribute calls

Adjust:  980125  UABMNST   First version.
         020826  EMIKRON   New version for new DICT
         040524  Magnus Svensson - Adapted to not be included in public NdbApi
                                   unless the user wants to use it.

  NOTE: This file is only used as a compatibility layer for old test programs,
        New programs should use NdbDictionary.hpp
*****************************************************************************/

#include <ndb_global.h>
#include <NdbApi.hpp>
#include <NdbSchemaOp.hpp>
#include <NdbSchemaCon.hpp>


/*****************************************************************************
NdbSchemaOp(Ndb* aNdb, Table* aTable);

Return Value:  None
Parameters:    aNdb: Pointers to the Ndb object.
               aTable: Pointers to the Table object
Remark:        Create an object of NdbSchemaOp. 
*****************************************************************************/
NdbSchemaOp::NdbSchemaOp(Ndb* aNdb) : 
  theNdb(aNdb),
  theSchemaCon(NULL),
  m_currentTable(NULL)
{
}//NdbSchemaOp::NdbSchemaOp()

/*****************************************************************************
~NdbSchemaOp();

Remark:         Delete tables for connection pointers (id).
*****************************************************************************/
NdbSchemaOp::~NdbSchemaOp( )
{
}//~NdbSchemaOp::NdbSchemaOp()
     
/*****************************************************************************
int createTable( const char* tableName )
*****************************************************************************/
int
NdbSchemaOp::createTable(const char* aTableName, 
                         Uint32 aTableSize, 
                         KeyType aTupleKey,
                         int aNrOfPages, 
                         FragmentType aFragmentType, 
                         int aKValue,
                         int aMinLoadFactor,
                         int aMaxLoadFactor,
                         int aMemoryType,
                         bool aStoredTable)
{
  if(m_currentTable != 0){
    return -1;
  }

  m_currentTable = new NdbDictionary::Table(aTableName);
  m_currentTable->setKValue(aKValue);
  m_currentTable->setMinLoadFactor(aMinLoadFactor);
  m_currentTable->setMaxLoadFactor(aMaxLoadFactor);
  m_currentTable->setLogging(aStoredTable);
  m_currentTable->setFragmentType(NdbDictionary::Object::FragAllMedium);
  return 0;
}//NdbSchemaOp::createTable()

/******************************************************************************
int createAttribute( const char* anAttrName,            
                         KeyType aTupleyKey,            
                             int anAttrSize,                    
                             int anArraySize,                           
                        AttrType anAttrType,
                        SafeType aSafeType,             
                     StorageMode aStorageMode,
                             int aNullAttr,
                             int aStorageAttr );

******************************************************************************/
int     
NdbSchemaOp::createAttribute( const char* anAttrName,                   
                              KeyType aTupleKey,                        
                              int anAttrSize,                   
                              int anArraySize,                          
                              AttrType anAttrType,      
                              StorageMode aStorageMode,
                              bool nullable,
                              int aStorageAttr,
			      int aDistributionKeyFlag,
			      int aDistributionGroupFlag,
			      int aDistributionGroupNoOfBits,
                              bool aAutoIncrement,
                              const char* aDefaultValue)
{
  if (m_currentTable == 0){
    return -1;
  }//if
  
  NdbDictionary::Column col(anAttrName);
  switch(anAttrType){
  case Signed:
    if(anAttrSize == 64)
      col.setType(NdbDictionary::Column::Bigint);
    else
      col.setType(NdbDictionary::Column::Int);
    break;
  case UnSigned:
    if(anAttrSize == 64)
      col.setType(NdbDictionary::Column::Bigunsigned);
    else
      col.setType(NdbDictionary::Column::Unsigned);
    break;
  case Float:
    if(anAttrSize == 64)
      col.setType(NdbDictionary::Column::Double);
    else
      col.setType(NdbDictionary::Column::Float);
    break;
  case String:
    col.setType(NdbDictionary::Column::Char);
    break;
  case NoAttrTypeDef:
    abort();
  }
  col.setLength(anArraySize);
  col.setNullable(nullable);
  if(aTupleKey != NoKey)
    col.setPrimaryKey(true);
  else
    col.setPrimaryKey(false);

  col.setDistributionKey(aDistributionKeyFlag);
  col.setAutoIncrement(aAutoIncrement);
  col.setDefaultValue(aDefaultValue != 0 ? aDefaultValue : "");
  
  m_currentTable->addColumn(col);
  return 0;      
}

/******************************************************************************
void release();

Remark:        Release all objects connected to the schemaop object.
******************************************************************************/
void
NdbSchemaOp::release(){
}//NdbSchemaOp::release()

/******************************************************************************
int sendRec()

Return Value:   Return 0 : send was successful.
                Return -1: In all other case.   
Parameters:
Remark:         Send and receive signals for schema transaction based on state
******************************************************************************/
int
NdbSchemaOp::sendRec(){
  int retVal = 0;
  if(m_currentTable == 0){
    retVal = -1;
  } else {
    retVal = theNdb->getDictionary()->createTable(* m_currentTable);
    delete m_currentTable;
    theSchemaCon->theError.code = theNdb->getDictionary()->getNdbError().code;
  }
  
  return retVal;
}//NdbSchemaOp::sendRec()

/******************************************************************************
int init();

Return Value:  Return 0 : init was successful.
               Return -1: In all other case.  
Remark:        Initiates SchemaOp record after allocation.
******************************************************************************/
int
NdbSchemaOp::init(NdbSchemaCon* aSchemaCon)
{
  theSchemaCon = aSchemaCon;
  return 0;
}//NdbSchemaOp::init()


const NdbError &
NdbSchemaOp::getNdbError() const
{
   return theSchemaCon->getNdbError();
}

