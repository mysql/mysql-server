/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"

#include <NdbSqlUtil.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/ScanTab.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TcKeyReq.hpp>

#define DEBUG_NEXT_RESULT 0

static const int Err_scanAlreadyComplete = 4120;

NdbScanOperation::NdbScanOperation(Ndb* aNdb, NdbOperation::Type aType) :
  NdbOperation(aNdb, aType),
  m_transConnection(NULL)
{
  theParallelism = 0;
  m_allocated_receivers = 0;
  m_prepared_receivers = 0;
  m_api_receivers = 0;
  m_conf_receivers = 0;
  m_sent_receivers = 0;
  m_receivers = 0;
  m_array = new Uint32[1]; // skip if on delete in fix_receivers
  theSCAN_TABREQ = 0;
  m_executed = false;
  m_scan_buffer= NULL;
  m_scanUsingOldApi= true;
  m_readTuplesCalled= false;
  m_interpretedCodeOldApi= NULL;
}

NdbScanOperation::~NdbScanOperation()
{
  for(Uint32 i = 0; i<m_allocated_receivers; i++){
    m_receivers[i]->release();
    theNdb->releaseNdbScanRec(m_receivers[i]);
  }
  delete[] m_array;
  assert(m_scan_buffer==NULL);
}

void
NdbScanOperation::setErrorCode(int aErrorCode) const
{
  NdbScanOperation *pnonConstThis=const_cast<NdbScanOperation *>(this);

  NdbTransaction* tmp = theNdbCon;
  pnonConstThis->theNdbCon = m_transConnection;
  NdbOperation::setErrorCode(aErrorCode);
  pnonConstThis->theNdbCon = tmp;
}

void
NdbScanOperation::setErrorCodeAbort(int aErrorCode) const
{
  NdbScanOperation *pnonConstThis=const_cast<NdbScanOperation *>(this);

  NdbTransaction* tmp = theNdbCon;
  pnonConstThis->theNdbCon = m_transConnection;
  NdbOperation::setErrorCodeAbort(aErrorCode);
  pnonConstThis->theNdbCon = tmp;
}

/*****************************************************************************
 * int init();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/
int
NdbScanOperation::init(const NdbTableImpl* tab, NdbTransaction* myConnection)
{
  m_transConnection = myConnection;

  if (NdbOperation::init(tab, myConnection) != 0)
    return -1;

  theNdb->theRemainingStartTransactions++; // will be checked in hupp...
  NdbTransaction* aScanConnection = theNdb->hupp(myConnection);
  if (!aScanConnection){
    theNdb->theRemainingStartTransactions--;
    setErrorCodeAbort(theNdb->getNdbError().code);
    return -1;
  }

  // NOTE! The hupped trans becomes the owner of the operation
  theNdbCon= aScanConnection;

  initInterpreter();
  
  theStatus = GetValue;
  theOperationType = OpenScanRequest;
  theNdbCon->theMagicNumber = 0xFE11DF;
  theNoOfTupKeyLeft = tab->m_noOfDistributionKeys;
  m_ordered= false;
  m_descending= false;
  m_read_range_no = 0;
  m_executed = false;
  m_scanUsingOldApi= true;
  m_readTuplesCalled= false;
  m_interpretedCodeOldApi= NULL;
  m_pruneState= SPS_UNKNOWN;

  m_api_receivers_count = 0;
  m_current_api_receiver = 0;
  m_sent_receivers_count = 0;
  m_conf_receivers_count = 0;
  assert(m_scan_buffer==NULL);
  return 0;
}

int
NdbScanOperation::handleScanGetValuesOldApi()
{
  /* Handle old API-defined scan getValue(s) */
  assert(m_scanUsingOldApi);

  if (theReceiver.m_firstRecAttr != NULL) 
  {
    /* theReceiver has a list of RecAttrs which the user
     * wants to read.  Traverse it, adding signals to the
     * request to read them, *similar* to extra GetValue
     * handling, except that we want to use the RecAttrs we've
     * already got.
     * Once these are added to the signal train, all other handling
     * is exactly the same as for normal NdbRecord 'extra GetValues'
     */
    const NdbRecAttr* recAttrToRead = theReceiver.m_firstRecAttr;

    while(recAttrToRead != NULL)
    {
      int res;
      res= insertATTRINFOHdr_NdbRecord(recAttrToRead->theAttrId, 0);
      if (unlikely(res == -1))
        return -1;
      recAttrToRead= recAttrToRead->next();
    }
 
    theInitialReadSize= theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;
  }

  return 0;
}

/* Method for adding interpreted code signals to a 
 * scan operation request.
 * Both main program words and subroutine words can 
 * be added in one method as scans do not use 
 * the final update or final read sections.
 */
int
NdbScanOperation::addInterpretedCode()
{
  Uint32 mainProgramWords= 0;
  Uint32 subroutineWords= 0;
  const NdbInterpretedCode* code= m_interpreted_code;

  /* Any disk access? */
  if (code->m_flags & NdbInterpretedCode::UsesDisk)
  {
    m_flags &= ~Uint8(OF_NO_DISK);
  }


  /* Main program size depends on whether there's subroutines */
  mainProgramWords= code->m_first_sub_instruction_pos ?
    code->m_first_sub_instruction_pos :
    code->m_instructions_length;
  
  int res = insertATTRINFOData_NdbRecord((const char*)code->m_buffer,
                                         mainProgramWords << 2);
  if (res == 0)
  {
    /* Add subroutines, if we have any */
    if (code->m_number_of_subs > 0)
    {
      assert(mainProgramWords > 0);
      assert(code->m_first_sub_instruction_pos > 0);
      
      Uint32 *subroutineStart= 
        &code->m_buffer[ code->m_first_sub_instruction_pos ];
      subroutineWords= 
        code->m_instructions_length -
        code->m_first_sub_instruction_pos;
      
      res = insertATTRINFOData_NdbRecord((const char*) subroutineStart,
                                         subroutineWords << 2);
    }

    /* Update signal section lengths */
    theInterpretedSize= mainProgramWords;
    theSubroutineSize= subroutineWords;
  }

  return res;
}

/* Method for handling scanoptions passed into 
 * NdbTransaction::scanTable or scanIndex
 */
int
NdbScanOperation::handleScanOptions(const ScanOptions *options)
{
  /* Options size has already been checked.
   * scan_flags, parallel and batch have been handled
   * already (see NdbTransaction::scanTable and scanIndex)
   */
  if ((options->optionsPresent & ScanOptions::SO_GETVALUE) &&
      (options->numExtraGetValues > 0))
  {
    if (options->extraGetValues == NULL)
    {
      setErrorCodeAbort(4299);
      /* Incorrect combination of ScanOption flags, 
       * extraGetValues ptr and numExtraGetValues */
      return -1;
    }

    /* Add extra getValue()s */
    for (unsigned int i=0; i < options->numExtraGetValues; i++)
    {
      NdbOperation::GetValueSpec *pvalSpec = &(options->extraGetValues[i]);

      pvalSpec->recAttr=NULL;

      if (pvalSpec->column == NULL)
      {
        setErrorCodeAbort(4295);
        // Column is NULL in Get/SetValueSpec structure
        return -1;
      }

      /* Call internal NdbRecord specific getValue() method
       * Same method handles table scans and index scans
       */
      NdbRecAttr *pra=
        getValue_NdbRecord_scan(&NdbColumnImpl::getImpl(*pvalSpec->column),
                                (char *) pvalSpec->appStorage);
        
      if (pra == NULL)
      {
        return -1;
      }
      
      pvalSpec->recAttr = pra;
    }
  }

  if (options->optionsPresent & ScanOptions::SO_PARTITION_ID)
  {
    /* Should not have any blobs defined at this stage */
    assert(theBlobList == NULL);
    assert(m_pruneState == SPS_UNKNOWN);
    
    /* Only allowed to set partition id for PK ops on UserDefined
     * partitioned tables
     */
    if(unlikely(! (m_attribute_record->flags & 
                   NdbRecord::RecHasUserDefinedPartitioning)))
    {
      /* Explicit partitioning info not allowed for table and operation*/
      setErrorCodeAbort(4546);
      return -1;
    }

    m_pruneState= SPS_FIXED;
    m_pruningKey= options->partitionId;
    
    /* And set the vars in the operation now too */
    theDistributionKey = options->partitionId;
    theDistrKeyIndicator_ = 1;
    assert((m_attribute_record->flags & NdbRecord::RecHasUserDefinedPartitioning) != 0);
    DBUG_PRINT("info", ("NdbScanOperation::handleScanOptions(dist key): %u",
                        theDistributionKey));
  }

  if (options->optionsPresent & ScanOptions::SO_INTERPRETED)
  {
    /* Check the program's for the same table as the
     * operation, within a major version number
     * Perhaps NdbInterpretedCode should not contain the table
     */
    const NdbDictionary::Table* codeTable= 
      options->interpretedCode->getTable();
    if (codeTable != NULL)
    {
      NdbTableImpl* impl= &NdbTableImpl::getImpl(*codeTable);
      
      if ((impl->m_id != (int) m_attribute_record->tableId) ||
          (table_version_major(impl->m_version) != 
           table_version_major(m_attribute_record->tableVersion)))
        return 4524; // NdbInterpretedCode is for different table`
    }

    if ((options->interpretedCode->m_flags & 
         NdbInterpretedCode::Finalised) == 0)
    {
      setErrorCodeAbort(4519);
      return -1; // NdbInterpretedCode::finalise() not called.
    }
    m_interpreted_code= options->interpretedCode;
  }

  /* User's operation 'tag' data. */
  if (options->optionsPresent & ScanOptions::SO_CUSTOMDATA)
  {
    m_customData = options->customData;
  }

  /* Preferred form of partitioning information */
  if (options->optionsPresent & ScanOptions::SO_PART_INFO)
  {
    Uint32 partValue;
    Ndb::PartitionSpec tmpSpec;
    const Ndb::PartitionSpec* pSpec= options->partitionInfo;
    if (unlikely(validatePartInfoPtr(pSpec,
                                     options->sizeOfPartInfo,
                                     tmpSpec) ||
                 getPartValueFromInfo(pSpec,
                                      m_currentTable,
                                      &partValue)))
      return -1;
    
    assert(m_pruneState == SPS_UNKNOWN);
    m_pruneState= SPS_FIXED;
    m_pruningKey= partValue;
    
    theDistributionKey= partValue;
    theDistrKeyIndicator_= 1;
    DBUG_PRINT("info", ("Set distribution key from partition spec to %u",
                        partValue));
  }

  return 0;
}

/**
 * generatePackedReadAIs
 * This method is adds AttrInfos to the current signal train to perform
 * a packed read of the requested columns.
 * It is used by table scan and index scan.
 */
int
NdbScanOperation::generatePackedReadAIs(const NdbRecord *result_record,
                                        bool& haveBlob,
                                        const Uint32 * m_read_mask)
{
  Bitmask<MAXNROFATTRIBUTESINWORDS> readMask;
  Uint32 columnCount= 0;
  Uint32 maxAttrId= 0;

  haveBlob= false;
  
  for (Uint32 i= 0; i<result_record->noOfColumns; i++)
  {
    const NdbRecord::Attr *col= &result_record->columns[i];
    Uint32 attrId= col->attrId;

    assert(!(attrId & AttributeHeader::PSEUDO));

    /* Skip column if result_mask says so and we don't need
     * to read it 
     */
    if (!BitmaskImpl::get(MAXNROFATTRIBUTESINWORDS, m_read_mask, attrId)) 
      continue;

    /* Blob reads are handled with a getValue() in NdbBlob.cpp. */
    if (unlikely(col->flags & NdbRecord::IsBlob))
    {
      m_keyInfo= 1;                         // Need keyinfo for blob scan
      haveBlob= true;
      continue;
    }

    if (col->flags & NdbRecord::IsDisk)
      m_flags &= ~Uint8(OF_NO_DISK);

    if (attrId > maxAttrId)
      maxAttrId= attrId;

    readMask.set(attrId);
    columnCount++;
  }

  int result= 0;

  /* Are there any columns to read via NdbRecord? 
   * Old Api scans, and new Api scans which only read via extra getvalues
   * may have no 'NdbRecord reads'
   */
  if (columnCount > 0)
  {
    bool all= (columnCount == m_currentTable->m_columns.size());
    
    if (all)
      result= insertATTRINFOHdr_NdbRecord(AttributeHeader::READ_ALL, 
                                          columnCount);
    else
    {
      /* How many bitmask words are significant? */
      Uint32 sigBitmaskWords= (maxAttrId>>5) + 1;
      
      result= insertATTRINFOHdr_NdbRecord(AttributeHeader::READ_PACKED, 
                                          sigBitmaskWords << 2);
      if (result != -1)
        result= insertATTRINFOData_NdbRecord((const char*) &readMask.rep.data[0],
                                             sigBitmaskWords << 2); // Bitmask
    }
  }

  return result;
}

/**
 * scanImpl
 * This method is called by scanTableImpl() and scanIndexImpl() and
 * performs most of the signal building tasks that both scan
 * types share
 */
inline int
NdbScanOperation::scanImpl(const NdbScanOperation::ScanOptions *options,
                           const Uint32 * readMask)
{
  bool haveBlob= false;

  /* Add AttrInfos for packed read of cols in result_record */
  if (generatePackedReadAIs(m_attribute_record, haveBlob, readMask) != 0)
    return -1;

  theInitialReadSize= theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;

  /* Handle any getValue() calls made against the old API. */
  if (m_scanUsingOldApi)
  {
    if (handleScanGetValuesOldApi() !=0)
      return -1;
  }

  /* Handle scan options - always for old style scan API */
  if (options != NULL)
  {
    if (handleScanOptions(options) != 0)
      return -1;
  }

  /* Get Blob handles unless this is an old Api scan op 
   * For old Api Scan ops, the Blob handles are already
   * set up by the call to getBlobHandle()
   */
  if (unlikely(haveBlob) && !m_scanUsingOldApi)
  {
    if (getBlobHandlesNdbRecord(m_transConnection, readMask) == -1)
      return -1;
  }

  /* Add interpreted code words to ATTRINFO signal
   * chain as necessary
   */
  if (m_interpreted_code != NULL)
  {
    if (addInterpretedCode() == -1)
      return -1;
  }
  
  /* Scan is now fully defined, so let's start preparing
   * signals.
   */
  if (prepareSendScan(theNdbCon->theTCConPtr, 
                      theNdbCon->theTransactionId,
                      readMask) == -1)
    /* Error code should be set */
    return -1;
  
  return 0;
}

int
NdbScanOperation::handleScanOptionsVersion(const ScanOptions*& optionsPtr, 
                                           Uint32 sizeOfOptions,
                                           ScanOptions& currOptions)
{
  /* Handle different sized ScanOptions */
  if (unlikely((sizeOfOptions !=0) &&
               (sizeOfOptions != sizeof(ScanOptions))))
  {
    /* Different size passed, perhaps it's an old client */
    if (sizeOfOptions == sizeof(ScanOptions_v1))
    {
      const ScanOptions_v1* oldOptions= 
        (const ScanOptions_v1*) optionsPtr;

      /* v1 of ScanOptions, copy into current version
       * structure and update options ptr
       */
      currOptions.optionsPresent= oldOptions->optionsPresent;
      currOptions.scan_flags= oldOptions->scan_flags;
      currOptions.parallel= oldOptions->parallel;
      currOptions.batch= oldOptions->batch;
      currOptions.extraGetValues= oldOptions->extraGetValues;
      currOptions.numExtraGetValues= oldOptions->numExtraGetValues;
      currOptions.partitionId= oldOptions->partitionId;
      currOptions.interpretedCode= oldOptions->interpretedCode;
      currOptions.customData= oldOptions->customData;
      
      /* New fields */
      currOptions.partitionInfo= NULL;
      currOptions.sizeOfPartInfo= 0;
      
      optionsPtr= &currOptions;
    }
    else
    {
      /* No other versions supported currently */
      setErrorCodeAbort(4298);
      /* Invalid or unsupported ScanOptions structure */
      return -1;
    }
  }
  return 0;
}

