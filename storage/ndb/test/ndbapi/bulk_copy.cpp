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


#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>

#include <getarg.h>


int setValuesFromLine(NdbOperation* pOp,
		      const NdbDictionary::Table* pTab, 
		      char* line){

  int check = 0;
  char* p = line;
  char* pn;
  char buf[8000];
  // Loop through each attribute in this table	 
  for (int a = 0; a<pTab->getNoOfColumns(); a++){

    pn = p;
    while (*pn != ';')
      pn++;
    
    memset(buf, 0, sizeof(buf));
    strncpy(buf, p, pn-p);
    //    ndbout << a << ": " << buf << endl;
    const NdbDictionary::Column* attr = pTab->getColumn(a);            
    switch (attr->getType()){
    case NdbDictionary::Column::Unsigned:
      Int32 sval;
      if (sscanf(buf, "%d", &sval) == 0)
	return -2;
      check = pOp->setValue(a, sval);
      break;

    case NdbDictionary::Column::Int:
      Uint32 uval;
      if (sscanf(buf, "%u", &uval) == 0)
	return -2;
      check = pOp->setValue(a, uval);
      break;

    case NdbDictionary::Column::Char:
      char buf2[8000];
      char* p2;
      memset(buf2, 0, sizeof(buf));
      p2 = &buf2[0];
      while(*p != ';'){
	*p2 = *p;
	p++;p2++;
      };
      *p2 = 0;
      check = pOp->setValue(a, buf2);
      break;

    default:
      check = -2;
      break;
    }

    // Move pointer to after next ";"
    while (*p != ';')
      p++;
    p++;

  }

  return check;
}


int insertLine(Ndb* pNdb, 
	       const NdbDictionary::Table* pTab,
	       char* line){
  int             check;
  int             retryAttempt = 0;
  int             retryMax = 5;
  NdbConnection   *pTrans;
  NdbOperation	  *pOp;

  while (retryAttempt < retryMax){

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      ERR(pNdb->getNdbError());
      NdbSleep_MilliSleep(50);
      retryAttempt++;
      continue;
    }

    pOp = pTrans->getNdbOperation(pTab->getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return -1;
    }

    check = pOp->insertTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return -1;
    }

    check = setValuesFromLine(pOp,
			      pTab,
			      line);
    if (check == -2){
      pNdb->closeTransaction(pTrans);
      return -2;
    }
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return -1;
    }


    // Execute the transaction and insert the record
    check = pTrans->execute( Commit ); 
    if(check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      pNdb->closeTransaction(pTrans);

      switch(err.status){
      case NdbError::Success:
	ERR(err);
	ndbout << "ERROR: NdbError reports success when transcaction failed" << endl;
	return -1;
	break;

      case NdbError::TemporaryError:      
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
	break;

      case NdbError::UnknownResult:
	ERR(err);
	return -1;
	break;

      case NdbError::PermanentError:
	switch (err.classification){
	case NdbError::ConstraintViolation:
	  // Tuple already existed, OK in this application, but should be reported
	  ndbout << err.code << " " << err.message << endl;
	  break;
	default:
	  ERR(err);
	  return -1;
	  break;
	}
	break;
      }
    }
    else{

      pNdb->closeTransaction(pTrans);
    }
    return 0;
  }
  return check;
}

int insertFile(Ndb* pNdb, 
	       const NdbDictionary::Table* pTab,
	       const char* fileName){

  const int MAX_LINE_LEN = 8000;
  char line[MAX_LINE_LEN];
  int lineNo = 0;

  FILE* instr = fopen(fileName, "r");
  if (instr == NULL){
    ndbout << "Coul'd not open " << fileName << endl;
    return -1;
  }

  while(fgets(line, MAX_LINE_LEN, instr)){
    lineNo++;

    if (line[strlen(line)-1] == '\n') {
      line[strlen(line)-1] = '\0';
    }

    int check = insertLine(pNdb, pTab, line);
    if (check == -2){
      ndbout << "Wrong format in input data file, line: " << lineNo << endl;
      fclose(instr);
      return -1;
    }
    if (check == -1){
      fclose(instr);
      return -1;

    }
  }

  fclose(instr);
  return 0;
}


int main(int argc, const char** argv){
  ndb_init();

  const char* _tabname = NULL;
  int _help = 0;
  
  struct getargs args[] = {
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program will bulk copy data from a file to a table in Ndb.\n";
  
  if(getarg(args, num_args, argc, argv, &optind) ||
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];
  ndbout << "Tablename: " << _tabname << endl;

  // Connect to Ndb
  Ndb MyNdb( "TEST_DB" );

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  // Check if table exists in db
  const NdbDictionary::Table* pTab = MyNdb.getDictionary()->getTable(_tabname);
  if(pTab == NULL){
    ndbout << " Table " << _tabname << " does not exist!" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  
  char buf[255];
  BaseString::snprintf(buf, sizeof(buf), "%s.data", (const char*)_tabname);
  if (insertFile(&MyNdb, pTab, buf) != 0){
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  return NDBT_ProgramExit(NDBT_OK);

}



