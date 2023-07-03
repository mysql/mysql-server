/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>
#include <ndb_opts.h>
#include <ndb_limits.h>

#include <NdbSleep.h>
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

static const char* opt_dbname = 0;
static bool opt_check_orphans = false;
static bool opt_delete_orphans = false;
static bool opt_check_missing = false;
static bool opt_add_missing = false;
static const char* opt_dump_file = 0;
static bool opt_verbose = false;

static FILE* g_dump_file = 0;
static FileOutputStream* g_dump_out = 0;
static NdbOut g_dump;

static Ndb_cluster_connection* g_ncc = 0;
static Ndb* g_ndb = 0;
static NdbDictionary::Dictionary* g_dic = 0;

static const char* g_tabname = 0;
static const NdbDictionary::Table* g_tab = 0;

struct Pk { // table pk
  const char* colname;
  Pk() {
    colname = 0;
  }
  ~Pk() {
    delete [] colname;
  }
};
static Pk* g_pklist = 0;
static int g_pkcount = 0;

struct Blob { // blob column and table
  int blobno;
  int colno;
  const char* colname;
  const char* blobname;
  const NdbDictionary::Column* blobcol;
  const NdbDictionary::Table* blobtab;
  Blob() {
    blobno = -1;
    colno = -1;
    colname = 0;
    blobname = 0;
    blobcol = 0;
    blobtab = 0;
  }
  ~Blob() {
    delete [] colname;
    delete [] blobname;
  }
};
static Blob* g_bloblist = 0;
static int g_blobcount = 0;

static NdbTransaction* g_scantx = 0;
static NdbScanOperation* g_scanop = 0;

struct Val { // attr value scanned from blob
  const char* colname;
  NdbRecAttr* ra;
  Val() {
    colname = 0;
    ra = 0;
  }
  ~Val() {
    delete [] colname;
  }
};
static Val* g_vallist = 0;
static int g_valcount = 0;

#define CHK1(b) \
  if (!(b)) { \
    ret = -1; \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    ret = -1; \
    break; \
  }

// re-inventing strdup
static const char*
newstr(const char* s)
{
  require(s != 0);
  char* s2 = new char [strlen(s) + 1];
  strcpy(s2, s);
  return s2;
}

static NdbError
getNdbError(Ndb_cluster_connection* ncc)
{
  NdbError err;
  err.code = g_ncc->get_latest_error();
  err.message = g_ncc->get_latest_error_msg();
  return err;
}

static int
doconnect()
{
  int ret = 0;
  do
  {
    g_ncc = new Ndb_cluster_connection(opt_ndb_connectstring);
    CHK2(g_ncc->connect(opt_connect_retries - 1, opt_connect_retry_delay) == 0, getNdbError(g_ncc));
    CHK2(g_ncc->wait_until_ready(30, 10) == 0, getNdbError(g_ncc));

    g_ndb = new Ndb(g_ncc, opt_dbname);
    CHK2(g_ndb->init() == 0, g_ndb->getNdbError());
    CHK2(g_ndb->waitUntilReady(30) == 0, g_ndb->getNdbError());
    g_dic = g_ndb->getDictionary();

    g_info << "Connected" << endl;
  } while (false);
  return ret;
}

static void
dodisconnect()
{
  delete g_ndb;
  delete g_ncc;
  g_info << "Disconnected" << endl;
}

static void scanblobheadsclose();

