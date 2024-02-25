/*
   Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>

#define CHECKTRANS(trans) if ((trans) == NULL) {    \
    ndbout << "Error at line " << __LINE__ << endl; \
    NDB_ERR(pNdb->getNdbError());                   \
    return NDBT_FAILED; }

#define CHECKNOTNULL(p) if ((p) == NULL) {          \
    ndbout << "Error at line " << __LINE__ << endl; \
    NDB_ERR(trans->getNdbError());                  \
    trans->close();                                 \
    return NDBT_FAILED; }

#define CHECKEQUAL(v, e) if ((e) != (v)) {            \
    ndbout << "Error at line " << __LINE__ <<         \
      " expected " << v << endl;                      \
    NDB_ERR(trans->getNdbError());                    \
    trans->close();                                   \
    return NDBT_FAILED; }

#define CHECK(v) if (!(v)) {                      \
    ndbout << "Error at line " << __LINE__ <<         \
      endl;                                           \
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
  /* We insert the error twice to avoid what appear to be
   * races between the error insert and the subsequent
   * tests
   * Alternatively we could sleep here.
   */
  if (restarter->insertErrorInAllNodes(val) != 0){
    g_err << "error insert 1 (" << val << ") failed" << endl;
    return NDBT_FAILED;
  }
  if (restarter->insertErrorInAllNodes(val) != 0){
    g_err << "error insert 2 (" << val << ") failed" << endl;
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
   * No testing of short TCKEYREQ variants as they cannot be
   * generated in mysql-5.1-telco-6.4+
   * TODO : Add short variant testing to testUpgrade.
   */

  /* We just run on one table */
  if (strcmp(ctx->getTab()->getName(), "WIDE_2COL") != 0)
    return NDBT_OK;

  const Uint32 maxRowBytes= NDB_MAX_TUPLE_SIZE_IN_WORDS * sizeof(Uint32);
  const Uint32 maxKeyBytes= NDBT_Tables::MaxVarTypeKeyBytes;
  const Uint32 maxAttrBytes= NDBT_Tables::MaxKeyMaxVarTypeAttrBytes;
  const Uint32 srcBuffBytes= MAX(maxKeyBytes,maxAttrBytes);
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
                 maxKeyBytes);
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
  CHECKTRANS(trans);

  /* Activate error insert 8065 in this transaction, limits
   * any single import/append to 1 section
   */
  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          record, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8065));

  /* Ok, let's try an insert with a key bigger than 1 section.
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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

  // TODO : Add code to testUpgrade
#if 0
  /*
   * Short TCKEYREQ    KeyInfo accumulate       Consume + send long
   *                     (TCKEYREQ + KEYINFO)
   * Short TCKEYREQ    AttrInfo accumulate      Consume + send short key
   *                                             + long AI
   *                      (TCKEYREQ + ATTRINFO)
   */
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
#endif

  /* Finished with error insert, cleanup the error insertion
   * Error insert 8068 will free the hoarded segments
   */
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
 */
int testSegmentedSectionIx(NDBT_Context* ctx, NDBT_Step* step){
  /* 
   * Signal type       Exhausted @              How
   * -----------------------------------------------------
   * Long TCINDXREQ    Initial import           Consume + send 
   * Long TCINDXREQ    Build second TCKEYREQ    Consume + send short
   *                                             w. long base key
   */
  /* We will generate : 
   *   10 SS left : 
   *     Long IndexReq with too long Key/AttrInfo
   *    1 SS left :
   *     Long IndexReq read with short Key + Attrinfo to long 
   *       base table Key
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
  CHECKTRANS(trans);

  /* Activate error insert 8065 in this transaction, limits any
   * single append/import to 10 sections.
   */
  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          baseRecord, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8065));

  /* Ok, let's try an index read with a big index key.
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
                                            &smallKey[0],
                                            smallKeySize));

  /* Activate error insert 8066 in this transaction, limits a
   * single import/append to 1 section.
   * Note that the TRANSID_AI is received by TC as a short-signal
   * train, so no single append is large, but when the first
   * segment is used and append starts on the second, it will
   * fail.
   */
  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          baseRecord, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8066));
  CHECKEQUAL(0, trans->execute(NdbTransaction::NoCommit));
  
  CHECKNOTNULL(bigRead= trans->readTuple(ixRecord,
                                         bigAttrIxBuf,
                                         baseRecord,
                                         resultSpace));

  CHECKEQUAL(-1, trans->execute(NdbTransaction::NoCommit));

  /* ZGET_DATABUF_ERR expected */
  CHECKEQUAL(218, trans->getNdbError().code)

  trans->close();

  // TODO Move short signal testing to testUpgrade
