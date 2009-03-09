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


#ifndef DB2I_ILEBRIDGE_H
#define DB2I_ILEBRIDGE_H

#include "db2i_global.h"
#include "mysql_priv.h"
#include "as400_types.h"
#include "as400_protos.h"
#include "qmyse.h"
#include "db2i_errors.h"

typedef uint64_t FILE_HANDLE;
typedef my_thread_id CONNECTION_HANDLE;
const char SAVEPOINT_NAME[] = {0xD4,0xE2,0xD7,0xC9,0xD5,0xE3,0xC5,0xD9,0xD5,0x0};
const uint32 TACIT_ERRORS_SIZE=2;

enum db2i_InfoRequestSpec
{
  objLength = 1,
  rowCount = 2,
  deletedRowCount = 4,
  rowsPerKey = 8,
  meanRowLen = 16,
  lastModTime = 32,
  createTime = 64,
  ioCount = 128
}; 
  
extern  handlerton *ibmdb2i_hton;
struct IBMDB2I_SHARE;

const uint32 db2i_ileBridge_MAX_INPARM_SIZE = 512;
const uint32 db2i_ileBridge_MAX_OUTPARM_SIZE = 512;

extern pthread_key(IleParms*, THR_ILEPARMS);
struct IleParms
{
  char inParms[db2i_ileBridge_MAX_INPARM_SIZE];
  char outParms[db2i_ileBridge_MAX_OUTPARM_SIZE];
};

/**
  @class db2i_ileBridge

  Implements a connection-based interface to the QMY_* APIs
  
  @details  Each client connection that touches an IBMDB2I table has a "bridge"
  associated with it. This bridge is constructed on first use and provides a
  more C-like interface to the APIs. As well, it is reponsible for tracking 
  connection scoped information such as statement transaction state and error
  message text. The bridge is destroyed when the connection ends.
*/
class db2i_ileBridge
{
  enum ileFuncs
  {
    funcRegisterParameterSpaces,
    funcRegisterSpace,
    funcUnregisterSpace,
    funcProcessRequest,
    funcListEnd
  };

  static db2i_ileBridge* globalBridge;    
public:
    
  
  static int setup();
  static void takedown();

  /**
    Obtain a pointer to the bridge for the current connection.
    
    If a MySQL client connection is on the stack, we get the associated brideg.
    Otherwise, we use the globalBridge. 
  */
  static db2i_ileBridge* getBridgeForThread()
  {
    THD* thd = current_thd;
    if (likely(thd))  
      return getBridgeForThread(thd);

    return globalBridge;  
  }

  /**
    Obtain a pointer to the bridge for the specified connection.

    If a bridge exists already, we return it immediately. Otherwise, prepare
    a new bridge for the connection.    
  */
  static db2i_ileBridge* getBridgeForThread(const THD* thd)
  {
    void* thdData = *thd_ha_data(thd, ibmdb2i_hton);
    if (likely(thdData != NULL))
      return (db2i_ileBridge*)(thdData);

    db2i_ileBridge* newBridge = createNewBridge(thd->thread_id);
    *thd_ha_data(thd, ibmdb2i_hton) = (void*)newBridge;
    return newBridge;
  }

  static void destroyBridgeForThread(const THD* thd);
  static void registerPtr(const void* ptr, ILEMemHandle* receiver);
  static void unregisterPtr(ILEMemHandle handle);
  int32 allocateFileDefn(ILEMemHandle definitionSpace,
                         ILEMemHandle handleSpace,
                         uint16 fileCount,
                         const char* schemaName,
                         uint16 schemaNameLength,
                         ILEMemHandle formatSpace,
                         uint32 formatSpaceLen);
  int32 allocateFileInstance(FILE_HANDLE defnHandle,
                             ILEMemHandle inuseSpace,
                             FILE_HANDLE* instance);
  int32 deallocateFile(FILE_HANDLE fileHandle,
                       bool postDropTable=FALSE);
  int32 read(FILE_HANDLE rfileHandle, 
             ILEMemHandle buf, 
             char accessIntent,
             char commitLevel,
             char orientation, 
             bool asyncRead = FALSE,
             ILEMemHandle rrn = 0,
             ILEMemHandle key = 0,
             uint32 keylen = 0,
             uint16 keyParts = 0,
             int pipeFD = -1);
  int32 readByRRN(FILE_HANDLE rfileHandle, 
                  ILEMemHandle buf,
                  uint32 inRRN,
                  char accessIntent,
                  char commitLevel);
  int32 writeRows(FILE_HANDLE rfileHandle, 
                  ILEMemHandle buf, 
                  char commitLevel,
                  int64* outIdVal,
                  bool* outIdGen,
                  uint32* dupKeyRRN,
                  char** dupKeyName,
                  uint32* dupKeyNameLen,
                  uint32* outIdIncrement);
  uint32 execSQL(const char* statement,
                 uint32 statementCount,
                 uint8  commitLevel,
                 bool autoCreateSchema = FALSE,
                 bool dropSchema = FALSE,
                 bool noCommit = FALSE,
                 FILE_HANDLE fileHandle = 0);
  int32 prepOpen(const char* statement,
                 FILE_HANDLE* rfileHandle,
                 uint32* recLength);
  int32 deleteRow(FILE_HANDLE rfileHandle,
                  uint32 rrn);
  int32 updateRow(FILE_HANDLE rfileHandle, 
                  uint32 rrn,
                  ILEMemHandle buf,
                  uint32* dupKeyRRN,
                  char** dupKeyName,
                  uint32* dupKeyNameLen);
  int32 commitmentControl(uint8 function);
  int32 savepoint(uint8 function,
                  const char* savepointName);
  int32 recordsInRange(FILE_HANDLE rfileHandle,
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
                       uint16* outRtnCode);
  int32 rrlslck(FILE_HANDLE rfileHandle,
                char accessIntent);
  int32 lockObj(FILE_HANDLE rfileHandle, 
                uint64 inTimeoutVal, 
                char inAction, 
                char inLockType,
                char inTimeout);
  int32 constraints(FILE_HANDLE rfileHandle,
                    ILEMemHandle inSpc, 
                    uint32 inSpcLen,
                    uint32* outLen,
                    uint32* outCnt);
  int32 optimizeTable(FILE_HANDLE rfileHandle);
  static int32 initILE(const char* aspName,
                       uint16* traceCtlPtr); 
  int32 initFileForIO(FILE_HANDLE rfileHandle,
                      char accessIntent,
                      char commitLevel,
                      uint16* inRecSize,
                      uint16* inRecNullOffset,
                      uint16* outRecSize,
                      uint16* outRecNullOffset);
  int32 readInterrupt(FILE_HANDLE fileHandle);  
  static int32 exitILE(); 
  