static int
scanblobheadsstart()
{
  int ret = 0;
  for (int retries = 0; retries < 10; retries++)
  {
    ret = 0;
    do
    {
      require(g_scantx == 0);
      g_scantx = g_ndb->startTransaction();
      CHK2(g_scantx != 0, g_ndb->getNdbError());

      g_scanop = g_scantx->getNdbScanOperation(g_tab);
      CHK2(g_scanop != 0, g_scantx->getNdbError());

      const NdbOperation::LockMode lm = NdbOperation::LM_Read;
      CHK2(g_scanop->readTuples(lm) == 0, g_scanop->getNdbError());

      for (int i = 0; i < g_pkcount; i++)
      {
        Val& v = g_vallist[i];
        v.ra = g_scanop->getValue(v.colname);
        CHK2(v.ra != 0, v.colname << ": " << g_scanop->getNdbError());
      }
      CHK1(ret == 0);

      /* Request all blob heads */
      for (int i=0; i < g_blobcount; i++)
      {
        const Blob& b = g_bloblist[i];
        NdbBlob* blobHandle = g_scanop->getBlobHandle(b.colno);
        CHK2(blobHandle != NULL, g_scanop->getNdbError());
      }

      CHK2(g_scantx->execute(NoCommit) == 0, g_scantx->getNdbError());
    } while (false);
    NdbError error = g_scantx->getNdbError();
    if (error.code == 0 || error.status != NdbError::TemporaryError)
      break;
    scanblobheadsclose(); // reset to original state for retry on temp err
  }
  return ret; // set to -1 on error in CHK2
}

static int
scanblobheadsnext(int *res)
{
  int ret = 0;
  do
  {
    *res = g_scanop->nextResult();
    CHK2(*res == 0 || *res == 1, g_scanop->getNdbError());
  } while (false);
  return ret; // set to -1 on error in CHK2
}

static void
scanblobheadsclose()
{
  if (g_scantx != 0)
  {
    g_ndb->closeTransaction(g_scantx);
    g_scantx = 0;
  }
}

static int
writepart(const Blob& b,
          const Uint32 p,
          const Uint64 expectedLength)
{
  int ret = 0;

  /**
   * Use some magic to force-insert a part
   */
  if (unlikely(b.blobcol->getStripeSize() != 0))
  {
    g_err << "Error : Blob column "
          << b.blobcol->getName()
          << " uses striping - insert not yet "
          << "supported by blob tool."
          << endl;
    return -1;
  };

  if (unlikely(b.blobcol->getBlobVersion() == NDB_BLOB_V1))
  {
    g_err << "Error : Blob column "
          << b.blobcol->getName()
          << " is a v1 Blob, not yet "
          << "supported by blob tool."
          << endl;
    return -1;
  }

  char padchar = 0;
  if (b.blobcol->getType() == NDB_TYPE_TEXT)
  {
    padchar = ' ';
  }
  const Uint32 bufSize = MAX_TUPLE_SIZE_IN_WORDS << 2;
  char buf[bufSize];
  memset(buf, padchar, bufSize);
  buf[0] = expectedLength & 0xff;
  buf[1] = (expectedLength >> 8) & 0xff;

  for (int retries = 0; retries < 10; retries++)
  {
    NdbTransaction *tx = 0;
    NdbOperation *writeOp = 0;
    ret = 0;
    do
    {
      tx = g_ndb->startTransaction();
      CHK2(tx != 0, g_ndb->getNdbError());

      writeOp = tx->getNdbOperation(b.blobtab);
      CHK2(writeOp != NULL, tx->getNdbError());
      CHK2(writeOp->writeTuple() == 0, writeOp->getNdbError());

      /* Main table PK */
      for (int i = 0; i < g_pkcount; i++)
      {
        Val& v = g_vallist[i];
        require(v.ra != 0);
        require(v.ra->isNULL() == 0);
        const char* data = v.ra->aRef();
        CHK2(writeOp->equal(v.colname, data) == 0, writeOp->getNdbError());
      }

      /* NDB$DIST == 0 */

      /* Part number */
      CHK2(writeOp->equal("NDB$PART", p) == 0, writeOp->getNdbError());

      /* Set NDB$PKID */
      CHK2(writeOp->setValue("NDB$PKID", 0) == 0, writeOp->getNdbError());

      /* Set NDB$DATA */
      CHK2(writeOp->setValue("NDB$DATA", buf) == 0, writeOp->getNdbError());

      CHK2(tx->execute(Commit) == 0, tx->getNdbError());
    } while (false);
    const NdbError error = tx->getNdbError();
    tx->close();
    if (error.code == 0 || error.status != NdbError::TemporaryError)
      break;
  }
  return ret; // set to -1 on error in CHK2
}

