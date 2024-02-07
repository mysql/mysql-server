/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUP_C
#define DBTUP_DEBUG_CPP
#include <ndb_limits.h>
#include <RefConvert.hpp>
#include <Vector.hpp>
#include <pc.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EventReport.hpp>
#include "Dbtup.hpp"

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include "AttributeOffset.hpp"
#ifdef TEST_MR
#include <time.h>
#endif

#define JAM_FILE_ID 411

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ------------------------ DEBUG MODULE -------------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */
void Dbtup::execDEBUG_SIG(Signal *signal) {
  PagePtr regPagePtr;
  jamEntry();
  regPagePtr.i = signal->theData[0];
  c_page_pool.getPtr(regPagePtr);
}  // Dbtup::execDEBUG_SIG()

#ifdef TEST_MR

void startTimer(struct timespec *tp) {
  clock_gettime(CLOCK_REALTIME, tp);
}  // startTimer()

int stopTimer(struct timespec *tp) {
  double timer_count;
  struct timespec theStopTime;
  clock_gettime(CLOCK_REALTIME, &theStopTime);
  timer_count =
      (double)(1000000 * ((double)theStopTime.tv_sec - (double)tp->tv_sec)) +
      (double)((double)((double)theStopTime.tv_nsec - (double)tp->tv_nsec) /
               (double)1000);
  return (int)timer_count;
}  // stopTimer()

#endif  // end TEST_MR

struct Chunk {
  Uint32 pageId;
  Uint32 pageCount;
};

void Dbtup::execDBINFO_SCANREQ(Signal *signal) {
  jamEntry();
  DbinfoScanReq req = *(DbinfoScanReq *)signal->theData;
  const Ndbinfo::ScanCursor *cursor =
      CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));

  Ndbinfo::Ratelimit rl;

  switch (req.tableId) {
    case Ndbinfo::POOLS_TABLEID: {
      jam();
      const DynArr256Pool::Info pmpInfo = c_page_map_pool.getInfo();

      const Ndbinfo::pool_entry pools[] = {
          {"Scan Lock",
           c_scanLockPool.getUsed(),
           c_scanLockPool.getSize(),
           c_scanLockPool.getEntrySize(),
           c_scanLockPool.getUsedHi(),
           {CFG_DB_NO_LOCAL_SCANS, CFG_DB_BATCH_SIZE, 0, 0},
           0},
          {"Scan Operation",
           c_scanOpPool.getUsed(),
           c_scanOpPool.getSize(),
           c_scanOpPool.getEntrySize(),
           c_scanOpPool.getUsedHi(),
           {CFG_DB_NO_LOCAL_SCANS, 0, 0, 0},
           0},
          {"Trigger",
           c_triggerPool.getUsed(),
           c_triggerPool.getSize(),
           c_triggerPool.getEntrySize(),
           c_triggerPool.getUsedHi(),
           {CFG_DB_NO_TRIGGERS, 0, 0, 0},
           0},
          {"Stored Proc",
           c_storedProcPool.getUsed(),
           c_storedProcPool.getSize(),
           c_storedProcPool.getEntrySize(),
           c_storedProcPool.getUsedHi(),
           {CFG_DB_NO_LOCAL_SCANS, 0, 0, 0},
           0},
          {"Build Index",
           c_buildIndexPool.getUsed(),
           c_buildIndexPool.getSize(),
           c_buildIndexPool.getEntrySize(),
           c_buildIndexPool.getUsedHi(),
           {0, 0, 0, 0},
           0},
          {"Operation",
           c_operation_pool.getUsed(),
           c_operation_pool.getSize(),
           c_operation_pool.getEntrySize(),
           c_operation_pool.getUsedHi(),
           {CFG_DB_NO_LOCAL_OPS, CFG_DB_NO_OPS, 0, 0},
           0},
          {"L2PMap pages",
           pmpInfo.pg_count,
           0, /* No real limit */
           pmpInfo.pg_byte_sz,
           /*
             No HWM for this row as it would be a fixed fraction of "Data
             memory" and therefore of limited interest.
           */
           0,
           {0, 0, 0},
           RG_DATAMEM},
          {"L2PMap nodes",
           pmpInfo.inuse_nodes,
           pmpInfo.pg_count *
               pmpInfo.nodes_per_page, /* Max within current pages */
           pmpInfo.node_byte_sz,
           /*
             No HWM for this row as it would be a fixed fraction of "Data
             memory" and therefore of limited interest.
           */
           0,
           {0, 0, 0},
           RT_DBTUP_PAGE_MAP},
          {"Data memory",
           m_pages_allocated,
           0,  // Allocated from global resource group RG_DATAMEM
           sizeof(Page),
           m_pages_allocated_max,
           {CFG_DB_DATA_MEM, 0, 0, 0},
           0},
          {NULL, 0, 0, 0, 0, {0, 0, 0, 0}, 0}};

      const size_t num_config_params =
          sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
      const Uint32 numPools = NDB_ARRAY_SIZE(pools);
      Uint32 pool = cursor->data[0];
      ndbrequire(pool < numPools);
      BlockNumber bn = blockToMain(number());
      while (pools[pool].poolname) {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(bn);          // block number
        row.write_uint32(instance());  // block instance
        row.write_string(pools[pool].poolname);

        row.write_uint64(pools[pool].used);
        row.write_uint64(pools[pool].total);
        row.write_uint64(pools[pool].used_hi);
        row.write_uint64(pools[pool].entry_size);
        for (size_t i = 0; i < num_config_params; i++)
          row.write_uint32(pools[pool].config_params[i]);
        row.write_uint32(GET_RG(pools[pool].record_type));
        row.write_uint32(GET_TID(pools[pool].record_type));
        ndbinfo_send_row(signal, req, row, rl);
        pool++;
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pool);
          return;
        }
      }
      break;
    }
    case Ndbinfo::TEST_TABLEID: {
      Uint32 counter = cursor->data[0];
      BlockNumber bn = blockToMain(number());
      while (counter < 1000) {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(bn);          // block number
        row.write_uint32(instance());  // block instance
        row.write_uint32(counter);
        Uint64 counter2 = counter;
        counter2 = counter2 << 32;
        row.write_uint64(counter2);
        ndbinfo_send_row(signal, req, row, rl);
        counter++;
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, counter);
          return;
        }
      }
      break;
    }
    default:
      break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

