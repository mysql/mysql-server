/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>

extern "C" {
#include <dba.h>
}

#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTimer.hpp>
#include <NDBT_Stats.hpp>
#include <NDBT_ReturnCodes.h>
#include <NdbMain.h>
#include <time.h>

#undef min
#undef max

static const int NP_Insert      = 0;
static const int NP_Update      = 1;
static const int NP_WriteUpdate = 2;
static const int NP_WriteInsert = 3;
static const int NP_Delete      = 4;
static const int NP_BulkRead    = 5;
static const int NP_MAX         = 5;

static const char * Operations[] = {
  "Insert  ",
  "Update  ",
  "WriteUpd",
  "WriteIns",
  "Delete  ",
  "BulkRead"
};

/**
 * Configuration variables
 */
static int NoOfTransactions         = 10000;
static int ParallellTransactions    = 1000;
static int OperationsPerTransaction = 10;
static int NoOfColumns              = 20;
static int BytesPerInsert           = 300;
static int BytesPerUpdate           = 200;
static int LoopCount                = 10;

/**
 * Global variables
 */
static char TableName[255];
static DBA_ColumnDesc_t    * ColumnDescriptions;
static DBA_ColumnBinding_t * InsertBindings;
static DBA_ColumnBinding_t * UpdateBindings; static int UpdateBindingColumns;
static DBA_ColumnBinding_t * DeleteBindings;

static char * TestData;
static DBA_Binding_t * InsertB;
static DBA_Binding_t * UpdateB;
static DBA_Binding_t * DeleteB;

/**
 * Function prototypes
 */
static void sequence(int loops);

inline void * getPtr(int rowNo) { return TestData+rowNo*BytesPerInsert;}
inline void   setPK(int rowNo, int pk){ * (int *)getPtr(rowNo) = pk; }

static void SetupTestData();
static void CleanupTestData();

static bool CreateTable();
static bool CleanTable();
static bool CreateBindings();

static void usage();

static
void
usage(){
  int ForceSend, Interval;
  DBA_GetParameter(0, &Interval);
  DBA_GetParameter(3, &ForceSend);

  ndbout << "newtonPerf" << endl
	 << "   -n Transactions per loop and operation (" 
	 << NoOfTransactions << ")" << endl
	 << "   -p parallell transactions (" << ParallellTransactions << ")"
	 << endl
	 << "   -o operations per transaction (" << OperationsPerTransaction 
	 << ")" << endl
	 << "   -a no of columns (" << NoOfColumns << ")" << endl
	 << "   -b Table size in bytes (" << BytesPerInsert << ")" << endl
	 << "   -u Bytes per update (" << BytesPerUpdate << ")" << endl
	 << "   -l Loop count (" << LoopCount << ")" << endl
	 << "   -i Interval (" << Interval << "ms)" << endl
	 << "   -f Force send algorithm (" << ForceSend << ")" << endl
	 << "   -h Help" << endl;

}

static
bool
parseArgs(int argc, const char **argv){
  bool a = false, b = false, u = false;

  for(int i = 1; i<argc; i++){
    if(argv[i][0] != '-'){
      ndbout << "Invalid argument: " << argv[i] << endl;
      return false;
    }

    if(argv[i][1] == 'h')
      return false;
    
    if(i == argc-1){
      ndbout << "Expecting argument to " << argv[i] << endl;
      return false;
    }
    
    switch(argv[i][1]){
    case 'n':
      NoOfTransactions = atoi(argv[i+1]);
      break;
    case 'p':
      ParallellTransactions = atoi(argv[i+1]);
      break;
    case 'o':
      OperationsPerTransaction = atoi(argv[i+1]);
      break;
    case 'a':
      NoOfColumns = atoi(argv[i+1]);
      a = true;
      break;
    case 'b':
      BytesPerInsert = atoi(argv[i+1]);
      b = true;
      break;
    case 'u':
      BytesPerUpdate = atoi(argv[i+1]);
      u = true;
      break;
    case 'l':
      LoopCount = atoi(argv[i+1]);
      break;
    case 'f':
      {
	const int val = atoi(argv[i+1]);
	if(DBA_SetParameter(3, val) != DBA_NO_ERROR){
	  ndbout << "Invalid force send algorithm: "  
		 << DBA_GetLatestErrorMsg()
		 << "(" << DBA_GetLatestError() << ")" << endl;
	  return false;
	}
      }
      break;
    case 'i':
      {
	const int val = atoi(argv[i+1]);
	if(DBA_SetParameter(0, val) != DBA_NO_ERROR){
	  ndbout << "Invalid NBP interval: " 
		 << DBA_GetLatestErrorMsg()
		 << "(" << DBA_GetLatestError() << ")" << endl;
	  return false;
	}
      }
      break;
    default:
      ndbout << "Invalid option: " << argv[i] << endl;
      return false;
    }
    i++;
  }
  if(a && !b) BytesPerInsert = 15 * NoOfColumns;
  if(!a && b) NoOfColumns = ((BytesPerInsert + 14) / 15)+1;
  
  if(!u)
    BytesPerUpdate = (2 * BytesPerInsert) / 3;

  bool t = true;
  if(NoOfColumns < 2) t = false;
  if(BytesPerInsert < 8) t = false;
  if(BytesPerUpdate < 8) t = false;
  
  if(!t){
    ndbout << "Invalid arguments combination of -a -b -u not working out" 
	   << endl;
    return false;
  }
  return true;
}

