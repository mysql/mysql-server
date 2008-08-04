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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>

#define CHECKNOTNULL(p) if ((p) == NULL) {          \
    ndbout << "Error at line " << __LINE__ << endl; \
    ERR(trans->getNdbError());                      \
    return NDBT_FAILED; }

#define CHECKEQUAL(v, e) if ((e) != (v)) {            \
    ndbout << "Error at line " << __LINE__ <<         \
      " expected " << v << endl;                      \
    ERR(trans->getNdbError());                        \
    return NDBT_FAILED; }


/* Setup memory as a long Varchar with 2 bytes of
 * length information
 */
Uint32 setLongVarchar(char* where, const char* what, Uint32 sz)
{
  where[0]=sz & 0xff;
  where[1]=(sz >> 8) & 0xff;
  memcpy(&where[2], what, sz);
  return (sz + 2);
}


/* Activate the given error insert in TC block
 * This is used for error insertion where a TCKEYREQ
 * is required to activate the error
 */
int activateErrorInsert(NdbTransaction* trans, 
                        const NdbRecord* record,
                        const NdbDictionary::Table* tab,
                        const char* buf,
                        NdbRestarter* restarter, 
                        Uint32 val)
{
  if (restarter->insertErrorInAllNodes(val) != 0){
    g_err << "error insert (val) failed" << endl;
    return NDBT_FAILED;
  }

  NdbOperation* insert= trans->getNdbOperation(tab);
  
  CHECKNOTNULL(insert);

  CHECKEQUAL(0, insert->insertTuple());

  CHECKEQUAL(0, insert->equal((Uint32) 0, 
                              NdbDictionary::getValuePtr
                              (record,
                               buf,
                               0)));
  CHECKEQUAL(0, insert->setValue(1,
                                 NdbDictionary::getValuePtr
                                 (record,
                                  buf,
                                  1)));

  CHECKEQUAL(0, trans->execute(NdbTransaction::NoCommit));

  CHECKEQUAL(0, trans->getNdbError().code);

  return NDBT_OK;
}
    
/* Test for correct behaviour using primary key operations
 * when an NDBD node's SegmentedSection pool is exhausted.
 * Long and Short TCKEYREQ variants are tested
 */