  int32 objectOverride(FILE_HANDLE rfileHandle,
                       ILEMemHandle buf,
                       uint32 recordWidth = 0);
  
  int32 retrieveTableInfo(FILE_HANDLE rfileHandle,
                          uint16 dataRequested,
                          ha_statistics& stats,
                          ILEMemHandle inSpc = NULL);

  int32 retrieveIndexInfo(FILE_HANDLE rfileHandle,
                          uint64* outPageCnt); 
  
  int32 closeConnection(CONNECTION_HANDLE conn);
  int32 quiesceFileInstance(FILE_HANDLE rfileHandle);
    
  /**
    Mark the beginning of a "statement transaction"
    
    @detail MySQL "statement transactions" (see sql/handler.cc) are implemented
            as DB2 savepoints having a predefined name.
    
    @return 0 if successful; error otherwise
  */
  uint32 beginStmtTx()
  {
    DBUG_ENTER("db2i_ileBridge::beginStmtTx");
    if (stmtTxActive)
      DBUG_RETURN(0);
    
    stmtTxActive = true;
    
    DBUG_RETURN(savepoint(QMY_SET_SAVEPOINT, SAVEPOINT_NAME));
  }

  /**
    Commit a "statement transaction"
    
    @return 0 if successful; error otherwise
  */
  uint32 commitStmtTx()
  {
    DBUG_ENTER("db2i_ileBridge::commitStmtTx");
    DBUG_ASSERT(stmtTxActive);
    stmtTxActive = false;
    DBUG_RETURN(savepoint(QMY_RELEASE_SAVEPOINT, SAVEPOINT_NAME));
  }
  
  /**
    Roll back a "statement transaction"
    
    @return 0 if successful; error otherwise
  */
  uint32 rollbackStmtTx()
  {
    DBUG_ENTER("db2i_ileBridge::rollbackStmtTx");
    DBUG_ASSERT(stmtTxActive);
    stmtTxActive = false;
    DBUG_RETURN(savepoint(QMY_ROLLBACK_SAVEPOINT, SAVEPOINT_NAME)); 
  }
  
        
  /**
    Provide storage for generating error messages.
    
    This storage must persist until the error message is retrieved from the 
    handler instance. It is for this reason that we associate it with the bridge.
    
    @return Pointer to heap storage of MYSQL_ERRMSG_SIZE bytes
  */
  char* getErrorStorage()
  {
    if (!connErrText)
    {
      connErrText = (char*)my_malloc(MYSQL_ERRMSG_SIZE, MYF(MY_WME));
      if (connErrText) connErrText[0] = 0;
    }
    
    return connErrText;
  }
  
  /**
    Free storage for generating error messages.
  */
  void freeErrorStorage()
  {
    if (likely(connErrText))
    {
      my_free(connErrText, MYF(0)); 
      connErrText = NULL;
    }
  }
  

  /**
    Store a file handle for later retrieval.
    
    If deallocateFile encounters a lock when trying to perform its operation,
    the file remains allocated but must be deallocated later. This function
    provides a way for the connection to "remember" that this deallocation is
    still needed.
    
    @param newname  The name of the file to be added
    @param newhandle  The handle associated with newname
    
  */
  void preserveHandle(const char* newname, FILE_HANDLE newhandle, IBMDB2I_SHARE* share)
  {
    pendingLockedHandles.add(newname, newhandle, share);
  }
  
