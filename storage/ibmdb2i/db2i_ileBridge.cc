/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/



#include "db2i_ileBridge.h"
#include "my_dbug.h"
#include "db2i_global.h"
#include "db2i_charsetSupport.h"
#include "db2i_errors.h"


// static class member data
ILEpointer* db2i_ileBridge::functionSymbols;
db2i_ileBridge* db2i_ileBridge::globalBridge;
#ifndef DBUG_OFF
uint32 db2i_ileBridge::registeredPtrs;
#endif

pthread_key(IleParms*, THR_ILEPARMS);

static void ileParmsDtor(void* parmsToFree)
{
  if (parmsToFree)
  {
    free_aligned(parmsToFree);
    DBUG_PRINT("db2i_ileBridge", ("Freeing space for parms"));
  }
}


/**
  Convert a timestamp in ILE time format into a unix time_t
*/
static inline time_t convertILEtime(const ILE_time_t& input)
{
  tm temp;
  
  temp.tm_sec = input.Second;
  temp.tm_min = input.Minute;
  temp.tm_hour = input.Hour;
  temp.tm_mday = input.Day;
  temp.tm_mon = input.Month-1;
  temp.tm_year = input.Year - 1900;
  temp.tm_isdst = -1;
  
  return mktime(&temp);
}

/**
  Allocate and intialize a new bridge structure
*/ 
db2i_ileBridge* db2i_ileBridge::createNewBridge(CONNECTION_HANDLE connID)
{
  DBUG_PRINT("db2i_ileBridge::createNewBridge",("Building new bridge..."));
  db2i_ileBridge* newBridge = (db2i_ileBridge*)my_malloc(sizeof(db2i_ileBridge), MYF(MY_WME));

  if (unlikely(newBridge == NULL))
    return NULL;

  newBridge->stmtTxActive = false;
  newBridge->connErrText = NULL;
  newBridge->pendingLockedHandles.head = NULL;
  newBridge->cachedConnectionID = connID;
      
  return newBridge;
}


void db2i_ileBridge::destroyBridge(db2i_ileBridge* bridge)
{
  bridge->freeErrorStorage();
  my_free(bridge, MYF(0)); 
}


void db2i_ileBridge::destroyBridgeForThread(const THD* thd)
{
  void* thdData = *thd_ha_data(thd, ibmdb2i_hton);
  if (thdData != NULL)
  {
    destroyBridge((db2i_ileBridge*)thdData);
  }
}


void db2i_ileBridge::registerPtr(const void* ptr, ILEMemHandle* receiver)
{
  static const arg_type_t ileSignature[] = { ARG_MEMPTR, ARG_END };
  
  if (unlikely(ptr == NULL))
  {
    *receiver = 0;
    return;
  }
  
  struct ArgList
  {
    ILEarglist_base base;
    ILEpointer ptr;
  } *arguments;

  char argBuf[sizeof(ArgList)+15];
  arguments = (ArgList*)roundToQuadWordBdy(argBuf);

  arguments->ptr.s.addr = (address64_t)(ptr);

  _ILECALL(&functionSymbols[funcRegisterSpace], 
           &arguments->base,
           ileSignature, 
           RESULT_INT64);
  
#ifndef DBUG_OFF
  uint32 truncHandle = arguments->base.result.r_uint64;
  DBUG_PRINT("db2i_ileBridge::registerPtr",("Register 0x%p with handle %d", ptr, truncHandle));
  getBridgeForThread()->registeredPtrs++;
#endif

  *receiver = arguments->base.result.r_uint64;
  return;
}

void db2i_ileBridge::unregisterPtr(ILEMemHandle handle)
{
  static const arg_type_t ileSignature[] = { ARG_UINT64, ARG_END };

  if (unlikely(handle == NULL))
    return;
  
  struct ArgList
  {
    ILEarglist_base base;
    uint64 handle;
  } *arguments;

  char argBuf[sizeof(ArgList)+15];
  arguments = (ArgList*)roundToQuadWordBdy(argBuf);

  arguments->handle = (uint64)(handle);

  _ILECALL(&functionSymbols[funcUnregisterSpace], 
           &arguments->base,
           ileSignature, 
           RESULT_VOID);

#ifndef DBUG_OFF
  DBUG_PRINT("db2i_ileBridge::unregisterPtr",("Unregister handle %d", (uint32)handle));
  getBridgeForThread()->registeredPtrs--;
#endif  
}