#if 0
  /*
   * Short TCINDXREQ   KeyInfo accumulate       Consume + send long
   *                     (TCINDXREQ + KEYINFO)
   * Short TCINDXREQ   AttrInfo accumulate      Consume + send short key
   *                                             + long AI
   *                     (TCINDXREQ + ATTRINFO)
   */
  /* Now try with a 'short' TCINDXREQ, generated using the old Api 
   * with a big index key value
   */
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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

#endif  

  /* Finished with error insert, cleanup the error insertion */
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
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


int testSegmentedSectionScan(NDBT_Context* ctx, NDBT_Step* step){
  /* Test that TC handling of segmented section exhaustion is
   * correct
   * Since NDBAPI always send long requests, that is all that
   * we test
   */
    /* We just run on one table */
  if (strcmp(ctx->getTab()->getName(), "WIDE_2COL") != 0)
    return NDBT_OK;

  const Uint32 maxRowBytes= NDB_MAX_TUPLE_SIZE_IN_WORDS * sizeof(Uint32);
  char smallKey[50];
  char smallRowBuf[maxRowBytes];

  Uint32 smallKeySize= setLongVarchar(&smallKey[0],
                                      "ShortKey",
                                      8);

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

  NdbRestarter restarter;
  Ndb* pNdb= GETNDB(step);

  /* Start a transaction on a specific node */
  NdbTransaction* trans= pNdb->startTransaction(ctx->getTab(),
                                                &smallKey[0],
                                                smallKeySize);
  CHECKTRANS(trans);

  /* Activate error insert 8066 in this transaction, limits a 
   * single import/append to 1 section.
   */
  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          record, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8066));

  /* A scan will always send 2 long sections (Receiver Ids,
   * AttrInfo)
   * Let's start a scan with > 2400 bytes of
   * ATTRINFO and see what happens
   */
  NdbScanOperation* scan= trans->getNdbScanOperation(ctx->getTab());

  CHECKNOTNULL(scan);

  CHECKEQUAL(0, scan->readTuples());

  /* Create a particularly useless program */
  NdbInterpretedCode prog;

  for (Uint32 w=0; w < 2500; w++)
    CHECKEQUAL(0, prog.load_const_null(1));

  CHECKEQUAL(0, prog.interpret_exit_ok());
  CHECKEQUAL(0, prog.finalise());

  CHECKEQUAL(0, scan->setInterpretedCode(&prog));

  CHECKEQUAL(0, trans->execute(NdbTransaction::NoCommit));

  // Scan errors arrive asynchronously into the ScanOperation.
  // However, errors should not become visible on the Transaction object
  // until after the nextResult-wait.
  CHECKEQUAL(0, trans->getNdbError().code);
  NdbSleep_MilliSleep(10);    // Not even after a long sleep.
  CHECKEQUAL(0, trans->getNdbError().code);

  CHECKEQUAL(-1, scan->nextResult());
  
  CHECKEQUAL(217, scan->getNdbError().code);
  CHECKEQUAL(217, trans->getNdbError().code);

  trans->close();

  /* Finished with error insert, cleanup the error insertion */
  CHECKTRANS(trans = pNdb->startTransaction(ctx->getTab(),
                                            &smallKey[0],
                                            smallKeySize));

  CHECKEQUAL(NDBT_OK, activateErrorInsert(trans, 
                                          record, 
                                          ctx->getTab(),
                                          smallRowBuf, 
                                          &restarter, 
                                          8068));

  CHECKEQUAL(0, trans->execute(NdbTransaction::Rollback));
  
  CHECKEQUAL(0, trans->getNdbError().code);

  trans->close();

  return NDBT_OK;
}

