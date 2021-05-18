/*
   Copyright (C) 2003-2006 MySQL AB
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



/*********************************************************************
Name:          NdbSchemaCon.cpp
Include:
Link:
Author:        UABMNST Mona Natterkvist UAB/B/SD
               EMIKRON Mikael Ronstrom                         
Date:          020826
Version:       3.0
Description:   Old Interface between application and NDB
Documentation:
Adjust:  980126  UABMNST   First version.
         020826  EMIKRON   New version adapted to new DICT version
         040524  Magnus Svensson - Adapted to not be included in public NdbApi
                                   unless the user wants to use it.

  NOTE: This file is only used as a compatibility layer for old test programs,
        New programs should use NdbDictionary.hpp
*********************************************************************/

#include <ndb_global.h>
#include <NdbApi.hpp>
#include <NdbSchemaCon.hpp>
#include <NdbSchemaOp.hpp>


/*********************************************************************
NdbSchemaCon(Ndb* aNdb);

Parameters:    aNdb: Pointers to the Ndb object 
Remark:        Creates a schemacon object. 
************************************************************************************************/
NdbSchemaCon::NdbSchemaCon( Ndb* aNdb ) :
  theNdb(aNdb),
  theFirstSchemaOpInList(NULL),
  theMagicNumber(0x75318642)
{ 
  theError.code = 0;
}//NdbSchemaCon::NdbSchemaCon()

/*********************************************************************
~NdbSchemaCon();

Remark:        Deletes the connection object. 
************************************************************************************************/
NdbSchemaCon::~NdbSchemaCon()
{
}//NdbSchemaCon::~NdbSchemaCon()

/*********************************************************************
NdbSchemaOp* getNdbSchemaOp();

Return Value    Return a pointer to a NdbSchemaOp object if getNdbSchemaOp was sussesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
************************************************************************************************/
NdbSchemaOp*
NdbSchemaCon::getNdbSchemaOp()
{ 
  NdbSchemaOp* tSchemaOp;
  if (theFirstSchemaOpInList != NULL) {
    theError.code = 4401;	// Only support one add table per transaction
    return NULL;
  }//if
  tSchemaOp = new NdbSchemaOp(theNdb);
  if ( tSchemaOp == NULL ) {
    theError.code = 4000;	// Could not allocate schema operation
    return NULL;
  }//if
  theFirstSchemaOpInList = tSchemaOp;
  int retValue = tSchemaOp->init(this);
  if (retValue == -1) {
    release();
    theError.code = 4000;	// Could not allocate buffer in schema operation
    return NULL;
  }//if
  return tSchemaOp;
}//NdbSchemaCon::getNdbSchemaOp()

/*********************************************************************
int execute();

Return Value:  Return 0 : execute was successful.
               Return -1: In all other case.  
Parameters :   aTypeOfExec: Type of execute.
Remark:        Initialise connection object for new transaction. 
************************************************************************************************/
int 
NdbSchemaCon::execute()
{
  if(theError.code != 0) {
    return -1;
  }//if

  NdbSchemaOp* tSchemaOp;

  tSchemaOp = theFirstSchemaOpInList;
  if (tSchemaOp == NULL) {
    theError.code = 4402;
    return -1;
  }//if

  if ((tSchemaOp->sendRec() == -1) || (theError.code != 0)) {
    // Error Code already set in other place
    return -1;
  }//if
  
  return 0;
}//NdbSchemaCon::execute()

/*********************************************************************
void release();

Remark:         Release all schemaop.
************************************************************************************************/
void 
NdbSchemaCon::release()
{
  NdbSchemaOp* tSchemaOp;
  tSchemaOp = theFirstSchemaOpInList;
  if (tSchemaOp != NULL) {
    tSchemaOp->release();
    delete tSchemaOp;
  }//if
  theFirstSchemaOpInList = NULL;
  return;
}//NdbSchemaCon::release()

#include <NdbError.hpp>

static void
update(const NdbError & _err){
  NdbError & error = (NdbError &) _err;
  ndberror_struct ndberror = (ndberror_struct)error;
  ndberror_update(&ndberror);
  error = NdbError(ndberror);
}

const 
NdbError & 
NdbSchemaCon::getNdbError() const {
  update(theError);
  return theError;
}




            