  /**
    Retrieve a file handle stored by preserveHandle().

    @param name  The name of the file to be retrieved.
    
    @return The handle associated with name    
  */
  FILE_HANDLE findAndRemovePreservedHandle(const char* name, IBMDB2I_SHARE** share)
  {
    FILE_HANDLE hdl = pendingLockedHandles.findAndRemove(name, share);
    return hdl;
  }
  
  /**
    Indicate which error messages should be suppressed on the next API call
    
    These functions are useful for ensuring that the provided error numbers
    are returned if a failure occurs but do not cause a spurious error message
    to be returned.
    
    @return A pointer to this instance
  */
  db2i_ileBridge* expectErrors(int32 er1)
  {
    tacitErrors[0]=er1;
    return this;
  }
  
  db2i_ileBridge* expectErrors(int32 er1, int32 er2)
  {
    tacitErrors[0]=er1;
    tacitErrors[1]=er2;
    return this;
  }

  /**
    Obtain the IBM i system message that accompanied the last API failure.
    
    @return A pointer to the 7 character message ID.
  */
  static const char* getErrorMsgID()
  {
    return ((Qmy_Error_output_t*)parms()->outParms)->MsgId;
  }
  
  /**
    Convert an API error code into the equivalent MySQL error code (if any)
    
    @param rc  The QMYSE API error code
    
    @return  If an equivalent exists, the MySQL error code; else rc
  */
  static int32 translateErrorCode(int32 rc)
  {
    if (likely(rc == 0))
      return 0;
    
    switch (rc)
    {
      case QMY_ERR_KEY_NOT_FOUND:
        return HA_ERR_KEY_NOT_FOUND;
      case QMY_ERR_DUP_KEY:
        return HA_ERR_FOUND_DUPP_KEY;
      case QMY_ERR_END_OF_FILE:
        return HA_ERR_END_OF_FILE;
      case QMY_ERR_LOCK_TIMEOUT:
        return HA_ERR_LOCK_WAIT_TIMEOUT;
      case QMY_ERR_CST_VIOLATION:
        return HA_ERR_NO_REFERENCED_ROW;
      case QMY_ERR_TABLE_NOT_FOUND:
        return HA_ERR_NO_SUCH_TABLE;
      case QMY_ERR_NON_UNIQUE_KEY:
        return ER_DUP_ENTRY;
      case QMY_ERR_MSGID:
        {
          if (memcmp(getErrorMsgID(), DB2I_CPF503A, 7) == 0)
            return HA_ERR_ROW_IS_REFERENCED;
          if (memcmp(getErrorMsgID(), DB2I_SQL0538, 7) == 0)
            return HA_ERR_CANNOT_ADD_FOREIGN;
        }
    }
    return rc;
  }
  
private:
    
  static db2i_ileBridge* createNewBridge(CONNECTION_HANDLE connID);
  static void destroyBridge(db2i_ileBridge* bridge);
  static int registerParmSpace(char* in, char* out);
  static int32 doIt();
  int32 doItWithLog();
  
  static _ILEpointer *functionSymbols;          ///< Array of ILE function pointers
  CONNECTION_HANDLE cachedConnectionID;         ///< The associated connection
  bool stmtTxActive;                            ///< Inside statement transaction
  char *connErrText;                            ///< Storage for error message
  int32 tacitErrors[TACIT_ERRORS_SIZE];         ///< List of errors to be suppressed

  static IleParms* initParmsForThread();

  /**
    Get space for passing parameters to the QMY_* APIs
    
    @details  A fixed-length parameter passing space is associated with each
    pthread. This space is allocated and registered by initParmsForThread() 
    the first time a pthread works with a bridge. The space is cached away
    and remains available until the pthread ends. It became necessary to 
    disassociate the parameter space from the bridge in order to support
    future enhancements to MySQL that sever the one-to-one relationship between
    pthreads and user connections. The QMY_* APIs scope a registered parameter
    space to the thread that executes the register operation.
  */
  static IleParms* parms()
  {
    IleParms* p = my_pthread_getspecific_ptr(IleParms*, THR_ILEPARMS);
    if (likely(p))
      return p;

    return initParmsForThread();    
  }
  
  class PreservedHandleList
  {
    friend db2i_ileBridge* db2i_ileBridge::createNewBridge(CONNECTION_HANDLE);
    public: 
      void add(const char* newname, FILE_HANDLE newhandle, IBMDB2I_SHARE* share);
      FILE_HANDLE findAndRemove(const char* fileName, IBMDB2I_SHARE** share);
      
    private:     
      struct NameHandlePair
      {
        char name[FN_REFLEN];
        FILE_HANDLE handle;
        IBMDB2I_SHARE* share;
        NameHandlePair* next;
      }* head;
  } pendingLockedHandles;

  
#ifndef DBUG_OFF
  bool cachedStateIsCoherent()
  {
      return (current_thd->thread_id == cachedConnectionID);
  }
  
  friend void db2i_ileBridge::unregisterPtr(ILEMemHandle);
  friend void db2i_ileBridge::registerPtr(const void*, ILEMemHandle*);
  static uint32 registeredPtrs;
#endif    
};



#endif