#ifdef VM_TRACE
static Uint32 fc_left = 0, fc_right = 0, fc_remove = 0;
#endif

void Dbtup::execDUMP_STATE_ORD(Signal *signal) {
  Uint32 type = signal->theData[0];

  (void)type;
#if 0
  if (type == 100) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 2;
    req->secondaryTableId = RNIL;
    req->userPtr = 2;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 101) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 4;
    req->secondaryTableId = 5;
    req->userPtr = 4;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 102) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 6;
    req->secondaryTableId = 8;
    req->userPtr = 6;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 103) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 2;
    req->secondaryTableId = RNIL;
    req->userPtr = 2;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
  if (type == 104) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 4;
    req->secondaryTableId = 5;
    req->userPtr = 4;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
  if (type == 105) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 6;
    req->secondaryTableId = 8;
    req->userPtr = 6;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
#endif
#ifdef ERROR_INSERT
  if (type == DumpStateOrd::EnableUndoDelayDataWrite) {
    DumpStateOrd *const dumpState = (DumpStateOrd *)&signal->theData[0];
    ndbout << "Dbtup:: delay write of datapages for table = "
           << dumpState->args[1] << endl;
    c_errorInsert4000TableId = dumpState->args[1];
    SET_ERROR_INSERT_VALUE(4000);
    return;
  }  // if