NDB_COMMAND(newton_perf, "newton_perf",
	    "newton_perf", "newton_perf", 65535){ 
  
  if(!parseArgs(argc, argv)){
    usage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  
  ndbout << "-----------" << endl;
  usage();
  ndbout << endl;

  SetupTestData();
  
  DBA_Open();
  
  if(!CreateTable()){
    DBA_Close();
    CleanupTestData();
    return 0;
  }

  if(!CreateBindings()){
    DBA_Close();
    CleanupTestData();
    return 0;
  }

  CleanTable();
  
  sequence(LoopCount);
  
  DBA_Close();
  CleanupTestData();

  DBA_DestroyBinding(InsertB);
  DBA_DestroyBinding(UpdateB);
  DBA_DestroyBinding(DeleteB);
}

static
void
ErrorMsg(const char * s){
  ndbout << s 
	 << ": " << DBA_GetLatestError() << "-" << DBA_GetLatestErrorMsg() 
	 << ", " << DBA_GetLatestNdbError()
	 << endl;
}

static
int
m4(int i){
  const int j = i - (i & 3);
  return j;
}

static
void
SetupTestData(){
  ndbout << "Creating testdata" << endl;

  ColumnDescriptions = new DBA_ColumnDesc_t[NoOfColumns];
  InsertBindings = new DBA_ColumnBinding_t[NoOfColumns];
  
  const int sz = m4((BytesPerInsert - ((NoOfColumns+1)/2)*4)/(NoOfColumns/2));
  int sum = 0;
  UpdateBindingColumns = 0;
  for(int i = 0; i<NoOfColumns; i++){
    char tmp[16];
    if((i % 2) == 0){
      sprintf(tmp, "I%d", i);
      ColumnDescriptions[i].DataType = DBA_INT;
      ColumnDescriptions[i].Size = 4;
      sum += 4;
    } else {
      sprintf(tmp, "S%d", i);
      ColumnDescriptions[i].DataType = DBA_CHAR;
      ColumnDescriptions[i].Size = sz;
      sum += sz;
    }
    ColumnDescriptions[i].IsKey = 0;
    ColumnDescriptions[i].Name  = strdup(tmp);

    InsertBindings[i].Name     = strdup(tmp);
    InsertBindings[i].DataType = ColumnDescriptions[i].DataType;
    InsertBindings[i].Size     = ColumnDescriptions[i].Size;
    InsertBindings[i].Offset   = sum - ColumnDescriptions[i].Size;
    InsertBindings[i].Ptr      = 0;
    
    if(sum <= BytesPerUpdate)
      UpdateBindingColumns++;
  }
  if(UpdateBindingColumns == 1)
    UpdateBindingColumns++;

  ColumnDescriptions[0].IsKey = 1;
  
  assert(sum <= BytesPerInsert);
  sprintf(TableName, "NEWTON_%d_%d", sum, NoOfColumns);
  
  UpdateBindings = new DBA_ColumnBinding_t[UpdateBindingColumns];
  memcpy(UpdateBindings, InsertBindings, 
	 UpdateBindingColumns*sizeof(DBA_ColumnBinding_t));
  
  DeleteBindings = new DBA_ColumnBinding_t[1];
  memcpy(DeleteBindings, InsertBindings, 
	 1*sizeof(DBA_ColumnBinding_t));
  
  TestData = (char *)malloc(NoOfTransactions * 
			    OperationsPerTransaction * BytesPerInsert);
  
  assert(TestData != 0);
  for(int i = 0; i<NoOfTransactions; i++)
    for(int j = 0; j<OperationsPerTransaction; j++){
      const int pk = i * OperationsPerTransaction + j;
      setPK(pk, pk);
    }
}

static
void
CleanupTestData(){
  free(TestData);
  for(int i = 0; i<NoOfColumns; i++){
    free((char*)ColumnDescriptions[i].Name);
    free((char*)InsertBindings[i].Name);
  }
  delete [] ColumnDescriptions;
  delete [] InsertBindings;
  delete [] UpdateBindings;
  delete [] DeleteBindings;
}


static bool CleanReturnValue = true;
static int  CleanCallbacks = 0;
static int  CleanRows = 0;

extern "C"
void
CleanCallback(DBA_ReqId_t reqId, DBA_Error_t error, DBA_ErrorCode_t ec){
  CleanCallbacks++;
  if(error == DBA_NO_ERROR)
    CleanRows++;
}

static
bool
CleanTable(){
  ndbout << "Cleaning table..." << flush;
  CleanReturnValue = true;
  CleanCallbacks = 0;
  CleanRows = 0;
  for(int i = 0; i<NoOfTransactions * OperationsPerTransaction; i++){
    DBA_ArrayDeleteRows(DeleteB, 
			getPtr(i), 1,
			CleanCallback);
    while((i-CleanCallbacks)>ParallellTransactions)
      NdbSleep_MilliSleep(100);
  }
  while(CleanCallbacks != (NoOfTransactions * OperationsPerTransaction))
    NdbSleep_SecSleep(1);

  ndbout << CleanRows << " rows deleted" << endl;
  
  return CleanReturnValue;
}

static
bool
CreateBindings(){
  ndbout << "Creating bindings" << endl;
  InsertB = UpdateB = DeleteB = 0;
  
  InsertB = DBA_CreateBinding(TableName, NoOfColumns, 
			       InsertBindings, BytesPerInsert);
  if(InsertB == 0){
    ErrorMsg("Failed to create insert bindings");
    return false;
  }

  UpdateB = DBA_CreateBinding(TableName, UpdateBindingColumns,
			       UpdateBindings, BytesPerInsert);
  if(UpdateB == 0){
    ErrorMsg("Failed to create update bindings");
    DBA_DestroyBinding(InsertB);
    return false;
  }

  DeleteB = DBA_CreateBinding(TableName, 1, 
			       DeleteBindings, BytesPerInsert);
  if(DeleteB == 0){
    ErrorMsg("Failed to create delete bindings");
    DBA_DestroyBinding(InsertB);
    DBA_DestroyBinding(UpdateB);
    return false;
  }
  return true;
}

static
bool
CreateTable(){
  ndbout << "Creating " << TableName << endl;
  return DBA_CreateTable( TableName, 
			  NoOfColumns, 
			  ColumnDescriptions ) == DBA_NO_ERROR;
}

/**
 * 
 */
static NdbTimer SequenceTimer;

static int CurrentOp    = NP_Insert;
static int SequenceSent = 0;
static int SequenceRecv = 0;
static NDBT_Stats SequenceStats[NP_MAX][4];
static NDBT_Stats SequenceLatency[NP_MAX];

static int           HashMax;
static DBA_ReqId_t * ReqHash;    // ReqId - Latency/Row 
static int         * ReqHashPos; // (row in StartTime)

static int SequenceLatencyPos;
static NDB_TICKS   * StartTime; 

static
inline
int
computeHashMax(int elements){
  HashMax = 1;
  while(HashMax < elements)
    HashMax *= 2;
  
  if(HashMax < 1024)
    HashMax = 1024;
  
  return HashMax;
}

static 
inline
int
hash(DBA_ReqId_t request){
  int r = (request >> 2) & (HashMax-1);
  return r;
}
 
static
inline
void
addRequest(DBA_ReqId_t request, int pos){
  
  int i = hash(request);
  
  while(ReqHash[i] != 0)
    i = ((i + 1) & (HashMax-1));
  
  ReqHash[i] = request;
  ReqHashPos[i] = pos;
}
 
static
inline
int
getRequest(DBA_ReqId_t request){
  
  int i = hash(request);
  
  while(ReqHash[i] != request)
    i = ((i + 1) & (HashMax-1));
  
  ReqHash[i] = 0;
  
  return ReqHashPos[i];
}

extern "C"
void
SequenceCallback(DBA_ReqId_t reqId, DBA_Error_t error, DBA_ErrorCode_t ec){
  int p = getRequest(reqId) - 1;
  
  if(error != DBA_NO_ERROR){
    ndbout << "p = " << p << endl;
    ndbout << "DBA_GetErrorMsg(" << error << ") = " 
	   << DBA_GetErrorMsg(error) << endl;
    ndbout << "DBA_GetNdbErrorMsg(" << ec << ") = " 
	   << DBA_GetNdbErrorMsg(ec) << endl;
    
    assert(error == DBA_NO_ERROR);
  }
  
  SequenceRecv++;
  if(SequenceRecv == NoOfTransactions){
    SequenceTimer.doStop();
  }

  if((p & 127) == 127){
    NDB_TICKS t = NdbTick_CurrentMillisecond() - StartTime[p];
    SequenceLatency[CurrentOp].addObservation(t);
  }
}

typedef DBA_ReqId_t (* DBA_ArrayFunction)( const DBA_Binding_t* pBindings, 
					   const void * pData,
					   int NbRows,
					   DBA_AsyncCallbackFn_t CbFunc );

inline
int
min(int a, int b){
  return a > b ? b : a;
}

static
void
SequenceOp(DBA_ArrayFunction func, const DBA_Binding_t* pBindings, int op){
  SequenceSent = 0;
  SequenceRecv = 0;
  SequenceLatencyPos = 1;
  CurrentOp = op;

  SequenceTimer.doStart();
  for(int i = 0; i<NoOfTransactions; ){
    const int l1 = ParallellTransactions - (SequenceSent - SequenceRecv);
    const int l2 = min(NoOfTransactions - i, l1);
    for(int j = 0; j<l2; j++){
      const DBA_ReqId_t r = func(pBindings, 
				 getPtr(i*OperationsPerTransaction),
				 OperationsPerTransaction,
				 SequenceCallback);
      assert(r != 0);
      SequenceSent++;
      addRequest(r, i + 1);
      i++;
      
      if((SequenceSent & 127) == 127){
	NDB_TICKS t = NdbTick_CurrentMillisecond();
	StartTime[i] = t;
      } 
    }
    if(l2 == 0)
      NdbSleep_MilliSleep(10);
  }

  while(SequenceRecv != SequenceSent)
    NdbSleep_SecSleep(1);
  
  ndbout << "Performed " << NoOfTransactions << " " << Operations[op]
	 << " in ";
  
  double p = NoOfTransactions * 1000;
  double t = SequenceTimer.elapsedTime();
  double o = p * OperationsPerTransaction;
  
  p /= t;
  o /= t;

  int _p = p;
  int _o = o;

  double b = 0;

  switch(op){
  case NP_Insert:
  case NP_WriteInsert:
  case NP_WriteUpdate:
  case NP_BulkRead:
    b = BytesPerInsert;
    break;
  case NP_Update:
    b = BytesPerUpdate;
    break;
  case NP_Delete:
    b = 4;
    break;
  default:
    b = 0;
  }
  b *= NoOfTransactions * OperationsPerTransaction;
  b /= t;
  int _b = b;

  SequenceStats[op][0].addObservation(t);
  SequenceStats[op][1].addObservation(p);
  SequenceStats[op][2].addObservation(o);
  SequenceStats[op][3].addObservation(b);
  
  int t2 = SequenceStats[op][0].getMean();
  int p2 = SequenceStats[op][1].getMean();
  int o2 = SequenceStats[op][2].getMean();
  int b2 = SequenceStats[op][3].getMean();

  ndbout << SequenceTimer.elapsedTime() << "(" << t2 << ")ms";
  ndbout << " -> " << _p << "(" << p2 << ") T/s - " << _o 
	 << "(" << o2 << ") O/s - " << _b << "(" << b2 << ") Kb/s" << endl;

  ndbout << "  Latency (ms) Avg: " << (int)SequenceLatency[op].getMean()
	 << " min: " << (int)SequenceLatency[op].getMin() 
	 << " max: " << (int)SequenceLatency[op].getMax()
	 << " stddev: " << (int)SequenceLatency[op].getStddev() 
	 << " n: " << SequenceLatency[op].getCount() << endl;
}

/**
 * Sequence 
 */
static
void
sequence(int loops){
  computeHashMax(ParallellTransactions);
  ReqHash    = new DBA_ReqId_t[HashMax];
  ReqHashPos = new int[HashMax];
  StartTime  = new NDB_TICKS[NoOfTransactions];
  
  for(int i = 0; i<NP_MAX; i++){
    SequenceLatency[i].reset();
    for(int j = 0; j<4; j++)
      SequenceStats[i][j].reset();
  }
  for(int i = 0; i<loops; i++){
    ndbout << "Loop #" << (i+1) << endl;
    SequenceOp(DBA_ArrayInsertRows, InsertB, NP_Insert);

    // BulkRead
    
    SequenceOp(DBA_ArrayUpdateRows, UpdateB, NP_Update);
    SequenceOp(DBA_ArrayWriteRows,  InsertB, NP_WriteUpdate);
    SequenceOp(DBA_ArrayDeleteRows, DeleteB, NP_Delete);
    SequenceOp(DBA_ArrayWriteRows,  InsertB, NP_WriteInsert);
    SequenceOp(DBA_ArrayDeleteRows, DeleteB, NP_Delete);
    ndbout << "-------------------" << endl << endl;
  }

  delete [] ReqHash;
  delete [] ReqHashPos;
  delete [] StartTime;
}