/**
   Initialize the bridge component
   
   @details  Resolves srvpgm and function names of the APIs. If this fails, 
   the approrpiate operating system support (PTFs) is probably not installed.
   
   WARNING:
   Must be called before any other functions in this class are used!!!!
   Can only be called by a single thread! 
*/
int db2i_ileBridge::setup()
{
  static const char funcNames[db2i_ileBridge::funcListEnd][32] = 
  {
    {"QmyRegisterParameterSpaces"},
    {"QmyRegisterSpace"},
    {"QmyUnregisterSpace"},
    {"QmyProcessRequest"}
  };

  DBUG_ENTER("db2i_ileBridge::setup");

  int actmark = _ILELOAD("QSYS/QMYSE", ILELOAD_LIBOBJ);
  if ( actmark == -1 )
  {
    DBUG_PRINT("db2i_ileBridge::setup", ("srvpgm activation failed"));
    DBUG_RETURN(1);
  }

  functionSymbols = (ILEpointer*)malloc_aligned(sizeof(ILEpointer) * db2i_ileBridge::funcListEnd);
  
  for (int i = 0; i < db2i_ileBridge::funcListEnd; i++)
  {
    if (_ILESYM(&functionSymbols[i], actmark, funcNames[i]) == -1)
    {
      DBUG_PRINT("db2i_ileBridge::setup", 
                 ("resolve of %s failed", funcNames[i]));
      DBUG_RETURN(errno);
    }
  }
  
  pthread_key_create(&THR_ILEPARMS, &ileParmsDtor);
    
#ifndef DBUG_OFF
  registeredPtrs = 0;
#endif

  globalBridge = createNewBridge(0);
  
  DBUG_RETURN(0);
}

/**
  Cleanup any resources before shutting down plug-in
*/
void db2i_ileBridge::takedown()
{
  if (globalBridge)
    destroyBridge(globalBridge);
  free_aligned(functionSymbols);
}

/** 
  Call off to QmyProcessRequest to perform the API that the caller prepared
*/
inline int32 db2i_ileBridge::doIt()
{ 
  static const arg_type_t ileSignature[] = {ARG_END};
  
  struct ArgList
  {
    ILEarglist_base base;
  } *arguments;
  
  char argBuf[sizeof(ArgList)+15];
  arguments = (ArgList*)roundToQuadWordBdy(argBuf);
  
  _ILECALL(&functionSymbols[funcProcessRequest], 
           &arguments->base,
           ileSignature, 
           RESULT_INT32);
   
  return translateErrorCode(arguments->base.result.s_int32.r_int32);
}

/** 
  Call off to QmyProcessRequest to perform the API that the caller prepared and
  log any errors that may occur.
*/
inline int32 db2i_ileBridge::doItWithLog()
{
  int32 rc = doIt();

  if (unlikely(rc))
  {
    // Only report errors that we weren't expecting
    if (rc != tacitErrors[0] &&
        rc != tacitErrors[1] &&
        rc != QMY_ERR_END_OF_BLOCK)
      reportSystemAPIError(rc, (Qmy_Error_output_t*)parms()->outParms);
  }
  memset(tacitErrors, 0, sizeof(tacitErrors));

  return rc;
}