int testDropSignalFragments(NDBT_Context* ctx, NDBT_Step* step){
  /* Segmented section exhaustion results in dropped signals
   * Fragmented signals split one logical signal over multiple
   * physical signals (to cope with the MAX_SIGNAL_LENGTH=32kB
   * limitation).
   * This testcase checks that when individual signals comprising
   * a fragmented signal (in this case SCANTABREQ) are dropped, the
   * system behaves correctly.
   * Correct behaviour is to behave in the same way as if the signal
   * was not fragmented, and for SCANTABREQ, to return a temporary
   * resource error.
   */
  NdbRestarter restarter;
  Ndb* pNdb= GETNDB(step);

  /* SEND > ((2 * MAX_SEND_MESSAGE_BYTESIZE) + SOME EXTRA) 
   * This way we get at least 3 fragments
   * However, as this is generally > 64kB, it's too much AttrInfo for
   * a ScanTabReq, so the 'success' case returns error 874
   */
  const Uint32 PROG_WORDS= 16500; 

  struct SubCase
  {
    Uint32 errorInsertCode;
    int expectedRc;
  };
  const Uint32 numSubCases= 5;
  const SubCase cases[numSubCases]= 
  /* Error insert   Scanrc */
    {{          0,     874},  // Normal, success which gives too much AI error
     {       8074,     217},  // Drop first fragment -> error 217
     {       8075,     217},  // Drop middle fragment(s) -> error 217
     {       8076,     217},  // Drop last fragment -> error 217
     {       8077,     217}}; // Drop all fragments -> error 217
  const Uint32 numIterations= 50;
  
  Uint32 buff[ PROG_WORDS + 10 ]; // 10 extra for final 'return' etc.

  for (Uint32 iteration=0; iteration < (numIterations * numSubCases); iteration++)
  {
    /* Start a transaction */
    NdbTransaction* trans= pNdb->startTransaction();
    CHECKTRANS(trans);

    SubCase subcase= cases[iteration % numSubCases];

    Uint32 errorInsertVal= subcase.errorInsertCode;
    // printf("Inserting error : %u\n", errorInsertVal);
    /* We insert the error twice, to bias races between
     * error-insert propagation and the succeeding scan
     * in favour of error insert winning!
     * This problem needs a more general fix
     */
    CHECKEQUAL(0, restarter.insertErrorInAllNodes(errorInsertVal));
    CHECKEQUAL(0, restarter.insertErrorInAllNodes(errorInsertVal));

    NdbScanOperation* scan= trans->getNdbScanOperation(ctx->getTab());
    
    CHECKNOTNULL(scan);
    
    CHECKEQUAL(0, scan->readTuples());
    
    /* Create a large program, to give a large SCANTABREQ */
    NdbInterpretedCode prog(ctx->getTab(), buff, PROG_WORDS + 10);
    
    for (Uint32 w=0; w < PROG_WORDS; w++)
      CHECKEQUAL(0, prog.load_const_null(1));
    
    CHECKEQUAL(0, prog.interpret_exit_ok());
    CHECKEQUAL(0, prog.finalise());
    
    CHECKEQUAL(0, scan->setInterpretedCode(&prog));
    
    CHECKEQUAL(0, trans->execute(NdbTransaction::NoCommit));

    // Scan errors arrive asynchronously into the ScanOperation.
    // However, they should not become visible on the Transaction object
    // until after the nextResult-wait.
    CHECKEQUAL(0, trans->getNdbError().code);
    NdbSleep_MilliSleep(10);    // Not even after a long sleep.
    CHECKEQUAL(0, trans->getNdbError().code);

    CHECKEQUAL(-1, scan->nextResult());
    
    int expectedResult= subcase.expectedRc;
    CHECKEQUAL(expectedResult, scan->getNdbError().code);
    CHECKEQUAL(expectedResult, trans->getNdbError().code);

    scan->close();
    
    trans->close();
  }

  restarter.insertErrorInAllNodes(0);

  return NDBT_OK;
}