int
NdbScanOperation::scanTableImpl(const NdbRecord *result_record,
                                NdbOperation::LockMode lock_mode,
                                const unsigned char *result_mask,
                                const NdbScanOperation::ScanOptions *options,
                                Uint32 sizeOfOptions)
{
  int res;
  Uint32 scan_flags = 0;
  Uint32 parallel = 0;
  Uint32 batch = 0;

  ScanOptions currentOptions;

  if (options != NULL)
  {
    if (handleScanOptionsVersion(options, sizeOfOptions, currentOptions))
      return -1;
    
    /* Process some initial ScanOptions - most are 
     * handled later
     */
    if (options->optionsPresent & ScanOptions::SO_SCANFLAGS)
      scan_flags = options->scan_flags;
    if (options->optionsPresent & ScanOptions::SO_PARALLEL)
      parallel = options->parallel;
    if (options->optionsPresent & ScanOptions::SO_BATCH)
      batch = options->batch;
  }
#if 0 // ToDo: this breaks optimize index, but maybe there is a better solution
  if (result_record->flags & NdbRecord::RecIsIndex)
  {
    setErrorCodeAbort(4340);
    return -1;
  }
#endif

  m_attribute_record= result_record;
  AttributeMask readMask;
  m_attribute_record->copyMask(readMask.rep.data, result_mask);

  /* Process scan definition info */
  res= processTableScanDefs(lock_mode, scan_flags, parallel, batch);
  if (res == -1)
    return -1;

  theStatus= NdbOperation::UseNdbRecord;
  /* Call generic scan code */
  return scanImpl(options, readMask.rep.data);
}


int
NdbScanOperation::getPartValueFromInfo(const Ndb::PartitionSpec* partInfo,
                                       const NdbTableImpl* table,
                                       Uint32* partValue)
{
  switch(partInfo->type)
  {
  case Ndb::PartitionSpec::PS_USER_DEFINED:
  {
    assert(table->m_fragmentType == NdbDictionary::Object::UserDefined);
    *partValue= partInfo->UserDefined.partitionId;
    return 0;
  }

  case Ndb::PartitionSpec::PS_DISTR_KEY_PART_PTR:
  {
    assert(table->m_fragmentType != NdbDictionary::Object::UserDefined);
    Uint32 hashVal;
    int ret= Ndb::computeHash(&hashVal, table, 
                              partInfo->KeyPartPtr.tableKeyParts,
                              partInfo->KeyPartPtr.xfrmbuf, 
                              partInfo->KeyPartPtr.xfrmbuflen);
    if (ret == 0)
    {
      /* We send the hash result here (rather than the partitionId
       * generated by doing some function on the hash)
       * Note that KEY and LINEAR KEY native partitioning hash->partitionId
       * mapping functions are idempotent so that they can be
       * applied multiple times to their result without changing it.  
       * DIH will apply them, so there's no need to also do it here in API, 
       * unless we want to see which physical partition we *think* will 
       * hold the values.
       * Only possible advantage is that we could identify some locality
       * not shown in the hash result.  This is only *safe* for schemes
       * which cannot change the hash->partitionId mapping function
       * online.
       * Can add as an optimisation if necessary.
       */
      *partValue= hashVal;
      return 0;
    }
    else
    {
      setErrorCodeAbort(ret);
      return -1;
    }
  }
  
  case Ndb::PartitionSpec::PS_DISTR_KEY_RECORD:
  {
    assert(table->m_fragmentType != NdbDictionary::Object::UserDefined);
    Uint32 hashVal;
    int ret= Ndb::computeHash(&hashVal,
                              partInfo->KeyRecord.keyRecord,
                              partInfo->KeyRecord.keyRow,
                              partInfo->KeyRecord.xfrmbuf, 
                              partInfo->KeyRecord.xfrmbuflen);
    if (ret == 0)
    {
      /* See comments above about sending hashResult rather than
       * partitionId
       */
      *partValue= hashVal;
      return 0;
    }
    else
    {
      setErrorCodeAbort(ret);
      return -1;
    }
  }
  }
  
  /* 4542 : Unknown partition information type */
  setErrorCodeAbort(4542);
  return -1;
}

/*
  Compare two rows on some prefix of the index.
  This is used to see if we can determine that all rows in an index range scan
  will come from a single fragment (if the two rows bound a single distribution
  key).
 */
static int
compare_index_row_prefix(const NdbRecord *rec,
                         const char *row1,
                         const char *row2,
                         Uint32 prefix_length)
{
  Uint32 i;

  if (row1 == row2) // Easy case with same ptrs
    return 0;

  for (i= 0; i<prefix_length; i++)
  {
    const NdbRecord::Attr *col= &rec->columns[rec->key_indexes[i]];

    bool is_null1= col->is_null(row1);
    bool is_null2= col->is_null(row2);
    if (is_null1)
    {
      if (!is_null2)
        return -1;
      /* Fall-through to compare next one. */
    }
    else
    {
      if (is_null2)
        return 1;

      Uint32 offset= col->offset;
      Uint32 maxSize= col->maxSize;
      const char *ptr1= row1 + offset;
      const char *ptr2= row2 + offset;

      /*  bug#56853 */
      char buf1[NdbRecord::Attr::SHRINK_VARCHAR_BUFFSIZE];
      char buf2[NdbRecord::Attr::SHRINK_VARCHAR_BUFFSIZE];
      if (col->flags & NdbRecord::IsMysqldShrinkVarchar)
      {
        Uint32 len1;
        bool ok1 = col->shrink_varchar(row1, len1, buf1);
        assert(ok1);
        ptr1 = buf1;
        Uint32 len2;
        bool ok2 = col->shrink_varchar(row2, len2, buf2);
        assert(ok2);
        ptr2 = buf2;
      }

      void *info= col->charset_info;
      int res=
        (*col->compare_function)(info, ptr1, maxSize, ptr2, maxSize);
      if (res)
      {
        return res;
      }
    }
  }

  return 0;
}

int
NdbIndexScanOperation::getDistKeyFromRange(const NdbRecord *key_record,
                                           const NdbRecord *result_record,
                                           const char *row,
                                           Uint32* distKey)
{
  const Uint32 MaxKeySizeInLongWords= (NDB_MAX_KEY_SIZE + 7) / 8; 
  // Note: xfrm:ed key can/will be bigger than MaxKeySizeInLongWords
  Uint64 tmp[ MaxKeySizeInLongWords * MAX_XFRM_MULTIPLY ];
  char* tmpshrink = (char*)tmp;
  Uint32 tmplen = (Uint32)sizeof(tmp);
  
  /* This can't work for User Defined partitioning */
  assert(key_record->table->m_fragmentType != 
         NdbDictionary::Object::UserDefined);

  Ndb::Key_part_ptr ptrs[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY+1];
  Uint32 i;
  for (i = 0; i<key_record->distkey_index_length; i++)
  {
    const NdbRecord::Attr *col =
      &key_record->columns[key_record->distkey_indexes[i]];
    if (col->flags & NdbRecord::IsMysqldShrinkVarchar)
    {
      if (tmplen >= 256)
      {
        Uint32 len;
        bool len_ok = col->shrink_varchar(row, len, tmpshrink);
        if (!len_ok)
        {
          /* 4209 : Length parameter in equal/setValue is incorrect */
          setErrorCodeAbort(4209);
          return -1;
        }
        ptrs[i].ptr = tmpshrink;
        tmpshrink += len;
        tmplen -= len;
      }
      else
      {
        /* 4207 : Key size is limited to 4092 bytes */
        setErrorCodeAbort(4207);
        return -1;
      }
    }
    else
    {
      ptrs[i].ptr = row + col->offset;
    }
    ptrs[i].len = col->maxSize;
  }
  ptrs[i].ptr = 0;
  
  Uint32 hashValue;
  int ret = Ndb::computeHash(&hashValue, result_record->table,
                             ptrs, tmpshrink, tmplen);
  if (ret == 0)
  {
    *distKey = hashValue;
    return 0;
  }
  else
  {
#ifdef VM_TRACE
    ndbout << "err: " << ret << endl;
#endif
    setErrorCodeAbort(ret);
    return -1;
  }
}

int
NdbScanOperation::validatePartInfoPtr(const Ndb::PartitionSpec*& partInfo,
                                      Uint32 sizeOfPartInfo,
                                      Ndb::PartitionSpec& tmpSpec)
{  
  if (unlikely(sizeOfPartInfo != sizeof(Ndb::PartitionSpec)))
  {
    if (sizeOfPartInfo == sizeof(Ndb::PartitionSpec_v1))
    {
      const Ndb::PartitionSpec_v1* oldPSpec= 
        (const Ndb::PartitionSpec_v1*) partInfo;
      
      /* Let's upgrade to the latest variant */
      tmpSpec.type= oldPSpec->type;
      if (tmpSpec.type == Ndb::PartitionSpec_v1::PS_USER_DEFINED)
      {
        tmpSpec.UserDefined.partitionId= oldPSpec->UserDefined.partitionId;
      }
      else
      {
        tmpSpec.KeyPartPtr.tableKeyParts= oldPSpec->KeyPartPtr.tableKeyParts;
        tmpSpec.KeyPartPtr.xfrmbuf= oldPSpec->KeyPartPtr.xfrmbuf;
        tmpSpec.KeyPartPtr.xfrmbuflen= oldPSpec->KeyPartPtr.xfrmbuflen;
      }
      
      partInfo= &tmpSpec;
    }
    else
    {
      /* 4545 : Invalid or Unsupported PartitionInfo structure */
      setErrorCodeAbort(4545);
      return -1;
    }
  }
  
  if (partInfo->type != Ndb::PartitionSpec::PS_NONE)
  {
    if (m_pruneState == SPS_FIXED)
    {
      /* 4543 : Duplicate partitioning information supplied */
      setErrorCodeAbort(4543);
      return -1;
    }
    
    if ((partInfo->type == Ndb::PartitionSpec::PS_USER_DEFINED) !=
        ((m_currentTable->m_fragmentType == NdbDictionary::Object::UserDefined)))
    {
      /* Mismatch between type of partitioning info supplied, and table's
       * partitioning type
       */
      /* 4544 : Wrong partitionInfo type for table */
      setErrorCodeAbort(4544);
      return -1;
    }
  }
  else
  {
    /* PartInfo supplied, but set to NONE */
    partInfo= NULL;
  }

  return 0;
}


int
NdbIndexScanOperation::setBound(const NdbRecord* key_record,
                                const IndexBound& bound)
{
  return setBound(key_record, bound, NULL, 0);
}

/** 
 * setBound()
 *
 * This method is called from scanIndex() and setBound().  
 * It adds a bound to an Index Scan.
 * It can be passed extra partitioning information.
 */
int 
NdbIndexScanOperation::setBound(const NdbRecord *key_record,
                                const IndexBound& bound,
                                const Ndb::PartitionSpec* partInfo,
                                Uint32 sizeOfPartInfo)
{
  if (unlikely((theStatus != NdbOperation::UseNdbRecord)))
  {
    setErrorCodeAbort(4284);
    /* Cannot mix NdbRecAttr and NdbRecord methods in one operation */
    return -1;
  }

  if (unlikely(key_record == NULL))
  {
    setErrorCodeAbort(4285);
    /* NULL NdbRecord pointer */
    return -1;
  }

  /* Has the user supplied an open range (no bounds)? */
  const bool openRange= (((bound.low_key == NULL) && 
                          (bound.high_key == NULL)) ||
                         ((bound.low_key_count == 0) && 
                          (bound.high_key_count == 0)));
  
  /* Check the base table's partitioning scheme 
   * (Ordered index itself has 'undefined' fragmentation)
   */
  bool tabHasUserDefPartitioning= (m_currentTable->m_fragmentType == 
                                   NdbDictionary::Object::UserDefined);

  /* Validate explicit partitioning info if it's supplied */
  Ndb::PartitionSpec tmpSpec;
  if (partInfo)
  {
    /* May update the PartInfo ptr */
    if (validatePartInfoPtr(partInfo,
                            sizeOfPartInfo,
                            tmpSpec))
      return -1;
  }

  m_num_bounds++;

  if (unlikely((m_num_bounds > 1) &&
               (m_multi_range == 0)))
  {
    /* > 1 IndexBound, but not MRR */
    setErrorCodeAbort(4509);
    /* Non SF_MultiRange scan cannot have more than one bound */
    return -1;
  }

  Uint32 j;
  Uint32 key_count, common_key_count;
  Uint32 range_no;
  Uint32 bound_head;

  range_no= bound.range_no;
  if (unlikely(range_no > MaxRangeNo))
  {
    setErrorCodeAbort(4286);
    return -1;
  }

  /* Check valid ordering of supplied range numbers */
  if ( m_read_range_no && m_ordered )
  {
    if (unlikely((m_num_bounds > 1) &&
                 (range_no <= m_previous_range_num))) 
    {
      setErrorCodeAbort(4282);
      /* range_no not strictly increasing in ordered multi-range index scan */
      return -1;
    }
    
    m_previous_range_num= range_no;
  }

  key_count= bound.low_key_count;
  common_key_count= key_count;
  if (key_count < bound.high_key_count)
    key_count= bound.high_key_count;
  else
    common_key_count= bound.high_key_count;

  if (unlikely(key_count > key_record->key_index_length))
  {
    /* Too many keys specified for key bound. */
    setErrorCodeAbort(4281);
    return -1;
  }

  /* We need to get a ptr to the first word of this
   * range so that we can set the total length of the range 
   * (and range num) at the end of writing out the range.
   */
  Uint32* firstRangeWord= NULL;
  const Uint32 keyLenBeforeRange= theTupKeyLen;

  if (likely(!openRange))
  {
    /* If low and high key pointers are the same and key counts are
     * the same, we send as an Eq bound to save bandwidth.
     * This will not send an EQ bound if :
     *   - Different numbers of high and low keys are EQ
     *   - High and low keys are EQ, but use different ptrs
     * This could be improved in future with another setBound() variant.
     */
    const bool isEqRange= 
      (bound.low_key == bound.high_key) &&
      (bound.low_key_count == bound.high_key_count) &&
      (bound.low_inclusive && bound.high_inclusive); // Does this matter?

    if (isEqRange)
    {
      /* Using BoundEQ will result in bound being sent only once */
      for (j= 0; j<key_count; j++)
      {
        ndbrecord_insert_bound(key_record, key_record->key_indexes[j],
                               bound.low_key, BoundEQ, firstRangeWord);
      }
    }
    else
    {
      /* Distinct upper and lower bounds, must specify them independently */
      /* Note :  Protocol allows individual columns to be specified as EQ
       * or some prefix of columns.  This is not currently supported from
       * NDBAPI.
       */
      for (j= 0; j<key_count; j++)
      {
        Uint32 bound_type;
        /* If key is part of lower bound */
        if (bound.low_key && j<bound.low_key_count)
        {
          /* Inclusive if defined, or matching rows can include this value */
          bound_type= bound.low_inclusive  || j+1 < bound.low_key_count ?
            BoundLE : BoundLT;
          ndbrecord_insert_bound(key_record, key_record->key_indexes[j],
                                 bound.low_key, bound_type, firstRangeWord);
        }
        /* If key is part of upper bound */
        if (bound.high_key && j<bound.high_key_count)
        {
          /* Inclusive if defined, or matching rows can include this value */
          bound_type= bound.high_inclusive  || j+1 < bound.high_key_count ?
            BoundGE : BoundGT;
          ndbrecord_insert_bound(key_record, key_record->key_indexes[j],
                                 bound.high_key, bound_type, firstRangeWord);
        }
      }
    }
  }
  else
  {
    /* Open range - all rows must be returned.
     * To encode this, we'll request all rows where the first
     * key column value is >= NULL
     */
    insert_open_bound(key_record, firstRangeWord);
  }

  /* Set the length of this range
   * Length = TupKeyLen@range end - TupKeyLen@ range start
   * Pack into Uint32 with range no and bound type as described 
   * in KeyInfo.hpp
   */
  assert(firstRangeWord != NULL);
  
  bound_head= *firstRangeWord;
  bound_head|=
    (theTupKeyLen - keyLenBeforeRange) << 16 | (range_no << 4);
  *firstRangeWord= bound_head;


  /* Now determine if the scan can (continue to) be pruned to one
   * partition
   * 
   * This can only be the case if 
   *   - There's no overriding partition id/info specified in 
   *     ScanOptions 
   *     AND
   *   - This range scan can be pruned to 1 partition 'value'
   *     AND
   *   - All previous ranges (MRR) were partition pruned 
   *     to the same partition 'value'
   *
   * Where partition 'value' is either a partition id or a hash
   * that maps to one in the kernel.
   */
  if ((m_pruneState == SPS_UNKNOWN) ||      // First range
      (m_pruneState == SPS_ONE_PARTITION))  // Previous ranges are commonly pruned
  {
    bool currRangeHasOnePartVal= false;
    Uint32 currRangePartValue= 0;

    /* Determine whether this range scan can be pruned */
    if (partInfo)
    {
      /* Explicit partitioning info supplied, use it to get a value */
      currRangeHasOnePartVal= true;

      if (getPartValueFromInfo(partInfo,
                               m_attribute_record->table,
                               &currRangePartValue))
      {
        return -1;
      }
    }
    else
    {
      if (likely(!tabHasUserDefPartitioning))
      {
        /* Attempt to get implicit partitioning info from range bounds - 
         * only possible if they are present and bound a single value 
         * of the table's distribution keys
         */
        Uint32 index_distkeys = key_record->m_no_of_distribution_keys;
        Uint32 table_distkeys = m_attribute_record->m_no_of_distribution_keys;
        Uint32 distkey_min= key_record->m_min_distkey_prefix_length;
        if (index_distkeys == table_distkeys &&   // Index has all base table d-keys
            common_key_count >= distkey_min &&    // Bounds have all d-keys
            bound.low_key &&                      // Have both bounds
            bound.high_key &&
            0==compare_index_row_prefix(key_record,     // Both bounds are same
                                        bound.low_key,
                                        bound.high_key,
                                        distkey_min))
        {
          assert(! openRange);
          currRangeHasOnePartVal= true;
          if (getDistKeyFromRange(key_record, m_attribute_record,
                                  bound.low_key,
                                  &currRangePartValue))
            return -1;
        }
      }
    }
     

    /* Determine whether this pruned range fits with any existing
     * range pruning
     * As we can currently only prune a single scan to one partition
     * (Not a set of partitions, or a set of partitions per range)
     * we can only prune if all ranges happen to be prune-able to the
     * same partition.
     * In future perhaps Ndb can be enhanced to support partition sets
     * and/or per-range partition pruning.
     */
    const ScanPruningState prevPruneState= m_pruneState;
    if (currRangeHasOnePartVal)
    {
      if (m_pruneState == SPS_UNKNOWN)
      {
        /* Prune the scan to use this range's partition value */
        m_pruneState= SPS_ONE_PARTITION;
        m_pruningKey= currRangePartValue;
      }
      else
      {
        /* If this range's partition value is the same as the previous
         * ranges then we can stay pruned, otherwise we cannot
         */
        assert(m_pruneState == SPS_ONE_PARTITION);
        if (currRangePartValue != m_pruningKey)
        {
          /* This range is found in a different partition to previous
           * range(s).  We cannot prune this scan.
           */
          m_pruneState= SPS_MULTI_PARTITION;
        }
      }
    }
    else
    {
      /* This range cannot be scanned by scanning a single partition
       * Therefore the scan must scan all partitions
       */
      m_pruneState= SPS_MULTI_PARTITION;
    }

    /* Now modify the SCANTABREQ */
    if (m_pruneState != prevPruneState)
    {
      theDistrKeyIndicator_= (m_pruneState == SPS_ONE_PARTITION);
      theDistributionKey= m_pruningKey;

      ScanTabReq *req= CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
      ScanTabReq::setDistributionKeyFlag(req->requestInfo, theDistrKeyIndicator_);
      req->distributionKey= theDistributionKey;
      theSCAN_TABREQ->setLength(ScanTabReq::StaticLength + theDistrKeyIndicator_);
    }
  } // if (m_pruneState == UNKNOWN / SPS_ONE_PARTITION)

  return 0;
} // ::setBound();