/**
  Interface to QMY_ALLOCATE_SHARE API
  
  See QMY_ALLOCATE_SHARE documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::allocateFileDefn(ILEMemHandle definitionSpace,
                                     ILEMemHandle handleSpace,
                                     uint16 fileCount,
                                     const char* schemaName,
                                     uint16 schemaNameLength,
                                     ILEMemHandle formatSpace,
                                     uint32 formatSpaceLen)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  
  IleParms* parmBlock = parms();
  Qmy_MAOS0100 *input = (Qmy_MAOS0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));
  
  input->Format = QMY_ALLOCATE_SHARE;
  input->ShrDefSpcHnd = definitionSpace;
  input->ShrHndSpcHnd = handleSpace;
  input->ShrDefCnt = fileCount;
  input->FmtSpcHnd = formatSpace;
  input->FmtSpcLen = formatSpaceLen;

  if (schemaNameLength > sizeof(input->SchNam))
  {
    // This should never happen!
    DBUG_ASSERT(0);
    return HA_ERR_GENERIC;
  }
  
  memcpy(input->SchNam, schemaName, schemaNameLength);  
  input->SchNamLen = schemaNameLength;
  
  input->CnnHnd = cachedConnectionID;

  int32 rc = doItWithLog();
  
  return rc;
}


/**
  Interface to QMY_ALLOCATE_INSTANCE API
  
  See QMY_ALLOCATE_INSTANCE documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::allocateFileInstance(FILE_HANDLE defnHandle,
                                           ILEMemHandle inuseSpace,
                                           FILE_HANDLE* instance)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  
  IleParms* parmBlock = parms();
  Qmy_MAOI0100 *input = (Qmy_MAOI0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_ALLOCATE_INSTANCE;
  input->ShrHnd = defnHandle;
  input->CnnHnd = cachedConnectionID;
  input->UseSpcHnd = inuseSpace;
  
  int32 rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MAOI0100_output* output = (Qmy_MAOI0100_output*)parmBlock->outParms;  
    DBUG_ASSERT(instance);
    *instance = output->ObjHnd;
  }

  return rc;
}


/**
  Interface to QMY_DEALLOCATE_OBJECT API
  
  See QMY_DEALLOCATE_OBJECT documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::deallocateFile(FILE_HANDLE rfileHandle,
                                   bool postDropTable)
{
  IleParms* parmBlock = parms();
  Qmy_MDLC0100 *input = (Qmy_MDLC0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_DEALLOCATE_OBJECT; 
  input->ObjHnd = rfileHandle;
  input->ObjDrp[0] = (postDropTable ? QMY_YES : QMY_NO);
  
  DBUG_PRINT("db2i_ileBridge::deallocateFile", ("Deallocating %d", (uint32)rfileHandle));
  
  int32 rc = doItWithLog();

  return rc;
}


/**
  Interface to QMY_OBJECT_INITIALIZATION API
  
  See QMY_OBJECT_INITIALIZATION documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::initFileForIO(FILE_HANDLE rfileHandle,
                                  char accessIntent,
                                  char commitLevel,
                                  uint16* inRecSize,
                                  uint16* inRecNullOffset,
                                  uint16* outRecSize,
                                  uint16* outRecNullOffset)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MOIX0100 *input = (Qmy_MOIX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_OBJECT_INITIALIZATION;
  input->CmtLvl[0] = commitLevel;
  input->Intent[0] = accessIntent;
  input->ObjHnd = rfileHandle;
  input->CnnHnd = cachedConnectionID;

  int32 rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MOIX0100_output* output = (Qmy_MOIX0100_output*)parmBlock->outParms; 
    *inRecSize = output->InNxtRowOff;
    *inRecNullOffset = output->InNullMapOff;
    *outRecSize = output->OutNxtRowOff;
    *outRecNullOffset = output->OutNullMapOff;
  }
  
  return rc;
}

    
/**
  Interface to QMY_READ_ROWS API for reading a row with a specific RRN.
  
  See QMY_READ_ROWS documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::readByRRN(FILE_HANDLE rfileHandle, 
                              ILEMemHandle buf,
                              uint32 inRRN,
                              char accessIntent,
                              char commitLevel)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MRDX0100 *input = (Qmy_MRDX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_READ_ROWS;
  input->CmtLvl[0] = commitLevel;
  input->ObjHnd = rfileHandle;
  input->Intent[0] = accessIntent;
  input->OutSpcHnd = (uint64)buf;
  input->RelRowNbr = inRRN;
  input->CnnHnd = cachedConnectionID;
    
  int32 rc = doItWithLog();

  if (rc == QMY_ERR_END_OF_BLOCK)
  {
    rc = 0;
    DBUG_PRINT("db2i_ileBridge::readByRRN", ("End of block signalled"));
  }
  
  return rc;
}


/**
  Interface to QMY_WRITE_ROWS API.
  
  See QMY_WRITE_ROWS documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::writeRows(FILE_HANDLE rfileHandle, 
                              ILEMemHandle buf, 
                              char commitLevel,
                              int64* outIdVal,
                              bool* outIdGen,
                              uint32* dupKeyRRN,
                              char** dupKeyName,
                              uint32* dupKeyNameLen,
                              uint32* outIdIncrement)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MWRT0100 *input = (Qmy_MWRT0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_WRITE_ROWS;
  input->CmtLvl[0] = commitLevel;
  
  input->ObjHnd = rfileHandle;
  input->InSpcHnd = (uint64_t) buf;
  input->CnnHnd = cachedConnectionID;
  
  int32 rc = doItWithLog();

  Qmy_MWRT0100_output_t* output = (Qmy_MWRT0100_output_t*)parmBlock->outParms;  
  if (likely(rc == 0 || rc == HA_ERR_FOUND_DUPP_KEY))
  {
    DBUG_ASSERT(dupKeyRRN && dupKeyName && dupKeyNameLen && outIdGen && outIdIncrement && outIdVal);
    *dupKeyRRN = output->DupRRN;
    *dupKeyName = (char*)parmBlock->outParms + output->DupObjNamOff;
    *dupKeyNameLen = output->DupObjNamLen;
    *outIdGen = (output->NewIdGen[0] == QMY_YES ? TRUE : FALSE);
    if (*outIdGen == TRUE)
    {
      *outIdIncrement = output->IdIncrement;
      *outIdVal = output->NewIdVal;
    }
  }
  
  return rc;
  
}

/**
  Interface to QMY_EXECUTE_IMMEDIATE API.
  
  See QMY_EXECUTE_IMMEDIATE documentation for more information about 
  parameters and return codes.
*/
uint32 db2i_ileBridge::execSQL(const char* statement,
                             uint32 statementCount,
                             uint8  commitLevel,
                             bool autoCreateSchema,
                             bool dropSchema,
                             bool noCommit,
                             FILE_HANDLE fileHandle)