int create100Tables(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;

  for (Uint32 t=0; t < 100; t++)
  {
    char tabnameBuff[10];
    BaseString::snprintf(tabnameBuff, sizeof(tabnameBuff), "TAB%u", t);
    
    NdbDictionary::Table tab;
    tab.setName(tabnameBuff);
    NdbDictionary::Column pk;
    pk.setName("PK");
    pk.setType(NdbDictionary::Column::Varchar);
    pk.setLength(20);
    pk.setNullable(false);
    pk.setPrimaryKey(true);
    tab.addColumn(pk);

    pNdb->getDictionary()->dropTable(tab.getName());
    if(pNdb->getDictionary()->createTable(tab) != 0)
    {
      ndbout << "Create table failed with error : "
             << pNdb->getDictionary()->getNdbError().code
             << " "
             << pNdb->getDictionary()->getNdbError().message
             << endl;
      return NDBT_FAILED;
    }
    
    ndbout << "Created table " << tabnameBuff << endl;
  }

  return NDBT_OK;
}

int drop100Tables(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();

  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;
    
  for (Uint32 t=0; t < 100; t++)
  {
    char tabnameBuff[10];
    BaseString::snprintf(tabnameBuff, sizeof(tabnameBuff), "TAB%u", t);
    
    if (pNdb->getDictionary()->dropTable(tabnameBuff) != 0)
    {
      ndbout << "Drop table failed with error : "
             << pNdb->getDictionary()->getNdbError().code
             << " "
             << pNdb->getDictionary()->getNdbError().message
             << endl;
    }
    else
    {
      ndbout << "Dropped table " << tabnameBuff << endl;
    }
  }
  
  return NDBT_OK;
}

int dropTable(NDBT_Context* ctx, NDBT_Step* step, Uint32 num)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();

  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;
    
  char tabnameBuff[10];
  BaseString::snprintf(tabnameBuff, sizeof(tabnameBuff), "TAB%u", num);
  
  if (pNdb->getDictionary()->dropTable(tabnameBuff) != 0)
  {
    ndbout << "Drop table failed with error : "
           << pNdb->getDictionary()->getNdbError().code
           << " "
           << pNdb->getDictionary()->getNdbError().message
           << endl;
  }
  else
  {
    ndbout << "Dropped table " << tabnameBuff << endl;
  }
  
  return NDBT_OK;
}


enum Scenarios
{
//  NORMAL,  // Commented to save some time.
  DROP_TABLE,
  RESTART_MASTER,
  RESTART_SLAVE,
  NUM_SCENARIOS
};


enum Tasks
{
  WAIT = 0,
  DROP_TABLE_REQ = 1,
  MASTER_RESTART_REQ = 2,
  SLAVE_RESTART_REQ = 3
};