int
NdbIndexScanOperation::scanIndexImpl(const NdbRecord *key_record,
                                     const NdbRecord *result_record,
                                     NdbOperation::LockMode lock_mode,
                                     const unsigned char *result_mask,
                                     const NdbIndexScanOperation::IndexBound *bound,
                                     const NdbScanOperation::ScanOptions *options,
                                     Uint32 sizeOfOptions)
{
  int res;
  Uint32 i;
  Uint32 scan_flags = 0;
  Uint32 parallel = 0;
  Uint32 batch = 0;

  ScanOptions currentOptions;

  if (options != NULL)
  {
    if (handleScanOptionsVersion(options, sizeOfOptions, currentOptions))
      return -1;
    
    /* Process some initial ScanOptions here
     * The rest will be handled later
     */
    if (options->optionsPresent & ScanOptions::SO_SCANFLAGS)
      scan_flags = options->scan_flags;
    if (options->optionsPresent & ScanOptions::SO_PARALLEL)
      parallel = options->parallel;
    if (options->optionsPresent & ScanOptions::SO_BATCH)
      batch = options->batch;
  }

  if (!(key_record->flags & NdbRecord::RecHasAllKeys))
  {
    setErrorCodeAbort(4292);
    return -1;
  }

  AttributeMask readMask;
  result_record->copyMask(readMask.rep.data, result_mask);

  if (scan_flags & (NdbScanOperation::SF_OrderBy | 
                    NdbScanOperation::SF_OrderByFull))
  {
    /**
     * For ordering, we need all keys in the result row.
     *
     * So for each key column, check that it is included in the result
     * NdbRecord.
     */
    Uint32 keymask[MAXNROFATTRIBUTESINWORDS];
    BitmaskImpl::clear(MAXNROFATTRIBUTESINWORDS, keymask);

    for (i = 0; i < key_record->key_index_length; i++)
    {
      Uint32 attrId = key_record->columns[key_record->key_indexes[i]].attrId;
      if (attrId >= result_record->m_attrId_indexes_length ||
          result_record->m_attrId_indexes[attrId] < 0)
      {
        setErrorCodeAbort(4292);
        return -1;
      }

      BitmaskImpl::set(MAXNROFATTRIBUTESINWORDS, keymask, attrId);
    }

    if (scan_flags & NdbScanOperation::SF_OrderByFull)
    {
      BitmaskImpl::bitOR(MAXNROFATTRIBUTESINWORDS, readMask.rep.data, keymask);
    }
    else if (!BitmaskImpl::contains(MAXNROFATTRIBUTESINWORDS, 
                                    readMask.rep.data, keymask))
    {
      setErrorCodeAbort(4341);
      return -1;
    }
  }
  
  if (!(key_record->flags & NdbRecord::RecIsIndex))
  {
    setErrorCodeAbort(4283);
    return -1;
  }
  if (result_record->flags & NdbRecord::RecIsIndex)
  {
    setErrorCodeAbort(4340);
    return -1;
  }

  /* Modify NdbScanOperation vars to indicate that we're an 
   * IndexScan
   */
  m_type= NdbOperation::OrderedIndexScan;
  m_currentTable= result_record->table;

  m_key_record = key_record;
  m_attribute_record= result_record;

  res= processIndexScanDefs(lock_mode, scan_flags, parallel, batch);
  if (res==-1)
    return -1;

  /* Fix theStatus as set in processIndexScanDefs(). */
  theStatus= NdbOperation::UseNdbRecord;
  
  /* Call generic scan code */
  res= scanImpl(options, readMask.rep.data);

  if (!res)
  {
    /*
     * Set up first key bound, if present
     * Extra bounds (MRR) can be added later
     */
    if (bound != NULL)
    {
      res= setBound(key_record, *bound);
    }
  }
  
  return res;
} // ::scanIndexImpl();


/* readTuples() method for table scans
 * This method performs minimal validation and initialisation,
 * deferring most of the work to a later call to processTableScanDefs
 * below.
 */
int 
NdbScanOperation::readTuples(NdbScanOperation::LockMode lm,
                             Uint32 scan_flags, 
                             Uint32 parallel,
                             Uint32 batch)
{
  // It is only possible to call readTuples if  readTuples hasn't
  // already been called
  if (m_readTuplesCalled)
  {
    setErrorCode(4605);
    return -1;
  }
  
  /* Save parameters for later */
  m_readTuplesCalled= true;
  m_savedLockModeOldApi= lm;
  m_savedScanFlagsOldApi= scan_flags;
  m_savedParallelOldApi= parallel;
  m_savedBatchOldApi= batch;

  /**
   * Old API always auto-added all key-colums
   */
  if (scan_flags & SF_OrderBy)
    m_savedScanFlagsOldApi |= SF_OrderByFull;

  return 0;
}

/* Most of the scan definition work for old + NdbRecord API scans is done here */
int 
NdbScanOperation::processTableScanDefs(NdbScanOperation::LockMode lm,
                                       Uint32 scan_flags, 
                                       Uint32 parallel,
                                       Uint32 batch)
{
  m_ordered = m_descending = false;
  m_pruneState= SPS_UNKNOWN;
  Uint32 fragCount = m_currentTable->m_fragmentCount;

  assert(fragCount > 0);
  
  if (parallel > fragCount || parallel == 0) {
     parallel = fragCount;
  }

  theNdbCon->theScanningOp = this;
  bool tupScan = (scan_flags & SF_TupScan);

#if 0 // XXX temp for testing
  { char* p = getenv("NDB_USE_TUPSCAN");
    if (p != 0) {
      unsigned n = atoi(p); // 0-10
      if ((unsigned int) (::time(0) % 10) < n) tupScan = true;
    }
  }
#endif
  if (scan_flags & SF_DiskScan)
  {
    tupScan = true;
    m_flags &= ~Uint8(OF_NO_DISK);
  }
  
  bool rangeScan= false;

  /* NdbRecord defined scan, handle IndexScan specifics */
  if ( (int) m_accessTable->m_indexType ==
       (int) NdbDictionary::Index::OrderedIndex )
  {
    if (m_currentTable == m_accessTable){
      // Old way of scanning indexes, should not be allowed
      m_currentTable = theNdb->theDictionary->
        getTable(m_currentTable->m_primaryTable.c_str());
      assert(m_currentTable != NULL);
    }
    assert (m_currentTable != m_accessTable);
    // Modify operation state
    theStatus = GetValue;
    theOperationType  = OpenRangeScanRequest;
    rangeScan = true;
    tupScan = false;
  }
  
  if (rangeScan && (scan_flags & (SF_OrderBy | SF_OrderByFull)))
    parallel = fragCount; /* Frag count of ordered index ==
                           * Frag count of base table
                           */
  
  theParallelism = parallel;    
  
  if(fix_receivers(parallel) == -1){
    setErrorCodeAbort(4000);
    return -1;
  }
  
  if (theSCAN_TABREQ == NULL) {
    setErrorCodeAbort(4000);
    return -1;
  }//if
  
  NdbImpl* impl = theNdb->theImpl;
  Uint32 nodeId = theNdbCon->theDBnode;
  Uint32 nodeVersion = impl->getNodeNdbVersion(nodeId);
  theSCAN_TABREQ->setSignal(GSN_SCAN_TABREQ, refToBlock(theNdbCon->m_tcRef));
  ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  req->apiConnectPtr = theNdbCon->theTCConPtr;
  req->tableId = m_accessTable->m_id;
  req->tableSchemaVersion = m_accessTable->m_version;
  req->storedProcId = 0xFFFF;
  req->buddyConPtr = theNdbCon->theBuddyConPtr;
  req->spare= 0;
  req->first_batch_size = batch; // Save user specified batch size
  
  Uint32 reqInfo = 0;
  if (!ndbd_scan_tabreq_implicit_parallelism(nodeVersion))
  {
    // Implicit parallelism implies support for greater
    // parallelism than storable explicitly in old reqInfo.
    if (parallel > PARALLEL_MASK)
    {
      setErrorCodeAbort(4000 /* TODO: TooManyFragments, to too old cluster version */);
      return -1;
    }
    ScanTabReq::setParallelism(reqInfo, parallel);
  }
  ScanTabReq::setScanBatch(reqInfo, 0);
  ScanTabReq::setRangeScanFlag(reqInfo, rangeScan);
  ScanTabReq::setTupScanFlag(reqInfo, tupScan);
  req->requestInfo = reqInfo;

  m_keyInfo = (scan_flags & SF_KeyInfo) ? 1 : 0;
  setReadLockMode(lm);

  Uint64 transId = theNdbCon->getTransactionId();
  req->transId1 = (Uint32) transId;
  req->transId2 = (Uint32) (transId >> 32);

  assert(theSCAN_TABREQ->next() == NULL);
  NdbApiSignal* tSignal= theNdb->getSignal();
  theSCAN_TABREQ->next(tSignal);
  theLastKEYINFO = tSignal;
  
  theKEYINFOptr= tSignal->getDataPtrSend();
  keyInfoRemain= NdbApiSignal::MaxSignalWords;
  theTotalNrOfKeyWordInSignal= 0;

  getFirstATTRINFOScan();
  return 0;
}

int
NdbScanOperation::setInterpretedCode(const NdbInterpretedCode *code)
{
  if (theStatus == NdbOperation::UseNdbRecord)
  {
    setErrorCodeAbort(4284); // Cannot mix NdbRecAttr and NdbRecord methods...
    return -1;
  }

  if ((code->m_flags & NdbInterpretedCode::Finalised) == 0)
  {
    setErrorCodeAbort(4519); //  NdbInterpretedCode::finalise() not called.
    return -1;
  }

  m_interpreted_code= code;
  
  return 0;
}

NdbInterpretedCode*
NdbScanOperation::allocInterpretedCodeOldApi()
{
  /* Should only be called once */
  assert (m_interpretedCodeOldApi == NULL);

  /* Old Api scans only */
  if (! m_scanUsingOldApi)
  {
    /* NdbScanFilter constructor taking NdbOperation is not 
     * supported for NdbRecord
     */
    setErrorCodeAbort(4536);
    return NULL;
  }

  m_interpretedCodeOldApi = new NdbInterpretedCode(m_currentTable->m_facade);

  if (m_interpretedCodeOldApi == NULL)
    setErrorCodeAbort(4000); // Memory allocation error

  return m_interpretedCodeOldApi;
}

void
NdbScanOperation::freeInterpretedCodeOldApi()
{
  if (m_interpretedCodeOldApi != NULL)
  {
    delete m_interpretedCodeOldApi;
    m_interpretedCodeOldApi= NULL;
  }
}


void
NdbScanOperation::setReadLockMode(LockMode lockMode)
{
  bool lockExcl, lockHoldMode, readCommitted;
  switch (lockMode)
  {
    case LM_CommittedRead:
      lockExcl= false;
      lockHoldMode= false;
      readCommitted= true;
      break;
    case LM_SimpleRead:
    case LM_Read:
      lockExcl= false;
      lockHoldMode= true;
      readCommitted= false;
      break;
    case LM_Exclusive:
      lockExcl= true;
      lockHoldMode= true;
      readCommitted= false;
      m_keyInfo= 1;
      break;
    default:
      /* Not supported / invalid. */
      assert(false);
  }
  theLockMode= lockMode;
  ScanTabReq *req= CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  Uint32 reqInfo= req->requestInfo;
  ScanTabReq::setLockMode(reqInfo, lockExcl);
  ScanTabReq::setHoldLockFlag(reqInfo, lockHoldMode);
  ScanTabReq::setReadCommittedFlag(reqInfo, readCommitted);
  req->requestInfo= reqInfo;
}