static int
checkpart(const Blob &b,
          bool *partOk,
          const Uint32 p,
          const Uint32 expectedLength)
{
  Uint32 ret = 0;
  const int inlineSize = b.blobcol->getInlineSize();
  const int partSize = b.blobcol->getPartSize();

  /* We use the normal blob handle to read the blob
   * but aligned to part boundaries as a way to
   * probe for missing parts
   */
  for (int retries = 0; retries < 10; retries++)
  {
    NdbTransaction *tx = 0;
    NdbOperation *op = 0;
    ret = 0;
    do
    {
      tx = g_ndb->startTransaction();
      CHK2(tx != 0, g_ndb->getNdbError());

      op = tx->getNdbOperation(g_tab);
      CHK2(op != 0, tx->getNdbError());

      const NdbOperation::LockMode lm = NdbOperation::LM_Read;
      CHK2(op->readTuple(lm) == 0, op->getNdbError());

      /* PK */
      for (int i = 0; i < g_pkcount; i++)
      {
        Val& v = g_vallist[i];
        require(v.ra != 0);
        require(v.ra->isNULL() == 0);
        const char* data = v.ra->aRef();
        CHK2(op->equal(v.colname, data) == 0, op->getNdbError());
      }

      /* Get blob handle and execute op */
      NdbBlob* partReadBlobHandle = op->getBlobHandle(b.colno);
      CHK2(partReadBlobHandle != NULL, op->getNdbError());

      CHK2(tx->execute(NoCommit) == 0, tx->getNdbError());

      /* Attempt to read part in isolation */
      const Uint64 offset = inlineSize + (p * Uint64(partSize));

      CHK2(partReadBlobHandle->setPos(offset) == 0,
           partReadBlobHandle->getNdbError());

      const Uint32 bufSize = MAX_TUPLE_SIZE_IN_WORDS << 2;
      char buf[bufSize];

      /* We treat failure to read as a missing part */
      Uint32 bytesRead = partSize;
      if (partReadBlobHandle->readData(buf, bytesRead) != 0)
      {
	const NdbError error = tx->getNdbError();
	if (error.status == NdbError::TemporaryError) {break;}

        if (error.code == 4267 || error.code == 626) {
          g_info << "Part not found" << endl;
          /* Missing part */
          *partOk = false;
        }
        else {
          g_err << "Unexpected error on reading part"
                << p
                << endl;
          g_err << error << endl;
          ret = -1;
        }
      }
      else
      {
        if (tx->execute(Commit) != 0)
        {
	  const NdbError error = tx->getNdbError();
	  if (error.code == 4267 || error.code == 626)
          {
	    g_info << "Part not found" << endl;
            /* Missing part */
            *partOk = false;
          }
          else
          {
            g_err << "Unexpected error on commiting read-part "
                  << p
                  << endl;
            g_err << error << endl;
            ret = -1;
          }
        }
      }
    } while (false);

    if (tx != nullptr) {
      const NdbError error = tx->getNdbError();
      tx->close();

      if (error.code == 0 || error.status != NdbError::TemporaryError)
	break;
    }
  }
  return ret; // set to -1 on error in CHK2
}

