/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NdbSchemaCon_H
#define NdbSchemaCon_H

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED

#include <ndb_types.h>
#include "NdbError.hpp"
#include <NdbSchemaOp.hpp>

class NdbSchemaOp;
class Ndb;
class NdbApiSignal;

/**
 * @class NdbSchemaCon
 * @brief Represents a schema transaction.
 *
 * When creating a new table,
 * the first step is to get a NdbSchemaCon object to represent 
 * the schema transaction.
 * This is done by calling Ndb::startSchemaTransaction.
 * 
 * The next step is to get a NdbSchemaOp object by calling 
 * NdbSchemaCon::getNdbSchemaOp.
 * The NdbSchemaOp object then has methods to define the table and 
 * its attributes.
 *
 * Finally, the NdbSchemaCon::execute method inserts the table 
 * into the database. 
 *
 * @note   Currently only one table can be added per transaction.
 * @note Deprecated, use NdbDictionary
 */
class NdbSchemaCon
{
friend class Ndb;
friend class NdbSchemaOp;
  
public:

  static 
  NdbSchemaCon* startSchemaTrans(Ndb* pNdb){
    return  new NdbSchemaCon(pNdb);
  }
  
  static 
  void closeSchemaTrans(NdbSchemaCon* pSchCon){
    delete pSchCon;
  }
  

  /**
   * Execute a schema transaction.
   * 
   * @return    0 if successful otherwise -1. 
   */
  int 		execute();	  
				  
  /**
   * Get a schemaoperation.
   *
   * @note Currently, only one operation per transaction is allowed.
   *
   * @return   Pointer to a NdbSchemaOp or NULL if unsuccessful.
   */ 
  NdbSchemaOp*	getNdbSchemaOp(); 
	
  /**
   * Get the latest error
   *
   * @return   Error object.
   */			     
  const NdbError & getNdbError() const;

private:

/******************************************************************************
 *	These are the create and delete methods of this class.
 *****************************************************************************/

  NdbSchemaCon(Ndb* aNdb); 
  ~NdbSchemaCon();

/******************************************************************************
 *	These are the private methods of this class.
 *****************************************************************************/

  void release();	         // Release all schemaop in schemaCon

 /***************************************************************************
  *	These methods are service methods to other classes in the NDBAPI.
  ***************************************************************************/

  int           checkMagicNumber();              // Verify correct object
  int           receiveDICTTABCONF(NdbApiSignal* aSignal);
  int           receiveDICTTABREF(NdbApiSignal* aSignal);


  int receiveCREATE_INDX_CONF(NdbApiSignal*);
  int receiveCREATE_INDX_REF(NdbApiSignal*);
  int receiveDROP_INDX_CONF(NdbApiSignal*);
  int receiveDROP_INDX_REF(NdbApiSignal*);


/*****************************************************************************
 *	These are the private variables of this class.
 *****************************************************************************/
  
 
  NdbError 	theError;	      	// Errorcode
  Ndb* 	theNdb;			// Pointer to Ndb object

  NdbSchemaOp*	theFirstSchemaOpInList;	// First operation in operation list.
  int 		theMagicNumber;	// Magic number 
};

inline
int
NdbSchemaCon::checkMagicNumber()
{
  if (theMagicNumber != 0x75318642)
    return -1;
  return 0;
}//NdbSchemaCon::checkMagicNumber()



#endif
#endif


