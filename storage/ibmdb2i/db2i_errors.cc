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


#include "db2i_errors.h"
#include "db2i_ileBridge.h"
#include "db2i_charsetSupport.h"
#include "mysql_priv.h"
#include "stdarg.h"

#define MAX_MSGSTRING 109

/*
  The following strings are associated with errors that can be produced
  within the storage engine proper.
*/
static const char* engineErrors[MAX_MSGSTRING] = 
{  
  {""},                 
  {"Error opening codeset conversion from %.64s to %.64s (errno = %d)"},      
  {"Invalid %-.10s name '%-.128s'"},                                          
  {"Unsupported move from '%-.128s' to '%-.128s' on RENAME TABLE statement"}, 
  {"The %-.64s character set is not supported."},       
  {"Auto_increment is not allowed for a partitioned table"},                  
  {"Character set conversion error due to unknown encoding scheme %d"},       
  {""}, 
  {"Table '%-.128s' was not found by the storage engine"},                    
  {"Could not resolve to %-.128s in library %-.10s type %-.10s (errno = %d)"},
  {"Error on _PGMCALL for program %-.10s in library %-.10s (error = %d)"},    
  {"Error on _ILECALL for API '%.128s' (error = %d)"},                        
  {"Error in iconv() function during character set conversion (errno = %d)"}, 
  {"Error from Get Encoding Scheme (QTQGESP) API: %d, %d, %d"},               
  {"Error from Get Related Default CCSID (QTQGRDC) API: %d, %d, %d"},         
  {"Data out of range for column '%.192s'"},                            
  {"Schema name '%.128s' exceeds maximum length of %d characters"},
  {"Multiple collations not supported in a single index or constraint"},
  {"Sort sequence was not found"},
  {"One or more characters in column %.128s were substituted during conversion"},
  {"A decimal column exceeded the maximum precision. Data may be truncated."},
  {"Some data returned by DB2 for table %s could not be converted for MySQL"},       
  {""},
  {"Column %.128s contains characters that cannot be converted"},
  {"An invalid name was specified for ibmdb2i_rdb_name."},
  {"A duplicate key was encountered for index '%.128s'"},
  {"A table with the same name exists but has incompatible column definitions."},
  {"The created table was discovered as an existing DB2 object."},
  {"Some attribute(s) defined for column '%.128s' may not be honored by accesses from DB2."},
};

/*
  The following strings are associated with errors that can be returned
  by the operating system via the QMY_* APIs. Most are very uncommon and
  indicate a bug somewhere.
*/
static const char* systemErrors[MAX_MSGSTRING] =
{
  {"Thread ID is too long"},
  {"Error creating a SPACE memory object"},
  {"Error creating a FILE memory object"},
  {"Error creating a SPACE synchronization token"},
  {"Error creating a FILE synchronization token"},
  {"See message %-.7s in joblog for job %-.6s/%-.10s/%-.10s."},
  {"Error unlocking a synchronization token when closing a connection"},
  {"Invalid action specified for an 'object lock' request"},
  {"Invalid action specified for a savepoint request"},
  {"Partial keys are not supported with an ICU sort sequence"},
  {"Error retrieving an ICU sort key"},
  {"Error converting single-byte sort sequence to UCS-2"},
  {"An unsupported collation was specified"},
  {"Validation failed for referenced table of foreign key constraint"},
  {"Error extracting table for constraint information"},
  {"Error extracting referenced table for constraint information"},
  {"Invalid action specified for a 'commitment control' request"},
  {"Invalid commitment control isolation level specified on 'open' request"},
  {"Invalid file handle"},
  {" "},
  {"Invalid option specified for returning data on 'read' request"},
  {"Invalid orientation specified for 'read' request"},
  {"Invalid option type specified for 'read' request"},
  {"Invalid isolation level for starting commitment control"},
  {"Error unlocking a synchronization token in module QMYALC"},
  {"Length of space for returned format is not long enough"},
  {"SQL XA transactions are currently unsupported by this interface"},
  {"The associated QSQSRVR job was killed or ended unexpectedly."},
  {"Error unlocking a synchronization token in module QMYSEI"},
  {"Error unlocking a synchronization token in module QMYSPO"},
  {"Error converting input CCSID from short form to long form"},
  {" "},
  {"Error getting associated CCSID for CCSID conversion"},
  {"Error converting a string from one CCSID to another"},
  {"Error unlocking a synchronization token"},
  {"Error destroying a synchronization token"},
  {"Error locking a synchronization token"},
  {"Error recreating a synchronization token"},
  {"A space handle was not specified for a constraint request"},
  {"An SQL cursor was specified for a delete request"},
  {" "},
  {"Error on delete request because current UFCB for connection is not open"},
  {"An SQL cursor was specified for an object initialization request"},
  {"An SQL cursor was specified for an object override request"},
  {"A space handle was not specified for an object override request"},
  {"An SQL cursor was specified for an information request"},
  {"An SQL cursor was specified for an object lock request"},
  {"An SQL cursor was specified for an optimize request"},
  {"A data handle was not specified for a read request"},
  {"A row number handle was not specified for a read request"},
  {"A key handle was not specified for a read request"},
  {"An SQL cursor was specified for an row estimation request"},
  {"A space handle was not specified for a row estimation request"},
  {"An SQL cursor was specified for a release record request"},
  {"A statement handle was not specified for an 'execute immediate' request"},
  {"A statement handle was not specified for a 'prepare open' request"},
  {"An SQL cursor was specified for an update request"},
  {"The UFCB was not open for read"},
  {"Error on update request because current UFCB for connection is not open"},
  {"A data handle was not specified for an update request"},
  {"An SQL cursor was specified for a write request"},
  {"A data handle was not specified for a write request"},
  {"An unknown function was specified on a process request"},
  {"A share definition was not specified for an 'allocate share' request"},
  {"A share handle was not specified for an 'allocate share' request"},
  {"A use count handle was not specified for an 'allocate share' request"},
  {"A 'records per key' handle was not specified for an information request"},
  {"Error resolving LOB addresss"},
  {"Length of a LOB space is too small"},
  {"An unknown function was specified for a server request"},
  {"Object authorization failed. See message %-.7s in joblog for job %-.6s/%-.10s/%-.10s. for more information."},
  {" "},
  {"Error locking mutex on server"},
  {"Error unlocking mutex on server"},
  {"Error checking for RDB name in RDB Directory"},
  {"Error creating mutex on server"},
  {"A table with that name already exists"},
  {" "},
  {"Error unlocking mutex"},
  {"Error connecting to server job"},
  {"Error connecting to server job"},
  {" "},
  {"Function check occurred while registering parameter spaces. See joblog."},
  {" "},
  {" "},
  {"End of block"},
  {"The file has changed and might not be compatible with the MySQL table definition"},
  {"Error giving pipe to server job"},
  {"There are open object locks when attempting to deallocate"},
  {"There is no open lock"},
  {" "},
  {" "},
  {"The maximum value for the auto_increment data type was exceeded"},
  {"Error occurred closing the pipe                "},
  {"Error occurred taking a descriptor for the pipe"},
  {"Error writing to pipe                          "},
  {"Server was interrupted                         "},
  {"No pipe descriptor exists for reuse            "},
  {"Error occurred during an SQL prepare statement "},
  {"Error occurred during an SQL open              "},
  {" "},
  {" "},
  {" "},
  {" "},
  {" "},
  {" "},
  {"An unspecified error was returned from the system."},
  {" "}                                                               
};

