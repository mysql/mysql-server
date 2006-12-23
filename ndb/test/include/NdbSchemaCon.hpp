/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
 * @note Depricated, use NdbDictionary
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