static int
processblobmissing(const Blob &b, Uint64 *missingParts, Uint64 *missingBytes, bool *blobOk)
{
  /* Check out the part sanity based on the blob length */
  int ret = 0;
  do
  {
    NdbBlob* blobHandle = g_scanop->getBlobHandle(b.colno);
    CHK2(blobHandle != NULL, g_scanop->getNdbError());

    int isNull = 0;

    CHK2(blobHandle->getNull(isNull) == 0, blobHandle->getNdbError());
    CHK1(isNull != -1); // Must not be undefined

    if (isNull == 1)
    {
      return 0;  // Null, no parts
    }

    Uint64 length = 0;
    CHK2(blobHandle->getLength(length) == 0, blobHandle->getNdbError());

    const int inlineSize = b.blobcol->getInlineSize();
    const int partSize = b.blobcol->getPartSize();

    if (length <= Uint64(inlineSize))
    {
      return 0; // Only inline data
    }

    length -= inlineSize;
  const Uint64 numParts = (length + partSize -1) / partSize;
  const Uint64 lastPartBytes = length % partSize;

  /* Check parts */
  for (Uint32 p=0; p < numParts; p++)
  {
    const bool lastPart = (p == numParts - 1);
    const Uint32 partBytes = (lastPart ? lastPartBytes : partSize);
    bool partOk = true;

    CHK1(checkpart(b,
                   &partOk,
                   p,
                   partBytes) == 0);

    if (!partOk)
    {
      *blobOk = false;
      (*missingParts)++;
      (*missingBytes)+= partBytes;

      if (opt_dump_file)
      {
        g_dump << "Column: " << b.colname
               << " Blob: " << b.blobname
               << " Key: (";

        for (int i = 0; i < g_pkcount; i++)
        {
          const Val& v = g_vallist[i];
          g_dump << *v.ra;
          if (i + 1 < g_pkcount)
            g_dump << ";";
        }
        g_dump << ") ";
        const Uint64 partOffset = inlineSize +
          (p * Uint64(partSize));

        g_dump << "Missing part: " << p
               << " Byte range : "
               << partOffset
               << " - "
               << partOffset + partSize
               << endl;
      }

      if (opt_add_missing)
      {
        ret = writepart(b,
                        p,
                        partBytes);

        if (ret == 0)
        {
          /* Double-check that we can now read it */
          partOk = true;
          CHK1(checkpart(b,
                         &partOk,
                         p,
                         partBytes) == 0);
          if (!partOk)
          {
              g_err << "Failed to read part "
                    << p
                    << " after successful write."
                    << endl;
              ret = -1;
              break;
            }
          }

          if (opt_dump_file)
          {
            g_dump << "  Part " << p
                   << " inserted with blank data."
                   << endl;
          }
        }
      }
    }
  } while (false);
  return ret; // set to -1 on error in CHK2
}

static int
domissing()
{
  if (!(opt_check_missing || opt_add_missing))
  {
    return 0;
  }

  int ret = 0;
  do
  {
    /**
     * Scan table, reading blob heads
     * Then check each blob in turn for missing parts
     */

    if (opt_dump_file)
    {
      g_dump << "Missing parts check" << endl;
    }

    Uint64 rowCount = 0;
    Uint64 brokenRowCount = 0;
    Uint64 brokenBlobCount = 0;
    Uint64 totMissingPartCount = 0;
    Uint64 totMissingByteCount = 0;

    CHK1(scanblobheadsstart() == 0);
    while (true)
    {
      int res;
      res = -1;
      CHK1(scanblobheadsnext(&res) == 0);
      if (res != 0)
        break;
      rowCount++;

      bool rowOk = true;
      for (int i=0; i < g_blobcount; i++)
      {
        const Blob& b = g_bloblist[i];
        bool blobOk = true;
        CHK1(processblobmissing(b,
                             &totMissingPartCount,
                             &totMissingByteCount,
                             &blobOk) == 0);
        if (unlikely(!blobOk))
        {
          rowOk = false;
          brokenBlobCount++;
        }
      }

      if (!rowOk)
      {
        brokenRowCount++;
      }
    }
    CHK1(ret == 0);

    g_err << endl;
    g_err << "Total rows in table: " << rowCount << endl;
    g_err << "Rows with blobs with missing part(s): " << brokenRowCount << endl;
    g_err << "Blobs with missing part(s): " << brokenBlobCount << endl;
    g_err << "Total missing part(s): " << totMissingPartCount << endl;
    g_err << "Total missing byte(s): " << totMissingByteCount << endl;
    g_err << endl;
    if (opt_dump_file)
    {
      g_dump << endl;
      g_dump << "Total rows in table: " << rowCount << endl;
      g_dump << "Rows with blobs with missing part(s): " << brokenRowCount << endl;
      g_dump << "Blobs with missing part(s): " << brokenBlobCount << endl;
      g_dump << "Total missing part(s): " << totMissingPartCount << endl;
      g_dump << "Total missing byte(s): " << totMissingByteCount << endl;
      g_dump << endl;
    }

    if (opt_add_missing)
    {
      g_err << "Total part(s) added: " << totMissingPartCount << endl;
      g_err << endl;
      if (opt_dump_file)
      {
        g_dump << "Total part(s) added: " << totMissingPartCount << endl;
        g_dump << endl;
      }
    }
    else
    {
      /* checking - return -1 if there are any rows with problems */
      if (brokenRowCount > 0)
      {
        ret = -1;
      }
    }
  } while (false);
  scanblobheadsclose();
  return ret; // set to -1 on error in CHK2
}

