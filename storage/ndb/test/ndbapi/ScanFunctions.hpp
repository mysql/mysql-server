/* Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>



struct Attrib {
  int numAttribs;
  int attribs[1024];
};
class AttribList {
public:
  AttribList(){}
  ~AttribList(){
    for(unsigned i = 0; i < attriblist.size(); i++){      
      delete attriblist[i];
    }
  }
  void buildAttribList(const NdbDictionary::Table* pTab); 
  Vector<Attrib*> attriblist;
};


// Functions that help out in testing that we may call 
// scan functions in wrong order etc
// and receive a proper errormessage
class ScanFunctions {
public:
  ScanFunctions(const NdbDictionary::Table& _tab) : tab(_tab){
  }
  enum ActionType {
    CloseWithoutStop,
    NextScanWhenNoMore,
    ExecuteScanWithOutOpenScan,
    OnlyOneScanPerTrans,
    OnlyOneOpBeforeOpenScan,
    OnlyOpenScanOnce,
    OnlyOneOpInScanTrans,
    CheckInactivityTimeOut,
    CheckInactivityBeforeClose ,
    NoCloseTransaction,
    EqualAfterOpenScan
  };


  int  scanReadFunctions(Ndb* pNdb,
			 int records,
			 int parallelism,
			 ActionType action,
			 bool exclusive);
private:
  const NdbDictionary::Table& tab;
};


inline 
int 
ScanFunctions::scanReadFunctions(Ndb* pNdb,
				 int records,
				 int parallelism,
				 ActionType action,
				 bool exclusive){
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int sleepTime = 10;
  int                  check;
  NdbConnection	       *pTrans = 0;
  NdbScanOperation     *pOp = 0;

  while (true){
    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }
    
    // Execute the scan without defining a scan operation
    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    if( pOp->readTuples(exclusive ? 
			NdbScanOperation::LM_Exclusive : 
			NdbScanOperation::LM_Read) ) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    
    if (action == OnlyOpenScanOnce){
      // Call openScan one more time when it's already defined
      if( pOp->readTuples(NdbScanOperation::LM_Read) ) {
	NDB_ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }
    
    if (action==EqualAfterOpenScan){
      check = pOp->equal(tab.getColumn(0)->getName(), 10);
      if( check == -1 ) {
	NDB_ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }	
    }
    
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if(pOp->getValue(tab.getColumn(a)->getName()) == NULL) {
	NDB_ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }      
    
    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    int abortCount = records / 10;
    bool abortTrans = (action==CloseWithoutStop);
    int eof;
    int rows = 0;
    eof = pOp->nextResult();
    
    while(eof == 0){
      rows++;
      
      if (abortCount == rows && abortTrans == true){
	g_info << "Scan is aborted after "<<abortCount<<" rows" << endl;
	
	if (action != CloseWithoutStop){
	  // Test that we can closeTrans without stopScan
	  pOp->close();
	  if( check == -1 ) {
	    NDB_ERR(pTrans->getNdbError());
	    pNdb->closeTransaction(pTrans);
	    return NDBT_FAILED;
	  }
	}

	
	pNdb->closeTransaction(pTrans);
	return NDBT_OK;
      }
      
      if(action == CheckInactivityTimeOut){
	if ((rows % (records / 10)) == 0){
	  // Sleep for a long time before calling nextScanResult
	  if (sleepTime > 1)
	    sleepTime--;
	  g_info << "Sleeping "<<sleepTime<<" secs " << endl;
	  NdbSleep_SecSleep(sleepTime); 
	}
      }

      eof = pOp->nextResult();
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	
	// Be cruel, call nextScanResult after error
	for(int i=0; i<10; i++){
	  eof = pOp->nextResult();
	  if(eof == 0){
	    g_err << "nextScanResult returned eof = " << eof << endl
		   << " That is an error when there are no more records" << endl;
	    return NDBT_FAILED;
	  }
	}
	// Be cruel end

	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	g_info << "Starting over" << endl;

	// If test is CheckInactivityTimeOut
	// error 296 is expected
	if ((action == CheckInactivityTimeOut) &&
	    (err.code == 296))
	  return NDBT_OK;

	continue;
      }
      NDB_ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (action == NextScanWhenNoMore){
      g_info << "Calling nextScanresult when there are no more records" << endl;
      for(int i=0; i<10; i++){
	eof = pOp->nextResult();
	if(eof == 0){
	  g_err << "nextScanResult returned eof = " << eof << endl
		 << " That is an error when there are no more records" << endl;
	  return NDBT_FAILED;
	}
      }

    }
    if(action == CheckInactivityBeforeClose){
      // Sleep for a long time before calling close
      g_info << "NdbSleep_SecSleep(5) before close transaction" << endl;
      NdbSleep_SecSleep(5); 
    }
    if(action == NoCloseTransaction)
      g_info << "Forgetting to close transaction" << endl;
    else
      pNdb->closeTransaction(pTrans);

    g_info << rows << " rows have been read" << endl;
    if (records != 0 && rows != records){
      g_err << "Check expected number of records failed" << endl 
	     << "  expected=" << records <<", " << endl
	     << "  read=" << rows <<  endl;
      return NDBT_FAILED;
    }
    
    return NDBT_OK;
  }
  return NDBT_FAILED;


}

void AttribList::buildAttribList(const NdbDictionary::Table* pTab){
  attriblist.clear();

  Attrib* attr;
  // Build attrib definitions that describes which attributes to read
  // Try to build strange combinations, not just "all" or all PK's

  // Scan without reading any attributes
  attr = new Attrib;
  attr->numAttribs = 0;
  attriblist.push_back(attr);
  int i;
  for(i = 1; i < pTab->getNoOfColumns(); i++){
    attr = new Attrib;
    attr->numAttribs = i;
    for(int a = 0; a<i; a++)
      attr->attribs[a] = a;
    attriblist.push_back(attr);
  }
  for(i = pTab->getNoOfColumns()-1; i > 0; i--){
    attr = new Attrib;
    attr->numAttribs = i;
    for(int a = 0; a<i; a++)
      attr->attribs[a] = a;
    attriblist.push_back(attr);
  }
  for(i = pTab->getNoOfColumns(); i > 0;  i--){
    attr = new Attrib;
    attr->numAttribs = pTab->getNoOfColumns() - i;
    for(int a = 0; a<pTab->getNoOfColumns() - i; a++)
      attr->attribs[a] = pTab->getNoOfColumns()-a-1;
    attriblist.push_back(attr); 
  }  
  for(i = 1; i < pTab->getNoOfColumns(); i++){
    attr = new Attrib;
    attr->numAttribs = pTab->getNoOfColumns() - i;
    for(int a = 0; a<pTab->getNoOfColumns() - i; a++)
      attr->attribs[a] = pTab->getNoOfColumns()-a-1;
    attriblist.push_back(attr); 
  }  
  for(i = 1; i < pTab->getNoOfColumns(); i++){
    attr = new Attrib;
    attr->numAttribs = 2;
    for(int a = 0; a<2; a++){
      attr->attribs[a] = i%pTab->getNoOfColumns();
    }
    attriblist.push_back(attr);
  }

  // Last 
  attr = new Attrib;
  attr->numAttribs = 1;
  attr->attribs[0] = pTab->getNoOfColumns()-1;
  attriblist.push_back(attr);

  // Last and first
  attr = new Attrib;
  attr->numAttribs = 2;
  attr->attribs[0] = pTab->getNoOfColumns()-1;
  attr->attribs[1] = 0;
  attriblist.push_back(attr); 

  // First and last
  attr = new Attrib;
  attr->numAttribs = 2;
  attr->attribs[0] = 0;
  attr->attribs[1] = pTab->getNoOfColumns()-1;
  attriblist.push_back(attr);  

#if 1
  for(unsigned j = 0; j < attriblist.size(); j++){

    g_info << attriblist[j]->numAttribs << ": " ;
    for(int a = 0; a < attriblist[j]->numAttribs; a++)
      g_info << attriblist[j]->attribs[a] << ", ";
    g_info << endl;
  }
#endif

}