/**
  This function builds the text string for an error code, and substitutes
  a variable number of replacement variables into the string.
*/
void getErrTxt(int errCode, ...)
{
  va_list args; 
  va_start(args,errCode);
  char* buffer = db2i_ileBridge::getBridgeForThread()->getErrorStorage();
  const char* msg;
  
  if (errCode >= QMY_ERR_MIN && errCode <= QMY_ERR_SQ_OPEN)
    msg = systemErrors[errCode - QMY_ERR_MIN];
  else
  {
    DBUG_ASSERT(errCode >= DB2I_FIRST_ERR && errCode <= DB2I_LAST_ERR);
    msg = engineErrors[errCode - DB2I_FIRST_ERR];
  }
  
  (void) my_vsnprintf (buffer, MYSQL_ERRMSG_SIZE, msg, args);
  va_end(args);
  fprintf(stderr,"ibmdb2i error %d: %s\n",errCode,buffer);
  DBUG_PRINT("error", ("ibmdb2i error %d: %s",errCode,buffer));
}

static inline void trimSpace(char* str)
{
  char* end = strchr(str, ' ');
  if (end) *end = 0;
}
  

/**
  Generate the error text specific to an API error returned by a QMY_* API.
  
  @parm errCode  The error value
  @parm errInfo  The structure containing the message and job identifiers.
*/
void reportSystemAPIError(int errCode, const Qmy_Error_output *errInfo)
{
  if (errCode >= QMY_ERR_MIN && errCode <= QMY_ERR_SQ_OPEN)
  {
    switch(errCode)
    {
      case QMY_ERR_MSGID:
      case QMY_ERR_NOT_AUTH:
      {
        DBUG_ASSERT(errInfo);
        char jMsg[8];                         // Error message ID
        char jName[11];                       // Job name
        char jUser[11];                       // Job user
        char jNbr[7];                         // Job number
        memset(jMsg, 0, sizeof(jMsg));
        memset(jName, 0, sizeof(jMsg));
        memset(jUser, 0, sizeof(jMsg));
        memset(jMsg, 0, sizeof(jMsg));

        convFromEbcdic(errInfo->MsgId,jMsg,sizeof(jMsg)-1);
        convFromEbcdic(errInfo->JobName,jName,sizeof(jName)-1);
        trimSpace(jName);
        convFromEbcdic(errInfo->JobUser,jUser,sizeof(jUser)-1);
        trimSpace(jUser);
        convFromEbcdic(errInfo->JobNbr,jNbr,sizeof(jNbr)-1);
        getErrTxt(errCode,jMsg,jNbr,jUser,jName);                                
       }
       break;
      case QMY_ERR_RTNFMT:
      {
        getErrTxt(QMY_ERR_LVLID_MISMATCH);
      }
        break;    
      default:
        getErrTxt(errCode);
        break;
    }
  }
}


/**
  Generate a warning for the specified error.
*/
void warning(THD *thd, int errCode, ...) 
{
  va_list args; 
  va_start(args,errCode);
  char buffer[MYSQL_ERRMSG_SIZE];                                               
  const char* msg;
  
  DBUG_ASSERT(errCode >= DB2I_FIRST_ERR && errCode <= DB2I_LAST_ERR);
  msg = engineErrors[errCode - DB2I_FIRST_ERR];
  
  (void) my_vsnprintf (buffer, MYSQL_ERRMSG_SIZE, msg, args);
  va_end(args);
  DBUG_PRINT("warning", ("ibmdb2i warning %d: %s",errCode,buffer));
  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, errCode, buffer);
}