int testWorker(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(ctx->getTab()->getName(), "T1") != 0)
    return NDBT_OK;

  /* Worker step to run in a separate thread for
   * blocking activities
   * Generally the blocking of the DIH table definition flush
   * blocks the completion of the drop table/node restarts,
   * so this must be done in a separate thread to avoid
   * deadlocks.
   */
  
  while (!ctx->isTestStopped())
  {
    ndbout_c("Worker : waiting for request...");
    ctx->getPropertyWait("DIHWritesRequest", 1);
  
    if (!ctx->isTestStopped())
    {
      Uint32 req = ctx->getProperty("DIHWritesRequestType", (Uint32)0);

      switch ((Tasks) req)
      {
      case DROP_TABLE_REQ:
      {
        /* Drop table */
        ndbout_c("Worker : dropping table");
        if (dropTable(ctx, step, 2) != NDBT_OK)
        {
          return NDBT_FAILED;
        }
        ndbout_c("Worker : table dropped.");
        break;
      }
      case MASTER_RESTART_REQ:
      {
        ndbout_c("Worker : restarting Master");
        
        NdbRestarter restarter;
        int master_nodeid = restarter.getMasterNodeId();
        ndbout_c("Worker : Restarting Master (%d)...", master_nodeid);
        if (restarter.restartOneDbNode2(master_nodeid, 
                                        NdbRestarter::NRRF_NOSTART |
                                        NdbRestarter::NRRF_FORCE |
                                        NdbRestarter::NRRF_ABORT) ||
            restarter.waitNodesNoStart(&master_nodeid, 1) ||
            restarter.startAll())
        {
          ndbout_c("Worker : Error restarting Master.");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Waiting for master to recover...");
        if (restarter.waitNodesStarted(&master_nodeid, 1))
        {
          ndbout_c("Worker : Error waiting for Master restart");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Master recovered.");
        break;
      }
      case SLAVE_RESTART_REQ:
      {
        NdbRestarter restarter;
        int slave_nodeid = restarter.getRandomNotMasterNodeId(rand());
        ndbout_c("Worker : Restarting non-master (%d)...", slave_nodeid);
        if (restarter.restartOneDbNode2(slave_nodeid, 
                                        NdbRestarter::NRRF_NOSTART |
                                        NdbRestarter::NRRF_FORCE |
                                        NdbRestarter::NRRF_ABORT) ||
            restarter.waitNodesNoStart(&slave_nodeid, 1) ||
            restarter.startAll())
        {
          ndbout_c("Worker : Error restarting Slave.");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Waiting for replica to recover...");
        if (restarter.waitNodesStarted(&slave_nodeid, 1))
        {
          ndbout_c("Worker : Error waiting for Slave restart");
          return NDBT_FAILED;
        }
        ndbout_c("Worker : Slave recovered.");
        break;
      }
      default:
      { 
        break;
      }
      }
    }
    ctx->setProperty("DIHWritesRequestType", (Uint32) 0);
    ctx->setProperty("DIHWritesRequest", (Uint32) 2);
  }
  
  ndbout_c("Worker, done.");
  return NDBT_OK;
}

int testSlowDihFileWrites(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Testcase checks behaviour with slow flushing of DIH table definitions
   * This caused problems in the past by exhausting the DIH page pool
   * Now there's a concurrent operations limit.
   * Check that it behaves with many queued ops, parallel drop/node restarts
   */
  
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(ctx->getTab()->getName(), "T1") != 0)
    return NDBT_OK;

  /* 1. Activate slow write error insert
   * 2. Trigger LCP
   * 3. Wait some time, periodically producing info on 
   *    the internal state
   * 4. Perform some parallel action (drop table/node restarts)
   * 5. Wait some time, periodically producing info on 
   *    the internal state
   * 6. Clear the error insert
   * 7. Wait a little longer
   * 8. Done.
   */
  NdbRestarter restarter;

  for (Uint32 scenario = 0;  scenario < NUM_SCENARIOS; scenario++)
  {
    ndbout_c("Inserting error 7235");
    restarter.insertErrorInAllNodes(7235);
    
    ndbout_c("Triggering LCP");
    int dumpArg = 7099;
    restarter.dumpStateAllNodes(&dumpArg, 1);
    
    const Uint32 periodSeconds = 10;
    Uint32 waitPeriods = 6;
    dumpArg = 7032;
    
    for (Uint32 p=0; p<waitPeriods; p++)
    {
      if (p == 3)
      {
        switch ((Scenarios) scenario)
        {
        case DROP_TABLE:
        {
          /* Drop one of the early-created tables */
          ndbout_c("Requesting DROP TABLE");
          ctx->setProperty("DIHWritesRequestType", (Uint32) DROP_TABLE_REQ);
          ctx->setProperty("DIHWritesRequest", (Uint32) 1);
          break;
        }
        case RESTART_MASTER:
        {
          ndbout_c("Requesting Master restart");
          ctx->setProperty("DIHWritesRequestType", (Uint32) MASTER_RESTART_REQ);
          ctx->setProperty("DIHWritesRequest", (Uint32) 1);

          break;
        }
        case RESTART_SLAVE:
        {
          ndbout_c("Requesting Slave restart");
          ctx->setProperty("DIHWritesRequestType", (Uint32) SLAVE_RESTART_REQ);
          ctx->setProperty("DIHWritesRequest", (Uint32) 1);

          break;
        }
        default:
          break;
        }
      }

      ndbout_c("Dumping DIH page info to ndbd stdout");
      restarter.dumpStateAllNodes(&dumpArg, 1);
      NdbSleep_MilliSleep(periodSeconds * 1000);
    }
    
    ndbout_c("Clearing error insert...");
    restarter.insertErrorInAllNodes(0);
    
    waitPeriods = 2;
    for (Uint32 p=0; p<waitPeriods; p++)
    {
      ndbout_c("Dumping DIH page info to ndbd stdout");
      restarter.dumpStateAllNodes(&dumpArg, 1);
      NdbSleep_MilliSleep(periodSeconds * 1000);
    }
    
    ndbout_c("Waiting for worker to finish task...");
    ctx->getPropertyWait("DIHWritesRequest", 2);
    
    if (ctx->isTestStopped())
      return NDBT_OK;

    ndbout_c("Done.");
  }  

  /* Finish up */
  ctx->stopTest();

  return NDBT_OK;
}

int testNdbfsBulkOpen(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;

  g_err << "Getting all nodes to create + open a number of files in parallel"
        << endl;
  int dumpArg = 667;
  CHECK(restarter.dumpStateAllNodes(&dumpArg, 1) == 0);

  ndbout_c("Giving time for the open to complete");
  NdbSleep_MilliSleep(30*1000);

  ndbout_c("Crash DB nodes that have not completed opening files");
  dumpArg = 668;
  CHECK(restarter.dumpStateAllNodes(&dumpArg, 1) == 0);

  g_err << "Checking any data node crashed" << endl;
  uint num_nodes = restarter.getNumDbNodes();
  int *dead_nodes = new int[num_nodes];
  for (uint i = 0; i < num_nodes; ++i)
  {
    dead_nodes[i] = 0;
  }
  int dead_node = restarter.checkClusterAlive(dead_nodes, num_nodes);
  if (dead_node != 0)
  {
    g_err << "Data node " << dead_node << " crashed" << endl;
  }
  CHECK(dead_node == 0);

  g_err << "Restarting nodes to get rid of error insertion effects"
        << endl;
  // restartAll(initial=true) doesn't remove CMVMI either
  CHECK(restarter.restartAll() == 0);
  const int timeout = 300;
  CHECK(restarter.waitClusterStarted(timeout) == 0);
  Ndb* pNdb = GETNDB(step);
  CHECK(pNdb->waitUntilReady(timeout) == 0);
  CHK_NDB_READY(pNdb);

  return NDBT_OK;
}


NDBT_TESTSUITE(testLimits);

TESTCASE("ExhaustSegmentedSectionPk",
         "Test behaviour at Segmented Section exhaustion for PK"){
  INITIALIZER(testSegmentedSectionPk);
}

TESTCASE("ExhaustSegmentedSectionIX",
         "Test behaviour at Segmented Section exhaustion for Unique index"){
  INITIALIZER(testSegmentedSectionIx);
}
TESTCASE("ExhaustSegmentedSectionScan",
         "Test behaviour at Segmented Section exhaustion for Scan"){
  INITIALIZER(testSegmentedSectionScan);
}

TESTCASE("DropSignalFragments",
         "Test behaviour of Segmented Section exhaustion with fragmented signals"){
  INITIALIZER(testDropSignalFragments);
}

TESTCASE("SlowDihFileWrites",
         "Test behaviour of slow Dih table file writes")
{
  INITIALIZER(create100Tables);
  STEP(testWorker);
  STEP(testSlowDihFileWrites);
  FINALIZER(drop100Tables);
}
TESTCASE("NdbfsBulkOpen",
         "Test behaviour of NdbFs bulk file open")
{
  INITIALIZER(testNdbfsBulkOpen);
}

NDBT_TESTSUITE_END(testLimits)

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testLimits);
  return testLimits.execute(argc, argv);
}