int
NdbScanOperation::fix_receivers(Uint32 parallel){
  assert(parallel > 0);
  if(parallel > m_allocated_receivers){
    const Uint32 sz = parallel * (4*sizeof(char*)+sizeof(Uint32));

    /* Allocate as Uint64 to ensure proper alignment for pointers. */
    Uint64 * tmp = new Uint64[(sz+7)/8];
    if (tmp == NULL)
    {
      setErrorCodeAbort(4000);
      return -1;
    }
    // Save old receivers
    memcpy(tmp, m_receivers, m_allocated_receivers*sizeof(char*));
    delete[] m_array;
    m_array = (Uint32*)tmp;
    
    m_receivers = (NdbReceiver**)tmp;
    m_api_receivers = m_receivers + parallel;
    m_conf_receivers = m_api_receivers + parallel;
    m_sent_receivers = m_conf_receivers + parallel;
    m_prepared_receivers = (Uint32*)(m_sent_receivers + parallel);

    // Only get/init "new" receivers
    NdbReceiver* tScanRec;
    for (Uint32 i = m_allocated_receivers; i < parallel; i ++) {
      tScanRec = theNdb->getNdbScanRec();
      if (tScanRec == NULL) {
        setErrorCodeAbort(4000);
        return -1;
      }//if
      m_receivers[i] = tScanRec;
      tScanRec->init(NdbReceiver::NDB_SCANRECEIVER, this);
    }
    m_allocated_receivers = parallel;
  }
  
  reset_receivers(parallel, 0);
  return 0;
}

/**
 * Move receiver from send array to conf:ed array
 */
void
NdbScanOperation::receiver_delivered(NdbReceiver* tRec){
  if(theError.code == 0){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("receiver_delivered");
    
    Uint32 idx = tRec->m_list_index;
    Uint32 last = m_sent_receivers_count - 1;
    if(idx != last){
      NdbReceiver * move = m_sent_receivers[last];
      m_sent_receivers[idx] = move;
      move->m_list_index = idx;
    }
    m_sent_receivers_count = last;
    
    last = m_conf_receivers_count;
    m_conf_receivers[last] = tRec;
    m_conf_receivers_count = last + 1;
  }
}

/**
 * Remove receiver as it's completed
 */
void
NdbScanOperation::receiver_completed(NdbReceiver* tRec){
  if(theError.code == 0){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("receiver_completed");
    
    Uint32 idx = tRec->m_list_index;
    Uint32 last = m_sent_receivers_count - 1;
    if(idx != last){
      NdbReceiver * move = m_sent_receivers[last];
      m_sent_receivers[idx] = move;
      move->m_list_index = idx;
    }
    m_sent_receivers_count = last;
  }
}

/*****************************************************************************
 * int getFirstATTRINFOScan()
 *
 * Return Value:  Return 0:   Successful
 *                Return -1:  All other cases
 * Parameters:    None:            Only allocate the first signal.
 * Remark:        When a scan is defined we need to use this method instead 
 *                of insertATTRINFO for the first signal. 
 *                This is because we need not to mess up the code in 
 *                insertATTRINFO with if statements since we are not 
 *                interested in the TCKEYREQ signal.
 *****************************************************************************/
int
NdbScanOperation::getFirstATTRINFOScan()
{
  NdbApiSignal* tSignal;

  tSignal = theNdb->getSignal();
  if (tSignal == NULL){
    setErrorCodeAbort(4000);      
    return -1;    
  }

  theAI_LenInCurrAI = AttrInfo::SectionSizeInfoLength;
  theATTRINFOptr = &tSignal->getDataPtrSend()[AttrInfo::SectionSizeInfoLength];
  attrInfoRemain= NdbApiSignal::MaxSignalWords - AttrInfo::SectionSizeInfoLength;
  tSignal->setLength(AttrInfo::SectionSizeInfoLength);
  theFirstATTRINFO = tSignal;
  theCurrentATTRINFO = tSignal;
  theCurrentATTRINFO->next(NULL);

  return 0;
}

int
NdbScanOperation::executeCursor(int nodeId)
{
  /*
   * Call finaliseScanOldApi() for old style scans before
   * proceeding
   */  
  bool locked = false;
  NdbImpl* theImpl = theNdb->theImpl;

  int res = 0;
  if (m_scanUsingOldApi && finaliseScanOldApi() == -1)
  {
    res = -1;
    goto done;
  }

  {
    locked = true;
    NdbTransaction * tCon = theNdbCon;
    theImpl->lock();
    
    Uint32 seq = tCon->theNodeSequence;
    
    if (theImpl->get_node_alive(nodeId) &&
        (theImpl->getNodeSequence(nodeId) == seq)) {
      
      tCon->theMagicNumber = 0x37412619;
      
      if (doSendScan(nodeId) == -1)
      {
        res = -1;
        goto done;
      }
      
      m_executed= true; // Mark operation as executed
    } 
    else
    {
      if (!(theImpl->get_node_stopping(nodeId) &&
            (theImpl->getNodeSequence(nodeId) == seq)))
      {
        TRACE_DEBUG("The node is hard dead when attempting to start a scan");
        setErrorCode(4029);
        tCon->theReleaseOnClose = true;
      } 
      else 
      {
        TRACE_DEBUG("The node is stopping when attempting to start a scan");
        setErrorCode(4030);
      }//if
      res = -1;
      tCon->theCommitStatus = NdbTransaction::Aborted;
    }//if
  }

done:
    /**
   * Set pointers correctly
   *   so that nextResult will handle it correctly
   *   even if doSendScan was never called
   *   bug#42454
   */
  m_curr_row = 0;
  m_sent_receivers_count = theParallelism;
  if(m_ordered)
  {
    m_current_api_receiver = theParallelism;
    m_api_receivers_count = theParallelism;
  }

  if (locked)
    theImpl->unlock();

  return res;
}


int 
NdbScanOperation::nextResult(bool fetchAllowed, bool forceSend)
{
  /* Defer to NdbRecord implementation, which will copy values
   * out into the user's RecAttr objects.
   */
  const char * dummyOutRowPtr;

  if (unlikely(! m_scanUsingOldApi))
  {
    /* Cannot mix NdbRecAttr and NdbRecord methods in one operation */
    setErrorCode(4284);
    return -1;
  }

  return nextResult(&dummyOutRowPtr,
                    fetchAllowed,
                    forceSend);
}

/* nextResult() for NdbRecord operation. */
int
NdbScanOperation::nextResult(const char ** out_row_ptr,
                             bool fetchAllowed, bool forceSend)
{
  int res;

  if ((res = nextResultNdbRecord(*out_row_ptr, fetchAllowed, forceSend)) == 0)
  {
    NdbBlob* tBlob= theBlobList;
    NdbRecAttr *getvalue_recattr= theReceiver.m_firstRecAttr;
    if (((UintPtr)tBlob | (UintPtr)getvalue_recattr) != 0)
    {
      const Uint32 idx= m_current_api_receiver;
      assert(idx < m_api_receivers_count);
      const NdbReceiver *receiver= m_api_receivers[idx];

      /* First take care of any getValue(). */
      if (getvalue_recattr != NULL)
      {
        if (receiver->get_AttrValues(getvalue_recattr) == -1)
          return -1;
      }

      /* Handle blobs. */
      if (tBlob)
      {
        Uint32 infoword;                          // Not used for blobs
        Uint32 key_length;
        const char *key_data;
        res= receiver->get_keyinfo20(infoword, key_length, key_data);
        if (res == -1)
          return -1;

        do
        {
          if (tBlob->atNextResultNdbRecord(key_data, key_length*4) == -1)
            return -1;
          tBlob= tBlob->theNext;
        } while (tBlob != 0);
        /* Flush blob part ops on behalf of user. */
        if (m_transConnection->executePendingBlobOps() == -1)
          return -1;
      }
    }
    return 0;
  }
  return res;
}

int
NdbScanOperation::nextResultCopyOut(char * buffer,
                                    bool fetchAllowed, bool forceSend)
{
  const char * data;
  int result;
  if ((result = nextResult(&data, fetchAllowed, forceSend)) == 0)
  {
    memcpy(buffer, data, m_attribute_record->m_row_size);
  }
  return result;
}

int
NdbScanOperation::nextResultNdbRecord(const char * & out_row,
                                      bool fetchAllowed, bool forceSend)
{
  if (m_ordered)
  {
    return ((NdbIndexScanOperation*)this)->next_result_ordered_ndbrecord
      (out_row, fetchAllowed, forceSend);
  }

  /* Return a row immediately if any is available. */
  while (m_current_api_receiver < m_api_receivers_count)
  {
    NdbReceiver *tRec= m_api_receivers[m_current_api_receiver];
    out_row = tRec->getNextRow();
    if (out_row != NULL)
    {
      return 0;
    }
    m_current_api_receiver++;
  }

  if (!fetchAllowed)
  {
    /*
      Application wants to be informed that no more rows are available
      immediately.
    */
    return 2;
  }

  /* Now we have to wait for more rows (or end-of-file on all receivers). */
  Uint32 nodeId = theNdbCon->theDBnode;
  NdbImpl* theImpl = theNdb->theImpl;
  Uint32 timeout= theImpl->get_waitfor_timeout();
  int retVal= 2;
  Uint32 idx, last;
  /*
    The rest needs to be done under mutex due to synchronization with receiver
    thread.
  */
  PollGuard poll_guard(* theImpl);

  const Uint32 seq= theNdbCon->theNodeSequence;

  if(theError.code)
  {
    /**
     * The scan is already complete (Err_scanAlreadyComplete)
     * or is in some error.
     *
     * Either there is a bug in the api application such that
     * it calls nextResult()/nextResultNdbRecord() again
     * after getting return value 1 (meaning end of scan) or
     * -1 (for error).
     *
     * Or there seems to be a bug in ndbapi that put operation
     * in error between calls.
     *
     * Or an error have been received.
     *
     * In any case, keep and propagate error and fail.
     */
    if (theError.code != Err_scanAlreadyComplete)
      setErrorCode(theError.code);
    return -1;
  }

  if(seq == theImpl->getNodeSequence(nodeId) &&
     send_next_scan(m_current_api_receiver, false) == 0)
  {
    idx= m_current_api_receiver;
    last= m_api_receivers_count;

    do {
      if (theError.code){
        setErrorCode(theError.code);
        return -1;
      }

      Uint32 cnt= m_conf_receivers_count;
      Uint32 sent= m_sent_receivers_count;

      if (cnt > 0)
      {
        /* New receivers with completed batches available. */
        memcpy(m_api_receivers+last, m_conf_receivers, cnt * sizeof(char*));
        last+= cnt;
        theImpl->incClientStat(Ndb::ScanBatchCount, cnt);
        m_conf_receivers_count= 0;
      }
      else if (retVal == 2 && sent > 0)
      {
        /* No completed... */
        theImpl->incClientStat(Ndb::WaitScanResultCount, 1);
        
        int ret_code= poll_guard.wait_scan(3*timeout, nodeId, forceSend);
        if (ret_code == 0 && seq == theImpl->getNodeSequence(nodeId)) {
          continue;
        } else if(ret_code == -1){
          retVal= -1;
        } else {
          idx= last;
          retVal= -2; //return_code;
        }
      }
      else if (retVal == 2)
      {
        /**
         * No completed & no sent -> EndOfData
         * Make sure user gets error if he tries again.
         */
        theError.code= Err_scanAlreadyComplete;
        return 1;
      }

      if (retVal == 0)
        break;

      while (idx < last)
      {
        NdbReceiver* tRec= m_api_receivers[idx];
        if ((out_row = tRec->getNextRow()) != NULL)
        {
          retVal= 0;
          break;
        }
        idx++;
      }
    } while(retVal == 2);

    m_api_receivers_count= last;
    m_current_api_receiver= idx;
  } else {
    retVal = -3;
  }

  switch(retVal)
  {
  case 0:
  case 1:
  case 2:
    return retVal;
  case -1:
    setErrorCode(4008); // Timeout
    break;
  case -2:
    setErrorCode(4028); // Node fail
    break;
  case -3: // send_next_scan -> return fail (set error-code self)
    if(theError.code == 0)
      setErrorCode(4028); // seq changed = Node fail
    break;
  }

  theNdbCon->theTransactionIsStarted= false;
  theNdbCon->theReleaseOnClose= true;
  return -1;
}

int
NdbScanOperation::send_next_scan(Uint32 cnt, bool stopScanFlag)
{
  if(cnt > 0){
    NdbApiSignal tSignal(theNdb->theMyRef);
    tSignal.setSignal(GSN_SCAN_NEXTREQ, refToBlock(theNdbCon->m_tcRef));
    
    Uint32* theData = tSignal.getDataPtrSend();
    theData[0] = theNdbCon->theTCConPtr;
    theData[1] = stopScanFlag == true ? 1 : 0;
    Uint64 transId = theNdbCon->theTransactionId;
    theData[2] = (Uint32) transId;
    theData[3] = (Uint32) (transId >> 32);
    
    /**
     * Prepare ops
     */
    Uint32 last = m_sent_receivers_count;
    Uint32 * prep_array = (cnt > 21 ? m_prepared_receivers : theData + 4);
    Uint32 sent = 0;
    for(Uint32 i = 0; i<cnt; i++){
      NdbReceiver * tRec = m_api_receivers[i];
      if((prep_array[sent] = tRec->m_tcPtrI) != RNIL)
      {
        m_sent_receivers[last+sent] = tRec;
        tRec->m_list_index = last+sent;
        tRec->prepareSend();
        sent++;
      }
    }
    memmove(m_api_receivers, m_api_receivers+cnt, 
            (theParallelism-cnt) * sizeof(char*));
    
    int ret = 0;
    if(sent)
    {
      Uint32 nodeId = theNdbCon->theDBnode;
      NdbImpl* impl = theNdb->theImpl;
      if(cnt > 21){
        tSignal.setLength(4);
        LinearSectionPtr ptr[3];
        ptr[0].p = prep_array;
        ptr[0].sz = sent;
        ret = impl->sendSignal(&tSignal, nodeId, ptr, 1);
      } else {
        tSignal.setLength(4+sent);
        ret = impl->sendSignal(&tSignal, nodeId);
      }
    }
    m_sent_receivers_count = last + sent;
    m_api_receivers_count -= cnt;
    m_current_api_receiver = 0;
    
    return ret;
  }
  return 0;
}

int 
NdbScanOperation::prepareSend(Uint32  TC_ConnectPtr,
                              Uint64  TransactionId,
                              NdbOperation::AbortOption)
{
  abort();
  return 0;
}

int 
NdbScanOperation::doSend(int ProcessorId)
{
  return 0;
}

void NdbScanOperation::close(bool forceSend, bool releaseOp)
{
  DBUG_ENTER("NdbScanOperation::close");
  DBUG_PRINT("enter", ("this: 0x%lx  tcon: 0x%lx  con: 0x%lx  force: %d  release: %d",
                       (long) this,
                       (long) m_transConnection, (long) theNdbCon,
                       forceSend, releaseOp));

  if(m_transConnection){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("close() theError.code = %d "
               "m_api_receivers_count = %d "
               "m_conf_receivers_count = %d "
               "m_sent_receivers_count = %d",
               theError.code, 
               m_api_receivers_count,
               m_conf_receivers_count,
               m_sent_receivers_count);
    
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(* theNdb->theImpl);
    close_impl(forceSend, &poll_guard);
  }

  /* Free buffer used to store scan result set.
   * Result set lifetime ends when the cursor is closed.
   */
  if (m_scan_buffer)
  {
    delete[] m_scan_buffer;
    m_scan_buffer= NULL;
  }

  // Keep in local variables, as "this" might be destructed below
  NdbConnection* tCon = theNdbCon;
  NdbConnection* tTransCon = m_transConnection;
  Ndb* tNdb = theNdb;

  theNdbCon = NULL;
  m_transConnection = NULL;

  if (tTransCon && releaseOp) 
  {
    NdbIndexScanOperation* tOp = (NdbIndexScanOperation*)this;

    bool ret = true;
    if (theStatus != WaitResponse)
    {
      /**
       * Not executed yet
       */
      ret = 
        tTransCon->releaseScanOperation(&tTransCon->m_theFirstScanOperation,
                                        &tTransCon->m_theLastScanOperation,
                                        tOp);
    }
    else
    {
      ret = tTransCon->releaseScanOperation(&tTransCon->m_firstExecutedScanOp,
                                            0, tOp);
    }
    assert(ret);
  }
  
  tCon->theScanningOp = 0;
  tNdb->closeTransaction(tCon);
  tNdb->theImpl->decClientStat(Ndb::TransCloseCount, 1); /* Correct stats */
  tNdb->theRemainingStartTransactions--;
  DBUG_VOID_RETURN;
}