static int
scanblobpartsstart(const Blob& b)
{
  int ret = 0;

  do
  {
    require(g_scantx == 0);
    g_scantx = g_ndb->startTransaction();
    CHK2(g_scantx != 0, g_ndb->getNdbError());

    g_scanop = g_scantx->getNdbScanOperation(b.blobtab);
    CHK2(g_scanop != 0, g_scantx->getNdbError());

    const NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
    CHK2(g_scanop->readTuples(lm) == 0, g_scanop->getNdbError());

    for (int i = 0; i < g_valcount; i++)
    {
      Val& v = g_vallist[i];
      v.ra = g_scanop->getValue(v.colname);
      CHK2(v.ra != 0, v.colname << ": " << g_scanop->getNdbError());
    }
    CHK1(ret == 0);

    CHK2(g_scantx->execute(NoCommit) == 0, g_scantx->getNdbError());
  } while (false);
  return ret; // set to -1 on error in CHK2
}

static int
scanblobpartsnext(const Blob& b, int *res)
{
  int ret = 0;
  do
  {
    *res = g_scanop->nextResult();
    CHK2((*res == 0) || (*res == 1), g_scanop->getNdbError());
    g_info << b.blobname << ": nextResult: res=" << *res << endl;
  } while (false);
  return ret; // set to -1 on error in CHK2
}

static void
scanblobpartsclose(const Blob& b)
{
  if (g_scantx != 0)
  {
    g_ndb->closeTransaction(g_scantx);
    g_scantx = 0;
  }
}

static int
checkorphan(const Blob&b, const Uint64 minLength, int *res)
{
  int ret = 0;
  for (int retries = 0; retries < 10; retries++)
  {
    NdbTransaction *tx = 0;
    NdbOperation *op = 0;
    ret = 0;
    do
    {
      tx = g_ndb->startTransaction();
      CHK2(tx != 0, g_ndb->getNdbError());
      op = tx->getNdbOperation(g_tab);
      CHK2(op != 0, tx->getNdbError());

      const NdbOperation::LockMode lm = NdbOperation::LM_Read;
      CHK2(op->readTuple(lm) == 0, op->getNdbError());

      for (int i = 0; i < g_pkcount; i++)
      {
        Val& v = g_vallist[i];
        require(v.ra != 0);
        require(v.ra->isNULL() == 0);
        const char* data = v.ra->aRef();
        CHK2(op->equal(v.colname, data) == 0, op->getNdbError());
      }
      CHK1(ret == 0);

      NdbBlob* blobHandle = op->getBlobHandle(b.colno);

      tx->execute(Commit);
      if (tx->getNdbError().code == 626)
      {
        g_info << "parent not found" << endl;
        *res = 1; // not found
        break;
      }
      else
      {
        CHK2(tx->getNdbError().code == 0, tx->getNdbError());
        Uint64 blobLength;
        CHK2(blobHandle->getLength(blobLength) == 0, blobHandle->getNdbError());

        // if (blobLength > 2)
        //{
        //  blobLength = blobLength/2;
        //}

        if (blobLength < minLength)
        {
          g_info << "parent too short : "
                 << blobLength << " < "
                 << minLength
                 << endl;
          *res = 1;
        }
        else
        {
          *res = 0; // found
        }
      }
    } while (false);
    const NdbError error = tx->getNdbError();
    tx->close();
    if (error.code == 0 || error.status != NdbError::TemporaryError)
      break;
  }
  return ret; // set to -1 on error in CHK2
}

