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

// 
//  select_all.cpp:  Prints all rows of a table
//
//  Usage:  select_all <table_name>+

#include <NdbApi.hpp>
 
// Used for cout
#include <iostream>
using namespace std;
#include <stdio.h>
#include <string.h>

#define APIERROR(error) \
  { cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
         << error.code << ", msg: " << error.message << "." << endl; \
    exit(-1); }

void usage(const char* prg) {
  cout << "Usage: " << prg << " <table name>" << endl;
  cout << "Prints all rows of table named <table name>" << endl;
  exit(0);
}

/*****************************************************************************
 *************************** Result Set Container ****************************
 *****************************************************************************/

/*
 * Container of NdbRecAttr objects.
 * (NdbRecAttr objects are database rows read by a scan operation.)
 */
class ResultSetContainer {
public:
  /**
   * Initialize ResultSetContainer object for table named <tableName>
   * - Allocates memory 
   * - Fetches attribute names from NDB Cluster
   */
  void init(NdbDictionary::Dictionary* dict, const char* tableName);
  
  /**
   * Get no of attributes for stored NdbRecAttr objects
   */
  int getNoOfAttributes() const;

  /**
   * Get NdbRecAttr object no i
   */
  NdbRecAttr* & getAttrStore(int i);

  /**
   * Get attribute name of attribute no i
   */
  const char*  getAttrName(int i) const;
  
  /**
   * Print header of rows
   */
  void header() const;

private:
  int         m_cols;      // No of attributes for stored NdbRecAttr objects
  char        **m_names;   // Names of attributes
  NdbRecAttr  **m_data;    // The actual stored NdbRecAttr objects
};

void ResultSetContainer::init(NdbDictionary::Dictionary * dict, 
			      const char* tableName) 
{
  // Get Table object from NDB (this contains metadata about all tables)
  const NdbDictionary::Table * tab = dict->getTable(tableName);
  
  // Get table id of the table we are interested in
  if (tab == 0) APIERROR(dict->getNdbError()); // E.g. table didn't exist
  
  // Get no of attributes and allocate memory
  m_cols = tab->getNoOfColumns();
  m_names = new char*       [m_cols];
  m_data  = new NdbRecAttr* [m_cols];
  
  // Store all attribute names for the table
  for (int i = 0; i < m_cols; i++) {
    m_names[i] = new char[255];
    BaseString::snprintf(m_names[i], 255, "%s", tab->getColumn(i)->getName());
  }
}

int          ResultSetContainer::getNoOfAttributes() const {return m_cols;}
NdbRecAttr*& ResultSetContainer::getAttrStore(int i)       {return m_data[i];}
const char*  ResultSetContainer::getAttrName(int i) const  {return m_names[i];}

/*****************************************************************************
 **********************************  MAIN  ***********************************
 *****************************************************************************/

int main(int argc, const char** argv) 
{
  ndb_init();
  Ndb* myNdb = new Ndb("ndbapi_example4"); // Object representing the database
  NdbConnection* myNdbConnection;          // For transactions
  NdbOperation* myNdbOperation;            // For operations
  int check;

  if (argc != 2) {
    usage(argv[0]);
    exit(0);
  }
  const char* tableName = argv[1];

  /*******************************************
   * Initialize NDB and wait until its ready *
   *******************************************/
  if (myNdb->init() == -1) { 
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  if (myNdb->waitUntilReady(30) != 0) {
    cout << "NDB was not ready within 30 secs." << endl;
    exit(-1);
  }

  /***************************
   * Define and execute scan *
   ***************************/
  cout << "Select * from " << tableName << endl;

  ResultSetContainer * container = new ResultSetContainer;
  container->init(myNdb->getDictionary(), tableName);
  
  myNdbConnection = myNdb->startTransaction();
  if (myNdbConnection == NULL) APIERROR(myNdb->getNdbError());
  
  myNdbOperation = myNdbConnection->getNdbOperation(tableName);
  if (myNdbOperation == NULL) APIERROR(myNdbConnection->getNdbError());
  
  // Define the operation to be an 'openScanRead' operation.
  check = myNdbOperation->openScanRead(1); 
  if (check == -1) APIERROR(myNdbConnection->getNdbError());
  
  // Set interpreted program to just be the single instruction 
  // 'interpret_exit_ok'.  (This approves all rows of the table.)
  if (myNdbOperation->interpret_exit_ok() == -1) 
    APIERROR(myNdbConnection->getNdbError());
  
  // Get all attribute values of the row
  for(int i = 0; i < container->getNoOfAttributes(); i++){
    if((container->getAttrStore(i) = 
	myNdbOperation->getValue(container->getAttrName(i))) == 0) 
      APIERROR(myNdbConnection->getNdbError());
  }

  // Execute scan operation
  check = myNdbConnection->executeScan();      	        
  if (check == -1) APIERROR(myNdbConnection->getNdbError());
  
  /****************
   * Print header *
   ****************/
  for (int i = 0; i < container->getNoOfAttributes(); i++) 
    cout << container->getAttrName(i) << "\t";
  
  cout << endl;
  for (int i = 0; i < container->getNoOfAttributes(); i++) {
    for (int j = strlen(container->getAttrName(i)); j > 0; j--)
      cout << "-";
    cout << "\t";
  }
  cout << "\n";

  /**************
   * Scan table *
   **************/
  int eof;
  int rows = 0;

  // Print all rows of table
  while ((eof = myNdbConnection->nextScanResult()) == 0) {
    rows++;
    
    for (int i = 0; i < container->getNoOfAttributes(); i++) {
      if (container->getAttrStore(i)->isNULL()) {
	cout << "NULL";
      } else {

	// Element size of value (No of bits per element in attribute value)
	const int  size = container->getAttrStore(i)->attrSize();
	
	// No of elements in an array attribute (Is 1 if non-array attribute)
	const int aSize = container->getAttrStore(i)->arraySize();
	
	switch(container->getAttrStore(i)->attrType()){
	case UnSigned:
	  switch(size) {
	  case 8: cout << container->getAttrStore(i)->u_64_value(); break;
	  case 4: cout << container->getAttrStore(i)->u_32_value(); break;
	  case 2: cout << container->getAttrStore(i)->u_short_value(); break;
	  case 1: cout << (unsigned) container->getAttrStore(i)->u_char_value();
	    break;
	  default: cout << "Unknown size" << endl;
	  }
	  break;
	  
	case Signed:
	  switch(size) {
	  case 8: cout << container->getAttrStore(i)->int64_value(); break;
	  case 4: cout << container->getAttrStore(i)->int32_value(); break;
	  case 2: cout << container->getAttrStore(i)->short_value(); break;
	  case 1: cout << (int) container->getAttrStore(i)->char_value(); break;
	  default: cout << "Unknown size" << endl;
	  }
	  break;
	  
	case String:
	  {
	    char* buf = new char[aSize+1];
	    memcpy(buf, container->getAttrStore(i)->aRef(), aSize);
	    buf[aSize] = 0;
	    cout << buf;
	    delete [] buf;
	  }
	  break;
	  
	case Float:
	  cout << container->getAttrStore(i)->float_value();
	  break;
	  
	default:
	  cout << "Unknown";
	  break;
	}
      }
      cout << "\t";
    }
    cout << endl;
  }
  if (eof == -1) APIERROR(myNdbConnection->getNdbError());
  
  myNdb->closeTransaction(myNdbConnection);
  
  cout << "Selected " << rows << " rows." << endl;
}