void
NdbScanOperation::execCLOSE_SCAN_REP(){
  m_conf_receivers_count = 0;
  m_sent_receivers_count = 0;
}

void NdbScanOperation::release()
{
  if(theNdbCon != 0 || m_transConnection != 0){
    close();
  }
  for(Uint32 i = 0; i<m_allocated_receivers; i++){
    m_receivers[i]->release();
  }
  if (m_scan_buffer)
  {
    delete[] m_scan_buffer;
    m_scan_buffer= NULL;
  }

  NdbOperation::release();
  
  if(theSCAN_TABREQ)
  {
    theNdb->releaseSignal(theSCAN_TABREQ);
    theSCAN_TABREQ = 0;
  }
}

/*
 * This method finalises an Old API defined scan
 * This is done just prior to scan execution
 * The parameters provided via the RecAttr scan interface are
 * used to create an NdbRecord based scan
 */
int NdbScanOperation::finaliseScanOldApi()
{
  /* For a scan we use an NdbRecord structure for this
   * table, and add the user-requested values in a similar
   * way to the extra GetValues mechanism
   */
  assert(theOperationType == OpenScanRequest ||
         theOperationType == OpenRangeScanRequest);

  /* Prepare ScanOptions structure using saved parameters */
  ScanOptions options;
  options.optionsPresent=(ScanOptions::SO_SCANFLAGS |
                          ScanOptions::SO_PARALLEL |
                          ScanOptions::SO_BATCH);

  options.scan_flags= m_savedScanFlagsOldApi;
  options.parallel= m_savedParallelOldApi;
  options.batch= m_savedBatchOldApi;

  if (theDistrKeyIndicator_ == 1)
  {
    /* User has defined a partition id specifically */
    options.optionsPresent |= ScanOptions::SO_PARTITION_ID;
    options.partitionId= theDistributionKey;
  }

  /* customData or interpretedCode should 
   * already be set in the operation members - no need 
   * to pass in as ScanOptions
   */

  /* Next, call scanTable, passing in some of the 
   * parameters we saved
   * It will look after building the correct signals
   */
  int result= -1;

  const unsigned char* emptyMask= 
    (const unsigned char*) NdbDictionaryImpl::m_emptyMask;

  if (theOperationType == OpenScanRequest)
    /* Create table scan operation with an empty
     * mask for NdbRecord values
     */
    result= scanTableImpl(m_currentTable->m_ndbrecord,
                          m_savedLockModeOldApi,
                          emptyMask,
                          &options,
                          sizeof(ScanOptions));
  else
  {
    assert(theOperationType == OpenRangeScanRequest);
    NdbIndexScanOperation *isop = 
      static_cast<NdbIndexScanOperation*>(this);

    if (isop->currentRangeOldApi != NULL)
    {
      /* Add current bound to bound list */
      if (isop->buildIndexBoundOldApi(0) != 0)
        return -1;
    }
    
    /* If this is an ordered scan, then we need
     * the pk columns in the mask, otherwise we
     * don't
     */
    const unsigned char * resultMask= 
      ((m_savedScanFlagsOldApi & (SF_OrderBy | SF_OrderByFull)) !=0) ? 
      m_accessTable->m_pkMask : 
      emptyMask;

    result= isop->scanIndexImpl(m_accessTable->m_ndbrecord,
                                m_currentTable->m_ndbrecord,
                                m_savedLockModeOldApi,
                                resultMask,
                                NULL, // All bounds added below
                                &options,
                                sizeof(ScanOptions));

    /* Add any bounds that were specified */
    if (isop->firstRangeOldApi != NULL)
    {
      NdbRecAttr* bound= isop->firstRangeOldApi;
      while (bound != NULL)
      {
        if (isop->setBound( m_accessTable->m_ndbrecord,
                            *isop->getIndexBoundFromRecAttr(bound) ) != 0)
          return -1;
        
        bound= bound->next();
      }
    }

    isop->releaseIndexBoundsOldApi();
  }

  /* Free any scan-owned ScanFilter generated InterpretedCode
   * object
   */
  freeInterpretedCodeOldApi();

  return result;
}

/***************************************************************************
int prepareSendScan(Uint32 aTC_ConnectPtr,
                    Uint64 aTransactionId,
                    const Uint32 * readMask)

Return Value:   Return 0 : preparation of send was succesful.
                Return -1: In all other case.   
Parameters:     aTC_ConnectPtr: the Connect pointer to TC.
                aTransactionId: the Transaction identity of the transaction.
Remark:         Puts the the final data into ATTRINFO signal(s)  after this 
                we know the how many signal to send and their sizes
***************************************************************************/
int NdbScanOperation::prepareSendScan(Uint32 aTC_ConnectPtr,
                                      Uint64 aTransactionId,
                                      const Uint32 * readMask)
{
  if (theInterpretIndicator != 1 ||
      (theOperationType != OpenScanRequest &&
       theOperationType != OpenRangeScanRequest)) {
    setErrorCodeAbort(4005);
    return -1;
  }

  theErrorLine = 0;

  /* All scans use NdbRecord at this stage */
  assert(m_attribute_record);

  /**
   * Prepare all receivers
   */
  theReceiver.prepareSend();
  bool keyInfo = m_keyInfo;
  Uint32 key_size= keyInfo ? m_attribute_record->m_keyLenInWords : 0;

  /**
   * The number of records sent by each LQH is calculated and the kernel
   * is informed of this number by updating the SCAN_TABREQ signal
   */
  ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  Uint32 batch_size = req->first_batch_size; // User specified
  Uint32 batch_byte_size;
  theReceiver.calculate_batch_size(theParallelism,
                                   batch_size,
                                   batch_byte_size);
  ScanTabReq::setScanBatch(req->requestInfo, batch_size);
  req->batch_byte_size= batch_byte_size;
  req->first_batch_size= batch_size;

  /**
   * Set keyinfo, nodisk and distribution key flags in 
   * ScanTabReq
   *  (Always request keyinfo when using blobs)
   */
  Uint32 reqInfo = req->requestInfo;
  ScanTabReq::setKeyinfoFlag(reqInfo, keyInfo);
  ScanTabReq::setNoDiskFlag(reqInfo, (m_flags & OF_NO_DISK) != 0);

  /* Set distribution key info if required */
  ScanTabReq::setDistributionKeyFlag(reqInfo, theDistrKeyIndicator_);
  req->requestInfo = reqInfo;
  req->distributionKey= theDistributionKey;
  theSCAN_TABREQ->setLength(ScanTabReq::StaticLength + theDistrKeyIndicator_);

  /* All scans use NdbRecord internally */
  assert(theStatus == UseNdbRecord);
  

  /**
   * Calculate memory req. for the NdbReceiverBuffer and its row buffer:
   *
   * Scan results are stored into a buffer in a 'packed' format
   * by the NdbReceiver. When each row is fetched (made 'current'),
   * NdbReceiver unpack it into a row buffer as specified by the
   * NdbRecord argument (and RecAttrs are put into their destination)
   */
  Uint32 bufsize= NdbReceiver::result_bufsize(batch_size,
                                              batch_byte_size,
                                              1,
                                              m_attribute_record,
                                              readMask,
                                              theReceiver.m_firstRecAttr,
                                              key_size,
                                              m_read_range_no);
  assert((bufsize % sizeof(Uint32)) == 0); //Size returned as Uint32 aligned

  /* Calculate row buffer size, align it for (hopefully) improved memory access.  */
  Uint32 full_rowsize= NdbReceiver::ndbrecord_rowsize(m_attribute_record,
                                                 m_read_range_no);

  /**
   * Alloc total buffers for all fragments in one big chunk. 
   * Alloced as Uint32 to fullfil alignment req for NdbReceiveBuffers.
   */
  assert(theParallelism > 0);
  const Uint32 alloc_size = ((full_rowsize+bufsize)*theParallelism) / sizeof(Uint32);
  Uint32 *buf= new Uint32[alloc_size];
  if (!buf)
  {
    setErrorCodeAbort(4000); // "Memory allocation error"
    return -1;
  }
  assert(!m_scan_buffer);
  m_scan_buffer= buf;
  
  for (Uint32 i = 0; i<theParallelism; i++)
  {
    m_receivers[i]->do_setup_ndbrecord(m_attribute_record, 
                                       reinterpret_cast<char*>(buf),
                                       m_read_range_no, (key_size > 0));
    buf+= full_rowsize/sizeof(Uint32);

    NdbReceiverBuffer* recbuf =
    NdbReceiver::initReceiveBuffer(buf,
                                   bufsize, batch_size);

    m_receivers[i]->prepareReceive(recbuf);
    buf+= bufsize/sizeof(Uint32);
  }

  /* Update ATTRINFO section sizes info */
  if (doSendSetAISectionSizes() == -1)
    return -1;

  return 0;
}

int
NdbScanOperation::doSendSetAISectionSizes()
{
  // Set the scan AI section sizes.
  Uint32* sectionSizesPtr= theFirstATTRINFO->getDataPtrSend();
  *sectionSizesPtr++ = theInitialReadSize;
  *sectionSizesPtr++ = theInterpretedSize;
  *sectionSizesPtr++ = 0; // Update size 
  *sectionSizesPtr++ = 0; // Final read size
  *sectionSizesPtr   = theSubroutineSize;

  return 0;
}


/*****************************************************************************
int doSendScan()

Return Value:   Return >0 : send was succesful, returns number of signals sent
                Return -1: In all other case.   
Parameters:     aProcessorId: Receiving processor node
Remark:         Sends the ATTRINFO signal(s)
*****************************************************************************/
int
NdbScanOperation::doSendScan(int aProcessorId)
{
  if (theInterpretIndicator != 1 ||
      (theOperationType != OpenScanRequest &&
       theOperationType != OpenRangeScanRequest)) {
      setErrorCodeAbort(4005);
      return -1;
  }
  
  assert(theSCAN_TABREQ != NULL);
  
  /* Check that we don't have too much AttrInfo */
  if (unlikely(theTotalCurrAI_Len > ScanTabReq::MaxTotalAttrInfo)) {
    setErrorCode(4257);
    return -1;
  }

  /* SCANTABREQ always has 2 mandatory sections and an optional
   * third section
   * Section 0 : List of receiver Ids NDBAPI has allocated 
   *             for the scan
   * Section 1 : ATTRINFO section
   * Section 2 : Optional KEYINFO section
   */
  GenericSectionPtr secs[3];
  LinearSectionIterator receiverIdIterator(m_prepared_receivers,
                                           theParallelism);
  SignalSectionIterator attrInfoIter(theFirstATTRINFO);
  SignalSectionIterator keyInfoIter(theSCAN_TABREQ->next());

  secs[0].sectionIter= &receiverIdIterator;
  secs[0].sz= theParallelism;

  secs[1].sectionIter= &attrInfoIter;
  secs[1].sz= theTotalCurrAI_Len;

  Uint32 numSections= 2;

  if (theTupKeyLen)
  {
    secs[2].sectionIter= &keyInfoIter;
    secs[2].sz= theTupKeyLen;
    numSections= 3;
  }

  NdbImpl* impl = theNdb->theImpl;
  {
    const Ndb::ClientStatistics counterIndex = (numSections == 3)? 
      Ndb::RangeScanCount : 
      Ndb::TableScanCount;
    impl->incClientStat(counterIndex, 1);
    if (getPruned())
      impl->incClientStat(Ndb::PrunedScanCount, 1);
  }
  Uint32 tcNodeVersion = impl->getNodeNdbVersion(aProcessorId);
  bool forceShort = impl->forceShortRequests;
  bool sendLong = ( tcNodeVersion >= NDBD_LONG_SCANTABREQ) &&
    ! forceShort;
  
  if (sendLong)
  {
    /* Send Fragmented as SCAN_TABREQ can be large */
    if (impl->sendFragmentedSignal(theSCAN_TABREQ,
                                   aProcessorId,
                                   &secs[0],
                                   numSections) == -1)
    {
      setErrorCode(4002);
      return -1;
    }
  }
  else
  {
    /* Send a 'short' SCANTABREQ - e.g. long SCANTABREQ
     * with signalIds as first section, followed by
     * AttrInfo and KeyInfo trains
     */
    Uint32 attrInfoLen = secs[1].sz;
    Uint32 keyInfoLen = (numSections == 3)? secs[2].sz : 0;

    ScanTabReq* scanTabReq = (ScanTabReq*) theSCAN_TABREQ->getDataPtrSend();
    Uint32 connectPtr = scanTabReq->apiConnectPtr;
    Uint32 transId1 = scanTabReq->transId1;
    Uint32 transId2 = scanTabReq->transId2;

    /* Modify ScanTabReq to carry length of keyinfo and attrinfo */
    scanTabReq->attrLenKeyLen = (keyInfoLen << 16) | attrInfoLen;

    /* Send with receiver Ids as first and only section */
    if (impl->sendSignal(theSCAN_TABREQ, aProcessorId, &secs[0], 1) == -1)
    {
      setErrorCode(4002);
      return -1;
    }

    if (keyInfoLen)
    {
      GSIReader keyInfoReader(secs[2].sectionIter);
      theSCAN_TABREQ->theVerId_signalNumber = GSN_KEYINFO;
      KeyInfo* keyInfo = (KeyInfo*) theSCAN_TABREQ->getDataPtrSend();
      keyInfo->connectPtr = connectPtr;
      keyInfo->transId[0] = transId1;
      keyInfo->transId[1] = transId2;

      while(keyInfoLen)
      {
        Uint32 dataWords = MIN(keyInfoLen, KeyInfo::DataLength);
        keyInfoReader.copyNWords(&keyInfo->keyData[0], dataWords);
        theSCAN_TABREQ->setLength(KeyInfo::HeaderLength + dataWords);

        if (impl->sendSignal(theSCAN_TABREQ, aProcessorId) == -1)
        {
          setErrorCode(4002);
          return -1;
        }
        keyInfoLen -= dataWords;
      }
    }

    GSIReader attrInfoReader(secs[1].sectionIter);
    theSCAN_TABREQ->theVerId_signalNumber = GSN_ATTRINFO;
    AttrInfo* attrInfo = (AttrInfo*) theSCAN_TABREQ->getDataPtrSend();
    attrInfo->connectPtr = connectPtr;
    attrInfo->transId[0] = transId1;
    attrInfo->transId[1] = transId2;
    
    while(attrInfoLen)
    {
      Uint32 dataWords = MIN(attrInfoLen, AttrInfo::DataLength);
      attrInfoReader.copyNWords(&attrInfo->attrData[0], dataWords);
      theSCAN_TABREQ->setLength(AttrInfo::HeaderLength + dataWords);

      if (impl->sendSignal(theSCAN_TABREQ, aProcessorId) == -1)
      {
        setErrorCode(4002);
        return -1;
      }
      attrInfoLen -= dataWords;
    }
  }

  theStatus = WaitResponse;  
  return 1; // 1 signal sent
}//NdbOperation::doSendScan()


/* This method retrieves a pointer to the keyinfo for the current
 * row - it is used when creating a scan takeover operation
 */
int
NdbScanOperation::getKeyFromKEYINFO20(Uint32* data, Uint32 & size)
{
  NdbRecAttr * tRecAttr = m_curr_row;
  if(tRecAttr)
  {
    const Uint32 * src = (Uint32*)tRecAttr->aRef();

    assert(tRecAttr->get_size_in_bytes() > 0);
    assert(tRecAttr->get_size_in_bytes() < 65536);
    const Uint32 len = (tRecAttr->get_size_in_bytes() + 3)/4-1;

    assert(size >= len);
    memcpy(data, src, 4*len);
    size = len;
    return 0;
  }
  return -1;
}