static int
deleteorphan(const Blob& b)
{
  int ret = 0;
  for (int retries = 0; retries < 10; retries++)
  {
    NdbTransaction *tx = 0;
    ret = 0;
    do
    {
      tx = g_ndb->startTransaction();
      CHK2(tx != 0, g_ndb->getNdbError());
      CHK2(g_scanop->deleteCurrentTuple(tx) == 0, g_scanop->getNdbError());
      CHK2(tx->execute(Commit) == 0, tx->getNdbError());
    } while (false);
    const NdbError error = tx->getNdbError();
    tx->close();
    if (error.code == 0 || error.status != NdbError::TemporaryError)
      break;
  }
  return ret; // set to -1 on error in CHK2
}

static int
doorphan(const Blob& b)
{
  if (!(opt_check_orphans || opt_delete_orphans))
  {
    return 0;
  }

  const int inlineSize = b.blobcol->getInlineSize();
  const int partSize = b.blobcol->getPartSize();

  int ret = 0;
  do
  {
    g_err << "Checking for orphan parts on blob #" << b.blobno << " " << b.colname
          << " " << b.blobname << endl;

    if (opt_dump_file)
    {
      g_dump << "Orphan parts check" << endl;
      g_dump << "Column: " << b.colname << endl;
      g_dump << "Blob: " << b.blobname << endl;
      g_dump << "Orphans (table key; blob part number):" << endl;
    }

    int totcount = 0;
    int orphancount = 0;

    CHK1(scanblobpartsstart(b) == 0);
    while (1)
    {
      int res;
      res = -1;
      CHK1(scanblobpartsnext(b, &res) == 0);
      if (res != 0)
        break;
      totcount++;

      /**
       * Calculate minimum blob length required for this part
       * to need to exist
       */
      Uint32 partNum = g_vallist[g_valcount - 1].ra->u_32_value();
      Uint64 minLength = inlineSize +
        (partNum * Uint64(partSize)) + 1;

      res = -1;
      CHK1(checkorphan(b, minLength, &res) == 0);
      if (res != 0)
      {
        orphancount++;
        if (opt_dump_file)
        {
          g_dump << "Key: ";
          for (int i = 0; i < g_valcount; i++)
          {
            const Val& v = g_vallist[i];
            g_dump << *v.ra;
            if (i + 1 < g_valcount)
              g_dump << ";";
          }
          g_dump << endl;
        }
        if (opt_delete_orphans)
        {
          CHK1(deleteorphan(b) == 0);
        }
      }
    }
    CHK1(ret == 0);

    const Uint64 orphanbytes = orphancount * b.blobcol->getPartSize();
    g_err << "Total parts: " << totcount << endl;
    g_err << "Orphan parts: " << orphancount << endl;
    g_err << "Orphan bytes: " << orphanbytes << endl;
    g_err << endl;
    if (opt_dump_file)
    {
      g_dump << "Total parts: " << totcount << endl;
      g_dump << "Orphan parts: " << orphancount << endl;
      g_dump << "Orphan bytes: " << orphanbytes << endl;
      g_dump << endl;
    }

    if (!opt_delete_orphans)
    {
      if (orphancount > 0)
      {
        ret = -1;
      }
    }
  } while (false);

  scanblobpartsclose(b);
  return ret;
}