#endif
#if defined VM_TRACE
  if (type == 1211 || type == 1212 || type == 1213) {
    Uint32 seed = (Uint32)time(0);
    if (signal->getLength() > 1) seed = signal->theData[1];
    g_eventLogger->info("Startar modul test av Page Manager (seed: 0x%x)",
                        seed);
    srand(seed);

    Vector<Chunk> chunks;
    const Uint32 LOOPS = 1000;
    Uint32 sum_req = 0;
    Uint32 sum_conf = 0;
    Uint32 sum_loop = 0;
    Uint32 max_loop = 0;
    for (Uint32 i = 0; i < LOOPS; i++) {
      // Case
      Uint32 c = (rand() % 3);
      Resource_limit rl;
      m_ctx.m_mm.get_resource_limit(RG_DATAMEM, rl);
      const Uint32 free = rl.m_max - rl.m_curr;

      Uint32 alloc = 0;
      if (free <= 1) {
        c = 0;
        alloc = 1;
      } else
        alloc = 1 + (rand() % (free - 1));

      if (chunks.size() == 0 && c == 0) {
        c = 1 + rand() % 2;
      }

      if (type == 1211)
        g_eventLogger->info("loop=%d case=%d free=%d alloc=%d", i, c, free,
                            alloc);

      if (type == 1213) {
        c = 1;
        alloc = 2 + (sum_conf >> 3) + (sum_conf >> 4);
      }
      switch (c) {
        case 0: {  // Release
          const int ch = rand() % chunks.size();
          Chunk chunk = chunks[ch];
          chunks.erase(ch);
          returnCommonArea(chunk.pageId, chunk.pageCount);
        } break;
        case 2: {  // Seize(n) - fail
          alloc += free;
          sum_req += free;
          goto doalloc;
        }
        case 1: {  // Seize(n) (success)
          sum_req += alloc;
        doalloc:
          Chunk chunk;
          allocConsPages(jamBuffer(), alloc, chunk.pageCount, chunk.pageId);
          ndbrequire(chunk.pageCount <= alloc);
          if (chunk.pageCount != 0) {
            chunks.push_back(chunk);
            if (chunk.pageCount != alloc) {
              if (type == 1211)
                g_eventLogger->info(
                    "  Tried to allocate %d - only allocated %d - free: %d",
                    alloc, chunk.pageCount, free);
            }
          } else {
            g_eventLogger->info("  Failed to alloc %d pages with %d pages free",
                                alloc, free);
          }

          sum_conf += chunk.pageCount;
          Uint32 tot = fc_left + fc_right + fc_remove;
          sum_loop += tot;
          if (tot > max_loop) max_loop = tot;

          for (Uint32 i = 0; i < chunk.pageCount; i++) {
            PagePtr pagePtr;
            pagePtr.i = chunk.pageId + i;
            c_page_pool.getPtr(pagePtr);
          }

          if (alloc == 1 && free > 0) {
            ndbrequire(chunk.pageCount == alloc);
          }
        } break;
      }
    }
    while (chunks.size() > 0) {
      Chunk chunk = chunks.back();
      returnCommonArea(chunk.pageId, chunk.pageCount);
      chunks.erase(chunks.size() - 1);
    }

    g_eventLogger->info(
        "Got %u%% of requested allocs, loops : %u 100*avg: %u max: %u",
        (100 * sum_conf) / sum_req, sum_loop, 100 * sum_loop / LOOPS, max_loop);
  }
#endif

#ifdef ERROR_INSERT
  if (signal->theData[0] == DumpStateOrd::SchemaResourceSnapshot) {
    {
      Uint64 defaultValueWords = 0;
      if (DefaultValuesFragment.i != RNIL)
        defaultValueWords = calculate_used_var_words(DefaultValuesFragment.p);
      Uint32 defaultValueWordsHi = (Uint32)(defaultValueWords >> 32);
      Uint32 defaultValueWordsLo = (Uint32)(defaultValueWords & 0xFFFFFFFF);
      RSS_OP_SNAPSHOT_SAVE(defaultValueWordsHi);
      RSS_OP_SNAPSHOT_SAVE(defaultValueWordsLo);
    }
    RSS_OP_SNAPSHOT_SAVE(cnoOfFreeFragoprec);
    RSS_OP_SNAPSHOT_SAVE(cnoOfFreeFragrec);
    RSS_OP_SNAPSHOT_SAVE(cnoOfFreeTabDescrRec);

    RSS_AP_SNAPSHOT_SAVE2(c_storedProcPool, c_storedProcCountNonAPI);
    return;
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceCheckLeak) {
    {
      Uint64 defaultValueWords = 0;
      if (DefaultValuesFragment.i != RNIL)
        defaultValueWords = calculate_used_var_words(DefaultValuesFragment.p);
      Uint32 defaultValueWordsHi = (Uint32)(defaultValueWords >> 32);
      Uint32 defaultValueWordsLo = (Uint32)(defaultValueWords & 0xFFFFFFFF);
      RSS_OP_SNAPSHOT_CHECK(defaultValueWordsHi);
      RSS_OP_SNAPSHOT_CHECK(defaultValueWordsLo);
    }
    RSS_OP_SNAPSHOT_CHECK(cnoOfFreeFragoprec);
    RSS_OP_SNAPSHOT_CHECK(cnoOfFreeFragrec);
    RSS_OP_SNAPSHOT_CHECK(cnoOfFreeTabDescrRec);

    RSS_AP_SNAPSHOT_CHECK2(c_storedProcPool, c_storedProcCountNonAPI);
    return;
  }