/*****************************************************************************
 * NdbOperation* takeOverScanOp(NdbTransaction* updateTrans);
 *
 * Parameters:     The update transactions NdbTransaction pointer.
 * Return Value:   A reference to the transferred operation object 
 *                   or NULL if no success.
 * Remark:         Take over the scanning transactions NdbOperation 
 *                 object for a tuple to an update transaction, 
 *                 which is the last operation read in nextScanResult()
 *                 (theNdbCon->thePreviousScanRec)
 *
 *     FUTURE IMPLEMENTATION:   (This note was moved from header file.)
 *     In the future, it will even be possible to transfer 
 *     to a NdbTransaction on another Ndb-object.  
 *     In this case the receiving NdbTransaction-object must call 
 *     a method receiveOpFromScan to actually receive the information.  
 *     This means that the updating transactions can be placed
 *     in separate threads and thus increasing the parallelism during
 *     the scan process. 
 ****************************************************************************/
NdbOperation*
NdbScanOperation::takeOverScanOp(OperationType opType, NdbTransaction* pTrans)
{
  if (!m_scanUsingOldApi)
  {
    setErrorCodeAbort(4284);
    return NULL;
  }

  if (!m_keyInfo)
  {
    // Cannot take over lock if no keyinfo was requested
    setErrorCodeAbort(4604);
    return NULL;
  }

  /*
   * Get the Keyinfo from the NdbRecord result row
   */
  Uint32 infoword= 0;
  Uint32 len= 0;
  const char *src= NULL;

  Uint32 idx= m_current_api_receiver;
  if (idx >= m_api_receivers_count)
    return NULL;
  const NdbReceiver *receiver= m_api_receivers[m_current_api_receiver];

  /* Get this row's KeyInfo data */
  int res= receiver->get_keyinfo20(infoword, len, src);
  if (res == -1)
    return NULL;

  NdbOperation * newOp = pTrans->getNdbOperation(m_currentTable);
  if (newOp == NULL){
    return NULL;
  }
  pTrans->theSimpleState = 0;
    
  assert(len > 0);
  assert(len < 16384);

  newOp->theTupKeyLen = len;
  newOp->theOperationType = opType;
  newOp->m_abortOption = AbortOnError;
  switch (opType) {
  case (ReadRequest):
    newOp->theLockMode = theLockMode;
    // Fall through
  case (DeleteRequest):
    newOp->theStatus = GetValue;
    break;
  default:
    newOp->theStatus = SetValue;
  }
  const Uint32 tScanInfo = infoword & 0x3FFFF;
  const Uint32 tTakeOverFragment = infoword >> 20;
  {
    UintR scanInfo = 0;
    TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
    TcKeyReq::setTakeOverScanFragment(scanInfo, tTakeOverFragment);
    TcKeyReq::setTakeOverScanInfo(scanInfo, tScanInfo);
    newOp->theScanInfo = scanInfo;
    newOp->theDistrKeyIndicator_ = 1;
    newOp->theDistributionKey = tTakeOverFragment;
  }
  
  // Copy the first 8 words of key info from KEYINF20 into TCKEYREQ
  TcKeyReq * tcKeyReq = CAST_PTR(TcKeyReq,newOp->theTCREQ->getDataPtrSend());
  Uint32 i = MIN(TcKeyReq::MaxKeyInfo, len);
  memcpy(tcKeyReq->keyInfo, src, 4*i);
  src += i * 4;

  if(i < len){
    NdbApiSignal* tSignal = theNdb->getSignal();
    newOp->theTCREQ->next(tSignal); 
    
    Uint32 left = len - i;
    while(tSignal && left > KeyInfo::DataLength){
      tSignal->setSignal(GSN_KEYINFO, refToBlock(pTrans->m_tcRef));
      tSignal->setLength(KeyInfo::MaxSignalLength);
      KeyInfo * keyInfo = CAST_PTR(KeyInfo, tSignal->getDataPtrSend());
      memcpy(keyInfo->keyData, src, 4 * KeyInfo::DataLength);
      src += 4 * KeyInfo::DataLength;
      left -= KeyInfo::DataLength;
      
      tSignal->next(theNdb->getSignal());
      tSignal = tSignal->next();
      newOp->theLastKEYINFO = tSignal;
    }
    
    if(tSignal && left > 0){
      tSignal->setSignal(GSN_KEYINFO, refToBlock(pTrans->m_tcRef));
      tSignal->setLength(KeyInfo::HeaderLength + left);
      newOp->theLastKEYINFO = tSignal;
      KeyInfo * keyInfo = CAST_PTR(KeyInfo, tSignal->getDataPtrSend());
      memcpy(keyInfo->keyData, src, 4 * left);
    }      
  }
  /* create blob handles automatically for a delete - other ops must
   * create manually
   */
  if (opType == DeleteRequest && m_currentTable->m_noOfBlobs != 0) {
    for (unsigned i = 0; i < m_currentTable->m_columns.size(); i++) {
      NdbColumnImpl* c = m_currentTable->m_columns[i];
      assert(c != 0);
      if (c->getBlobType()) {
        if (newOp->getBlobHandle(pTrans, c) == NULL)
          return NULL;
      }
    }
  }
  
  return newOp;
}

NdbOperation*
NdbScanOperation::takeOverScanOpNdbRecord(OperationType opType,
                                          NdbTransaction* pTrans,
                                          const NdbRecord *record,
                                          char *row,
                                          const unsigned char *mask,
                                          const NdbOperation::OperationOptions *opts,
                                          Uint32 sizeOfOptions)
{
  int res;

  if (!m_attribute_record)
  {
    setErrorCodeAbort(4284);
    return NULL;
  }
  if (!record)
  {
    setErrorCodeAbort(4285);
    return NULL;
  }
  if (!m_keyInfo)
  {
    // Cannot take over lock if no keyinfo was requested
    setErrorCodeAbort(4604);
    return NULL;
  }
  if (record->flags & NdbRecord::RecIsIndex)
  {
    /* result_record must be a base table ndbrecord, not an index ndbrecord */
    setErrorCodeAbort(4340);
    return NULL;
  }
  if (m_blob_lock_upgraded)
  {
    /* This was really a CommittedRead scan, which does not support
     * lock takeover
     */
    /* takeOverScanOp, to take over a scanned row one must explicitly 
     * request keyinfo on readTuples call
     */
    setErrorCodeAbort(4604);
    return NULL;
  }

  NdbOperation *op= pTrans->getNdbOperation(record->table, NULL);
  if (!op)
    return NULL;

  pTrans->theSimpleState= 0;
  op->theStatus= NdbOperation::UseNdbRecord;
  op->theOperationType= opType;
  op->m_abortOption= AbortOnError;
  op->m_key_record= NULL;       // This means m_key_row has KEYINFO20 data
  op->m_attribute_record= record;
  /*
    The m_key_row pointer is only valid until next call of
    nextResult(fetchAllowed=true). But that is ok, since the lock is also
    only valid until that time, so the application must execute() the new
    operation before then.
   */

  /* Now find the current row, and extract keyinfo. */
  Uint32 idx= m_current_api_receiver;
  if (idx >= m_api_receivers_count)
    return NULL;
  const NdbReceiver *receiver= m_api_receivers[m_current_api_receiver];
  Uint32 infoword;
  res= receiver->get_keyinfo20(infoword, op->m_keyinfo_length, op->m_key_row);
  if (res==-1)
    return NULL;
  Uint32 scanInfo= 0;
  TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
  Uint32 fragment= infoword >> 20;
  TcKeyReq::setTakeOverScanFragment(scanInfo, fragment);
  TcKeyReq::setTakeOverScanInfo(scanInfo, infoword & 0x3FFFF);
  op->theScanInfo= scanInfo;
  op->theDistrKeyIndicator_= 1;
  op->theDistributionKey= fragment;

  op->m_attribute_row= row;
  AttributeMask readMask;
  record->copyMask(readMask.rep.data, mask);

  if (opType == ReadRequest)
  {
    op->theLockMode= theLockMode;
    /*
     * Apart from taking over the row lock, we also support reading again,
     * though typical usage will probably use an empty mask to read nothing.
     */
    op->theReceiver.getValues(record, row);
  }
  else if (opType == DeleteRequest && row != NULL)
  {
    /* Delete with a 'pre-read' - prepare the Receiver */
    op->theReceiver.getValues(record, row);
  }


  /* Handle any OperationOptions */
  if (opts != NULL)
  {
    /* Delegate to static method in NdbOperation */
    Uint32 result = NdbOperation::handleOperationOptions (opType,
                                                          opts,
                                                          sizeOfOptions,
                                                          op);
    if (result != 0)
    {
      setErrorCodeAbort(result);
      return NULL;
    }
  }


  /* Setup Blob handles... */
  switch (opType)
  {
  case ReadRequest:
  case UpdateRequest:
    if (unlikely(record->flags & NdbRecord::RecHasBlob))
    {
      if (op->getBlobHandlesNdbRecord(pTrans, readMask.rep.data) == -1)
        return NULL;
    }
    
    break;

  case DeleteRequest:
    /* Create blob handles if required, to properly delete all blob parts
     * If a pre-delete-read was requested, check that it does not ask for
     * Blob columns to be read.
     */
    if (unlikely(record->flags & NdbRecord::RecTableHasBlob))
    {
      if (op->getBlobHandlesNdbRecordDelete(pTrans,
                                            row != NULL,
                                            readMask.rep.data) == -1)
        return NULL;
    }
    break;
  default:
    assert(false);
    return NULL;
  }

  /* Now prepare the signals to be sent...
   */
  int returnCode=op->buildSignalsNdbRecord(pTrans->theTCConPtr, 
                                           pTrans->theTransactionId,
                                           readMask.rep.data);

  if (returnCode)
  {
    // buildSignalsNdbRecord should have set the error status
    // So we can return NULL
    return NULL;
  }

  return op;
}

NdbBlob*
NdbScanOperation::getBlobHandle(const char* anAttrName)
{
  const NdbColumnImpl* col= m_currentTable->getColumn(anAttrName);
  
  if (col != NULL)
  {
    /* We need the row KeyInfo for Blobs
     * Old Api scans have saved flags at this point
     */
    if (m_scanUsingOldApi)
      m_savedScanFlagsOldApi|= SF_KeyInfo;
    else
      m_keyInfo= 1;
    
    return NdbOperation::getBlobHandle(m_transConnection, col);
  }
  else
  {
    setErrorCode(4004);
    return NULL;
  }
}

NdbBlob*
NdbScanOperation::getBlobHandle(Uint32 anAttrId)
{
  const NdbColumnImpl* col= m_currentTable->getColumn(anAttrId);
  
  if (col != NULL)
  {
    /* We need the row KeyInfo for Blobs 
     * Old Api scans have saved flags at this point
     */
    if (m_scanUsingOldApi)
      m_savedScanFlagsOldApi|= SF_KeyInfo;
    else
      m_keyInfo= 1;
    
    return NdbOperation::getBlobHandle(m_transConnection, col);
  }
  else
  {
    setErrorCode(4004);
    return NULL;
  }
}

/** 
 * getValue_NdbRecord_scan
 * This variant is called when the ScanOptions::GETVALUE mechanism is
 * used to add extra GetValues to an NdbRecord defined scan.
 * It is not used for supporting old-Api scans
 */
NdbRecAttr*
NdbScanOperation::getValue_NdbRecord_scan(const NdbColumnImpl* attrInfo,
                                          char* aValue)
{
  DBUG_ENTER("NdbScanOperation::getValue_NdbRecord_scan");
  int res;
  NdbRecAttr *ra;
  DBUG_PRINT("info", ("Column: %u", attrInfo->m_attrId));

  if (attrInfo->m_storageType == NDB_STORAGETYPE_DISK)
  {
    m_flags &= ~Uint8(OF_NO_DISK);
  }

  res= insertATTRINFOHdr_NdbRecord(attrInfo->m_attrId, 0);
  if (res==-1)
    DBUG_RETURN(NULL);

  theInitialReadSize= theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;
  ra= theReceiver.getValue(attrInfo, aValue);
  if (!ra)
  {
    setErrorCodeAbort(4000);
    DBUG_RETURN(NULL);
  }
  theErrorLine++;
  DBUG_RETURN(ra);
}

/**
 * getValue_NdbRecAttr_scan
 * This variant is called when the old Api getValue() method is called
 * against a ScanOperation.  It adds a RecAttr object to the scan.
 * Signals to request that the value be read are added when the old Api
 * scan is finalised.
 * This method is not used to process ScanOptions::GETVALUE extra gets
 */
NdbRecAttr*
NdbScanOperation::getValue_NdbRecAttr_scan(const NdbColumnImpl* attrInfo,
                                           char* aValue)
{
  NdbRecAttr *recAttr= NULL;

  /* Get a RecAttr object, which is linked in to the Receiver's
   * RecAttr linked list, and return to caller
   */
  if (attrInfo != NULL)
  {
    if (attrInfo->m_storageType == NDB_STORAGETYPE_DISK)
    {
      m_flags &= ~Uint8(OF_NO_DISK);
    }
  
    recAttr = theReceiver.getValue(attrInfo, aValue);
    
    if (recAttr != NULL)
      theErrorLine++;
    else {
      /* MEMORY ALLOCATION ERROR */
      setErrorCodeAbort(4000);
    }
  }
  else {
    /* Attribute name or id not found in the table */
    setErrorCodeAbort(4004);
  }

  return recAttr;
}

NdbRecAttr*
NdbScanOperation::getValue_impl(const NdbColumnImpl *attrInfo, char *aValue)
{
  if (theStatus == UseNdbRecord)
    return getValue_NdbRecord_scan(attrInfo, aValue);
  else
    return getValue_NdbRecAttr_scan(attrInfo, aValue);
}

NdbIndexScanOperation::NdbIndexScanOperation(Ndb* aNdb)
  : NdbScanOperation(aNdb, NdbOperation::OrderedIndexScan)
{
  firstRangeOldApi= NULL;
  lastRangeOldApi= NULL;
  currentRangeOldApi= NULL;

}

NdbIndexScanOperation::~NdbIndexScanOperation(){
}

int
NdbIndexScanOperation::setBound(const char* anAttrName, int type, 
                                const void* aValue)
{
  return setBound(m_accessTable->getColumn(anAttrName), type, aValue);
}

int
NdbIndexScanOperation::setBound(Uint32 anAttrId, int type, 
                                const void* aValue)
{
  return setBound(m_accessTable->getColumn(anAttrId), type, aValue);
}

int
NdbIndexScanOperation::equal_impl(const NdbColumnImpl* anAttrObject, 
                                  const char* aValue)
{
  return setBound(anAttrObject, BoundEQ, aValue);
}

NdbRecAttr*
NdbIndexScanOperation::getValue_impl(const NdbColumnImpl* attrInfo, 
                                     char* aValue){
  /* Defer to ScanOperation implementation */
  // TODO : IndexScans always fetch PK columns via their key NdbRecord
  // If the user also requests them, we should avoid fetching them 
  // twice.
  return NdbScanOperation::getValue_impl(attrInfo, aValue);
}


/* Helper for setBound called via the old Api.  
 * Key bound information is stored in the operation for later
 * processing using the normal NdbRecord setBound interface.
 */