static int
doblobs()
{
  g_err << "Processing " << g_blobcount
        << " blobs in table " << g_tabname
        << endl;

  /* domissing handles all blobs in one pass */
  int missing_ret = domissing();

  int doorphan_ret = 0;
  /* doorphan has one pass per blob */
  for (int i = 0; i < g_blobcount; i++)
  {
    const Blob& b = g_bloblist[i];
    doorphan_ret |= doorphan(b);
  }
  return (missing_ret | doorphan_ret);
}

static int
isblob(const NdbDictionary::Column* c)
{
  if (c->getType() == NdbDictionary::Column::Blob ||
      c->getType() == NdbDictionary::Column::Text)
    if (c->getPartSize() != 0)
      return 1;
  return 0;
}

static int
getobjs()
{
  int ret = 0;
  do
  {
    g_tab = g_dic->getTable(g_tabname);
    CHK2(g_tab != 0, g_tabname << ": " << g_dic->getNdbError());
    const int tabid = g_tab->getObjectId();
    const int ncol = g_tab->getNoOfColumns();

    g_pklist = new Pk [NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
    for (int i = 0; i < ncol; i++)
    {
      const NdbDictionary::Column* c = g_tab->getColumn(i);
      if (c->getPrimaryKey())
      {
        Pk& p = g_pklist[g_pkcount++];
        const char* colname = c->getName();
        p.colname = newstr(colname);
      }
    }
    require(g_pkcount != 0 && g_pkcount == g_tab->getNoOfPrimaryKeys());

    g_valcount = g_pkcount + 1;
    g_vallist = new Val [g_valcount];
    for (int i = 0; i < g_valcount; i++)
    {
      Val& v = g_vallist[i];
      if (i < g_pkcount)
      {
        const Pk& p = g_pklist[i];
        v.colname = newstr(p.colname);
      }
      if (i == g_pkcount + 0) // first blob attr to scan
      {
        v.colname = newstr("NDB$PART");
      }
    }

    if (g_blobcount == 0)
    {
      for (int i = 0; i < ncol; i++)
      {
        const NdbDictionary::Column* c = g_tab->getColumn(i);
        if (isblob(c))
        {
          Blob& b = g_bloblist[g_blobcount++];
          const char* colname = c->getName();
          b.colname = newstr(colname);
        }
      }
    }

    for (int i = 0; i < g_blobcount; i++)
    {
      Blob& b = g_bloblist[i];
      b.blobno = i;
      const NdbDictionary::Column* c = g_tab->getColumn(b.colname);
      CHK2(c != 0, g_tabname << ": " << b.colname << ": no such column");
      CHK2(isblob(c), g_tabname << ": " << b.colname << ": not a blob");
      b.blobcol = c;
      b.colno = c->getColumnNo();
      {
        char blobname[100];
        sprintf(blobname, "NDB$BLOB_%d_%d", tabid, b.colno);
        b.blobname = newstr(blobname);
      }
      b.blobtab = g_dic->getTable(b.blobname);
      CHK2(b.blobtab != 0, g_tabname << ": " << b.colname << ": " << b.blobname << ": " << g_dic->getNdbError());
    }
    CHK1(ret == 0);
  } while (false);
  return ret;
}

static int
doall()
{
  int ret = 0;
  do
  {
    if (opt_dump_file)
    {
      g_dump_file = fopen(opt_dump_file, "w");
      CHK2(g_dump_file != 0, opt_dump_file << ": " << strerror(errno));
      g_dump_out = new FileOutputStream(g_dump_file);
      new (&g_dump) NdbOut(*g_dump_out);

      g_dump << "table: " << g_tabname << endl;
      g_dump << "actions: ";
      if (opt_check_orphans)
        g_dump << "check-orphans ";
      if (opt_delete_orphans)
        g_dump << "delete-orphans ";
      if (opt_check_missing)
        g_dump << "check-missing ";
      if (opt_add_missing)
        g_dump << "add-missing";

      g_dump << endl << endl;
    }
    CHK1(doconnect() == 0);
    CHK1(getobjs() == 0);
    if (g_blobcount == 0)
    {
      g_err << g_tabname << ": no blob columns" << endl;
      break;
    }
    CHK1(doblobs() == 0);
  } while (false);

  dodisconnect();
  if (g_dump_file != 0)
  {
    g_dump << "result: "<< (ret == 0 ? "ok" : "failed") << endl;
    flush(g_dump);
    if (fclose(g_dump_file) != 0)
    {
      g_err << opt_dump_file << ": write failed: " << strerror(errno) << endl;
    }
    g_dump_file = 0;
  }
  return ret;
}

static struct my_option
my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "database", 'd', "Name of database table is in",
    &opt_dbname, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "check-orphans", NDB_OPT_NOSHORT, "Check for orphan blob parts",
    &opt_check_orphans, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "delete-orphans", NDB_OPT_NOSHORT, "Delete orphan blob parts",
    &opt_delete_orphans, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "check-missing", NDB_OPT_NOSHORT, "Check for missing Blob parts",
    &opt_check_missing, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "add-missing", NDB_OPT_NOSHORT, "Write missing Blob parts",
    &opt_add_missing, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "dump-file", NDB_OPT_NOSHORT,
   "Write orphan keys (table key and part number) into file",
    &opt_dump_file, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  { "verbose", 'v', "Verbose messages",
    &opt_verbose, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options
};

static void
short_usage_sub()
{
  ndb_short_usage_sub("table [blobcolumn..]");
  printf("Default is to process all blob/text columns in table\n");
  printf("(1) Check for orphan parts with --check-orphans --dump=out1.txt\n");
  printf("(2) Delete orphan parts with --delete-orphans --dump=out2.txt\n");
  printf("(3) Check for missing parts with --check-missing --dump=out3.txt\n");
  printf("(4) Add missing parts with --add-missing --dump=out4.txt\n");
  printf("\n");
}

static void
usage()
{
  printf("%s: check and repair blobs\n", my_progname);
}

static int
checkopts(int argc, char** argv)
{
  if (opt_dbname == 0)
    opt_dbname = "TEST_DB";

  if (argc < 1)
  {
    g_err << "Table name required" << endl;
    usage();
    return 1;
  }
  g_tabname = newstr(argv[0]);

  g_bloblist = new Blob [NDB_MAX_ATTRIBUTES_IN_TABLE];
  g_blobcount = argc - 1;
  for (int i = 0; i < g_blobcount; i++)
  {
    Blob& b = g_bloblist[i];
    b.colname = newstr(argv[1 + i]);
  }

  if (! (opt_check_orphans ||
         opt_delete_orphans ||
         opt_check_missing ||
         opt_add_missing))
  {
    g_err << "Action (--check-orphans etc) required" << endl;
    usage();
    return 1;
  }
  return 0;
}

static void
freeall()
{
  delete [] g_tabname;
  delete [] g_pklist;
  delete [] g_bloblist;
  delete [] g_vallist;

  delete g_dump_out;
  if (g_dump_file != 0)
    (void)fclose(g_dump_file);
}

int
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub, usage);
  int ret = opts.handle_options();
  if (ret != 0 || checkopts(argc, argv) != 0)
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  setOutputLevel(opt_verbose ? 2 : 0);

  ret = doall();
  freeall();
  if (ret == -1)
    return NDBT_ProgramExit(NDBT_FAILED);
  return NDBT_ProgramExit(NDBT_OK);
}