{
  IleParms* parmBlock = parms();
  Qmy_MSEI0100 *input = (Qmy_MSEI0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_EXECUTE_IMMEDIATE;
  
  registerPtr(statement, &input->StmtsSpcHnd); 

  input->NbrStmts = statementCount;
  *(uint16*)(&input->StmtCCSID) = 850;
  input->AutoCrtSchema[0] = (autoCreateSchema == TRUE ? QMY_YES : QMY_NO);
  input->DropSchema[0] = (dropSchema == TRUE ? QMY_YES : QMY_NO); 
  input->CmtLvl[0] = commitLevel;
  if ((commitLevel == QMY_NONE && statementCount == 1) || noCommit)
  {
    input->CmtBefore[0] = QMY_NO;
    input->CmtAfter[0] = QMY_NO;
  }
  else
  {
    input->CmtBefore[0] = QMY_YES;
    input->CmtAfter[0] = QMY_YES;
  }
  input->CnnHnd = current_thd->thread_id;
  input->ObjHnd = fileHandle;

  int32 rc = doItWithLog();
  
  unregisterPtr(input->StmtsSpcHnd);
  
  return rc;
}

/**
  Interface to QMY_PREPARE_OPEN_CURSOR API.
  
  See QMY_PREPARE_OPEN_CURSOR documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::prepOpen(const char* statement,
                             FILE_HANDLE* rfileHandle,
                             uint32* recLength) 
{
  IleParms* parmBlock = parms();
  Qmy_MSPO0100 *input = (Qmy_MSPO0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_PREPARE_OPEN_CURSOR;
  
  registerPtr(statement, &input->StmtsSpcHnd ); 
  *(uint16*)(&input->StmtCCSID) = 850;
  input->CnnHnd = current_thd->thread_id;

  int32 rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MSPO0100_output* output = (Qmy_MSPO0100_output*)parmBlock->outParms;
    *rfileHandle = output->ObjHnd;
    *recLength = max(output->InNxtRowOff, output->OutNxtRowOff);    
  }

  
  unregisterPtr(input->StmtsSpcHnd);
  
  return rc;
}


/**
  Interface to QMY_DELETE_ROW API.
  
  See QMY_DELETE_ROW documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::deleteRow(FILE_HANDLE rfileHandle,
                              uint32 rrn)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MDLT0100 *input = (Qmy_MDLT0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
    
  input->Format = QMY_DELETE_ROW;  
  input->ObjHnd = rfileHandle;
  input->RelRowNbr = rrn;
  input->CnnHnd = cachedConnectionID;
            
  int32 rc = doItWithLog();

  return rc;
}


/**
  Interface to QMY_UPDATE_ROW API.
  
  See QMY_UPDATE_ROW documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::updateRow(FILE_HANDLE rfileHandle, 
                              uint32 rrn,
                              ILEMemHandle buf,
                              uint32* dupKeyRRN,
                              char** dupKeyName,
                              uint32* dupKeyNameLen)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MUPD0100 *input = (Qmy_MUPD0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
    
  input->Format = QMY_UPDATE_ROW;
  input->ObjHnd = rfileHandle;
  input->InSpcHnd = (uint64)buf;
  input->RelRowNbr = rrn;
  input->CnnHnd = cachedConnectionID;
            
  int32 rc = doItWithLog();
  
  if (rc == HA_ERR_FOUND_DUPP_KEY) 
  {
    Qmy_MUPD0100_output* output = (Qmy_MUPD0100_output*)parmBlock->outParms;
    DBUG_ASSERT(dupKeyRRN && dupKeyName && dupKeyNameLen);
    *dupKeyRRN = output->DupRRN;
    *dupKeyName = (char*)parmBlock->outParms + output->DupObjNamOff;
    *dupKeyNameLen = output->DupObjNamLen;
  }

  return rc;
}

/**
  Interface to QMY_DESCRIBE_RANGE API.
  
  See QMY_DESCRIBE_RANGE documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::recordsInRange(FILE_HANDLE defnHandle,
                                   ILEMemHandle inSpc,
                                   uint32 inKeyCnt,
                                   uint32 inLiteralCnt,
                                   uint32 inBoundsOff,
                                   uint32 inLitDefOff,
                                   uint32 inLiteralsOff,
                                   uint32 inCutoff,
                                   uint32 inSpcLen,
                                   uint16 inEndByte,
                                   uint64* outRecCnt,
                                   uint16* outRtnCode)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  
  IleParms* parmBlock = parms();
  Qmy_MDRG0100 *input = (Qmy_MDRG0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_DESCRIBE_RANGE;
  input->ShrHnd = defnHandle;
  input->SpcHnd = (uint64)inSpc;
  input->KeyCnt = inKeyCnt;
  input->LiteralCnt = inLiteralCnt;
  input->BoundsOff = inBoundsOff;
  input->LitDefOff = inLitDefOff;
  input->LiteralsOff = inLiteralsOff;
  input->Cutoff = inCutoff;
  input->SpcLen = inSpcLen;
  input->EndByte = inEndByte;
  input->CnnHnd = cachedConnectionID;

  int rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MDRG0100_output* output = (Qmy_MDRG0100_output*)parmBlock->outParms;
    DBUG_ASSERT(outRecCnt && outRtnCode);
    *outRecCnt = output->RecCnt;
    *outRtnCode = output->RtnCode;
  }

  return rc;
}


/**
  Interface to QMY_RELEASE_ROW API.
  
  See QMY_RELEASE_ROW documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::rrlslck(FILE_HANDLE rfileHandle, char accessIntent)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  
  IleParms* parmBlock = parms();
  Qmy_MRRX0100 *input = (Qmy_MRRX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_RELEASE_ROW;
  
  input->ObjHnd = rfileHandle;
  input->CnnHnd = cachedConnectionID;
  input->Intent[0] = accessIntent;
            
  int32 rc = doItWithLog();

  return rc;
}

/**
  Interface to QMY_LOCK_OBJECT API.
  
  See QMY_LOCK_OBJECT documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::lockObj(FILE_HANDLE defnHandle, 
                            uint64 lockVal, 
                            char lockAction, 
                            char lockType,
                            char lockTimeout)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MOLX0100 *input = (Qmy_MOLX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
    
  input->Format = QMY_LOCK_OBJECT;
  input->ShrHnd = defnHandle;
  input->LckTimeoutVal = lockVal;
  input->Action[0] = lockAction;
  input->LckTyp[0] = lockType;
  input->LckTimeout[0] = lockTimeout;
  input->CnnHnd = cachedConnectionID;
        
  int32 rc = doItWithLog();

  return rc;
}

/**
  Interface to QMY_DESCRIBE_CONSTRAINTS API.
  
  See QMY_DESCRIBE_CONSTRAINTS documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::constraints(FILE_HANDLE defnHandle,
                                ILEMemHandle inSpc,
                                uint32 inSpcLen,
                                uint32* outLen,  
                                uint32* outCnt)     
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MDCT0100 *input = (Qmy_MDCT0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_DESCRIBE_CONSTRAINTS;
  input->ShrHnd = defnHandle;
  input->CstSpcHnd = (uint64)inSpc;
  input->CstSpcLen = inSpcLen;
  input->CnnHnd = cachedConnectionID;

  int32 rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MDCT0100_output* output = (Qmy_MDCT0100_output*)parmBlock->outParms;
    DBUG_ASSERT(outLen && outCnt);
    *outLen = output->NeededLen;
    *outCnt = output->CstCnt;
  }

  return rc;
}


/**
  Interface to QMY_REORGANIZE_TABLE API.
  
  See QMY_REORGANIZE_TABLE documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::optimizeTable(FILE_HANDLE defnHandle)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MRGX0100 *input = (Qmy_MRGX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_REORGANIZE_TABLE;
  input->ShrHnd = defnHandle;
  input->CnnHnd = cachedConnectionID;
  
  int32 rc = doItWithLog();

  return rc;
}


/**
  Interface to QMY_PROCESS_COMMITMENT_CONTROL API.
  
  See QMY_PROCESS_COMMITMENT_CONTROL documentation for more information about 
  parameters and return codes.
*/
int32 db2i_ileBridge::commitmentControl(uint8 function)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MCCX0100 *input = (Qmy_MCCX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
    
  input->Format = QMY_PROCESS_COMMITMENT_CONTROL;              
  input->Function[0] = function;
  input->CnnHnd = cachedConnectionID;
 
  int32 rc = doItWithLog();

  return rc;
}


/**
  Interface to QMY_PROCESS_SAVEPOINT API.
  
  See QMY_PROCESS_SAVEPOINT documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::savepoint(uint8 function,
                                const char* savepointName)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  DBUG_PRINT("db2i_ileBridge::savepoint",("%d %s", (uint32)function, savepointName));
  
  IleParms* parmBlock = parms();
  Qmy_MSPX0100 *input = (Qmy_MSPX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  char* savPtNam = (char*)(input+1);
  
  input->Format  = QMY_PROCESS_SAVEPOINT;

  if (strlen(savepointName) > MAX_DB2_SAVEPOINTNAME_LENGTH)
  {
    DBUG_ASSERT(0);
    return HA_ERR_GENERIC;
  }
  strcpy(savPtNam, savepointName);
            
  input->Function[0] = function;
  input->SavPtNamOff = savPtNam - (char*)(input);
  input->SavPtNamLen = strlen(savepointName);
  input->CnnHnd = cachedConnectionID;
 
  int32 rc = doItWithLog();
  
  return rc;
}

static ILEMemHandle traceSpcHandle;
/**
  Do initialization for  the QMY_* APIs.
  
  @parm aspName  The name of the relational database to use for all 
                 connections.
  
  @return  0 if successful; error otherwise
*/
int32 db2i_ileBridge::initILE(const char* aspName,
                              uint16* traceCtlPtr)
{
  // We forego the typical thread-based parms space because MySQL doesn't
  // allow us to clean it up before checking for memory leaks. As a result
  // we get a complaint about leaked memory on server shutdown.
  int32 rc;
  char inParms[db2i_ileBridge_MAX_INPARM_SIZE];
  char outParms[db2i_ileBridge_MAX_OUTPARM_SIZE];
  if (rc = registerParmSpace(inParms, outParms))
  {
    reportSystemAPIError(rc, NULL);
    return rc;
  }
  
  registerPtr(traceCtlPtr, &traceSpcHandle);
  
  struct ParmBlock
  {
    Qmy_MINI0100 parms;
  } *parmBlock = (ParmBlock*)inParms;
  
  memset(inParms, 0, sizeof(ParmBlock));
    
  parmBlock->parms.Format = QMY_INITIALIZATION;
      
  char paddedName[18];
  if (strlen(aspName) > sizeof(paddedName))
  {
    getErrTxt(DB2I_ERR_BAD_RDB_NAME);
    return DB2I_ERR_BAD_RDB_NAME;
  }
    
  memset(paddedName, ' ', sizeof(paddedName));
  memcpy(paddedName, aspName, strlen(aspName));      
  convToEbcdic(paddedName, parmBlock->parms.RDBName, strlen(paddedName));
  
  parmBlock->parms.RDBNamLen = strlen(paddedName);
  parmBlock->parms.TrcSpcHnd = traceSpcHandle;
            
  rc = doIt();

  if (rc)
  {
    reportSystemAPIError(rc, (Qmy_Error_output_t*)outParms);
  }
  
  return rc;  
}

/**
  Signal to the QMY_ APIs to perform any cleanup they need to do.
*/
int32 db2i_ileBridge::exitILE()
{
  IleParms* parmBlock = parms();
  Qmy_MCLN0100 *input = (Qmy_MCLN0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
    
  input->Format = QMY_CLEANUP;
  
  int32 rc = doIt();

  if (rc)
  {
    reportSystemAPIError(rc, (Qmy_Error_output_t*)parmBlock->outParms);
  }
  
  unregisterPtr(traceSpcHandle);

  DBUG_PRINT("db2i_ileBridge::exitILE", ("Registered ptrs remaining: %d", registeredPtrs));
#ifndef DBUG_OFF
  if (registeredPtrs != 0)
    printf("Oh no! IBMDB2I left some pointers registered. Count was %d.\n", registeredPtrs);
#endif  
  
  // This is needed to prevent SAFE_MALLOC from complaining at process termination.
  my_pthread_setspecific_ptr(THR_ILEPARMS, NULL);
  free_aligned(parmBlock);
    
  return rc;
  
}


/**
  Designate the specified addresses as parameter passing buffers.
  
  @parm in  Input to the API will go here; format is defined by the individual API
  @parm out  Output from the API will be; format is defined by the individual API
  
  @return  0 if success; error otherwise
*/
int db2i_ileBridge::registerParmSpace(char* in, char* out)
{
  static const arg_type_t ileSignature[] = { ARG_MEMPTR, ARG_MEMPTR, ARG_END };

  struct ArgList
  {
    ILEarglist_base base;
    ILEpointer input;
    ILEpointer output;
  } *arguments;

  char argBuf[sizeof(ArgList)+15];
  arguments = (ArgList*)roundToQuadWordBdy(argBuf);

  arguments->input.s.addr = (address64_t)(in);
  arguments->output.s.addr = (address64_t)(out);

  _ILECALL(&functionSymbols[funcRegisterParameterSpaces], 
           &arguments->base,
           ileSignature, 
           RESULT_INT32);

  return arguments->base.result.s_int32.r_int32;
}


/**
  Interface to QMY_OBJECT_OVERRIDE API.
  
  See QMY_OBJECT_OVERRIDE documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::objectOverride(FILE_HANDLE rfileHandle,
                                  ILEMemHandle buf,
                                  uint32 recordWidth)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MOOX0100 *input = (Qmy_MOOX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_OBJECT_OVERRIDE;  
  input->ObjHnd = rfileHandle;
  input->OutSpcHnd = (uint64)buf;
  input->NxtRowOff = recordWidth;
  input->CnnHnd = cachedConnectionID;

  int32 rc = doItWithLog();

  return rc;
}

/**
  Interface to QMY_DESCRIBE_OBJECT API for obtaining table stats.
  
  See QMY_DESCRIBE_OBJECT documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::retrieveTableInfo(FILE_HANDLE defnHandle,
                                      uint16 dataRequested,
                                      ha_statistics& stats,
                                      ILEMemHandle inSpc)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MDSO0100 *input = (Qmy_MDSO0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_DESCRIBE_OBJECT;  
  input->ShrHnd = defnHandle;
  input->CnnHnd = cachedConnectionID;
  
  if (dataRequested & objLength)
    input->RtnObjLen[0] = QMY_YES;
  if (dataRequested & rowCount)
    input->RtnRowCnt[0] = QMY_YES;
  if (dataRequested & deletedRowCount)
    input->RtnDltRowCnt[0] = QMY_YES;
  if (dataRequested & rowsPerKey)
  {
    input->RowKeyHnd = (uint64)inSpc;
    input->RtnRowKey[0] = QMY_YES;
  }
  if (dataRequested & meanRowLen)
    input->RtnMeanRowLen[0] = QMY_YES;
  if (dataRequested & lastModTime)
    input->RtnModTim[0] = QMY_YES;
  if (dataRequested & createTime)
    input->RtnCrtTim[0] = QMY_YES;
  if (dataRequested & ioCount)
    input->RtnEstIoCnt[0] = QMY_YES;
  
  int32 rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MDSO0100_output* output = (Qmy_MDSO0100_output*)parmBlock->outParms; 
    if (dataRequested & objLength)
      stats.data_file_length = output->ObjLen;
    if (dataRequested & rowCount)
      stats.records= output->RowCnt;
    if (dataRequested & deletedRowCount)
      stats.deleted = output->DltRowCnt;    
    if (dataRequested & meanRowLen)
      stats.mean_rec_length = output->MeanRowLen;
    if (dataRequested & lastModTime)
      stats.update_time = convertILEtime(output->ModTim);
    if (dataRequested & createTime)
      stats.create_time = convertILEtime(output->CrtTim);
    if (dataRequested & ioCount)
      stats.data_file_length = output->EstIoCnt;     
  }
  
  return rc;
}

/**
  Interface to QMY_DESCRIBE_OBJECT API for finding index size.
  
  See QMY_DESCRIBE_OBJECT documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::retrieveIndexInfo(FILE_HANDLE defnHandle,
                                      uint64* outPageCnt)    
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MDSO0100 *input = (Qmy_MDSO0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_DESCRIBE_OBJECT;
  input->ShrHnd = defnHandle;
  input->CnnHnd = cachedConnectionID;
  input->RtnPageCnt[0] = QMY_YES;
  
  int32 rc = doItWithLog();

  if (likely(rc == 0))
  {
    Qmy_MDSO0100_output* output = (Qmy_MDSO0100_output*)parmBlock->outParms;  
    *outPageCnt = output->PageCnt;
  }
  
  return rc;
}


/**
  Interface to QMY_CLOSE_CONNECTION API
  
  See QMY_CLOSE_CONNECTION documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::closeConnection(CONNECTION_HANDLE conn)
{
  IleParms* parmBlock = parms();
  Qmy_MCCN0100 *input = (Qmy_MCCN0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_CLOSE_CONNECTION;
  input->CnnHnd = conn;
  
  int32 rc = doItWithLog();

  return rc;
}


/**
  Interface to QMY_INTERRUPT API
  
  See QMY_INTERRUPT documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::readInterrupt(FILE_HANDLE fileHandle)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MINT0100 *input = (Qmy_MINT0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_INTERRUPT;
  input->CnnHnd = cachedConnectionID;
  input->ObjHnd = fileHandle;
  
  int32 rc = doItWithLog();

  if (rc == QMY_ERR_END_OF_BLOCK)
  {
    rc = 0;
    DBUG_PRINT("db2i_ileBridge::readInterrupt", ("End of block signalled"));
  }

  return rc;   
}

/**
  Interface to QMY_READ_ROWS API
  
  See QMY_READ_ROWS documentation for more information about parameters and 
  return codes.
*/
int32 db2i_ileBridge::read(FILE_HANDLE rfileHandle, 
                         ILEMemHandle buf, 
                         char accessIntent,
                         char commitLevel,
                         char orientation, 
                         bool asyncRead,
                         ILEMemHandle rrn,
                         ILEMemHandle key,
                         uint32 keylen,
                         uint16 keyParts,
                         int pipeFD)
{
  DBUG_ASSERT(cachedStateIsCoherent());
  IleParms* parmBlock = parms();
  Qmy_MRDX0100 *input = (Qmy_MRDX0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  

  input->Format = QMY_READ_ROWS;
  input->CmtLvl[0] = commitLevel;
  
  input->ObjHnd = rfileHandle;
  input->Intent[0] = accessIntent;
  input->OutSpcHnd = (uint64)buf;
  input->OutRRNSpcHnd = (uint64)rrn;
  input->RtnData[0] = QMY_RETURN_DATA;
  
  if (key)
  {
    input->KeySpcHnd = (uint64)key;
    input->KeyColsLen = keylen;
    input->KeyColsNbr = keyParts;
  }

  input->Async[0] = (asyncRead ? QMY_YES : QMY_NO);
  input->PipeDesc = pipeFD;
  input->Orientation[0] = orientation;
  input->CnnHnd = cachedConnectionID;
          
  int32 rc = doItWithLog();

  // QMY_ERR_END_OF_BLOCK is informational only, so we ignore it.
  if (rc == QMY_ERR_END_OF_BLOCK)
  {
    rc = 0;
    DBUG_PRINT("db2i_ileBridge::read", ("End of block signalled"));
  }
  
  return rc;
}


/**
  Interface to QMY_QUIESCE_OBJECT API
  
  See QMY_QUIESCE_OBJECT documentation for more information about parameters and
  return codes.
*/
int32 db2i_ileBridge::quiesceFileInstance(FILE_HANDLE rfileHandle)
{
  IleParms* parmBlock = parms();
  Qmy_MQSC0100 *input = (Qmy_MQSC0100*)&(parmBlock->inParms);
  memset(input, 0, sizeof(*input));  
  
  input->Format = QMY_QUIESCE_OBJECT; 
  input->ObjHnd = rfileHandle;
    
  int32 rc = doItWithLog();

#ifndef DBUG_OFF  
  if (unlikely(rc))
  {
    DBUG_ASSERT(0);
  }
#endif
  
  return rc;
}
  
void db2i_ileBridge::PreservedHandleList::add(const char* newname, FILE_HANDLE newhandle, IBMDB2I_SHARE* share) 
{
  NameHandlePair *newPair = (NameHandlePair*)my_malloc(sizeof(NameHandlePair), MYF(MY_WME)); 

  newPair->next = head;
  head = newPair;

  strcpy(newPair->name, newname);
  newPair->handle = newhandle;
  newPair->share = share;
  DBUG_PRINT("db2i_ileBridge", ("Added handle %d for %s", uint32(newhandle), newname));
}
    

FILE_HANDLE db2i_ileBridge::PreservedHandleList::findAndRemove(const char* fileName, IBMDB2I_SHARE** share)
{
  NameHandlePair* current = head;
  NameHandlePair* prev = NULL;

  while (current)
  {
    NameHandlePair* next = current->next;
    if (strcmp(fileName, current->name) == 0)
    { 
      FILE_HANDLE tmp = current->handle;
      *share = current->share;
      if (prev)
        prev->next = next;
      if (current == head)
        head = next;
      my_free(current, MYF(0));
      DBUG_PRINT("db2i_ileBridge", ("Found handle %d for %s", uint32(tmp), fileName));
      return tmp;
    }
    prev = current;
    current = next;
  }

  return 0;
}


IleParms* db2i_ileBridge::initParmsForThread()
{
  
  IleParms*  p = (IleParms*)malloc_aligned(sizeof(IleParms));
  DBUG_ASSERT((uint64)(&(p->outParms))% 16 == 0); // Guarantee that outParms are aligned correctly

  if (likely(p))
  {
    int32 rc = registerParmSpace((p->inParms), (p->outParms));
    if (likely(rc == 0))
    {
      my_pthread_setspecific_ptr(THR_ILEPARMS, p);
      DBUG_PRINT("db2i_ileBridge", ("Inited space for parms"));
      return p;
    }
    else
      reportSystemAPIError(rc, NULL);
  }

  return NULL;
}