int
NdbIndexScanOperation::setBoundHelperOldApi(OldApiBoundInfo& boundInfo,
                                            Uint32 maxKeyRecordBytes,
                                            Uint32 index_attrId,
                                            Uint32 valueLen,
                                            bool inclusive,
                                            Uint32 byteOffset,
                                            Uint32 nullbit_byte_offset,
                                            Uint32 nullbit_bit_in_byte,
                                            const void *aValue)
{
  Uint32 presentBitMask= (1 << (index_attrId & 0x1f));

  if ((boundInfo.keysPresentBitmap & presentBitMask) != 0)
  {
    /* setBound() called twice for same key */
    setErrorCodeAbort(4522);
    return -1;
  }

  /* Set bit in mask for key column presence */
  boundInfo.keysPresentBitmap |= presentBitMask;

  if ((index_attrId + 1) > boundInfo.highestKey)
  {
    // New highest key, check previous keys
    // are non-strict
    if (boundInfo.highestSoFarIsStrict)
    {
      /* Invalid set of range scan bounds */
      setErrorCodeAbort(4259);
      return -1;
    }
    boundInfo.highestKey= (index_attrId + 1);
    boundInfo.highestSoFarIsStrict= !inclusive;
  }
  else
  {
    /* Not highest, key, better not be strict */
    if (!inclusive)
    {
      /* Invalid set of range scan bounds */
      setErrorCodeAbort(4259);
      return -1;
    }
  }

  if (aValue != NULL)
  {
    /* Copy data into correct part of RecAttr */
    assert(valueLen > 0);
    assert(byteOffset + valueLen <= maxKeyRecordBytes);

    memcpy(boundInfo.key + byteOffset,
           aValue, 
           valueLen);
  }
  else
  {
    /* Set Null bit */
    assert(valueLen == 0);
    boundInfo.key[nullbit_byte_offset] |= 
      (1 << nullbit_bit_in_byte);
  }

  return 0;
}

/*
 * Define bound on index column in range scan.
 */
int
NdbIndexScanOperation::setBound(const NdbColumnImpl* tAttrInfo, 
                                int type, const void* aValue)
{
  if (!tAttrInfo)
  {
    setErrorCodeAbort(4318);    // Invalid attribute
    return -1;
  }
  if (theOperationType == OpenRangeScanRequest &&
      (0 <= type && type <= 4)) 
  {
    const NdbRecord *key_record= m_accessTable->m_ndbrecord;
    const Uint32 maxKeyRecordBytes= key_record->m_row_size;

    Uint32 valueLen = 0;
    if (aValue != NULL)
      if (! tAttrInfo->get_var_length(aValue, valueLen)) {
        /* Length parameter in equal/setValue is incorrect */
        setErrorCodeAbort(4209);
        return -1;
      }
    
    /* Get details of column from NdbRecord */
    Uint32 byteOffset= 0;
    
    /* Get the Attr struct from the key NdbRecord for this index Attr */
    Uint32 attrId= tAttrInfo->m_attrId;

    if (attrId >= key_record->key_index_length)
    {
      /* Attempt to set bound on non key column */
      setErrorCodeAbort(4535);
      return -1;
    }
    Uint32 columnNum= key_record->key_indexes[ attrId ];

    if (columnNum >= key_record->noOfColumns)
    {
      /* Internal error in NdbApi */
      setErrorCodeAbort(4005);
      return -1;
    }

    NdbRecord::Attr attr= key_record->columns[ columnNum ];
    
    byteOffset= attr.offset;
    
    bool inclusive= ! ((type == BoundLT) || (type == BoundGT));

    if (currentRangeOldApi == NULL)
    {
      /* Current bound is undefined, allocate space for definition */
      NdbRecAttr* boundSpace= theNdb->getRecAttr();
      if (boundSpace == NULL)
      {
        /* Memory allocation error */
        setErrorCodeAbort(4000);
        return -1;
      }
      if (boundSpace->setup(sizeof(OldApiScanRangeDefinition) + 
                            (2 * maxKeyRecordBytes) - 1, NULL) != 0)
      {
        theNdb->releaseRecAttr(boundSpace);
        /* Memory allocation error */
        setErrorCodeAbort(4000);
        return -1;
      }
      
      /* Initialise bounds definition info */
      OldApiScanRangeDefinition* boundsDef= 
        (OldApiScanRangeDefinition*) boundSpace->aRef();

      boundsDef->oldBound.lowBound.highestKey = 0;
      boundsDef->oldBound.lowBound.highestSoFarIsStrict = false;
      /* Should be STATIC_ASSERT */
      assert(NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY == 32);
      boundsDef->oldBound.lowBound.keysPresentBitmap = 0;
      
      boundsDef->oldBound.highBound= boundsDef->oldBound.lowBound;
      boundsDef->oldBound.lowBound.key= &boundsDef->space[ 0 ];
      boundsDef->oldBound.highBound.key= &boundsDef->space[ maxKeyRecordBytes ];
      
      currentRangeOldApi= boundSpace;
    }

    OldApiScanRangeDefinition* bounds=
      (OldApiScanRangeDefinition*) currentRangeOldApi->aRef();


    /* Add to lower bound if required */
    if (type == BoundEQ ||
        type == BoundLE ||
        type == BoundLT )
    {
      if (setBoundHelperOldApi(bounds->oldBound.lowBound,
                               maxKeyRecordBytes,
                               tAttrInfo->m_attrId,
                               valueLen,
                               inclusive,
                               byteOffset,
                               attr.nullbit_byte_offset,
                               attr.nullbit_bit_in_byte,
                               aValue) != 0)
        return -1;
    }

    /* Add to upper bound if required */
    if (type == BoundEQ ||
        type == BoundGE ||
        type == BoundGT)
    {
      if (setBoundHelperOldApi(bounds->oldBound.highBound,
                               maxKeyRecordBytes,
                               tAttrInfo->m_attrId,
                               valueLen,
                               inclusive,
                               byteOffset,
                               attr.nullbit_byte_offset,
                               attr.nullbit_bit_in_byte,
                               aValue) != 0)             
        return -1;
    }
    return 0;
  } 
  else {
    /* Can only call setBound/equal() for an NdbIndexScanOperation */
    setErrorCodeAbort(4514);
    return -1;
  }
}


/* Method called just prior to scan execution to initialise
 * the passed in IndexBound for the scan using the information
 * stored by the old API's setBound() call.
 * Return codes 
 *  0 == bound present and built
 *  1 == bound not present
 * -1 == error
 */
int
NdbIndexScanOperation::buildIndexBoundOldApi(int range_no)
{
  IndexBound ib;
  OldApiScanRangeDefinition* boundDef=
    (OldApiScanRangeDefinition*) currentRangeOldApi->aRef();

  int result = 1;
  
  if (boundDef->oldBound.lowBound.highestKey != 0)
  {
    /* Have a low bound 
     * Check that a contiguous set of keys are supplied.
     * Setup low part of IndexBound
     */
    Uint32 expectedValue= (~(Uint32) 0) >> (32 - boundDef->oldBound.lowBound.highestKey);
    
    if (boundDef->oldBound.lowBound.keysPresentBitmap != expectedValue)
    {
      /* Invalid set of range scan bounds */
      setErrorCodeAbort(4259);
      return -1;
    }

    ib.low_key= boundDef->oldBound.lowBound.key;
    ib.low_key_count= boundDef->oldBound.lowBound.highestKey;
    ib.low_inclusive= !boundDef->oldBound.lowBound.highestSoFarIsStrict;
    result= 0;
  }
  else
  {
    ib.low_key= NULL;
    ib.low_key_count= 0;
    ib.low_inclusive= false;
  }

  if (boundDef->oldBound.highBound.highestKey != 0)
  {
    /* Have a high bound 
     * Check that a contiguous set of keys are supplied.
     */
    Uint32 expectedValue= (~(Uint32) 0) >> (32 - boundDef->oldBound.highBound.highestKey);
    
    if (boundDef->oldBound.highBound.keysPresentBitmap != expectedValue)
    {
      /* Invalid set of range scan bounds */
      setErrorCodeAbort(4259);
      return -1;
    }

    ib.high_key= boundDef->oldBound.highBound.key;
    ib.high_key_count= boundDef->oldBound.highBound.highestKey;
    ib.high_inclusive= !boundDef->oldBound.highBound.highestSoFarIsStrict;
    result= 0;
  }
  else
  {
    ib.high_key= NULL;
    ib.high_key_count= 0;
    ib.high_inclusive= false;
  }
  
  ib.range_no= range_no;

  boundDef->ib= ib;

  assert( currentRangeOldApi->next() == NULL );

  if (lastRangeOldApi == NULL)
  {
    /* First bound */
    assert( firstRangeOldApi == NULL );
    firstRangeOldApi= lastRangeOldApi= currentRangeOldApi;
  }
  else 
  {
    /* Other bounds exist, add this to the end of the bounds list */
    assert( firstRangeOldApi != NULL );
    assert( lastRangeOldApi->next() == NULL );
    lastRangeOldApi->next(currentRangeOldApi);
    lastRangeOldApi= currentRangeOldApi;
  }
  
  currentRangeOldApi= NULL;

  return result;
}

const NdbIndexScanOperation::IndexBound* 
NdbIndexScanOperation::getIndexBoundFromRecAttr(NdbRecAttr* recAttr)
{
  return &((OldApiScanRangeDefinition*)recAttr->aRef())->ib;
}
/* Method called to release any resources allocated by the old 
 * Index Scan bound API
 */
void
NdbIndexScanOperation::releaseIndexBoundsOldApi()
{
  NdbRecAttr* bound= firstRangeOldApi;
  while (bound != NULL)
  {
    NdbRecAttr* release= bound;
    bound= bound->next();
    theNdb->releaseRecAttr(release);
  }

  if (currentRangeOldApi != NULL)
    theNdb->releaseRecAttr(currentRangeOldApi);

  firstRangeOldApi= lastRangeOldApi= currentRangeOldApi= NULL;
}


int
NdbIndexScanOperation::ndbrecord_insert_bound(const NdbRecord *key_record,
                                              Uint32 column_index,
                                              const char *row,
                                              Uint32 bound_type,
                                              Uint32*& firstWordOfBound)
{
  char buf[NdbRecord::Attr::SHRINK_VARCHAR_BUFFSIZE];
  const NdbRecord::Attr *column= &key_record->columns[column_index];

  bool is_null= column->is_null(row);
  Uint32 len= 0;
  const void *aValue= row+column->offset;

  if (!is_null)
  {
    bool len_ok;
    /* Support for special mysqld varchar format in keys. */
    if (column->flags & NdbRecord::IsMysqldShrinkVarchar)
    {
      len_ok= column->shrink_varchar(row, len, buf);
      aValue= buf;
    }
    else
    {
      len_ok= column->get_var_length(row, len);
    }
    if (!len_ok) {
      setErrorCodeAbort(4209);
      return -1;
    }
  }

  /* Add bound type */
  if (unlikely(insertKEYINFO_NdbRecord((const char*) &bound_type, 
                                       sizeof(Uint32))))
  {
    /* Some sort of allocation error */
    setErrorCodeAbort(4000);
    return -1;
  }
  
  assert( theKEYINFOptr != NULL );
  /* Grab ptr to first word of this bound if caller wants it */
  if (firstWordOfBound == NULL)
    firstWordOfBound= theKEYINFOptr - 1;

  AttributeHeader ah(column->index_attrId, len);

  /* Add AttrInfo header + data for bound */
  if (unlikely(insertKEYINFO_NdbRecord((const char*) &ah.m_value, 
                                       sizeof(Uint32)) ||
               insertKEYINFO_NdbRecord((const char*) aValue, len) ))
  {
    /* Some sort of allocation error */
    setErrorCodeAbort(4000);
    return -1;
  }
  
  return 0;
}

int
NdbIndexScanOperation::insert_open_bound(const NdbRecord *key_record,
                                         Uint32*& firstWordOfBound)
{
  /* We want to insert an open bound into a scan
   * This is done by requesting all rows with first key column
   * >= NULL (so, confusingly, bound is <= NULL)
   * Sending this as bound info for an open bound allows us to 
   * also send the range number etc so that MRR scans can include
   * open ranges.
   * Note that MRR scans with open ranges are an inefficient use of
   * MRR.  Really the application should realise that all rows are
   * being processed and only fetch them once.
   */
  const Uint32 bound_type= NdbIndexScanOperation::BoundLE;
  
  if (unlikely(insertKEYINFO_NdbRecord((const char*) &bound_type,
                                       sizeof(Uint32))))
  {
    /* Some sort of allocation error */
    setErrorCodeAbort(4000);
    return -1;
  }

  /* Grab ptr to first word of this bound if caller wants it */
  if (firstWordOfBound == NULL)
    firstWordOfBound= theKEYINFOptr - 1;
  
  /*
   * bug#57396 wrong attr id inserted.
   * First index attr id is 0, key_record not used.
   * Create NULL attribute header.
   */
  AttributeHeader ah(0, 0);

  if (unlikely(insertKEYINFO_NdbRecord((const char*) &ah.m_value,
                                       sizeof(Uint32))))
  {
    /* Some sort of allocation error */
    setErrorCodeAbort(4000);
    return -1;
  };
  
  return 0;
}

/* IndexScan readTuples - part of old scan API
 * This call does the minimum amount of validation and state
 * storage possible.  Most of the scan initialisation is done
 * later as part of processIndexScanDefs
 */
int
NdbIndexScanOperation::readTuples(LockMode lm,
                                  Uint32 scan_flags,
                                  Uint32 parallel,
                                  Uint32 batch)
{
  /* Defer to Scan Operation's readTuples */
  int res= NdbScanOperation::readTuples(lm, scan_flags, parallel, batch);
  
  /* Set up IndexScan specific members */
  if (res == 0 && 
      ( (int) m_accessTable->m_indexType ==
        (int) NdbDictionary::Index::OrderedIndex))
  {
    if (m_currentTable == m_accessTable){
      // Old way of scanning indexes, should not be allowed
      m_currentTable = theNdb->theDictionary->
        getTable(m_currentTable->m_primaryTable.c_str());
      assert(m_currentTable != NULL);
    }
    assert (m_currentTable != m_accessTable);
    // Modify operation state
    theStatus = GetValue;
    theOperationType  = OpenRangeScanRequest;
  }

  return res;
}

/* Most of the work of Index Scan definition for old and NdbRecord
 * Index scans is done in this method 
 */
int
NdbIndexScanOperation::processIndexScanDefs(LockMode lm,
                                            Uint32 scan_flags,
                                            Uint32 parallel,
                                            Uint32 batch)
{
  const bool order_by = scan_flags & (SF_OrderBy | SF_OrderByFull);
  const bool order_desc = scan_flags & SF_Descending;
  const bool read_range_no = scan_flags & SF_ReadRangeNo;
  m_multi_range = scan_flags & SF_MultiRange;
  
  /* Defer to table scan method */
  int res = NdbScanOperation::processTableScanDefs(lm, 
                                                   scan_flags, 
                                                   parallel, 
                                                   batch);
  if(!res && read_range_no)
  {
    m_read_range_no = 1;
    if (insertATTRINFOHdr_NdbRecord(AttributeHeader::RANGE_NO, 
                                    0) == -1)
      res = -1;
  }
  if (!res)
  {
    /**
     * Note that it is valid to have order_desc true and order_by false.
     *
     * This means that there will be no merge sort among partitions, but
     * each partition will still be returned in descending sort order.
     *
     * This is useful eg. if it is known that the scan spans only one
     * partition.
     */
     if (order_desc) {
       m_descending = true;
       ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
       ScanTabReq::setDescendingFlag(req->requestInfo, true);
     }
     if (order_by) {
       m_ordered = true;
       Uint32 cnt = m_accessTable->getNoOfColumns() - 1;
       m_sort_columns = cnt; // -1 for NDB$NODE
       m_current_api_receiver = m_sent_receivers_count;
       m_api_receivers_count = m_sent_receivers_count;
     }
    
    /* Should always have NdbRecord at this point */
    assert (m_attribute_record);
  }

  m_num_bounds = 0;
  m_previous_range_num = 0;

  return res;
}