int testSegmentedSectionPk(NDBT_Context* ctx, NDBT_Step* step){
  /*
   * Signal type       Exhausted @              How
   * -----------------------------------------------------
   * Long TCKEYREQ     Initial import           Consume + send
   * Long TCKEYREQ     Initial import, not first
   *                     TCKEYREQ in batch      Consume + send
   * Long TCKEYREQ     Initial import, not last
   *                     TCKEYREQ in batch      Consume + send
   * Short TCKEYREQ    KeyInfo accumulate       Consume + send long
   *                     (TCKEYREQ + KEYINFO)
   * Short TCKEYREQ    AttrInfo accumulate      Consume + send short key
   *                                             + long AI
   *                      (TCKEYREQ + ATTRINFO)
   */

  /* We just run on one table */
  if (strcmp(ctx->getTab()->getName(), "WIDE_2COL") != 0)
    return NDBT_OK;

  const Uint32 maxRowBytes= NDB_MAX_TUPLE_SIZE_IN_WORDS * sizeof(Uint32);
  const Uint32 srcBuffBytes= NDBT_Tables::MaxVarTypeKeyBytes;
  const Uint32 maxAttrBytes= NDBT_Tables::MaxKeyMaxVarTypeAttrBytes;
  char smallKey[50];
  char srcBuff[srcBuffBytes];
  char smallRowBuf[maxRowBytes];
  char bigKeyRowBuf[maxRowBytes];
  char bigAttrRowBuf[maxRowBytes];

  /* Small key for hinting to same TC */
  Uint32 smallKeySize= setLongVarchar(&smallKey[0],
                                      "ShortKey",
                                      8);

  /* Large value source */
  memset(srcBuff, 'B', srcBuffBytes);

  const NdbRecord* record= ctx->getTab()->getDefaultRecord();

  /* Setup buffers
   * Small row buffer with small key and small data
   */ 
  setLongVarchar(NdbDictionary::getValuePtr(record,
                                            smallRowBuf,
                                            0),
                 "ShortKey",
                 8);
  NdbDictionary::setNull(record, smallRowBuf, 0, false);

  setLongVarchar(NdbDictionary::getValuePtr(record,
                                            smallRowBuf,
                                            1),
                 "ShortData",
                 9);
  NdbDictionary::setNull(record, smallRowBuf, 1, false);

  /* Big key buffer with big key and small data*/
  setLongVarchar(NdbDictionary::getValuePtr(record,
                                            bigKeyRowBuf,
                                            0),
                 &srcBuff[0],
                 srcBuffBytes);
  NdbDictionary::setNull(record, bigKeyRowBuf, 0, false);

  setLongVarchar(NdbDictionary::getValuePtr(record,
                                            bigKeyRowBuf,
                                            1),
                 "ShortData",
                 9);
  NdbDictionary::setNull(record, bigKeyRowBuf, 1, false);

  /* Big AttrInfo buffer with small key and big data */
  setLongVarchar(NdbDictionary::getValuePtr(record,
                                            bigAttrRowBuf,
                                            0),
                 "ShortKey", 
                 8);
  NdbDictionary::setNull(record, bigAttrRowBuf, 0, false);

  setLongVarchar(NdbDictionary::getValuePtr(record,
                                            bigAttrRowBuf,
                                            1),
                 &srcBuff[0],
                 maxAttrBytes);
  NdbDictionary::setNull(record, bigAttrRowBuf, 1, false);

  NdbRestarter restarter;
  Ndb* pNdb= GETNDB(step);

  /* Start a transaction on a specific node */
  NdbTransaction* trans= pNdb->startTransaction(ctx->getTab(),
                                                &smallKey[0],
                                                smallKeySize);
  CHECKNOTNULL(trans);

  /* Activate error insert 8065 in this transaction, consumes
   * all but 10 SectionSegments
   */
  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          record, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8065));

  /* Ok, now the chosen TC's node should have only 10 
   * SegmentedSection buffers = ~ 60 words * 10 = 2400 bytes
   * Let's try an insert with a key bigger than that
   * Since it's part of the same transaction, it'll go via
   * the same TC.
   */
  const NdbOperation* bigInsert = trans->insertTuple(record, bigKeyRowBuf);

  CHECKNOTNULL(bigInsert);

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();
  
  /* Ok, now a long TCKEYREQ to the same TC - this
   * has slightly different abort handling since no other
   * operations exist in this new transaction.
   * We also change it so that import overflow occurs 
   * on the AttrInfo section
   */
  /* Start transaction on the same node */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));


  CHECKNOTNULL(bigInsert = trans->insertTuple(record, bigAttrRowBuf));

  CHECKEQUAL(-1,trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code);

  trans->close();

  /* Ok, now a long TCKEYREQ where we run out of SegmentedSections
   * on the first TCKEYREQ, but there are other TCKEYREQs following
   * in the same batch.  Check that abort handling is correct
   */
    /* Start transaction on the same node */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  /* First op in batch, will cause overflow */
  CHECKNOTNULL(bigInsert = trans->insertTuple(record, bigAttrRowBuf));
  
  /* Second op in batch, what happens to it? */
  const NdbOperation* secondOp;
  CHECKNOTNULL(secondOp = trans->insertTuple(record, bigAttrRowBuf));


  CHECKEQUAL(-1,trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code);

  trans->close();

  /* Now try with a 'short' TCKEYREQ, generated using the old Api 
   * with a big key value
   */
  /* Start transaction on the same node */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  NdbOperation* bigInsertOldApi;
  CHECKNOTNULL(bigInsertOldApi= trans->getNdbOperation(ctx->getTab()));

  CHECKEQUAL(0, bigInsertOldApi->insertTuple());
  CHECKEQUAL(0, bigInsertOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (record,
                                        bigKeyRowBuf,
                                        0)));
  CHECKEQUAL(0, bigInsertOldApi->setValue(1, 
                                          NdbDictionary::getValuePtr
                                          (record,
                                           bigKeyRowBuf,
                                           1)));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Now try with a 'short' TCKEYREQ, generated using the old Api 
   * with a big data value
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  CHECKNOTNULL(bigInsertOldApi= trans->getNdbOperation(ctx->getTab()));

  CHECKEQUAL(0, bigInsertOldApi->insertTuple());
  CHECKEQUAL(0, bigInsertOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (record,
                                        bigAttrRowBuf,
                                        0)));
  CHECKEQUAL(0, bigInsertOldApi->setValue(1, 
                                          NdbDictionary::getValuePtr
                                          (record,
                                           bigAttrRowBuf,
                                           1)));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Change error insert so that next TCKEYREQ will grab
   * all but one SegmentedSection so that we can then test SegmentedSection
   * exhaustion when importing the Key/AttrInfo words from the
   * TCKEYREQ signal itself.
   */
  restarter.insertErrorInAllNodes(8066);


  /* Now a 'short' TCKEYREQ, there will be space to import the
   * short key, but not the AttrInfo
   */
  /* Start transaction on same node */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  CHECKNOTNULL(bigInsertOldApi= trans->getNdbOperation(ctx->getTab()));
  
  CHECKEQUAL(0, bigInsertOldApi->insertTuple());
  CHECKEQUAL(0, bigInsertOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (record,
                                        smallRowBuf,
                                        0)));
  CHECKEQUAL(0, bigInsertOldApi->setValue(1, NdbDictionary::getValuePtr
                                          (record,
                                           smallRowBuf,
                                           1)));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Change error insert so that there are no SectionSegments 
   * This will cause failure when attempting to import the
   * KeyInfo from the TCKEYREQ
   */
  restarter.insertErrorInAllNodes(8067);

  /* Now a 'short' TCKEYREQ - there will be no space to import the key */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  CHECKNOTNULL(bigInsertOldApi= trans->getNdbOperation(ctx->getTab()));
  
  CHECKEQUAL(0, bigInsertOldApi->insertTuple());
  CHECKEQUAL(0, bigInsertOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (record,
                                        smallRowBuf,
                                        0)));
  CHECKEQUAL(0, bigInsertOldApi->setValue(1, 
                                          NdbDictionary::getValuePtr
                                          (record,
                                           smallRowBuf,
                                           1)));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();  


  /* Finished with error insert, cleanup the error insertion
   * Error insert 8068 will free the hoarded segments
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));

  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          record, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8068));

  trans->execute(NdbTransaction::Rollback);
  
  CHECKEQUAL(0, trans->getNdbError().code);

  trans->close();

  return NDBT_OK;
}
  
/* Test for correct behaviour using unique key operations
 * when an NDBD node's SegmentedSection pool is exhausted.
 * Long and Short TCKEYREQ variants are tested
 */