#endif
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  if (type == DumpStateOrd::TupSetTransientPoolMaxSize) {
    jam();
    if (signal->getLength() < 3) return;
    const Uint32 pool_index = signal->theData[1];
    const Uint32 new_size = signal->theData[2];
    if (pool_index >= c_transient_pool_count) return;
    c_transient_pools[pool_index]->setMaxSize(new_size);
    return;
  }
  if (type == DumpStateOrd::TupResetTransientPoolMaxSize) {
    jam();
    if (signal->getLength() < 2) return;
    const Uint32 pool_index = signal->theData[1];
    if (pool_index >= c_transient_pool_count) return;
    c_transient_pools[pool_index]->resetMaxSize();
    return;
  }
#endif
}  // Dbtup::execDUMP_STATE_ORD()

/* ---------------------------------------------------------------- */
/* ---------      MEMORY       CHECK        ----------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execMEMCHECKREQ(Signal *signal) {
  TablerecPtr regTabPtr;
  regTabPtr.i = 2;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  if (tablerec && regTabPtr.p->tableStatus == DEFINED)
    validate_page(regTabPtr.p, 0);

#if 0
  const Dbtup::Tablerec& tab = *tup->tabptr.p;

  PagePtr regPagePtr;
  Uint32* data = &signal->theData[0];

  jamEntry();
  BlockReference blockref = signal->theData[0];
  Uint32 i;
  for (i = 0; i < 25; i++) {
    jam();
    data[i] = 0;
  }//for
  for (i = 0; i < 16; i++) {
    regPagePtr.i = cfreepageList[i];
    jam();
    while (regPagePtr.i != RNIL) {
      jam();
      ptrCheckGuard(regPagePtr, cnoOfPage, cpage);
      regPagePtr.i = regPagePtr.p->next_page;
      data[0]++;
    }//while
  }//for
  sendSignal(blockref, GSN_MEMCHECKCONF, signal, 25, JBB);
#endif
}  // Dbtup::memCheck()

// ------------------------------------------------------------------------
// Help function to be used when debugging. Prints out a tuple page.
// printLimit is the number of bytes that is printed out from the page. A
// page is of size 32768 bytes as of March 2003.
// ------------------------------------------------------------------------
void Dbtup::printoutTuplePage(Uint32 fragid, Uint32 pageid, Uint32 printLimit) {
  PagePtr tmpPageP;
  FragrecordPtr tmpFragP;
  TablerecPtr tmpTableP;

  ndbrequire(c_page_pool.getPtr(tmpPageP, pageid));

  tmpFragP.i = fragid;
  ptrCheckGuard(tmpFragP, cnoOfFragrec, fragrecord);

  tmpTableP.i = tmpFragP.p->fragTableId;
  ptrCheckGuard(tmpTableP, cnoOfTablerec, tablerec);

  ndbout << "Fragid: " << fragid << " Pageid: " << pageid << endl
         << "----------------------------------------" << endl;

  ndbout << "PageHead : ";
  ndbout << endl;
}  // Dbtup::printoutTuplePage

#ifdef VM_TRACE
NdbOut &operator<<(NdbOut &out, const Dbtup::Operationrec &op) {
  out << "[Operationrec " << hex << &op;
  // table
  out << " [fragmentPtr " << hex << op.fragmentPtr << "]";
  // type
  out << " [op_type " << dec << op.op_type << "]";
  out << " [delete_insert_flag " << dec;
  out << op.op_struct.bit_field.delete_insert_flag << "]";
  // state
  out << " [tuple_state " << dec << op.tuple_state << "]";
  out << " [trans_state " << dec << op.trans_state << "]";
  out << " [in_active_list " << dec << op.op_struct.bit_field.in_active_list
      << "]";
  // links
  out << " [prevActiveOp " << hex << op.prevActiveOp << "]";
  out << " [nextActiveOp " << hex << op.nextActiveOp << "]";
  // tuples
  out << " [tupVersion " << hex << op.op_struct.bit_field.tupVersion << "]";
  out << " [m_tuple_location " << op.m_tuple_location << "]";
  out << " [m_copy_tuple_location " << op.m_copy_tuple_location << "]";
  out << "]";
  return out;
}

// uses global tabptr
NdbOut &operator<<(NdbOut &out, const Dbtup::Th &th) {
  unsigned i = 0;
  out << "[Th " << hex << &th;
  out << " [op " << hex << th.data[i++] << "]";
  out << " [version " << hex << (Uint16)th.data[i++] << "]";
  out << "]";
  return out;
}
#endif

#ifdef VM_TRACE
template class Vector<Chunk>;
#endif
// uses global tabptr

NdbOut &operator<<(NdbOut &out, const Local_key &key) {
  out << "[ m_page_no: " << dec << key.m_page_no << " m_file_no: " << dec
      << key.m_file_no << " m_page_idx: " << dec << key.m_page_idx << "]";
  return out;
}

char *printLocal_Key(char buf[], int bufsize, const Local_key &key) {
  BaseString::snprintf(buf, bufsize,
                       "[ m_page_no: %u m_file_no: %u m_page_idx: %u ]",
                       key.m_page_no, key.m_file_no, key.m_page_idx);
  return buf;
}

static NdbOut &operator<<(NdbOut &out,
                          const Dbtup::Tablerec::Tuple_offsets &off) {
  out << "[ null_words: " << (Uint32)off.m_null_words
      << " null off: " << (Uint32)off.m_null_offset
      << " disk_off: " << off.m_disk_ref_offset
      << " fixheadsz: " << off.m_fix_header_size
      << " max_var_off: " << off.m_max_var_offset << " ]";

  return out;
}

NdbOut &operator<<(NdbOut &out, const Dbtup::Tablerec &tab) {
  out << "[ total_rec_size: " << tab.total_rec_size
      << " checksum: " << !!(tab.m_bits & Dbtup::Tablerec::TR_Checksum)
      << " attr: " << tab.m_no_of_attributes
      << " disk: " << tab.m_no_of_disk_attributes
      << " mm: " << tab.m_offsets[Dbtup::MM]
      << " [ fix: " << tab.m_attributes[Dbtup::MM].m_no_of_fixsize
      << " var: " << tab.m_attributes[Dbtup::MM].m_no_of_varsize << "]"

      << " dd: " << tab.m_offsets[Dbtup::DD]
      << " [ fix: " << tab.m_attributes[Dbtup::DD].m_no_of_fixsize
      << " var: " << tab.m_attributes[Dbtup::DD].m_no_of_varsize << "]"
      << " ]" << endl;
  return out;
}

NdbOut &operator<<(NdbOut &out, const AttributeDescriptor &off) {
  Uint32 word;
  memcpy(&word, &off, 4);
  return out;
}

NdbOut &operator<<(NdbOut &out, const AttributeOffset &off) {
  Uint32 word;
  memcpy(&word, &off, 4);
  out << "[ offset: " << AttributeOffset::getOffset(word)
      << " nullpos: " << AttributeOffset::getNullFlagPos(word);
  if (AttributeOffset::getCharsetFlag(word))
    out << " charset: %d" << AttributeOffset::getCharsetPos(word);
  out << " ]";
  return out;
}