int compare_ndbrecord(const NdbReceiver *r1,
                      const NdbReceiver *r2,
                      const NdbRecord *key_record,
                      const NdbRecord *result_record,
                      bool descending,
                      bool read_range_no)
{
  Uint32 i;
  int jdir= 1 - 2 * (int)descending;

  assert(jdir == 1 || jdir == -1);

  const char *a_row= r1->getCurrentRow();
  const char *b_row= r2->getCurrentRow();

  /* First compare range_no if needed. */
  if (read_range_no)
  {
    const Uint32 a_range_no= r1->get_range_no();
    const Uint32 b_range_no= r2->get_range_no();
    if (a_range_no != b_range_no)
      return (a_range_no < b_range_no ? -1 : 1);
  }

  for (i= 0; i<key_record->key_index_length; i++)
  {
    const NdbRecord::Attr *key_col =
      &key_record->columns[key_record->key_indexes[i]];
    assert(key_col->attrId < result_record->m_attrId_indexes_length);
    int col_idx = result_record->m_attrId_indexes[key_col->attrId];
    assert(col_idx >= 0);
    assert((Uint32)col_idx < result_record->noOfColumns);
    const NdbRecord::Attr *result_col = &result_record->columns[col_idx];

    bool a_is_null= result_col->is_null(a_row);
    bool b_is_null= result_col->is_null(b_row);
    if (a_is_null)
    {
      if (!b_is_null)
        return -1 * jdir;
    }
    else
    {
      if (b_is_null)
        return 1 * jdir;

      Uint32 offset= result_col->offset;
      Uint32 maxSize= result_col->maxSize;
      const char *a_ptr= a_row + offset;
      const char *b_ptr= b_row + offset;
      void *info= result_col->charset_info;
      int res=
        (*result_col->compare_function)
            (info, a_ptr, maxSize, b_ptr, maxSize);
      if (res)
      {
        return res * jdir;
      }
    }
  }

  return 0;
}

/* This function performs the merge sort of the parallel ordered index scans
 * to produce a single sorted stream of rows to the application.
 *
 * To ensure the correct ordering, before a row can be returned, the function
 * must ensure that all fragments have either returned at least one row, or 
 * indicated that they have no more rows to return.
 *
 * The function maintains an array of receivers, one per fragment, sorted by
 * the relative ordering of their next rows.  Each time a row is taken from 
 * the 'top' receiver, it is re-inserted in the ordered list of receivers
 * which requires O(log2(NumReceivers)) comparisons.
 */
int
NdbIndexScanOperation::next_result_ordered_ndbrecord(const char * & out_row,
                                                     bool fetchAllowed,
                                                     bool forceSend)
{
  Uint32 current;

  /*
    Retrieve more rows if necessary, then sort the array of receivers.

    The special case m_current_api_receiver==theParallelism is for the
    initial call, where we need to wait for and sort all receviers.
  */
  if (m_current_api_receiver==theParallelism ||
      !m_api_receivers[m_current_api_receiver]->getNextRow())
  {
    if (!fetchAllowed)
      return 2;                                 // No more data available now

    /* Wait for all receivers to be retrieved. */
    int count= ordered_send_scan_wait_for_all(forceSend);
    if (count == -1)
      return -1;

    /*
      Insert all newly retrieved receivers in sorted array.
      The receivers are left in m_conf_receivers for us to move into place.
    */
    current= m_current_api_receiver;
    for (int i= 0; i < count; i++)
    {
      const char *nextRow = m_conf_receivers[i]->getNextRow();  // Fetch first
      assert(nextRow != NULL);  ((void)nextRow);
      ordered_insert_receiver(current--, m_conf_receivers[i]);
    }
    m_current_api_receiver= current;
    theNdb->theImpl->incClientStat(Ndb::ScanBatchCount, count);
  }
  else
  {
    /*
      Just make sure the first receiver (from which we just returned a row, so
      it may no longer be in the correct sort position) is placed correctly.
    */
    current= m_current_api_receiver;
    ordered_insert_receiver(current + 1, m_api_receivers[current]);
  }

  /* Now just return the next row (if any). */
  if (current < theParallelism && 
      (out_row= m_api_receivers[current]->getCurrentRow()) != NULL)
  {
    return 0;
  }
  else
  {
    theError.code= Err_scanAlreadyComplete;
    return 1;                                   // End-of-file
  }
}

/* Insert a newly fully-retrieved receiver in the correct sorted place. */
void
NdbIndexScanOperation::ordered_insert_receiver(Uint32 start,
                                               NdbReceiver *receiver)
{
  /*
    Binary search to find the position of the first receiver with no rows
    smaller than the first row for this receiver. We need to insert this
    receiver just before that position.
  */
  Uint32 first= start;
  Uint32 last= theParallelism;
  while (first < last)
  {
    Uint32 idx= (first+last)/2;
    int res= compare_ndbrecord(receiver,
                               m_api_receivers[idx],
                               m_key_record,
                               m_attribute_record,
                               m_descending,
                               m_read_range_no);
    if (res <= 0)
      last= idx;
    else
      first= idx+1;
  }

  /* Move down any receivers that go before this one, then insert it. */
  if (last > start)
    memmove(&m_api_receivers[start-1],
            &m_api_receivers[start],
            (last - start) * sizeof(m_api_receivers[0]));
  m_api_receivers[last-1]= receiver;
}

/*
  This method is called during (NdbRecord) ordered index scans when all rows
  from one batch of one fragment scan are exhausted (identified by
  m_current_api_receiver).

  It sends a SCAN_NEXTREQ signal for the fragment and waits for the batch to
  be fully received.

  As a special case, it is also called at the start of the scan. In this case,
  no signal is sent, it just waits for the initial batch to be fully received
  from all fragments.

  The method returns -1 for error, and otherwise the number of fragments that
  were received (this will be 0 or 1, except for the initial call where it
  will be equal to theParallelism).

  The NdbReceiver object(s) are left in the m_conf_receivers array. Note that
  it is safe to read from m_conf_receivers without mutex protection immediately
  after return from this method; as all fragments are fully received no new
  receivers can enter that array until the next call to this method.
*/
int
NdbIndexScanOperation::ordered_send_scan_wait_for_all(bool forceSend)
{
  NdbImpl* impl = theNdb->theImpl;
  Uint32 timeout= impl->get_waitfor_timeout();

  PollGuard poll_guard(* impl);
  if(theError.code)
    return -1;

  Uint32 seq= theNdbCon->theNodeSequence;
  Uint32 nodeId= theNdbCon->theDBnode;
  if (seq == impl->getNodeSequence(nodeId) &&
      !send_next_scan_ordered(m_current_api_receiver))
  {
    impl->incClientStat(Ndb::WaitScanResultCount, 1);
    while (m_sent_receivers_count > 0 && !theError.code)
    {      
      int ret_code= poll_guard.wait_scan(3*timeout, nodeId, forceSend);
      if (ret_code == 0 && seq == impl->getNodeSequence(nodeId))
        continue;
      if(ret_code == -1){
        setErrorCode(4008);
      } else {
        setErrorCode(4028);
      }
      return -1;
    }

    if(theError.code){
      setErrorCode(theError.code);
      return -1;
    }

    Uint32 new_receivers= m_conf_receivers_count;
    m_conf_receivers_count= 0;
    return new_receivers;
  } else {
    setErrorCode(4028);
    return -1;
  }
}

/*
  This method is used in ordered index scan to acknowledge the reception of
  one batch of fragment scan rows and request the sending of another batch (it
  sends a SCAN_NEXTREQ signal with one scan fragment record pointer).

  It is called with the argument IDX set to the value of
  m_current_api_receiver, the receiver for the fragment scan to acknowledge.
  This receiver is moved from the m_api_receivers array to the
  m_sent_receivers array.

  This method is called with the PollGuard mutex held on the transporter.
*/
int
NdbIndexScanOperation::send_next_scan_ordered(Uint32 idx)
{
  if(idx == theParallelism)
    return 0;
  
  NdbReceiver* tRec = m_api_receivers[idx];
  NdbApiSignal tSignal(theNdb->theMyRef);
  tSignal.setSignal(GSN_SCAN_NEXTREQ, refToBlock(theNdbCon->m_tcRef));
  
  Uint32 last = m_sent_receivers_count;
  Uint32* theData = tSignal.getDataPtrSend();
  Uint32* prep_array = theData + 4;
  
  m_current_api_receiver = idx + 1;
  if((prep_array[0] = tRec->m_tcPtrI) == RNIL)
  {
    if(DEBUG_NEXT_RESULT)
      ndbout_c("receiver completed, don't send");
    return 0;
  }
  
  theData[0] = theNdbCon->theTCConPtr;
  theData[1] = 0;
  Uint64 transId = theNdbCon->theTransactionId;
  theData[2] = (Uint32) transId;
  theData[3] = (Uint32) (transId >> 32);
  
  /**
   * Prepare ops
   */
  m_sent_receivers[last] = tRec;
  tRec->m_list_index = last;
  tRec->prepareSend();
  m_sent_receivers_count = last + 1;
  
  Uint32 nodeId = theNdbCon->theDBnode;
  NdbImpl * impl = theNdb->theImpl;
  tSignal.setLength(4+1);
  int ret= impl->sendSignal(&tSignal, nodeId);
  return ret;
}

int
NdbScanOperation::close_impl(bool forceSend, PollGuard *poll_guard)
{
  NdbImpl* impl = theNdb->theImpl;
  Uint32 timeout= impl->get_waitfor_timeout();
  Uint32 seq = theNdbCon->theNodeSequence;
  Uint32 nodeId = theNdbCon->theDBnode;
  
  if (seq != impl->getNodeSequence(nodeId))
  {
    theNdbCon->theReleaseOnClose = true;
    return -1;
  }
  
  /**
   * Wait for outstanding
   */
  impl->incClientStat(Ndb::WaitScanResultCount, 1);
  while(theError.code == 0 && m_sent_receivers_count)
  {    
    int return_code= poll_guard->wait_scan(3*timeout, nodeId, forceSend);
    switch(return_code){
    case 0:
      break;
    case -1:
      setErrorCode(4008);
    case -2:
      m_api_receivers_count = 0;
      m_conf_receivers_count = 0;
      m_sent_receivers_count = 0;
      theNdbCon->theReleaseOnClose = true;
      return -1;
    }
  }

  if(theError.code)
  {
    m_api_receivers_count = 0;
    m_current_api_receiver = m_ordered ? theParallelism : 0;
  }


  /**
   * move all conf'ed into api
   *   so that send_next_scan can check if they needs to be closed
   */
  Uint32 api = m_api_receivers_count;
  Uint32 conf = m_conf_receivers_count;

  if(m_ordered)
  {
    /**
     * Ordered scan, keep the m_api_receivers "to the right"
     */
    memmove(m_api_receivers, m_api_receivers+m_current_api_receiver, 
            (theParallelism - m_current_api_receiver) * sizeof(char*));
    api = (theParallelism - m_current_api_receiver);
    m_api_receivers_count = api;
  }
  
  if(DEBUG_NEXT_RESULT)
    ndbout_c("close_impl: [order api conf sent curr parr] %d %d %d %d %d %d",
             m_ordered, api, conf, 
             m_sent_receivers_count, m_current_api_receiver, theParallelism);
  
  if(api+conf)
  {
    /**
     * There's something to close
     *   setup m_api_receivers (for send_next_scan)
     */
    memcpy(m_api_receivers+api, m_conf_receivers, conf * sizeof(char*));
    m_api_receivers_count = api + conf;
    m_conf_receivers_count = 0;
  }
  
  // Send close scan
  if(send_next_scan(api+conf, true) == -1)
  {
    theNdbCon->theReleaseOnClose = true;
    return -1;
  }
  
  /**
   * wait for close scan conf
   */
  impl->incClientStat(Ndb::WaitScanResultCount, 1);
  while(m_sent_receivers_count+m_api_receivers_count+m_conf_receivers_count)
  {
    int return_code= poll_guard->wait_scan(3*timeout, nodeId, forceSend);
    switch(return_code){
    case 0:
      break;
    case -1:
      setErrorCode(4008);
    case -2:
      m_api_receivers_count = 0;
      m_conf_receivers_count = 0;
      m_sent_receivers_count = 0;
      theNdbCon->theReleaseOnClose = true;
      return -1;
    }
  }

  /* Rather nasty way to clean up IndexScan resources if
   * any 
   */
  if (theOperationType == OpenRangeScanRequest)
  {
    NdbIndexScanOperation *isop= 
      reinterpret_cast<NdbIndexScanOperation*> (this);

    /* Release any Index Bound resources */
    isop->releaseIndexBoundsOldApi();
  }

  /* Free any scan-owned ScanFilter generated InterpretedCode
   * object (old Api only)
   */
  freeInterpretedCodeOldApi();

  return 0;
}

void
NdbScanOperation::reset_receivers(Uint32 parallell, Uint32 ordered){
  for(Uint32 i = 0; i<parallell; i++){
    m_receivers[i]->m_list_index = i;
    m_prepared_receivers[i] = m_receivers[i]->getId();
    m_sent_receivers[i] = m_receivers[i];
    m_conf_receivers[i] = 0;
    m_api_receivers[i] = 0;
    m_receivers[i]->prepareSend();
  }
  
  m_api_receivers_count = 0;
  m_current_api_receiver = 0;
  m_sent_receivers_count = 0;
  m_conf_receivers_count = 0;
}

int
NdbIndexScanOperation::end_of_bound(Uint32 no)
{
  DBUG_ENTER("end_of_bound");
  DBUG_PRINT("info", ("Range number %u", no));

  if (! (m_savedScanFlagsOldApi & SF_MultiRange || no == 0))
  {
    setErrorCodeAbort(4509);
    /* Non SF_MultiRange scan cannot have more than one bound */
    DBUG_RETURN(-1);
  }

  if (currentRangeOldApi == NULL)
  {
    setErrorCodeAbort(4259);
    /* Invalid set of range scan bounds */
    DBUG_RETURN(-1);
  }

  /* If it's an ordered scan and we're reading range numbers
   * back then check that range numbers are strictly 
   * increasing
   */
  if ((m_savedScanFlagsOldApi & (SF_OrderBy | SF_OrderByFull)) &&
      (m_savedScanFlagsOldApi & SF_ReadRangeNo))
  {
    Uint32 expectedNum= 0;
    
    if (lastRangeOldApi != NULL)
    {
      assert( firstRangeOldApi != NULL );
      expectedNum = 
        getIndexBoundFromRecAttr(lastRangeOldApi)->range_no + 1;
    }
    
    if (no != expectedNum)
    {
      setErrorCodeAbort(4282);
      /* range_no not strictly increasing in ordered multi-range index scan */
      DBUG_RETURN(-1);
    }
  }
  
  if (buildIndexBoundOldApi(no) != 0)
    DBUG_RETURN(-1);
      
  DBUG_RETURN(0);
}

int
NdbIndexScanOperation::get_range_no()
{
  assert(m_attribute_record);

  if (m_read_range_no)
  {
    Uint32 idx= m_current_api_receiver;
    if (idx >= m_api_receivers_count)
      return -1;
    
    const NdbReceiver *tRec= m_api_receivers[m_current_api_receiver];
    return tRec->get_range_no();
  }
  return -1;
}

const NdbOperation *
NdbScanOperation::lockCurrentTuple(NdbTransaction *takeOverTrans,
                                   const NdbRecord *result_rec,
                                   char *result_row,
                                   const unsigned char *result_mask,
                                   const NdbOperation::OperationOptions *opts,
                                   Uint32 sizeOfOptions)
{
  unsigned char empty_mask[NDB_MAX_ATTRIBUTES_IN_TABLE>>3];
  /* Default is to not read any attributes, just take over the lock. */
  if (!result_row)
  {
    bzero(empty_mask, sizeof(empty_mask));
    result_mask= &empty_mask[0];
  }
  return takeOverScanOpNdbRecord(NdbOperation::ReadRequest, takeOverTrans,
                                 result_rec, result_row, 
                                 result_mask, opts, sizeOfOptions);
}

bool
NdbScanOperation::getPruned() const
{
  /* Note that for old Api scans, the bounds are not added until 
   * execute() time, so this will return false until after execute
   */
  return ((m_pruneState == SPS_ONE_PARTITION) ||
          (m_pruneState == SPS_FIXED));
}

NdbBlob*
NdbScanOperation::getBlobHandle(const char* anAttrName) const
{
  return NdbOperation::getBlobHandle(anAttrName);
}

NdbBlob*
NdbScanOperation::getBlobHandle(Uint32 anAttrId) const
{
  return NdbOperation::getBlobHandle(anAttrId);
}