int testSegmentedSectionIx(NDBT_Context* ctx, NDBT_Step* step){
  /* 
   * Signal type       Exhausted @              How
   * -----------------------------------------------------
   * Long TCINDXREQ    Initial import           Consume + send 
   * Long TCINDXREQ    Build second TCKEYREQ    Consume + send short
   *                                             w. long base key
   * Short TCINDXREQ   KeyInfo accumulate       Consume + send long
   *                     (TCINDXREQ + KEYINFO)
   * Short TCINDXREQ   AttrInfo accumulate      Consume + send short key
   *                                             + long AI
   *                     (TCINDXREQ + ATTRINFO)
   */
  /* We will generate : 
   *   10 SS left : 
   *     Long IndexReq with too long Key/AttrInfo
   *     Long IndexReq read with short Key + Attrinfo to long 
   *       base table Key
   *     Short IndexReq with long Keyinfo
   *     Short IndexReq with long AttrInfo
   *   1 SS left
   *     Short IndexReq with any AttrInfo
   *   0 SS left
   *     Short IndexReq with any key info 
   */
  /* We just run on one table */
  if (strcmp(ctx->getTab()->getName(), "WIDE_2COL_IX") != 0)
    return NDBT_OK;

  const char* indexName= "WIDE_2COL_IX$NDBT_IDX0";
  const Uint32 maxRowBytes= NDB_MAX_TUPLE_SIZE_IN_WORDS * sizeof(Uint32);
  const Uint32 srcBuffBytes= NDBT_Tables::MaxVarTypeKeyBytes;
  const Uint32 maxIndexKeyBytes= NDBT_Tables::MaxKeyMaxVarTypeAttrBytesIndex;
  /* We want to use 6 Segmented Sections, each of 60 32-bit words, including
   * a 2 byte length overhead
   * (We don't want to use 10 Segmented Sections as in some scenarios TUP 
   *  uses Segmented Sections when sending results, and if we use TUP on
   *  the same node, the exhaustion will occur in TUP, which is not what
   *  we're testing)
   */
  const Uint32 mediumPrimaryKeyBytes= (6* 60 * 4) - 2;
  char smallKey[50];
  char srcBuff[srcBuffBytes];
  char smallRowBuf[maxRowBytes];
  char bigKeyIxBuf[maxRowBytes];
  char bigAttrIxBuf[maxRowBytes];
  char bigKeyRowBuf[maxRowBytes];
  char resultSpace[maxRowBytes];

  /* Small key for hinting to same TC */
  Uint32 smallKeySize= setLongVarchar(&smallKey[0],
                                      "ShortKey",
                                      8);

  /* Large value source */
  memset(srcBuff, 'B', srcBuffBytes);

  Ndb* pNdb= GETNDB(step);

  const NdbRecord* baseRecord= ctx->getTab()->getDefaultRecord();
  const NdbRecord* ixRecord= pNdb->
    getDictionary()->getIndex(indexName,
                              ctx->getTab()->getName())->getDefaultRecord();

  /* Setup buffers
   * Small row buffer with short key and data in base table record format
   */ 
  setLongVarchar(NdbDictionary::getValuePtr(baseRecord,
                                            smallRowBuf,
                                            0),
                 "ShortKey",
                 8);
  NdbDictionary::setNull(baseRecord, smallRowBuf, 0, false);

  setLongVarchar(NdbDictionary::getValuePtr(baseRecord,
                                            smallRowBuf,
                                            1),
                 "ShortData",
                 9);
  NdbDictionary::setNull(baseRecord, smallRowBuf, 1, false);

  /* Big index key buffer
   * Big index key (normal row attribute) in index record format
   * Index's key is attrid 1 from the base table
   * This could get confusing !
   */
  
  setLongVarchar(NdbDictionary::getValuePtr(ixRecord,
                                            bigKeyIxBuf,
                                            1),
                 &srcBuff[0],
                 maxIndexKeyBytes);
  NdbDictionary::setNull(ixRecord, bigKeyIxBuf, 1, false);

  /* Big AttrInfo buffer
   * Small key and large attrinfo in base table record format */
  setLongVarchar(NdbDictionary::getValuePtr(baseRecord,
                                            bigAttrIxBuf,
                                            0),
                 "ShortIXKey", 
                 10);

  NdbDictionary::setNull(baseRecord, bigAttrIxBuf, 0, false);

  setLongVarchar(NdbDictionary::getValuePtr(baseRecord,
                                            bigAttrIxBuf,
                                            1),
                 &srcBuff[0],
                 maxIndexKeyBytes);
  NdbDictionary::setNull(baseRecord, bigAttrIxBuf, 1, false);

  /* Big key row buffer 
   * Medium sized key and small attrinfo (index key) in
   * base table record format
   */
  setLongVarchar(NdbDictionary::getValuePtr(baseRecord,
                                            bigKeyRowBuf,
                                            0),
                 &srcBuff[0], 
                 mediumPrimaryKeyBytes);

  NdbDictionary::setNull(baseRecord, bigKeyRowBuf, 0, false);

  setLongVarchar(NdbDictionary::getValuePtr(baseRecord,
                                            bigKeyRowBuf,
                                            1),
                 "ShortIXKey",
                 10);
  NdbDictionary::setNull(baseRecord, bigKeyRowBuf, 1, false);


  /* Start a transaction on a specific node */
  NdbTransaction* trans= pNdb->startTransaction(ctx->getTab(),
                                                &smallKey[0],
                                                smallKeySize);
  /* Insert a row in the base table with a big PK, and
   * small data (Unique IX key).  This is used later to lookup
   * a big PK and cause overflow when reading TRANSID_AI in TC.
   */
  CHECKNOTNULL(trans->insertTuple(baseRecord,
                                  bigKeyRowBuf));

  CHECKEQUAL(0, trans->execute(NdbTransaction::Commit));

  NdbRestarter restarter;
  /* Start a transaction on a specific node */
  trans= pNdb->startTransaction(ctx->getTab(),
                                &smallKey[0],
                                smallKeySize);
  CHECKNOTNULL(trans);

  /* Activate error insert 8065 in this transaction, consumes
   * all but 10 SectionSegments
   */
  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          baseRecord, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8065));

  /* Ok, now the chosen TC's node should have only 10 
   * SegmentedSection buffers = ~ 60 words * 10 = 2400 bytes
   * Let's try an index read with an index key bigger than that
   * Since it's part of the same transaction, it'll go via
   * the same TC.
   */
  const NdbOperation* bigRead= trans->readTuple(ixRecord,
                                                bigKeyIxBuf,
                                                baseRecord,
                                                resultSpace);

  CHECKNOTNULL(bigRead);

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  
  /* Ok, now a long TCINDXREQ to the same TC - this
   * has slightly different abort handling since no other
   * operations exist in this new transaction.
   */
  /* Start a transaction on a specific node */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));

  CHECKNOTNULL(trans->readTuple(ixRecord,
                                bigKeyIxBuf,
                                baseRecord,
                                resultSpace));
  
  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code);
  
  trans->close();

  /* Now a TCINDXREQ that overflows, but is not the last in the
   * batch, what happens to the other TCINDXREQ in the batch?
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));

  CHECKNOTNULL(trans->readTuple(ixRecord,
                                bigKeyIxBuf,
                                baseRecord,
                                resultSpace));
  /* Another read */
  CHECKNOTNULL(trans->readTuple(ixRecord,
                                bigKeyIxBuf,
                                baseRecord,
                                resultSpace));
  
  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code);
  
  trans->close();


  /* Next we read a tuple with a large primary key via the unique
   * index.  The index read itself should be fine, but
   * pulling in the base table PK will cause abort due to overflow
   * handling TRANSID_AI
   */
  /* Start a transaction on a specific node */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));

  CHECKNOTNULL(bigRead= trans->readTuple(ixRecord,
                                         bigAttrIxBuf,
                                         baseRecord,
                                         resultSpace));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Now try with a 'short' TCINDXREQ, generated using the old Api 
   * with a big index key value
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  const NdbDictionary::Index* index;
  CHECKNOTNULL(index= pNdb->getDictionary()->
               getIndex(indexName,
                        ctx->getTab()->getName()));

  NdbIndexOperation* bigReadOldApi;
  CHECKNOTNULL(bigReadOldApi= trans->getNdbIndexOperation(index));

  CHECKEQUAL(0, bigReadOldApi->readTuple());
  /* We use the attribute id of the index, not the base table here */
  CHECKEQUAL(0, bigReadOldApi->equal((Uint32)0, 
                                     NdbDictionary::getValuePtr
                                     (ixRecord,
                                      bigKeyIxBuf,
                                      1)));

  CHECKNOTNULL(bigReadOldApi->getValue((Uint32)1));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Now try with a 'short' TCINDXREQ, generated using the old Api 
   * with a big attrinfo value
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  NdbIndexOperation* bigUpdateOldApi;
  CHECKNOTNULL(bigUpdateOldApi= trans->getNdbIndexOperation(index));

  CHECKEQUAL(0, bigUpdateOldApi->updateTuple());
  /* We use the attribute id of the index, not the base table here */
  CHECKEQUAL(0, bigUpdateOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (baseRecord,
                                        smallRowBuf,
                                        1)));

  CHECKEQUAL(0, bigUpdateOldApi->setValue((Uint32)1,
                                          NdbDictionary::getValuePtr
                                          (baseRecord,
                                           bigAttrIxBuf,
                                           1)));
  
  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Change error insert so that next TCINDXREQ will grab
   * all but one SegmentedSection
   */
  restarter.insertErrorInAllNodes(8066);

  /* Now a short TCINDXREQ where the KeyInfo from the TCINDXREQ
   * can be imported, but the ATTRINFO can't
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));
  
  CHECKNOTNULL(bigUpdateOldApi= trans->getNdbIndexOperation(index));

  CHECKEQUAL(0, bigUpdateOldApi->updateTuple());
  /* We use the attribute id of the index, not the base table here */
  CHECKEQUAL(0, bigUpdateOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (baseRecord,
                                        smallRowBuf,
                                        1)));

  CHECKEQUAL(0, bigUpdateOldApi->setValue((Uint32)1,
                                          NdbDictionary::getValuePtr
                                          (baseRecord,
                                           bigAttrIxBuf,
                                           1)));
  
  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Change error insert so that there are no SectionSegments */
  restarter.insertErrorInAllNodes(8067);

  /* Now a short TCINDXREQ where the KeyInfo from the TCINDXREQ
   * can't be imported
   */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));

  CHECKNOTNULL(bigUpdateOldApi= trans->getNdbIndexOperation(index));

  CHECKEQUAL(0, bigUpdateOldApi->updateTuple());
  /* We use the attribute id of the index, not the base table here */
  CHECKEQUAL(0, bigUpdateOldApi->equal((Uint32)0, 
                                       NdbDictionary::getValuePtr
                                       (baseRecord,
                                        smallRowBuf,
                                        1)));

  CHECKEQUAL(0, bigUpdateOldApi->setValue((Uint32)1,
                                          NdbDictionary::getValuePtr
                                          (baseRecord,
                                           bigAttrIxBuf,
                                           1)));
  
  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  /* Finished with error insert, cleanup the error insertion */
  CHECKNOTNULL(trans= pNdb->startTransaction(ctx->getTab(),
                                             &smallKey[0],
                                             smallKeySize));

  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          baseRecord, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8068));

  trans->execute(NdbTransaction::Rollback);
  
  CHECKEQUAL(0, trans->getNdbError().code);

  trans->close();

  return NDBT_OK;
}


NDBT_TESTSUITE(testLimits);

TESTCASE("ExhaustSegmentedSectionPk",
         "Test behaviour at Segmented Section exhaustion for PK"){
  INITIALIZER(testSegmentedSectionPk);
}

TESTCASE("ExhaustSegmentedSectionIX",
         "Test behaviour at Segmented Section exhaustion for PK"){
  INITIALIZER(testSegmentedSectionIx);
}

NDBT_TESTSUITE_END(testLimits);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testLimits);
  return testLimits.execute(argc, argv);
}
