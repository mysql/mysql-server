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
#define DBTUP_META_CPP
#include <ndb_limits.h>
#include <RefConvert.hpp>
#include <cstring>
#include <pc.hpp>
#include "Dbtup.hpp"
// Error codes
#include <AttributeDescriptor.hpp>
#include <backup/Backup.hpp>
#include <backup/BackupFormat.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/TupFrag.hpp>
#include "../dblqh/Dblqh.hpp"
#include "AttributeOffset.hpp"
#include "my_sys.h"

#include <EventLogger.hpp>

#define JAM_FILE_ID 424

#ifdef VM_TRACE
//#define DEBUG_DISK 1
//#define DEBUG_TUP_META 1
//#define DEBUG_TUP_META_EXTRA 1
//#define DEBUG_DROP_TAB 1
#endif

#ifdef DEBUG_DROP_TAB
#define DEB_DROP_TAB(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_DROP_TAB(arglist) \
  do {                        \
  } while (0)
#endif

#ifdef DEBUG_DISK
#define DEB_DISK(arglist)        \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_DISK(arglist) \
  do {                    \
  } while (0)
#endif

#ifdef DEBUG_TUP_META
#define DEB_TUP_META(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_TUP_META(arglist) \
  do {                        \
  } while (0)
#endif

#ifdef DEBUG_TUP_META_EXTRA
#define DEB_TUP_META_EXTRA(arglist) \
  do {                              \
    g_eventLogger->info arglist;    \
  } while (0)
#else
#define DEB_TUP_META_EXTRA(arglist) \
  do {                              \
  } while (0)
#endif

void Dbtup::execCREATE_TAB_REQ(Signal *signal) {
  jamEntry();

  CreateTabReq reqCopy = *(CreateTabReq *)signal->getDataPtr();
  CreateTabReq *req = &reqCopy;

  TablerecPtr regTabPtr;
  FragoperrecPtr fragOperPtr;
  regTabPtr.i = req->tableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  if (regTabPtr.p->tableStatus != NOT_DEFINED) {
    jam();
    g_eventLogger->info("regTabPtr.p->tableStatus : %u",
                        regTabPtr.p->tableStatus);
    terrorCode = CreateTableRef::TableAlreadyExist;
    goto sendref;
  }

  if (cfirstfreeFragopr == RNIL) {
    jam();
    terrorCode = ZNOFREE_FRAGOP_ERROR;
    goto sendref;
  }

  seizeFragoperrec(fragOperPtr);
  fragOperPtr.p->tableidFrag = regTabPtr.i;
  fragOperPtr.p->attributeCount = req->noOfAttributes;
  std::memset(fragOperPtr.p->m_null_bits, 0,
              sizeof(fragOperPtr.p->m_null_bits));
  fragOperPtr.p->charsetIndex = 0;
  fragOperPtr.p->lqhBlockrefFrag = req->senderRef;
  fragOperPtr.p->m_extra_row_gci_bits =
      req->GCPIndicator > 1 ? req->GCPIndicator - 1 : 0;
  fragOperPtr.p->m_extra_row_author_bits = req->extraRowAuthorBits;

  regTabPtr.p->m_createTable.m_fragOpPtrI = fragOperPtr.i;
  regTabPtr.p->m_createTable.defValSectionI = RNIL;
  regTabPtr.p->tableStatus = DEFINING;
  regTabPtr.p->m_bits = 0;
  regTabPtr.p->m_bits |= (req->checksumIndicator ? Tablerec::TR_Checksum : 0);
  regTabPtr.p->m_bits |= (req->GCPIndicator ? Tablerec::TR_RowGCI : 0);
  regTabPtr.p->m_bits |=
      (req->forceVarPartFlag ? Tablerec::TR_ForceVarPart : 0);
  regTabPtr.p->m_bits |=
      (req->GCPIndicator > 1 ? Tablerec::TR_ExtraRowGCIBits : 0);
  regTabPtr.p->m_bits |=
      (req->extraRowAuthorBits ? Tablerec::TR_ExtraRowAuthorBits : 0);

  regTabPtr.p->m_offsets[MM].m_disk_ref_offset = 0;
  regTabPtr.p->m_offsets[MM].m_null_words = 0;
  regTabPtr.p->m_offsets[MM].m_fix_header_size = 0;
  regTabPtr.p->m_offsets[MM].m_max_var_offset = 0;
  regTabPtr.p->m_offsets[MM].m_max_dyn_offset = 0;
  regTabPtr.p->m_offsets[MM].m_dyn_null_words = 0;

  regTabPtr.p->m_offsets[DD].m_disk_ref_offset = 0;
  regTabPtr.p->m_offsets[DD].m_null_words = 0;
  regTabPtr.p->m_offsets[DD].m_fix_header_size = 0;
  regTabPtr.p->m_offsets[DD].m_max_var_offset = 0;
  regTabPtr.p->m_offsets[DD].m_max_dyn_offset = 0;
  regTabPtr.p->m_offsets[DD].m_dyn_null_words = 0;

  regTabPtr.p->m_attributes[MM].m_no_of_fixsize = 0;
  regTabPtr.p->m_attributes[MM].m_no_of_varsize = 0;
  regTabPtr.p->m_attributes[MM].m_no_of_dynamic = 0;
  regTabPtr.p->m_attributes[MM].m_no_of_dyn_fix = 0;
  regTabPtr.p->m_attributes[MM].m_no_of_dyn_var = 0;
  regTabPtr.p->m_attributes[DD].m_no_of_fixsize = 0;
  regTabPtr.p->m_attributes[DD].m_no_of_varsize = 0;
  regTabPtr.p->m_attributes[DD].m_no_of_dynamic = 0;
  regTabPtr.p->m_attributes[DD].m_no_of_dyn_fix = 0;
  regTabPtr.p->m_attributes[DD].m_no_of_dyn_var = 0;

  // Reserve space for bitmap length
  regTabPtr.p->m_dyn_null_bits[MM] = DYN_BM_LEN_BITS;
  regTabPtr.p->m_dyn_null_bits[DD] = DYN_BM_LEN_BITS;
  regTabPtr.p->noOfKeyAttr = req->noOfKeyAttr;
  regTabPtr.p->noOfCharsets = req->noOfCharsets;
  regTabPtr.p->m_no_of_attributes = req->noOfAttributes;
  regTabPtr.p->dynTabDescriptor[MM] = RNIL;
  regTabPtr.p->dynTabDescriptor[DD] = RNIL;
  regTabPtr.p->m_no_of_extra_columns = 0;

  if (regTabPtr.p->m_bits & Tablerec::TR_ExtraRowGCIBits) {
    jam();
    regTabPtr.p->m_no_of_extra_columns++;
  }

  if (regTabPtr.p->m_bits & Tablerec::TR_ExtraRowAuthorBits) {
    jam();
    regTabPtr.p->m_no_of_extra_columns++;
  }

  {
    Uint32 offset[10];
    Uint32 allocSize = getTabDescrOffsets(
        req->noOfAttributes, req->noOfCharsets, req->noOfKeyAttr,
        regTabPtr.p->m_no_of_extra_columns, offset);
    Uint32 tableDescriptorRef = allocTabDescr(allocSize);
    if (tableDescriptorRef == RNIL) {
      jam();
      goto error;
    }
    setUpDescriptorReferences(tableDescriptorRef, regTabPtr.p, offset);
  }

  {
    CreateTabConf *conf = (CreateTabConf *)signal->getDataPtrSend();
    conf->senderData = req->senderData;
    conf->senderRef = reference();
    conf->tupConnectPtr = fragOperPtr.i;
    sendSignal(req->senderRef, GSN_CREATE_TAB_CONF, signal,
               CreateTabConf::SignalLength, JBB);
  }

  return;

error:
  regTabPtr.p->tableStatus = NOT_DEFINED;
  releaseFragoperrec(fragOperPtr);

sendref:
  CreateTabRef *ref = (CreateTabRef *)signal->getDataPtrSend();
  ref->senderData = req->senderData;
  ref->senderRef = reference();
  ref->errorCode = terrorCode;
  sendSignal(req->senderRef, GSN_CREATE_TAB_REF, signal,
             CreateTabRef::SignalLength, JBB);
}

void Dbtup::execTUP_ADD_ATTRREQ(Signal *signal) {
  FragoperrecPtr fragOperPtr;
  TablerecPtr regTabPtr;

  jamEntry();
  fragOperPtr.i = signal->theData[0];
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  Uint32 attrId = signal->theData[2];
  Uint32 attrDescriptor = signal->theData[3];
  // DICT sends charset number in upper half
  Uint32 csNumber = (signal->theData[4] >> 16);

  ndbrequire(csNumber < NDB_ARRAY_SIZE(all_charsets));

  regTabPtr.i = fragOperPtr.p->tableidFrag;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  ndbrequire(fragOperPtr.p->attributeCount > 0);
  fragOperPtr.p->attributeCount--;
  const bool lastAttr = (fragOperPtr.p->attributeCount == 0);

  Uint32 extraAttrId = 0;

  Uint32 firstTabDesIndex = regTabPtr.p->tabDescriptor + (attrId * ZAD_SIZE);
  setTabDescrWord(firstTabDesIndex, attrDescriptor);
  Uint32 attrLen = AttributeDescriptor::getSize(attrDescriptor);

  Uint32 attrDes2 = 0;
  Uint32 bytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
  Uint32 words = (bytes + 3) / 4;
  Uint32 ind = AttributeDescriptor::getDiskBased(attrDescriptor);
  if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
    jam();
    Uint32 null_pos;
    ndbrequire(ind <= 1);
    null_pos = fragOperPtr.p->m_null_bits[ind];

    if (AttributeDescriptor::getNullable(attrDescriptor)) {
      jam();
      fragOperPtr.p->m_null_bits[ind]++;
    }

    if (AttributeDescriptor::getArrayType(attrDescriptor) ==
            NDB_ARRAYTYPE_FIXED ||
        ind == DD) {
      jam();
      regTabPtr.p->m_attributes[ind].m_no_of_fixsize++;
      if (attrLen == 0) {
        /* Static bit type. */
        jam();
        Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
        fragOperPtr.p->m_null_bits[ind] += bitCount;
      }
    } else {
      jam();
      regTabPtr.p->m_attributes[ind].m_no_of_varsize++;
    }
    if (null_pos > AO_NULL_FLAG_POS_MASK) {
      jam();
      terrorCode = ZTOO_MANY_BITS_ERROR;
      goto error;
    }
    AttributeOffset::setNullFlagPos(attrDes2, null_pos);
  } else {
    jam();
    /* A dynamic attribute. */
    ndbrequire(ind == MM);
    regTabPtr.p->m_attributes[ind].m_no_of_dynamic++;
    /*
     * The dynamic attribute format always require a 'null' bit. So
     * storing NOT NULL attributes as dynamic is not all that useful
     * (but not harmful in any way either).
     * Later we might implement NOT NULL DEFAULT xxx by storing the value
     * xxx internally as 'null'.
     */

    Uint32 null_pos = regTabPtr.p->m_dyn_null_bits[ind];

    if (AttributeDescriptor::getArrayType(attrDescriptor) ==
        NDB_ARRAYTYPE_FIXED) {
      /* A fixed-size dynamic attribute. */
      jam();
      if (AttributeDescriptor::getSize(attrDescriptor) == 0) {
        jam();
        /**
         * Bit type. These are stored directly in the bitmap.
         * This means that we will still use some space for a dynamic NULL
         * bittype if a following dynamic attribute is non-NULL.
         */
        Uint32 bits = AttributeDescriptor::getArraySize(attrDescriptor);
        /**
         * The NULL bit is stored after the data bits, so that we automatically
         * ensure that the full size bitmap is stored when non-NULL.
         */
        null_pos += bits;
        regTabPtr.p->m_dyn_null_bits[ind] += bits + 1;
      } else {
        jam();
        /*
         * We use one NULL bit per 4 bytes of dynamic fixed-size attribute. So
         * for dynamic fixsize longer than 64 bytes (16 null bits), it is more
         * efficient to store them as dynamic varsize internally.
         */
        if (words > InternalMaxDynFix) goto treat_as_varsize;

        regTabPtr.p->m_attributes[ind].m_no_of_dyn_fix++;
        Uint32 null_bits = (bytes + 3) >> 2;
        regTabPtr.p->m_dyn_null_bits[ind] += null_bits;
      }
    } else {
      /* A variable-sized dynamic attribute. */
    treat_as_varsize:
      jam();
      regTabPtr.p->m_attributes[ind].m_no_of_dyn_var++;
      regTabPtr.p->m_dyn_null_bits[ind]++;
    }
    if (null_pos > AO_NULL_FLAG_POS_MASK) {
      jam();
      terrorCode = ZTOO_MANY_BITS_ERROR;
      goto error;
    }
    AttributeOffset::setNullFlagPos(attrDes2, null_pos);

    ndbassert((regTabPtr.p->m_attributes[ind].m_no_of_dyn_var +
               regTabPtr.p->m_attributes[ind].m_no_of_dyn_fix) <=
              regTabPtr.p->m_attributes[ind].m_no_of_dynamic);
  }
  handleCharsetPos(csNumber, regTabPtr.p->charsetArray,
                   regTabPtr.p->noOfCharsets, fragOperPtr.p->charsetIndex,
                   attrDes2);
  setTabDescrWord(firstTabDesIndex + 1, attrDes2);

  if ((ERROR_INSERTED(4009) && attrId == 0) ||
      (ERROR_INSERTED(4010) && lastAttr)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    terrorCode = 1;
    goto error;
  }

  if (!receive_defvalue(signal, regTabPtr)) {
    jam();
    goto error;
  }

  if ((ERROR_INSERTED(4032) && (attrId == 0)) ||
      (ERROR_INSERTED(4033) && lastAttr)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    terrorCode = 1;
    goto error;
  }

  if (!lastAttr) {
    jam();
    signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
    signal->theData[1] = lastAttr;
    sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, signal, 2,
               JBB);
    return;
  }

  if (fragOperPtr.p->m_extra_row_gci_bits) {
    jam();

    const Uint32 bits = fragOperPtr.p->m_extra_row_gci_bits;

    /**
     * Create attribute descriptor for extra row gci bits...
     */
    Uint32 desc = 0;
    Uint32 off = 0;

    AttributeDescriptor::setSize(desc, 0);  // bit
    AttributeDescriptor::setArraySize(desc, bits);
    AttributeOffset::setNullFlagPos(off, fragOperPtr.p->m_null_bits[MM]);
    fragOperPtr.p->m_null_bits[MM] += bits;

    if (fragOperPtr.p->m_null_bits[MM] > AO_NULL_FLAG_POS_MASK) {
      jam();
      terrorCode = ZTOO_MANY_BITS_ERROR;
      goto error;
    }

    Uint32 idx = regTabPtr.p->tabDescriptor;
    idx += ZAD_SIZE * (regTabPtr.p->m_no_of_attributes + extraAttrId);
    setTabDescrWord(idx, desc);
    setTabDescrWord(idx + 1, off);

    extraAttrId++;
  }

  if (fragOperPtr.p->m_extra_row_author_bits) {
    jam();

    const Uint32 bits = fragOperPtr.p->m_extra_row_author_bits;

    /**
     * Create attribute descriptor for extra row gci bits...
     */
    Uint32 desc = 0;
    Uint32 off = 0;

    AttributeDescriptor::setSize(desc, 0);  // bit
    AttributeDescriptor::setArraySize(desc, bits);
    AttributeOffset::setNullFlagPos(off, fragOperPtr.p->m_null_bits[MM]);
    fragOperPtr.p->m_null_bits[MM] += bits;

    if (fragOperPtr.p->m_null_bits[MM] > AO_NULL_FLAG_POS_MASK) {
      jam();
      terrorCode = ZTOO_MANY_BITS_ERROR;
      goto error;
    }

    Uint32 idx = regTabPtr.p->tabDescriptor;
    idx += ZAD_SIZE * (regTabPtr.p->m_no_of_attributes + extraAttrId);
    setTabDescrWord(idx, desc);
    setTabDescrWord(idx + 1, off);

    extraAttrId++;
  }

#define BTW(x) ((x + 31) >> 5)
  regTabPtr.p->m_offsets[MM].m_null_words = BTW(fragOperPtr.p->m_null_bits[MM]);
  regTabPtr.p->m_offsets[DD].m_null_words = BTW(fragOperPtr.p->m_null_bits[DD]);
#undef BTW

  {
    /* Allocate  dynamic descriptors. */
    for (Uint32 i = 0; i < NO_DYNAMICS; ++i) {
      jam();
      Uint32 offset[3];
      Uint32 allocSize = getDynTabDescrOffsets(
          (regTabPtr.p->m_dyn_null_bits[i] + 31) >> 5, offset);
      Uint32 dynTableDescriptorRef = allocTabDescr(allocSize);
      if (dynTableDescriptorRef == RNIL) {
        jam();
        goto error;
      }
      setupDynDescriptorReferences(dynTableDescriptorRef, regTabPtr.p, offset,
                                   i);
    }
  }

  /* Compute table aggregate metadata. */
  terrorCode = computeTableMetaData(regTabPtr, __LINE__);
  if (terrorCode) {
    jam();
    goto error;
  }

  if (store_default_record(regTabPtr) < 0) {
    jam();
    goto error;
  }

  ndbrequire(regTabPtr.p->tableStatus == DEFINING);
  regTabPtr.p->tableStatus = DEFINED;

  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = lastAttr;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, signal, 2,
             JBB);

  releaseFragoperrec(fragOperPtr);
  return;
error : {
  /* Release any unprocessed sections */
  SectionHandle handle(this, signal);
  releaseSections(handle);
}
  /* Release segmented section used to receive Attr default value */
  releaseSection(regTabPtr.p->m_createTable.defValSectionI);
  regTabPtr.p->m_createTable.defValSectionI = RNIL;
  free_var_part(DefaultValuesFragment.p, regTabPtr.p,
                &regTabPtr.p->m_default_value_location);
  regTabPtr.p->m_default_value_location.setNull();

  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = terrorCode;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTRREF, signal, 2,
             JBB);

  return;
}

bool Dbtup::receive_defvalue(Signal *signal, const TablerecPtr &regTabPtr) {
  jam();
  Uint32 defValueBytes = 0;
  Uint32 defValueWords = 0;
  Uint32 attrId = signal->theData[2];
  Uint32 attrDescriptor = signal->theData[3];

  Uint32 attrLen = AttributeDescriptor::getSize(attrDescriptor);
  Uint32 arrayType = AttributeDescriptor::getArrayType(attrDescriptor);
  Uint32 arraySize = AttributeDescriptor::getArraySize(attrDescriptor);

  const Uint32 numSections = signal->getNoOfSections();

  if (numSections == 0) {
    jam();
    return true;
  }

  jam();
  SectionHandle handle(this, signal);
  SegmentedSectionPtr ptr;
  ndbrequire(handle.getSection(ptr, TupAddAttrReq::DEFAULT_VALUE_SECTION_NUM));

  SimplePropertiesSectionReader r(ptr, getSectionSegmentPool());
  r.reset();

  Uint32 ahIn;
  ndbrequire(r.getWord(&ahIn));

  defValueBytes = AttributeHeader::getByteSize(ahIn);
  defValueWords = (defValueBytes + 3) / 4;

  Uint32 *dst = NULL;
  AttributeHeader ah(attrId, defValueBytes);

  if (defValueBytes == 0) {
    jam();
    releaseSections(handle);
    return true;
  }

  /* We have a default value, double check to be sure this is not
   * a primary key
   */
  if (AttributeDescriptor::getPrimaryKey(attrDescriptor)) {
    jam();
    releaseSections(handle);
    /* Default value for primary key column not supported */
    terrorCode = 792;
    return false;
  }

  Uint32 bytes;
  if (attrLen)
    bytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
  else
    bytes =
        ((arraySize + AD_SIZE_IN_WORDS_OFFSET) >> AD_SIZE_IN_WORDS_SHIFT) * 4;

  terrorCode = 0;

  if (attrLen) {
    if (arrayType == NDB_ARRAYTYPE_FIXED) {
      jam();
      if (defValueBytes != bytes) {
        jam();
        terrorCode = ZBAD_DEFAULT_VALUE_LEN;
      }
    } else {
      jam();
      if (defValueBytes > bytes) {
        jam();
        terrorCode = ZBAD_DEFAULT_VALUE_LEN;
      }
    }
  } else {
    /*
     * The condition is for BIT type.
     * Even though it is fixed, the compare operator should be > rather than ==,
     * for the 4-byte alignment, the space for BIT type occupied 4 bytes at
     * least. yet the bytes of default value can be 1, 2, 3, 4, 5, 6, 7, 8
     * bytes.
     */
    jam();
    if (defValueBytes > bytes) {
      jam();
      terrorCode = ZBAD_DEFAULT_VALUE_LEN;
    }
  }

  jam();

  if (likely(!terrorCode)) {
    jam();
    dst = cinBuffer;

    ndbrequire(r.getWords(dst, defValueWords));

    /* Check that VAR types have valid inline length */
    if ((attrLen) && (arrayType != NDB_ARRAYTYPE_FIXED)) {
      jam();
      const uchar *valPtr = (const uchar *)dst;
      Uint32 internalVarSize = 0;

      if (arrayType == NDB_ARRAYTYPE_SHORT_VAR) {
        jam();
        internalVarSize = 1 + valPtr[0];
      } else if (arrayType == NDB_ARRAYTYPE_MEDIUM_VAR) {
        jam();
        internalVarSize = 2 + valPtr[0] + (256 * Uint32(valPtr[1]));
      } else {
        ndbabort();
      }

      if (unlikely(internalVarSize != defValueBytes)) {
        jam();
        terrorCode = ZBAD_DEFAULT_VALUE_LEN;
        releaseSections(handle);
        return false;
      }
    }

    if (likely(appendToSection(regTabPtr.p->m_createTable.defValSectionI,
                               (const Uint32 *)&ah, 1))) {
      if (likely(appendToSection(regTabPtr.p->m_createTable.defValSectionI,
                                 (const Uint32 *)dst, defValueWords))) {
        jam();
        releaseSections(handle);
        return true;
      }
    }
    jam();
    terrorCode = ZMEM_NOMEM_ERROR;
  }

  releaseSections(handle);
  return false;
}

void Dbtup::execTUPFRAGREQ(Signal *signal) {
  jamEntry();

  TupFragReq copy = *(TupFragReq *)signal->getDataPtr();
  TupFragReq *req = &copy;

  FragrecordPtr regFragPtr;

  Uint32 tableId = req->tableId;
  Uint32 userptr = req->userPtr;
  Uint32 userRef = req->userRef;
  Uint32 reqinfo = req->reqInfo;
  Uint32 fragId = req->fragId;
  Uint32 tablespace_id = req->tablespaceid;
  Uint32 changeMask = req->changeMask;
  Uint32 partitionId = req->partitionId;

  Uint64 maxRows = (((Uint64)req->maxRowsHigh) << 32) + req->maxRowsLow;
  Uint64 minRows = (((Uint64)req->minRowsHigh) << 32) + req->minRowsLow;

  (void)reqinfo;
  (void)maxRows;
  (void)minRows;

  if (req->userPtr == (Uint32)-1) {
    jam();
    abortAddFragOp(signal);
    return;
  }

  TablerecPtr regTabPtr;

#ifndef VM_TRACE
  // config mismatch - do not crash if release compiled
  if (tableId >= cnoOfTablerec) {
    jam();
    terrorCode = 800;
    goto sendref;
  }
#endif

  regTabPtr.i = tableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  if (regFragPtr.i != RNIL) {
    jam();
    terrorCode = ZEXIST_FRAG_ERROR;
    goto sendref;
  }

  if (cfirstfreefrag != RNIL) {
    jam();
    seizeFragrecord(regFragPtr);
  } else {
    jam();
    terrorCode = ZFULL_FRAGRECORD_ERROR;
    goto sendref;
  }

  for (Uint32 i = 0; i < NUM_TUP_FRAGMENT_MUTEXES; i++) {
    NdbMutex_Init(&regFragPtr.p->tup_frag_mutex[i]);
  }
  NdbMutex_Init(&regFragPtr.p->tup_frag_page_map_mutex);
  {
    Uint32 noAllocatedPages = 1;  // allocFragPage(regFragPtr.p);

    if (noAllocatedPages == 0) {
      jam();
      releaseFragrec(regFragPtr);
      terrorCode = ZNO_PAGES_ALLOCATED_ERROR;
      goto sendref;
    }
  }

  if (!addfragtotab(regTabPtr.p, fragId, regFragPtr.i)) {
    jam();
    releaseFragrec(regFragPtr);
    terrorCode = ZNO_FREE_TAB_ENTRY_ERROR;
    goto sendref;
  }

  if ((ERROR_INSERTED(4007) && regTabPtr.p->fragid[0] == fragId) ||
      (ERROR_INSERTED(4008) && regTabPtr.p->fragid[1] == fragId) ||
      ERROR_INSERTED(4050)) {
    jam();
    releaseFragrec(regFragPtr);
    remove_frag_from_tab(regTabPtr, fragId);
    CLEAR_ERROR_INSERT_VALUE;
    terrorCode = 1;
    goto sendref;
  }

  regFragPtr.p->fragStatus = Fragrecord::FS_ONLINE;
  regFragPtr.p->fragTableId = regTabPtr.i;
  regFragPtr.p->fragmentId = fragId;
  regFragPtr.p->partitionId = partitionId;
  regFragPtr.p->m_tablespace_id = tablespace_id;
  regFragPtr.p->m_undo_complete = 0;
  regFragPtr.p->m_lcp_scan_op = RNIL;
  regFragPtr.p->m_lcp_keep_list_head.setNull();
  regFragPtr.p->m_lcp_keep_list_tail.setNull();
  regFragPtr.p->noOfPages = 0;
  regFragPtr.p->noOfVarPages = 0;
  regFragPtr.p->m_varWordsFree = 0;
  regFragPtr.p->m_max_page_cnt = 0;
  regFragPtr.p->m_lcp_changed_rows = 0;
  regFragPtr.p->m_free_page_id_list = FREE_PAGE_RNIL;
  ndbrequire(regFragPtr.p->m_page_map.isEmpty());
  regFragPtr.p->m_restore_lcp_id = RNIL;
  regFragPtr.p->m_restore_local_lcp_id = 0;
  regFragPtr.p->m_fixedElemCount = 0;
  regFragPtr.p->m_row_count = 0;
  regFragPtr.p->m_prev_row_count = 0;
  regFragPtr.p->m_lcp_start_gci = 0;
  regFragPtr.p->m_varElemCount = 0;
  regFragPtr.p->m_committed_changes = 0;
  /**
   * Average row size will be calculated and set at each start of
   * an LCP. This value is used to calculate the proposed speed of
   * checkpoints. We initialise it to 200 to ensure that it has
   * a somewhat sensible value before we can calculate it.
   *
   * We can survive some miscalculated numbers before first LCP
   * is executed.
   */
  regFragPtr.p->m_average_row_size = 200;

  for (Uint32 i = 0; i < MAX_FREE_LIST + 1; i++)
    ndbrequire(regFragPtr.p->free_var_page_array[i].isEmpty());

  CreateFilegroupImplReq rep;
  std::memset(&rep, 0, sizeof(rep));
  if (regTabPtr.p->m_no_of_disk_attributes) {
    {
      jam();
      D("Tablespace_client - execTUPFRAGREQ");
      Tablespace_client tsman(0, this, c_tsman, 0, 0, 0,
                              regFragPtr.p->m_tablespace_id);
      ndbrequire(tsman.get_tablespace_info(&rep) == 0);
      regFragPtr.p->m_logfile_group_id = rep.tablespace.logfile_group_id;
    }
    {
      Page_cache_client pgman(this, c_pgman);
      int res = pgman.add_fragment(regTabPtr.i, fragId);
      jamEntry();
      if (res != 0) {
        jam();
        releaseFragrec(regFragPtr);
        remove_frag_from_tab(regTabPtr, fragId);
        terrorCode = ZNO_FREE_TAB_ENTRY_ERROR;
        goto sendref;
      }
    }
  } else {
    jam();
    regFragPtr.p->m_logfile_group_id = RNIL;
  }
  new (&regFragPtr.p->m_disk_alloc_info)
      Disk_alloc_info(regTabPtr.p, rep.tablespace.extent_size);

  if (AlterTableReq::getReorgFragFlag(changeMask)) {
    jam();
    regFragPtr.p->fragStatus = Fragrecord::FS_REORG_NEW;
  }

  signal->theData[0] = userptr;
  signal->theData[1] = fragId;
  signal->theData[2] = regFragPtr.i;
  sendSignal(userRef, GSN_TUPFRAGCONF, signal, 3, JBB);

  return;

sendref:
  signal->theData[0] = userptr;
  signal->theData[1] = terrorCode;
  sendSignal(userRef, GSN_TUPFRAGREF, signal, 2, JBB);
}

/*
  Store the default values for a table, as the ATTRINFO "program"
  (i.e AttributeHeader|Data AttributeHeader|Data...)
  in varsize memory associated with the dummy fragment(DefaultValuesFragment).
  There is a DBTUP global set of defaults records in DefaultValuesFragment.
  One record per table stored on varsize pages.

  Each Table_record has a Local_key pointing to start of its default values
  in TUP's default values fragment.
*/
int Dbtup::store_default_record(const TablerecPtr &regTabPtr) {
  Uint32 RdefValSectionI = regTabPtr.p->m_createTable.defValSectionI;
  jam();

  if (RdefValSectionI == RNIL)  // No default values are stored for the table
  {
    jam();
    if (ERROR_INSERTED(4034)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      terrorCode = 1;
      return -1;
    }

    return 0;
  }

  SegmentedSectionPtr defValSection;
  getSection(defValSection, RdefValSectionI);
  Uint32 sizes = defValSection.p->m_sz;
  /**
   * Alloc var-length memory for storing defaults
   */
  Uint32 *var_data_ptr =
      alloc_var_part(&terrorCode, DefaultValuesFragment.p, regTabPtr.p, sizes,
                     &regTabPtr.p->m_default_value_location);
  if (unlikely(var_data_ptr == 0)) {
    jam();
    /* Caller releases the default values section */
    return -1;
  }

  if (ERROR_INSERTED(4034)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    terrorCode = 1;
    return -1;
  }

  copy(var_data_ptr, RdefValSectionI);
  releaseSection(RdefValSectionI);
  regTabPtr.p->m_createTable.defValSectionI = RNIL;

  return 0;
}

bool Dbtup::addfragtotab(Tablerec *const regTabPtr, Uint32 fragId,
                         Uint32 fragIndex) {
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr->fragid); i++) {
    jam();
    if (regTabPtr->fragid[i] == RNIL) {
      jam();
      regTabPtr->fragid[i] = fragId;
      regTabPtr->fragrec[i] = fragIndex;
      return true;
    }
  }
  return false;
}

Uint32 Dbtup::get_frag_from_tab(TablerecPtr tabPtr, Uint32 fragId) {
  Uint32 fragIndex = RNIL;
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->fragid); i++) {
    jam();
    if (tabPtr.p->fragid[i] == fragId) {
      jam();
      fragIndex = tabPtr.p->fragrec[i];
      break;
    }
  }
  return fragIndex;
}

void Dbtup::remove_frag_from_tab(TablerecPtr tabPtr, Uint32 fragId) {
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->fragid); i++) {
    jam();
    if (tabPtr.p->fragid[i] == fragId) {
      jam();
      tabPtr.p->fragid[i] = RNIL;
      tabPtr.p->fragrec[i] = RNIL;
      return;
    }
  }
  ndbabort();
}

void Dbtup::getFragmentrec(FragrecordPtr &regFragPtr, Uint32 fragId,
                           Tablerec *const regTabPtr) {
#if defined(VM_TRACE) || defined(ERROR_INSERT) || defined(EXTRA_JAM)
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();
#endif

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr->fragid); i++) {
    thrjamDebug(jamBuf);
    if (regTabPtr->fragid[i] == fragId) {
      thrjamDebug(jamBuf);
      regFragPtr.i = regTabPtr->fragrec[i];
      ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
      return;
    }
  }
  regFragPtr.i = RNIL;
  ptrNull(regFragPtr);
}

void Dbtup::seizeFragrecord(FragrecordPtr &regFragPtr) {
  regFragPtr.i = cfirstfreefrag;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  cfirstfreefrag = regFragPtr.p->nextfreefrag;
  regFragPtr.p->nextfreefrag = RNIL;
  RSS_OP_ALLOC(cnoOfFreeFragrec);
}

void Dbtup::seizeFragoperrec(FragoperrecPtr &fragOperPtr) {
  fragOperPtr.i = cfirstfreeFragopr;
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  cfirstfreeFragopr = fragOperPtr.p->nextFragoprec;
  fragOperPtr.p->nextFragoprec = RNIL;
  fragOperPtr.p->inUse = true;
  RSS_OP_ALLOC(cnoOfFreeFragoprec);
}  // Dbtup::seizeFragoperrec()

void Dbtup::seizeAlterTabOperation(AlterTabOperationPtr &alterTabOpPtr) {
  alterTabOpPtr.i = cfirstfreeAlterTabOp;
  ptrCheckGuard(alterTabOpPtr, cnoOfAlterTabOps, alterTabOperRec);
  cfirstfreeAlterTabOp = alterTabOpPtr.p->nextAlterTabOp;
  alterTabOpPtr.p->nextAlterTabOp = RNIL;
}

void Dbtup::execALTER_TAB_REQ(Signal *signal) {
  jamEntry();

  AlterTabReq copy = *(AlterTabReq *)signal->getDataPtr();
  AlterTabReq *req = &copy;

  TablerecPtr regTabPtr;
  regTabPtr.i = req->tableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  switch ((AlterTabReq::RequestType)req->requestType) {
    case AlterTabReq::AlterTablePrepare: {
      jam();

      if (AlterTableReq::getAddAttrFlag(req->changeMask)) {
        jam();
        SectionHandle handle(this, signal);
        ndbrequire(handle.m_cnt == 1);
        ::copy(signal->theData + 25, handle.m_ptr[0]);
        releaseSections(handle);
      }
      handleAlterTablePrepare(signal, req, regTabPtr.p);
      return;
    }
    case AlterTabReq::AlterTableCommit: {
      jam();
      handleAlterTableCommit(signal, req, regTabPtr);
      return;
    }
    case AlterTabReq::AlterTableRevert: {
      jam();
      handleAlterTableAbort(signal, req, regTabPtr.p);
      return;
    }
    case AlterTabReq::AlterTableComplete: {
      jam();
      handleAlterTableComplete(signal, req, regTabPtr.p);
      return;
    }
    case AlterTabReq::AlterTableSumaEnable: {
      FragrecordPtr regFragPtr;
      for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr.p->fragrec); i++) {
        jam();
        if ((regFragPtr.i = regTabPtr.p->fragrec[i]) != RNIL) {
          jam();
          ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
          switch (regFragPtr.p->fragStatus) {
            case Fragrecord::FS_REORG_COMMIT_NEW:
              jam();
              if (0)
                g_eventLogger->info(
                    "tab: %u frag: %u toggle fragstate from %s to %s",
                    regFragPtr.p->fragTableId, regFragPtr.p->fragmentId,
                    "FS_REORG_COMMIT_NEW", "FS_REORG_COMPLETE_NEW");
              regFragPtr.p->fragStatus = Fragrecord::FS_REORG_COMPLETE_NEW;
              break;
            default:
              break;
          }
        }
      }
      sendAlterTabConf(signal, RNIL);
      return;
    }
    case AlterTabReq::AlterTableSumaFilter: {
      Uint32 gci = signal->theData[signal->getLength() - 1];
      regTabPtr.p->m_reorg_suma_filter.m_gci_hi = gci;
      FragrecordPtr regFragPtr;
      for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr.p->fragrec); i++) {
        jam();
        if ((regFragPtr.i = regTabPtr.p->fragrec[i]) != RNIL) {
          jam();
          ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
          switch (regFragPtr.p->fragStatus) {
            case Fragrecord::FS_REORG_COMMIT:
              jam();
              if (0)
                g_eventLogger->info(
                    "tab: %u frag: %u toggle fragstate from %s to %s (gci: %u)",
                    regFragPtr.p->fragTableId, regFragPtr.p->fragmentId,
                    "FS_REORG_COMMIT", "FS_REORG_COMPLETE", gci);
              regFragPtr.p->fragStatus = Fragrecord::FS_REORG_COMPLETE;
              break;
            default:
              break;
          }
        }
      }
      signal->theData[0] = ~Uint32(0);
      return;
    }
    case AlterTabReq::AlterTableReadOnly:
    case AlterTabReq::AlterTableReadWrite:
      jam();
      signal->theData[0] = 0;
      signal->theData[1] = RNIL;
      return;
    default:
      break;
  }
  ndbabort();
}

void Dbtup::handleAlterTablePrepare(Signal *signal, const AlterTabReq *req,
                                    const Tablerec *regTabPtr) {
  Uint32 connectPtr = RNIL;
  if (AlterTableReq::getAddAttrFlag(req->changeMask)) {
    jam();

    Uint32 noOfNewAttr = req->noOfNewAttr;
    Uint32 newNoOfCharsets = req->newNoOfCharsets;
    Uint32 newNoOfKeyAttrs = req->newNoOfKeyAttrs;

    Uint32 *attrInfo = signal->theData + 25;

    Uint32 oldNoOfAttr = regTabPtr->m_no_of_attributes;
    Uint32 newNoOfAttr = oldNoOfAttr + noOfNewAttr;

    /* Can only add attributes if varpart already present. */
    if ((regTabPtr->m_attributes[MM].m_no_of_varsize +
         regTabPtr->m_attributes[MM].m_no_of_dynamic +
         (regTabPtr->m_bits & Tablerec::TR_ForceVarPart)) == 0) {
      jam();
      sendAlterTabRef(signal, ZINVALID_ALTER_TAB);
      return;
    }

    AlterTabOperationPtr regAlterTabOpPtr;
    seizeAlterTabOperation(regAlterTabOpPtr);

    regAlterTabOpPtr.p->newNoOfAttrs = newNoOfAttr;
    regAlterTabOpPtr.p->newNoOfCharsets = newNoOfCharsets;
    regAlterTabOpPtr.p->newNoOfKeyAttrs = newNoOfKeyAttrs;

    /* Allocate a new (possibly larger) table descriptor buffer. */
    Uint32 allocSize = getTabDescrOffsets(
        newNoOfAttr, newNoOfCharsets, newNoOfKeyAttrs,
        regTabPtr->m_no_of_extra_columns, regAlterTabOpPtr.p->tabDesOffset);
    Uint32 tableDescriptorRef = allocTabDescr(allocSize);
    if (tableDescriptorRef == RNIL) {
      jam();
      releaseAlterTabOpRec(regAlterTabOpPtr);
      sendAlterTabRef(signal, terrorCode);
      return;
    }
    regAlterTabOpPtr.p->tableDescriptor = tableDescriptorRef;

    /*
      Get new pointers into tableDescriptor, and copy over old data.
      (Rest will be recomputed in computeTableMetaData() in case of
      ALTER_TAB_REQ[commit]).
    */
    Uint32 *desc = &tableDescriptor[tableDescriptorRef].tabDescr;
    const CHARSET_INFO **CharsetArray =
        (const CHARSET_INFO **)(desc + regAlterTabOpPtr.p->tabDesOffset[2]);
    memcpy(CharsetArray, regTabPtr->charsetArray,
           sizeof(*CharsetArray) * regTabPtr->noOfCharsets);
    Uint32 *const attrDesPtrStart = desc + regAlterTabOpPtr.p->tabDesOffset[4];
    Uint32 *attrDesPtr = attrDesPtrStart;
    memcpy(attrDesPtr, &tableDescriptor[regTabPtr->tabDescriptor].tabDescr,
           4 * ZAD_SIZE * oldNoOfAttr);

    /**
     * Copy extra columns descriptors to end of attrDesPtr
     */
    {
      const Uint32 *src = &tableDescriptor[regTabPtr->tabDescriptor].tabDescr;
      src += ZAD_SIZE * oldNoOfAttr;

      Uint32 *dst = attrDesPtr + (ZAD_SIZE * newNoOfAttr);
      memcpy(dst, src, 4 * ZAD_SIZE * regTabPtr->m_no_of_extra_columns);
    }

    attrDesPtr += ZAD_SIZE * oldNoOfAttr;

    /*
      Loop over the new attributes to add.
      - Save AttributeDescriptor word in new TabDescriptor record.
      - Compute charset pos, as we will not save original csNumber.
      - Compute size needed for dynamic bitmap mask allocation.
      - Compute number of dynamic varsize, needed for fixsize offset calculation
      in ALTER_TAB_REQ[commit];
    */
    Uint32 charsetIndex = regTabPtr->noOfCharsets;
    Uint32 dyn_nullbits = regTabPtr->m_dyn_null_bits[MM];
    if (dyn_nullbits == 0) {
      jam();
      dyn_nullbits = DYN_BM_LEN_BITS;
    }

    Uint32 noDynFix = regTabPtr->m_attributes[MM].m_no_of_dyn_fix;
    Uint32 noDynVar = regTabPtr->m_attributes[MM].m_no_of_dyn_var;
    Uint32 noDynamic = regTabPtr->m_attributes[MM].m_no_of_dynamic;
    for (Uint32 i = 0; i < noOfNewAttr; i++) {
      Uint32 attrDescriptor = *attrInfo++;
      Uint32 csNumber = (*attrInfo++ >> 16);
      Uint32 attrDes2 = 0;

      /* Only dynamic attributes possible for add attr */
      ndbrequire(AttributeDescriptor::getDynamic(attrDescriptor));
      ndbrequire(!AttributeDescriptor::getDiskBased(attrDescriptor));

      handleCharsetPos(csNumber, CharsetArray, newNoOfCharsets, charsetIndex,
                       attrDes2);

      Uint32 null_pos = dyn_nullbits;
      Uint32 arrType = AttributeDescriptor::getArrayType(attrDescriptor);
      noDynamic++;
      if (arrType == NDB_ARRAYTYPE_FIXED) {
        jam();
        Uint32 words = AttributeDescriptor::getSizeInWords(attrDescriptor);

        if (AttributeDescriptor::getSize(attrDescriptor) > 0) {
          jam();
          if (words > InternalMaxDynFix) goto treat_as_varsize;
          noDynFix++;
          dyn_nullbits += words;
        } else {
          /* Bit type. */
          jam();
          Uint32 bits = AttributeDescriptor::getArraySize(attrDescriptor);
          null_pos += bits;
          dyn_nullbits += bits + 1;
        }
      } else {
        jam();
      treat_as_varsize:
        noDynVar++;
        dyn_nullbits++;
      }
      AttributeOffset::setNullFlagPos(attrDes2, null_pos);

      *attrDesPtr++ = attrDescriptor;
      *attrDesPtr++ = attrDes2;
    }
    ndbassert(newNoOfCharsets == charsetIndex);
    ndbrequire(attrDesPtr == attrDesPtrStart + (ZAD_SIZE * newNoOfAttr));

    regAlterTabOpPtr.p->noOfDynNullBits = dyn_nullbits;
    ndbassert(noDynamic ==
              regTabPtr->m_attributes[MM].m_no_of_dynamic + noOfNewAttr);
    regAlterTabOpPtr.p->noOfDynFix = noDynFix;
    regAlterTabOpPtr.p->noOfDynVar = noDynVar;
    regAlterTabOpPtr.p->noOfDynamic = noDynamic;

    /* Allocate the new (possibly larger) dynamic descriptor. */
    allocSize = getDynTabDescrOffsets((dyn_nullbits + 31) >> 5,
                                      regAlterTabOpPtr.p->dynTabDesOffset);
    Uint32 dynTableDescriptorRef = RNIL;
    if (ERROR_INSERTED(4029)) {
      jam();
      dynTableDescriptorRef = RNIL;
      terrorCode = ZMEM_NOTABDESCR_ERROR;
      CLEAR_ERROR_INSERT_VALUE;
    } else {
      jam();
      dynTableDescriptorRef = allocTabDescr(allocSize);
    }
    if (dynTableDescriptorRef == RNIL) {
      jam();
      releaseTabDescr(tableDescriptorRef);
      releaseAlterTabOpRec(regAlterTabOpPtr);
      sendAlterTabRef(signal, terrorCode);
      return;
    }
    regAlterTabOpPtr.p->dynTableDescriptor = dynTableDescriptorRef;
    connectPtr = regAlterTabOpPtr.i;
  }

  sendAlterTabConf(signal, connectPtr);
}

void Dbtup::sendAlterTabRef(Signal *signal, Uint32 errorCode) {
  signal->theData[0] = errorCode;
  signal->theData[1] = RNIL;
}

void Dbtup::sendAlterTabConf(Signal *signal, Uint32 connectPtr) {
  signal->theData[0] = 0;
  signal->theData[1] = connectPtr;
}

void Dbtup::handleAlterTableCommit(Signal *signal, const AlterTabReq *req,
                                   TablerecPtr tabPtr) {
  Tablerec *regTabPtr = tabPtr.p;
  if (AlterTableReq::getAddAttrFlag(req->changeMask)) {
    jam();
    AlterTabOperationPtr regAlterTabOpPtr;
    regAlterTabOpPtr.i = req->connectPtr;
    ptrCheckGuard(regAlterTabOpPtr, cnoOfAlterTabOps, alterTabOperRec);

    /* Free old table descriptors. */
    releaseTabDescr(regTabPtr);

    /* Set new attribute counts. */
    regTabPtr->m_no_of_attributes = regAlterTabOpPtr.p->newNoOfAttrs;
    regTabPtr->noOfCharsets = regAlterTabOpPtr.p->newNoOfCharsets;
    regTabPtr->noOfKeyAttr = regAlterTabOpPtr.p->newNoOfKeyAttrs;
    regTabPtr->m_attributes[MM].m_no_of_dyn_fix =
        regAlterTabOpPtr.p->noOfDynFix;
    regTabPtr->m_attributes[MM].m_no_of_dyn_var =
        regAlterTabOpPtr.p->noOfDynVar;
    regTabPtr->m_attributes[MM].m_no_of_dynamic =
        regAlterTabOpPtr.p->noOfDynamic;
    regTabPtr->m_dyn_null_bits[MM] = regAlterTabOpPtr.p->noOfDynNullBits;

    /* Install the new (larger) table descriptors. */
    setUpDescriptorReferences(regAlterTabOpPtr.p->tableDescriptor, regTabPtr,
                              regAlterTabOpPtr.p->tabDesOffset);
    setupDynDescriptorReferences(regAlterTabOpPtr.p->dynTableDescriptor,
                                 regTabPtr,
                                 regAlterTabOpPtr.p->dynTabDesOffset);

    releaseAlterTabOpRec(regAlterTabOpPtr);

    /* Recompute aggregate table meta data. */
    computeTableMetaData(tabPtr, __LINE__);
  }

  if (AlterTableReq::getReorgFragFlag(req->changeMask)) {
    FragrecordPtr regFragPtr;
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr->fragrec); i++) {
      jam();
      if ((regFragPtr.i = regTabPtr->fragrec[i]) != RNIL) {
        jam();
        ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
        switch (regFragPtr.p->fragStatus) {
          case Fragrecord::FS_ONLINE:
            jam();
            regFragPtr.p->fragStatus = Fragrecord::FS_REORG_COMMIT;
            if (0)
              g_eventLogger->info(
                  "tab: %u frag: %u toggle fragstate from %s to %s",
                  regFragPtr.p->fragTableId, regFragPtr.p->fragmentId,
                  "FS_ONLINE", "FS_REORG_COMMIT");
            break;
          case Fragrecord::FS_REORG_NEW:
            jam();
            regFragPtr.p->fragStatus = Fragrecord::FS_REORG_COMMIT_NEW;
            if (0)
              g_eventLogger->info(
                  "tab: %u frag: %u toggle fragstate from %s to %s",
                  regFragPtr.p->fragTableId, regFragPtr.p->fragmentId,
                  "FS_REORG_NEW", "FS_REORG_COMMIT_NEW");
            break;
          default:
            jamLine(regFragPtr.p->fragStatus);
            ndbabort();
        }
      }
    }
  }

  sendAlterTabConf(signal, RNIL);
}

void Dbtup::handleAlterTableComplete(Signal *signal, const AlterTabReq *req,
                                     Tablerec *regTabPtr) {
  if (AlterTableReq::getReorgCompleteFlag(req->changeMask)) {
    FragrecordPtr regFragPtr;
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr->fragrec); i++) {
      jam();
      if ((regFragPtr.i = regTabPtr->fragrec[i]) != RNIL) {
        jam();
        ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
        switch (regFragPtr.p->fragStatus) {
          case Fragrecord::FS_REORG_COMPLETE:
            jam();
            if (0)
              g_eventLogger->info(
                  "tab: %u frag: %u toggle fragstate from %s to %s",
                  regFragPtr.p->fragTableId, regFragPtr.p->fragmentId,
                  "FS_REORG_COMPLETE", "FS_ONLINE");
            regFragPtr.p->fragStatus = Fragrecord::FS_ONLINE;
            break;
          case Fragrecord::FS_REORG_COMPLETE_NEW:
            jam();
            if (0)
              g_eventLogger->info(
                  "tab: %u frag: %u toggle fragstate from %s to %s",
                  regFragPtr.p->fragTableId, regFragPtr.p->fragmentId,
                  "FS_REORG_COMPLETE_NEW", "FS_ONLINE");
            regFragPtr.p->fragStatus = Fragrecord::FS_ONLINE;
            break;
          default:
            jamLine(regFragPtr.p->fragStatus);
            ndbabort();
        }
      }
    }
  }

  sendAlterTabConf(signal, RNIL);
}

void Dbtup::handleAlterTableAbort(Signal *signal, const AlterTabReq *req,
                                  const Tablerec *regTabPtr) {
  if (AlterTableReq::getAddAttrFlag(req->changeMask)) {
    jam();
    if (req->connectPtr != RNIL) {
      jam();
      AlterTabOperationPtr regAlterTabOpPtr;
      regAlterTabOpPtr.i = req->connectPtr;
      ptrCheckGuard(regAlterTabOpPtr, cnoOfAlterTabOps, alterTabOperRec);

      releaseTabDescr(regAlterTabOpPtr.p->tableDescriptor);
      releaseTabDescr(regAlterTabOpPtr.p->dynTableDescriptor);
      releaseAlterTabOpRec(regAlterTabOpPtr);
    }
  }

  sendAlterTabConf(signal, RNIL);
}

/*
  Update information for charset for a new attribute.
  If needed, attrDes2 will be updated with the correct charsetPos and
  charsetIndex will be updated to point to next free charsetPos slot.
*/
void Dbtup::handleCharsetPos(Uint32 csNumber, const CHARSET_INFO **charsetArray,
                             Uint32 noOfCharsets, Uint32 &charsetIndex,
                             Uint32 &attrDes2) {
  if (csNumber != 0) {
    const CHARSET_INFO *cs = all_charsets[csNumber];
    ndbrequire(cs != NULL);
    Uint32 i = 0;
    while (i < charsetIndex) {
      jam();
      if (charsetArray[i] == cs) break;
      i++;
    }
    if (i == charsetIndex) {
      jam();
      ndbrequire(i < noOfCharsets);
      charsetArray[i] = cs;
      charsetIndex++;
    }
    AttributeOffset::setCharsetPos(attrDes2, i);
  }
}

bool Dbtup::is_disk_columns_in_table(Uint32 tableId) {
  TablerecPtr regTabPtr;
  regTabPtr.i = tableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  return (regTabPtr.p->m_no_of_disk_attributes > 0);
}

/*
  This function (re-)computes aggregated metadata. It is called for
  both ALTER TABLE and CREATE TABLE.
 */
Uint32 Dbtup::computeTableMetaData(TablerecPtr tabPtr, Uint32 line) {
  Tablerec *regTabPtr = tabPtr.p;
  Uint32 dyn_null_words[2];

  for (Uint32 i = 0; i < NO_DYNAMICS; ++i) {
    jam();
    if (regTabPtr->m_dyn_null_bits[i] == DYN_BM_LEN_BITS) {
      jam();
      regTabPtr->m_dyn_null_bits[i] = 0;
    }
    dyn_null_words[i] = (regTabPtr->m_dyn_null_bits[i] + 31) >> 5;
    regTabPtr->m_offsets[i].m_dyn_null_words = dyn_null_words[i];
  }

  /* Compute the size of the static headers. */
  Uint32 pos[2] = {0, 0};
  if (regTabPtr->m_bits & Tablerec::TR_Checksum) {
    jam();
    pos[MM]++;
  }

  if (regTabPtr->m_bits & Tablerec::TR_RowGCI) {
    jam();
    pos[MM]++;
    pos[DD]++;
  }

  regTabPtr->m_no_of_disk_attributes =
      regTabPtr->m_attributes[DD].m_no_of_fixsize +
      regTabPtr->m_attributes[DD].m_no_of_varsize;

  regTabPtr->m_no_of_real_disk_attributes = regTabPtr->m_no_of_disk_attributes;

  DEB_DISK(("(%u) Tab(%u) no of disk attr: %u, line: %u", instance(), tabPtr.i,
            regTabPtr->m_no_of_disk_attributes, line));

  if (regTabPtr->m_no_of_disk_attributes > 0) {
    /* Room for disk part location. */
    regTabPtr->m_offsets[MM].m_disk_ref_offset =
        pos[MM] + Tuple_header::HeaderSize;
    pos[MM] += Disk_part_ref::SZ32;  // 8 bytes
    regTabPtr->m_bits |= Tablerec::TR_DiskPart;
  } else {
    regTabPtr->m_offsets[MM].m_disk_ref_offset =
        (pos[MM] + Tuple_header::HeaderSize) - Disk_part_ref::SZ32;
  }
  if (regTabPtr->m_attributes[MM].m_no_of_varsize ||
      regTabPtr->m_attributes[MM].m_no_of_dynamic) {
    jam();
    pos[MM] += Var_part_ref::SZ32;
    regTabPtr->m_bits &= ~(Uint32)Tablerec::TR_ForceVarPart;
  } else if (regTabPtr->m_bits & Tablerec::TR_ForceVarPart) {
    jam();
    pos[MM] += Var_part_ref::SZ32;
  }

  regTabPtr->m_offsets[MM].m_null_offset = pos[MM];
  regTabPtr->m_offsets[DD].m_null_offset = pos[DD];
  pos[MM] += regTabPtr->m_offsets[MM].m_null_words;
  pos[DD] += regTabPtr->m_offsets[DD].m_null_words;

  /*
    Compute the offsets for the attributes.
    For static fixed-size, this is the offset from the tuple pointer of the
    actual data.
    For static var-size and dynamic, this is the index into the offset array.

    We also compute the dynamic bitmasks here.
  */
  Uint32 *tabDesc = (Uint32 *)(tableDescriptor + regTabPtr->tabDescriptor);
  Uint32 fix_size[2] = {0, 0};
  Uint32 var_size[2] = {0, 0};
  Uint32 dyn_size[2] = {0, 0};
  Uint32 statvar_count = 0;
  Uint32 dynfix_count = 0;
  Uint32 dynvar_count = 0;
  Uint32 dynamic_count = 0;
  regTabPtr->blobAttributeMask.clear();
  regTabPtr->notNullAttributeMask.clear();
  regTabPtr->allPkAttributeMask.clear();
  regTabPtr->nonCharPkAttributeMask.clear();
  for (Uint32 i = 0; i < NO_DYNAMICS; ++i) {
    std::memset(regTabPtr->dynVarSizeMask[i], 0, dyn_null_words[i] << 2);
    std::memset(regTabPtr->dynFixSizeMask[i], 0, dyn_null_words[i] << 2);
  }

  for (Uint32 i = 0; i < regTabPtr->m_no_of_attributes; i++) {
    jam();
    Uint32 attrDescriptor = *tabDesc++;
    Uint32 attrDes2 = *tabDesc;
    Uint32 ind = AttributeDescriptor::getDiskBased(attrDescriptor);
    Uint32 attrLen = AttributeDescriptor::getSize(attrDescriptor);
    Uint32 arr = AttributeDescriptor::getArrayType(attrDescriptor);
    Uint32 size_in_words = AttributeDescriptor::getSizeInWords(attrDescriptor);
    Uint32 size_in_bytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
    Uint32 extType = AttributeDescriptor::getType(attrDescriptor);
    Uint32 off;

    if (extType == NDB_TYPE_BLOB || extType == NDB_TYPE_TEXT) {
      jam();
      regTabPtr->blobAttributeMask.set(i);
    }
    if (!AttributeDescriptor::getNullable(attrDescriptor)) {
      jam();
      regTabPtr->notNullAttributeMask.set(i);
    }
    if (AttributeDescriptor::getPrimaryKey(attrDescriptor)) {
      jam();
      regTabPtr->allPkAttributeMask.set(i);
      if (!AttributeOffset::getCharsetFlag(attrDes2)) {
        jam();
        regTabPtr->nonCharPkAttributeMask.set(i);
      }
    }
    if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
      if (arr == NDB_ARRAYTYPE_FIXED || ind == DD) {
        if (attrLen != 0) {
          jam();
          off = fix_size[ind] + pos[ind];
          fix_size[ind] += size_in_words;
        } else {
          jam();
          off = 0;  // Bit type
        }
      } else {
        jam();
        /* Static varsize. */
        ndbassert(ind == MM);
        off = statvar_count++;
        var_size[ind] += size_in_bytes;
      }
    } else {
      jam();
      /* Dynamic attribute. */
      dynamic_count++;
      ndbrequire(ind == MM);
      Uint32 null_pos = AttributeOffset::getNullFlagPos(attrDes2);
      dyn_size[ind] += (size_in_words << 2);
      if (arr == NDB_ARRAYTYPE_FIXED) {
        jam();
        // if (extType == NDB_TYPE_BLOB || extType == NDB_TYPE_TEXT)
        // regTabPtr->blobAttributeMask.set(i);
        // ToDo: I wonder what else is needed to handle BLOB/TEXT, if anything?

        if (attrLen != 0) {
          jam();
          if (size_in_words > InternalMaxDynFix) goto treat_as_varsize;

          off = dynfix_count++ + regTabPtr->m_attributes[ind].m_no_of_dyn_var;
          while (size_in_words-- > 0) {
            BitmaskImpl::set(dyn_null_words[ind],
                             regTabPtr->dynFixSizeMask[ind], null_pos++);
          }
        } else {
          jam();
          off = 0;  // Bit type
        }
      } else {
      treat_as_varsize:
        jam();
        off = dynvar_count++;
        BitmaskImpl::set(dyn_null_words[ind], regTabPtr->dynVarSizeMask[ind],
                         null_pos);
      }
    }
    if (off > AttributeOffset::getMaxOffset()) {
      jam();
      return ZTOO_LARGE_TUPLE_ERROR;
    }
    AttributeOffset::setOffset(attrDes2, off);
    *tabDesc++ = attrDes2;
  }
  ndbassert(dynvar_count == regTabPtr->m_attributes[MM].m_no_of_dyn_var);
  ndbassert(dynfix_count == regTabPtr->m_attributes[MM].m_no_of_dyn_fix);
  ndbassert(dynamic_count == regTabPtr->m_attributes[MM].m_no_of_dynamic);
  ndbassert(statvar_count == regTabPtr->m_attributes[MM].m_no_of_varsize);

  regTabPtr->m_offsets[MM].m_fix_header_size =
      Tuple_header::HeaderSize + fix_size[MM] + pos[MM];
  regTabPtr->m_offsets[DD].m_fix_header_size = fix_size[DD] + pos[DD];

  if (regTabPtr->m_attributes[DD].m_no_of_varsize == 0 &&
      regTabPtr->m_attributes[DD].m_no_of_fixsize > 0)
    regTabPtr->m_offsets[DD].m_fix_header_size += Tuple_header::HeaderSize;

  Uint32 mm_vars = regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 mm_dyns = regTabPtr->m_attributes[MM].m_no_of_dyn_fix +
                   regTabPtr->m_attributes[MM].m_no_of_dyn_var;
  Uint32 dd_vars = regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 dd_dyns = regTabPtr->m_attributes[DD].m_no_of_dynamic;

  regTabPtr->m_offsets[MM].m_max_var_offset = var_size[MM];
  /*
    Size of the expanded dynamic part. Needs room for bitmap, (N+1) 16-bit
    offset words with 32-bit padding, and all attribute data.
  */
  regTabPtr->m_offsets[MM].m_max_dyn_offset =
      (regTabPtr->m_offsets[MM].m_dyn_null_words << 2) +
      4 * ((mm_dyns + 2) >> 1) + dyn_size[MM];

  regTabPtr->m_offsets[DD].m_max_var_offset = var_size[DD];
  regTabPtr->m_offsets[DD].m_max_dyn_offset =
      (regTabPtr->m_offsets[DD].m_dyn_null_words << 2) +
      4 * ((dd_dyns + 2) >> 1) + dyn_size[DD];

  /* Room for data for all the attributes. */
  Uint32 total_rec_size = pos[MM] + fix_size[MM] + pos[DD] + fix_size[DD] +
                          ((var_size[MM] + 3) >> 2) +
                          ((dyn_size[MM] + 3) >> 2) +
                          ((var_size[DD] + 3) >> 2) + ((dyn_size[DD] + 3) >> 2);
  /*
    Room for offset arrays and dynamic bitmaps. There is one extra 16-bit
    offset in each offset array (for easy computation of final length).
    Also one word for storing total length of varsize+dynamic part
  */
  if (mm_vars + regTabPtr->m_attributes[MM].m_no_of_dynamic) {
    jam();
    total_rec_size += (mm_vars + 2) >> 1;
    total_rec_size += regTabPtr->m_offsets[MM].m_dyn_null_words;
    total_rec_size += (mm_dyns + 2) >> 1;
    total_rec_size += 1;
  }
  /* Disk data varsize offset array (not currently used). */
  if (dd_vars) total_rec_size += (dd_vars + 2) >> 1;
  /* Room for the header. */
  total_rec_size += Tuple_header::HeaderSize;
  if (regTabPtr->m_no_of_disk_attributes)
    total_rec_size += Tuple_header::HeaderSize;

  /* Room for changemask */
  total_rec_size += 1 + ((regTabPtr->m_no_of_attributes + 31) >> 5);

  total_rec_size += COPY_TUPLE_HEADER32;

  regTabPtr->total_rec_size = total_rec_size;

  DEB_TUP_META(("New total_rec_size set to %u", total_rec_size));

  setUpQueryRoutines(regTabPtr);
  setUpKeyArray(regTabPtr);
  return 0;
}

void Dbtup::undo_createtable_logsync_callback(Signal *signal, Uint32 ptrI,
                                              Uint32 res) {
  jamEntry();
  FragoperrecPtr fragOperPtr;
  fragOperPtr.i = ptrI;
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);

  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = 1;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, signal, 2,
             JBB);

  releaseFragoperrec(fragOperPtr);
}

/*
 * Descriptor has these parts:
 *
 * 0 readFunctionArray ( one for each attribute )
 * 1 updateFunctionArray ( ditto )
 * 2 charsetArray ( pointers to distinct CHARSET_INFO )
 * 3 readKeyArray ( attribute ids of keys )
 * 5 tabDescriptor ( attribute descriptors, each ZAD_SIZE )
 */
void Dbtup::setUpDescriptorReferences(Uint32 descriptorReference,
                                      Tablerec *const regTabPtr,
                                      const Uint32 *offset) {
  Uint32 *desc = &tableDescriptor[descriptorReference].tabDescr;
  regTabPtr->readFunctionArray = (ReadFunction *)(desc + offset[0]);
  regTabPtr->updateFunctionArray = (UpdateFunction *)(desc + offset[1]);
  regTabPtr->charsetArray = (const CHARSET_INFO **)(desc + offset[2]);
  regTabPtr->readKeyArray = descriptorReference + offset[3];
  regTabPtr->tabDescriptor = descriptorReference + offset[4];
  regTabPtr->m_real_order_descriptor = descriptorReference + offset[5];
}

void Dbtup::setupDynDescriptorReferences(Uint32 dynDescr,
                                         Tablerec *const regTabPtr,
                                         const Uint32 *offset, Uint32 ind) {
  regTabPtr->dynTabDescriptor[ind] = dynDescr;
  Uint32 *desc = &tableDescriptor[dynDescr].tabDescr;
  regTabPtr->dynVarSizeMask[ind] = desc + offset[0];
  regTabPtr->dynFixSizeMask[ind] = desc + offset[1];
}

Uint32 Dbtup::sizeOfReadFunction() {
  ReadFunction *tmp = (ReadFunction *)&tableDescriptor[0];
  TableDescriptor *start = &tableDescriptor[0];
  TableDescriptor *end = (TableDescriptor *)(tmp + 1);
  return (Uint32)(end - start);
}

void Dbtup::setUpKeyArray(Tablerec *const regTabPtr) {
  ndbrequire((regTabPtr->readKeyArray + regTabPtr->noOfKeyAttr) <
             cnoOfTabDescrRec);
  Uint32 *keyArray = &tableDescriptor[regTabPtr->readKeyArray].tabDescr;
  Uint32 countKeyAttr = 0;
  for (Uint32 i = 0; i < regTabPtr->m_no_of_attributes; i++) {
    jam();
    Uint32 refAttr = regTabPtr->tabDescriptor + (i * ZAD_SIZE);
    Uint32 attrDescriptor = getTabDescrWord(refAttr);
    if (AttributeDescriptor::getPrimaryKey(attrDescriptor)) {
      jam();
      AttributeHeader::init(&keyArray[countKeyAttr], i, 0);
      countKeyAttr++;
    }
  }
  ndbrequire(countKeyAttr == regTabPtr->noOfKeyAttr);

  /**
   * Setup real order array (16 bit per column)
   *
   * Sequence is [mm_fix mm_var mm_dynfix mm_dynvar dd_fix]
   */
  const Uint32 off = regTabPtr->m_real_order_descriptor;
  const Uint32 sz = (regTabPtr->m_no_of_attributes + 1) >> 1;
  ndbrequire((off + sz) < cnoOfTabDescrRec);

  Uint32 cnt = 0;
  Uint16 *order = (Uint16 *)&tableDescriptor[off].tabDescr;
  for (Uint32 type = 0; type < 5; type++) {
    for (Uint32 i = 0; i < regTabPtr->m_no_of_attributes; i++) {
      jam();
      Uint32 refAttr = regTabPtr->tabDescriptor + (i * ZAD_SIZE);
      Uint32 desc = getTabDescrWord(refAttr);
      Uint32 t = 0;

      if (AttributeDescriptor::getDynamic(desc) &&
          AttributeDescriptor::getArrayType(desc) == NDB_ARRAYTYPE_FIXED &&
          AttributeDescriptor::getSize(desc) == 0) {
        /*
          Dynamic bit types are stored inside the dynamic NULL bitmap, and are
          never expanded. So we do not need any real_order_descriptor for
          them.
        */
        jam();
        if (type == 0) cnt++;
        continue;
      }

      if ((AttributeDescriptor::getArrayType(desc) != NDB_ARRAYTYPE_FIXED &&
           !AttributeDescriptor::getDiskBased(desc)) ||
          (AttributeDescriptor::getDynamic(desc) &&
           AttributeDescriptor::getArrayType(desc) == NDB_ARRAYTYPE_FIXED &&
           AttributeDescriptor::getSizeInWords(desc) > InternalMaxDynFix)) {
        t += 1;
      }
      if (AttributeDescriptor::getDynamic(desc)) {
        t += 2;
      }
      if (AttributeDescriptor::getDiskBased(desc)) {
        t += 4;
      }
      ndbrequire(t < 5);  // Disk data currently only static/fixed
      if (t == type) {
        *order++ = i << ZAD_LOG_SIZE;
        cnt++;
      }
    }
  }
  ndbrequire(cnt == regTabPtr->m_no_of_attributes);
}

void Dbtup::releaseFragoperrec(FragoperrecPtr fragOperPtr) {
  fragOperPtr.p->inUse = false;
  fragOperPtr.p->nextFragoprec = cfirstfreeFragopr;
  cfirstfreeFragopr = fragOperPtr.i;
  RSS_OP_FREE(cnoOfFreeFragoprec);
}  // Dbtup::releaseFragoperrec()

void Dbtup::releaseAlterTabOpRec(AlterTabOperationPtr regAlterTabOpPtr) {
  regAlterTabOpPtr.p->nextAlterTabOp = cfirstfreeAlterTabOp;
  cfirstfreeAlterTabOp = regAlterTabOpPtr.i;
}

void Dbtup::deleteFragTab(Tablerec *const regTabPtr, Uint32 fragId) {
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(regTabPtr->fragid); i++) {
    jam();
    if (regTabPtr->fragid[i] == fragId) {
      jam();
      regTabPtr->fragid[i] = RNIL;
      regTabPtr->fragrec[i] = RNIL;
      return;
    }
  }
  ndbabort();
}

/*
 * LQH aborts on-going create table operation.  The table is later
 * dropped by DICT.
 */
void Dbtup::abortAddFragOp(Signal *signal) {
  FragoperrecPtr fragOperPtr;

  fragOperPtr.i = signal->theData[1];
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  ndbrequire(fragOperPtr.p->inUse);
  releaseFragoperrec(fragOperPtr);
}

void Dbtup::execDROP_TAB_REQ(Signal *signal) {
  jamEntry();
  if (ERROR_INSERTED(4013)) {
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    verifytabdes();
#endif
  }
  DropTabReq *req = (DropTabReq *)signal->getDataPtr();

  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  tabPtr.p->m_dropTable.tabUserRef = req->senderRef;
  tabPtr.p->m_dropTable.tabUserPtr = req->senderData;
  tabPtr.p->tableStatus = DROPPING;

  DEB_DISK(("(%u)Drop table(%u) start, pg_count: %u", instance(), tabPtr.i,
            c_page_map_pool_ptr->m_pg_count));

  signal->theData[0] = ZREL_FRAG;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = RNIL;
  sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
}

void Dbtup::releaseTabDescr(Tablerec *const regTabPtr) {
  Uint32 descriptor = regTabPtr->readKeyArray;
  if (descriptor != RNIL) {
    jam();
    Uint32 offset[10];
    getTabDescrOffsets(regTabPtr->m_no_of_attributes, regTabPtr->noOfCharsets,
                       regTabPtr->noOfKeyAttr, regTabPtr->m_no_of_extra_columns,
                       offset);

    regTabPtr->tabDescriptor = RNIL;
    regTabPtr->readKeyArray = RNIL;
    regTabPtr->readFunctionArray = NULL;
    regTabPtr->updateFunctionArray = NULL;
    regTabPtr->charsetArray = NULL;

    // move to start of descriptor
    descriptor -= offset[3];
    releaseTabDescr(descriptor);
  }

  /* Release dynamic descriptor, etc for mm and disk data. */

  for (Uint16 i = 0; i < NO_DYNAMICS; ++i) {
    jam();
    descriptor = regTabPtr->dynTabDescriptor[i];
    if (descriptor != RNIL) {
      jam();
      regTabPtr->dynTabDescriptor[i] = RNIL;
      regTabPtr->dynVarSizeMask[i] = NULL;
      regTabPtr->dynFixSizeMask[i] = NULL;
      releaseTabDescr(descriptor);
    }
  }
}

void Dbtup::releaseFragment(Signal *signal, Uint32 tableId,
                            Uint32 logfile_group_id) {
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  Uint32 fragIndex = RNIL;
  Uint32 fragId = RNIL;
  Uint32 i = 0;
  for (i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->fragid); i++) {
    jam();
    if (tabPtr.p->fragid[i] != RNIL) {
      jam();
      fragIndex = tabPtr.p->fragrec[i];
      fragId = tabPtr.p->fragid[i];
      break;
    }
  }
  if (fragIndex != RNIL) {
    jam();

    signal->theData[0] = ZUNMAP_PAGES;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = fragIndex;
    signal->theData[3] = 0;
    sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
    return;
  }

  CallbackPtr cb;
  if (logfile_group_id != RNIL) {
    jam();
    cb.m_callbackData = tabPtr.i;
    cb.m_callbackIndex = DROP_TABLE_LOG_BUFFER_CALLBACK;
    Uint32 sz = sizeof(Disk_undo::Drop) >> 2;
    D("Logfile_client - releaseFragment");
    Logfile_client lgman(this, c_lgman, logfile_group_id);
    int r0 = lgman.alloc_log_space(sz, false, false, jamBuffer());
    jamEntry();
    if (r0) {
      jam();
      warningEvent("Failed to alloc log space for drop table: %u", tabPtr.i);
      goto done;
    }

    int res = lgman.get_log_buffer(signal, sz, &cb);
    jamEntry();
    switch (res) {
      case 0:
        jam();
        return;
      case -1:
        jam();
        lgman.free_log_space(sz, jamBuffer());
        g_eventLogger->warning(
            "Out of space in RG_TRANSACTION_MEMORY resource,"
            " increase config parameter GlobalSharedMemory");
        warningEvent("Failed to get log buffer for drop table: %u", tabPtr.i);
        jamEntry();
        goto done;
      default:
        jam();
        goto execute;
    }
  }
done:
  drop_table_logsync_callback(signal, tabPtr.i, RNIL);
  return;
execute:
  jam();
  execute(signal, cb, logfile_group_id);
  return;
}

void Dbtup::drop_fragment_unmap_pages(Signal *signal, TablerecPtr tabPtr,
                                      FragrecordPtr fragPtr, Uint32 pos) {
  if (tabPtr.p->m_no_of_disk_attributes) {
    jam();
    Disk_alloc_info &alloc_info = fragPtr.p->m_disk_alloc_info;

    if (!alloc_info.m_unmap_pages.isEmpty()) {
      jam();
      signal->theData[0] = ZUNMAP_PAGES;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = fragPtr.i;
      signal->theData[3] = pos;
      sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
      return;
    }
    while (alloc_info.m_dirty_pages[pos].isEmpty() &&
           pos < EXTENT_SEARCH_MATRIX_COLS)
      pos++;

    if (pos == EXTENT_SEARCH_MATRIX_COLS) {
      jam();
      if (alloc_info.m_curr_extent_info_ptr_i != RNIL) {
        jam();
        Local_extent_info_list list(c_extent_pool,
                                    alloc_info.m_free_extents[0]);
        Ptr<Extent_info> ext_ptr;
        ndbrequire(
            c_extent_pool.getPtr(ext_ptr, alloc_info.m_curr_extent_info_ptr_i));
        list.addFirst(ext_ptr);
        alloc_info.m_curr_extent_info_ptr_i = RNIL;
      }

      drop_fragment_free_extent(signal, tabPtr, fragPtr, 0);
      return;
    }

    Ptr<Page> pagePtr;
    Page_pool *pool = (Page_pool *)&m_global_page_pool;
    jam();
    {
      Local_Page_list list(*pool, alloc_info.m_dirty_pages[pos]);
      list.first(pagePtr);
      list.remove(pagePtr);
    }

    Page_cache_client::Request req;
    req.m_page.m_page_no = pagePtr.p->m_page_no;
    req.m_page.m_file_no = pagePtr.p->m_file_no;
    req.m_table_id = fragPtr.p->fragTableId;
    req.m_fragment_id = fragPtr.p->fragmentId;

    req.m_callback.m_callbackData = pos;
    req.m_callback.m_callbackFunction =
        safe_cast(&Dbtup::drop_fragment_unmap_page_callback);

    int flags = Page_cache_client::COMMIT_REQ;
    Page_cache_client pgman(this, c_pgman);
    int res = pgman.get_page(signal, req, flags);
    jamEntry();
    switch (res) {
      case 0:
        jam();
        break;
      case -1:
        jam();
        break;
      default:
        jam();
        ndbrequire((Uint32)res == pagePtr.i);
        drop_fragment_unmap_page_callback(signal, pos, res);
    }
    return;
  }
  drop_fragment_free_extent(signal, tabPtr, fragPtr, 0);
}

void Dbtup::drop_fragment_unmap_page_callback(Signal *signal, Uint32 pos,
                                              Uint32 page_id) {
  jam();
  Ptr<GlobalPage> page;
  ndbrequire(m_global_page_pool.getPtr(page, page_id));

  Local_key key;
  key.m_page_no = ((Page *)page.p)->m_page_no;
  key.m_file_no = ((Page *)page.p)->m_file_no;

  Uint32 fragId = ((Page *)page.p)->m_fragment_id;
  Uint32 tableId = ((Page *)page.p)->m_table_id;
  Page_cache_client pgman(this, c_pgman);
  pgman.drop_page(key, page_id);
  jamEntry();

  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  getFragmentrec(fragPtr, fragId, tabPtr.p);

  signal->theData[0] = ZUNMAP_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragPtr.i;
  signal->theData[3] = pos;
  sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
}

void Dbtup::drop_fragment_free_extent(Signal *signal, TablerecPtr tabPtr,
                                      FragrecordPtr fragPtr, Uint32 pos) {
  if (tabPtr.p->m_no_of_disk_attributes) {
    jam();
    Disk_alloc_info &alloc_info = fragPtr.p->m_disk_alloc_info;
    for (; pos < EXTENT_SEARCH_MATRIX_SIZE; pos++) {
      jam();
      if (!alloc_info.m_free_extents[pos].isEmpty()) {
        jam();
        CallbackPtr cb;
        cb.m_callbackData = fragPtr.i;
        cb.m_callbackIndex = DROP_FRAGMENT_FREE_EXTENT_LOG_BUFFER_CALLBACK;
        execute(signal, cb, fragPtr.p->m_logfile_group_id);
        return;
      }
    }

    for (pos = 0; pos < EXTENT_SEARCH_MATRIX_COLS; pos++) {
      jam();
      ndbrequire(alloc_info.m_page_requests[pos].isEmpty());
      alloc_info.m_dirty_pages[pos].init();  // Clear dirty page list head
    }
  }

  signal->theData[0] = ZFREE_VAR_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}

void Dbtup::drop_table_log_buffer_callback(Signal *signal, Uint32 tablePtrI,
                                           Uint32 logfile_group_id) {
  ndbrequire(logfile_group_id != 0);
  TablerecPtr tabPtr;
  tabPtr.i = tablePtrI;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  ndbrequire(tabPtr.p->m_no_of_disk_attributes);

  Disk_undo::Drop drop;
  drop.m_table = tabPtr.i;
  Uint32 sz = sizeof(drop) >> 2;
  drop.m_type_length = (Disk_undo::UNDO_DROP << 16) | sz;
  D("Logfile_client - drop_table_log_buffer_callback");
  {
    Logfile_client lgman(this, c_lgman, logfile_group_id);

    Logfile_client::Change c[1] = {{&drop, sizeof(drop) >> 2}};
#ifdef DEBUG_TUP_META
    Uint64 lsn =
#endif
        lgman.add_entry_simple(c, 1, sz);
    jamEntry();

    DEB_TUP_META(("Add UNDO_TUP_DROP in lsn: %llu for tab: %u", lsn, tabPtr.i));

    /**
     * Normally we would eventually want a sync_lsn for this log entry
     * to ensure that this entry have reached the UNDO log.
     * This is not necessary here though and here is the reasoning why.
     * 1) The reason for writing this entry is to ensure that we don't
     *    apply any old UNDO log records towards new pages.
     *
     * An UNDO log record can only be applied if the page have a new
     * table id, fragment id and create table version. If the table id,
     * fragment id and table version belongs to the old table then
     * it won't be applied since this table is not being restored.
     *
     * Now a page cannot be written with a new table id, fragment id
     * and create table version unless it was first written to disk
     * and before this happened it was necessary to call sync_lsn
     * with the maximum LSN write of the page. This LSN is obviously
     * higher than the LSN received here, so we're safe that if a
     * page of an old table have been reused then also the
     * DROP TABLE undo log record is sync:ed to the UNDO log.
     *
     * So the conclusion is that we need to write the UNDO log record,
     * but there is no need to sync it right now, it will be sync:ed
     * before it is of any use.
     */
  }
  drop_table_logsync_callback(signal, tabPtr.i, logfile_group_id);
}

void Dbtup::drop_table_logsync_callback(Signal *signal, Uint32 tabPtrI,
                                        Uint32 logfile_group_id) {
  jam();
  TablerecPtr tabPtr;
  tabPtr.i = tabPtrI;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  DropTabConf *const dropConf = (DropTabConf *)signal->getDataPtrSend();
  dropConf->senderRef = reference();
  dropConf->senderData = tabPtr.p->m_dropTable.tabUserPtr;
  dropConf->tableId = tabPtr.i;
  sendSignal(tabPtr.p->m_dropTable.tabUserRef, GSN_DROP_TAB_CONF, signal,
             DropTabConf::SignalLength, JBB);

  releaseTabDescr(tabPtr.p);
  free_var_part(DefaultValuesFragment.p, tabPtr.p,
                &tabPtr.p->m_default_value_location);
  tabPtr.p->m_default_value_location.setNull();
  initTab(tabPtr.p);
}

void Dbtup::drop_fragment_free_extent_log_buffer_callback(Signal *signal,
                                                          Uint32 fragPtrI,
                                                          Uint32 unused) {
  jam();
  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  TablerecPtr tabPtr;
  tabPtr.i = fragPtr.p->fragTableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  ndbrequire(tabPtr.p->m_no_of_disk_attributes);
  Disk_alloc_info &alloc_info = fragPtr.p->m_disk_alloc_info;

  for (Uint32 pos = 0; pos < EXTENT_SEARCH_MATRIX_SIZE; pos++) {
    jam();
    if (!alloc_info.m_free_extents[pos].isEmpty()) {
      jam();
      Local_extent_info_list list(c_extent_pool,
                                  alloc_info.m_free_extents[pos]);
      Ptr<Extent_info> ext_ptr;
      list.first(ext_ptr);

      Uint64 lsn = 0;
      D("Tablespace_client - drop_fragment_free_extent_log_buffer_callback");
      Tablespace_client tsman(
          signal, this, c_tsman, tabPtr.i, fragPtr.p->fragmentId,
          c_lqh->getCreateSchemaVersion(tabPtr.i), fragPtr.p->m_tablespace_id);

      tsman.free_extent(&ext_ptr.p->m_key, lsn);
      jamEntry();
      c_extent_hash.remove(ext_ptr);
      list.release(ext_ptr);

      signal->theData[0] = ZFREE_EXTENT;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = fragPtr.i;
      signal->theData[3] = pos;
      sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
      return;
    }
  }
  ndbabort();
}

void Dbtup::drop_fragment_free_var_pages(Signal *signal) {
  jam();
  Uint32 tableId = signal->theData[1];
  Uint32 fragPtrI = signal->theData[2];

  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  PagePtr pagePtr;
  for (Uint32 i = 0; i < MAX_FREE_LIST + 1; i++) {
    jam();
    if (!fragPtr.p->free_var_page_array[i].isEmpty()) {
      jam();
      Local_Page_list list(c_page_pool, fragPtr.p->free_var_page_array[i]);
      ndbrequire(list.first(pagePtr));
      list.remove(pagePtr);
      returnCommonArea(pagePtr.i, 1);

      signal->theData[0] = ZFREE_VAR_PAGES;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = fragPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
      return;
    }
  }

  DynArr256::ReleaseIterator iter;
  DynArr256 map(c_page_map_pool_ptr, fragPtr.p->m_page_map);
  map.init(iter);
  signal->theData[0] = ZFREE_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragPtrI;
  memcpy(signal->theData + 3, &iter, sizeof(iter));
  sendSignal(reference(), GSN_CONTINUEB, signal, 3 + sizeof(iter) / 4, JBB);
}

void Dbtup::drop_fragment_free_pages(Signal *signal) {
  jam();
  Uint32 i;
  Uint32 tableId = signal->theData[1];
  Uint32 fragPtrI = signal->theData[2];
  DynArr256::ReleaseIterator iter;
  memcpy(&iter, signal->theData + 3, sizeof(iter));

  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  DynArr256 map(c_page_map_pool_ptr, fragPtr.p->m_page_map);
  Uint32 realpid;
  for (i = 0; i < 16; i++) {
    switch (map.release(iter, &realpid)) {
      case 0:
        jam();
        goto done;
      case 1:
        if (realpid != RNIL && ((realpid & FREE_PAGE_BIT) == 0)) {
          jam();
          /**
           * Both words in fragment page map can still have bit 30
           * set even for allocated pages to indicate the states
           * for lcp already scanned and state of page at last
           * LCP.
           */
          realpid &= PAGE_BIT_MASK;
          returnCommonArea(realpid, 1);
        }
        jam();
        break;
      case 2:
        jam();
        break;
    }
  }

  signal->theData[0] = ZFREE_PAGES;
  signal->theData[1] = tableId;
  signal->theData[2] = fragPtrI;
  memcpy(signal->theData + 3, &iter, sizeof(iter));
  sendSignal(reference(), GSN_CONTINUEB, signal, 3 + sizeof(iter) / 4, JBB);
  return;

done:
  for (i = 0; i < MAX_FREE_LIST + 1; i++) {
    ndbassert(fragPtr.p->free_var_page_array[i].isEmpty());
  }

  fragPtr.p->thFreeFirst.init();  // Clear free list head

  /**
   * Finish
   */
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  DEB_DISK(("(%u)Drop table(%u) done, pg_count: %u", instance(), tableId,
            c_page_map_pool_ptr->m_pg_count));
  /**
   * Remove LCP's for fragment
   */
  tabPtr.p->m_dropTable.m_fragPtrI = fragPtr.i;
  drop_fragment_fsremove_init(signal, tabPtr, fragPtr);
}

void Dbtup::drop_fragment_fsremove_done(Signal *signal, TablerecPtr tabPtr,
                                        FragrecordPtr fragPtr) {
  jam();
  DEB_TUP_META(("(%u)Done drop fragment: tab(%u,%u)", instance(),
                fragPtr.p->fragTableId, fragPtr.p->fragmentId));

  /**
   * LCP's removed...
   *   now continue with "next"
   */
  Uint32 logfile_group_id = fragPtr.p->m_logfile_group_id;

  remove_frag_from_tab(tabPtr, fragPtr.p->fragmentId);
  releaseFragrec(fragPtr);
  {
    Page_cache_client pgman(this, c_pgman);
    pgman.drop_fragment(tabPtr.i, fragPtr.p->fragmentId);
  }
  jam();
  if (tabPtr.p->tableStatus == DROPPING) {
    jam();
    signal->theData[0] = ZREL_FRAG;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = logfile_group_id;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
  } else {
    jam();
    DropFragConf *conf = (DropFragConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = tabPtr.p->m_dropTable.tabUserPtr;
    conf->tableId = tabPtr.i;
    sendSignal(tabPtr.p->m_dropTable.tabUserRef, GSN_DROP_FRAG_CONF, signal,
               DropFragConf::SignalLength, JBB);
    return;
  }
}

// Remove LCP
/**
 * Remove LCP control and data files
 * ---------------------------------
 * We remove all files related to the fragment which is dropped. This
 * includes all data files and both control files.
 * We will simply remove all potential files, most of them actually
 * don't exist, but we don't worry about removing non-existing files.
 *
 * If we need to optimise drop table then we should read the control
 * files to gather the information about which files are needed to
 * actually delete.
 */
void Dbtup::drop_fragment_fsremove_init(Signal *signal, TablerecPtr tabPtr,
                                        FragrecordPtr fragPtr) {
  jam();
  tabPtr.p->m_dropTable.m_lcpno = 0;
  tabPtr.p->m_dropTable.m_firstFileId = ZNIL;
  tabPtr.p->m_dropTable.m_lastFileId = 0;
  tabPtr.p->m_dropTable.m_numDataFiles = 0;

  DEB_TUP_META(("(%u)Start drop fragment: tab(%u,%u)", instance(),
                fragPtr.p->fragTableId, fragPtr.p->fragmentId));

  lcp_open_ctl_file(signal, tabPtr.i, fragPtr.p->fragTableId,
                    fragPtr.p->fragmentId, 0);
}

void Dbtup::lcp_open_ctl_file(Signal *signal, Uint32 tabPtrI, Uint32 tableId,
                              Uint32 fragmentId, Uint32 ctl_file) {
  FsOpenReq *req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = FsOpenReq::OM_READONLY;
  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  req->userPointer = tabPtrI;
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v5_setLcpNo(req->fileNumber, ctl_file);
  FsOpenReq::v5_setTableId(req->fileNumber, tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragmentId);
  req->page_size = 0;
  req->file_size_hi = UINT32_MAX;
  req->file_size_lo = UINT32_MAX;
  req->auto_sync_size = 0;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void Dbtup::execFSOPENREF(Signal *signal) {
  jamEntry();
  FsRef *ref = (FsRef *)signal->getDataPtr();
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;

  tabPtr.i = ref->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  if (tabPtr.p->m_dropTable.m_lcpno == 0) {
    jam();
    tabPtr.p->m_dropTable.m_lcpno = 1;
    lcp_open_ctl_file(signal, tabPtr.i, fragPtr.p->fragTableId,
                      fragPtr.p->fragmentId, 1);
    return;
  }
  lcp_read_completed(signal, tabPtr, fragPtr);
}

void Dbtup::execFSOPENCONF(Signal *signal) {
  FsConf *conf = (FsConf *)signal->getDataPtr();
  jamEntry();
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;

  tabPtr.i = conf->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  tabPtr.p->m_dropTable.m_filePointer = conf->filePointer;

  lcp_read_ctl_file(signal, tabPtr.i, conf->filePointer, fragPtr.p->fragTableId,
                    fragPtr.p->fragmentId, tabPtr.p->m_dropTable.m_lcpno);
}

void Dbtup::lcp_read_ctl_file(Signal *signal, Uint32 tabPtrI,
                              Uint32 filePointer, Uint32 tableId,
                              Uint32 fragmentId, Uint32 ctl_file) {
  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtrSend();
  req->userPointer = tabPtrI;
  req->filePointer = filePointer;
  req->userReference = reference();
  req->varIndex = 0;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatMemAddress);
  FsReadWriteReq::setPartialReadFlag(req->operationFlag, 1);
  req->data.memoryAddress.memoryOffset = 0;
  req->data.memoryAddress.fileOffset = 0;
  req->data.memoryAddress.size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, FsReadWriteReq::FixedLength + 3,
             JBA);
}

void Dbtup::execFSREADREF(Signal *signal) {
  jamEntry();
  ndbabort();
}

void Dbtup::execFSREADCONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;

  tabPtr.i = conf->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  const Uint32 bytesRead = conf->bytes_read;
  if (bytesRead != 0) {
    jam();
    if (handle_ctl_info(tabPtr, fragPtr, bytesRead)) {
      jam();
      DEB_DROP_TAB(
          ("(%u) handle_ctl_info failed, drop all tab(%u,%u)"
           ", ctl: %u",
           instance(), fragPtr.p->fragTableId, fragPtr.p->fragmentId,
           tabPtr.p->m_dropTable.m_lcpno));
      ndbassert(false);
      tabPtr.p->m_dropTable.m_firstFileId = 0;
      tabPtr.p->m_dropTable.m_numDataFiles = BackupFormat::NDB_MAX_LCP_FILES;
      tabPtr.p->m_dropTable.m_lastFileId = BackupFormat::NDB_MAX_LCP_FILES - 1;
      tabPtr.p->m_dropTable.m_lcpno = 1;
    } else {
      DEB_DROP_TAB(
          ("(%u) handle_ctl_info succeeded, drop all tab(%u,%u),"
           " ctl: %u",
           instance(), fragPtr.p->fragTableId, fragPtr.p->fragmentId,
           tabPtr.p->m_dropTable.m_lcpno));
    }
  }
  lcp_close_ctl_file(signal, tabPtr.i, tabPtr.p->m_dropTable.m_filePointer);
}

void Dbtup::lcp_close_ctl_file(Signal *signal, Uint32 tabPtrI,
                               Uint32 filePointer) {
  FsCloseReq *req = (FsCloseReq *)signal->getDataPtrSend();
  req->userPointer = tabPtrI;
  req->userReference = reference();
  req->filePointer = filePointer;
  req->fileFlag = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);
}

void Dbtup::execFSCLOSEREF(Signal *signal) {
  jamEntry();
  ndbabort();
}

void Dbtup::execFSCLOSECONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;

  tabPtr.i = conf->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  if (tabPtr.p->m_dropTable.m_lcpno == 0) {
    jam();
    tabPtr.p->m_dropTable.m_lcpno = 1;
    lcp_open_ctl_file(signal, tabPtr.i, fragPtr.p->fragTableId,
                      fragPtr.p->fragmentId, 1);
    return;
  }
  lcp_read_completed(signal, tabPtr, fragPtr);
}

bool Dbtup::handle_ctl_info(TablerecPtr tabPtr, FragrecordPtr fragPtr,
                            Uint32 bytesRead) {
  jam();
  BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (BackupFormat::LCPCtlFile *)&m_read_ctl_file_data[0];
  ndbassert(bytesRead == BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL ||
            bytesRead == BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG);
#ifdef DEBUG_DROP_TAB
  Uint32 createTableVersion = lcpCtlFilePtr->CreateTableVersion;
  DEB_DROP_TAB(("(%u)tab(%u,%u)handle_ctl_info table_version: %u", instance(),
                fragPtr.p->fragTableId, fragPtr.p->fragmentId,
                createTableVersion));
#endif
  if ((bytesRead != BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL &&
       bytesRead != BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG) ||
      !c_backup->convert_ctl_page_to_host(lcpCtlFilePtr)) {
    jam();
    return true;
  }
  Uint32 lastDataFileNumber = lcpCtlFilePtr->LastDataFileNumber;
  Uint32 numFiles = lcpCtlFilePtr->NumPartPairs;
  if (numFiles == 0) {
    jam();
    numFiles = 1;
  }
  Uint32 startFileNumber;
  if (lastDataFileNumber >= (numFiles - 1)) {
    jam();
    startFileNumber = lastDataFileNumber - (numFiles - 1);
  } else {
    jam();
    startFileNumber =
        BackupFormat::NDB_MAX_LCP_FILES + lastDataFileNumber - (numFiles - 1);
  }
  if (tabPtr.p->m_dropTable.m_firstFileId == ZNIL) {
    jam();
    tabPtr.p->m_dropTable.m_firstFileId = startFileNumber;
    tabPtr.p->m_dropTable.m_lastFileId = lastDataFileNumber;
    tabPtr.p->m_dropTable.m_numDataFiles = numFiles;
  } else {
    Uint32 this_last = lastDataFileNumber;
    Uint32 prev_last = tabPtr.p->m_dropTable.m_lastFileId;
    Uint32 diff;
    if (prev_last > this_last) {
      jam();
      diff = prev_last - this_last;
    } else if (prev_last < this_last) {
      jam();
      diff = this_last - prev_last;
    } else {
      /* equal, don't do anything since they should not differ */
      jam();
      return false;
    }
    if (diff > BackupFormat::NDB_MAX_FILES_PER_LCP &&
        diff < (BackupFormat::NDB_MAX_LCP_FILES -
                BackupFormat::NDB_MAX_FILES_PER_LCP)) {
      /**
       * Should not happen that they are more than 8 files apart.
       */
      jam();
      return true;
    }
    Uint32 last;
    if (diff <= BackupFormat::NDB_MAX_FILES_PER_LCP) {
      /* The larger is the last */
      jam();
      last = MAX(this_last, prev_last);
    } else {
      /* The smaller is the last */
      jam();
      last = MIN(this_last, prev_last);
      diff = BackupFormat::NDB_MAX_FILES_PER_LCP - diff;
    }
    tabPtr.p->m_dropTable.m_numDataFiles += diff;
    if (last == tabPtr.p->m_dropTable.m_lastFileId) {
      jam();
      /**
       * The last file is the same, so the new one needs to set the start
       * file number, but we need not set the last file number.
       */
      tabPtr.p->m_dropTable.m_firstFileId = startFileNumber;
    } else {
      jam();
      /**
       * The last file is the new one, so the new one needs not set the start
       * file number, but we need to set the last file number.
       */
      tabPtr.p->m_dropTable.m_lastFileId = last;
    }
  }
  return false;
}

void Dbtup::lcp_read_completed(Signal *signal, TablerecPtr tabPtr,
                               FragrecordPtr fragPtr) {
  if (tabPtr.p->m_dropTable.m_firstFileId == ZNIL) {
    jam();
    tabPtr.p->m_dropTable.m_firstFileId = 0;
    tabPtr.p->m_dropTable.m_lastFileId = 0;
    tabPtr.p->m_dropTable.m_numDataFiles = 1;
  }
  tabPtr.p->m_dropTable.m_file_type = 0;
  tabPtr.p->m_dropTable.m_outstanding_ops = 0;

  ndbrequire(tabPtr.p->m_dropTable.m_firstFileId <
             BackupFormat::NDB_MAX_LCP_FILES);
  ndbrequire(tabPtr.p->m_dropTable.m_lastFileId <
             BackupFormat::NDB_MAX_LCP_FILES);
  ndbrequire(tabPtr.p->m_dropTable.m_numDataFiles <=
             BackupFormat::NDB_MAX_LCP_FILES);
  ndbrequire(tabPtr.p->m_dropTable.m_numDataFiles > 0);
  DEB_TUP_META(
      ("(%u)Prepared drop fragment: tab(%u,%u)"
       " firstFileId: %u, lastFileId: %u, numDataFiles: %u",
       instance(), fragPtr.p->fragTableId, fragPtr.p->fragmentId,
       tabPtr.p->m_dropTable.m_firstFileId, tabPtr.p->m_dropTable.m_lastFileId,
       tabPtr.p->m_dropTable.m_numDataFiles));

  drop_fragment_fsremove(signal, tabPtr, fragPtr);
}

void Dbtup::drop_fragment_fsremove(Signal *signal, TablerecPtr tabPtr,
                                   FragrecordPtr fragPtr) {
  jam();
  Uint32 loop_count;
  Uint32 lcpno;
  Uint32 file_type = tabPtr.p->m_dropTable.m_file_type;

  if (file_type == 0 || file_type == 1) {
    jam();
    lcpno = tabPtr.p->m_dropTable.m_firstFileId;
    loop_count = MIN(8, (BackupFormat::NDB_MAX_LCP_FILES - lcpno));
    loop_count = MIN(loop_count, tabPtr.p->m_dropTable.m_numDataFiles);
    tabPtr.p->m_dropTable.m_firstFileId += loop_count;
    tabPtr.p->m_dropTable.m_firstFileId %= BackupFormat::NDB_MAX_LCP_FILES;
    tabPtr.p->m_dropTable.m_numDataFiles -= loop_count;
    if (tabPtr.p->m_dropTable.m_numDataFiles == 0) {
      jam();
      tabPtr.p->m_dropTable.m_file_type++;
      tabPtr.p->m_dropTable.m_numDataFiles =
          BackupFormat::NDB_MAX_FILES_PER_LCP;
    }
  } else if (file_type == 2) {
    jam();
    lcpno = 0;
    loop_count = 2;
    tabPtr.p->m_dropTable.m_file_type++;
  } else {
    jam();
    ndbrequire(file_type == 3);
    drop_fragment_fsremove_done(signal, tabPtr, fragPtr);
    return;
  }

  Uint32 fragId = fragPtr.p->fragmentId;
  Uint32 tableId = fragPtr.p->fragTableId;

  FsRemoveReq *req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = tabPtr.i;
  req->directory = 0;
  req->ownDirectory = 0;
  for (Uint32 i = 0; i < loop_count; i++) {
    FsOpenReq::setVersion(req->fileNumber, 5);
    if (file_type == 2) {
      jam();
      FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
      DEB_DROP_TAB(("(%u)Dropping ctl file for tab(%u,%u), ctl: %u", instance(),
                    tableId, fragId, lcpno));
    } else {
      jam();
      FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
    }
    FsOpenReq::v5_setLcpNo(req->fileNumber, lcpno++);
    FsOpenReq::v5_setTableId(req->fileNumber, tableId);
    FsOpenReq::v5_setFragmentId(req->fileNumber, fragId);
    sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, FsRemoveReq::SignalLength,
               JBB);
  }
  tabPtr.p->m_dropTable.m_outstanding_ops = loop_count;
}

void Dbtup::execFSREMOVEREF(Signal *signal) {
  jamEntry();
  FsRef *ref = (FsRef *)signal->getDataPtr();
  TablerecPtr tabPtr;

  const Uint32 userPointer = ref->userPointer;
  tabPtr.i = userPointer;
#ifdef DEBUG_DROP_TAB
  FragrecordPtr fragPtr;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  Uint32 fragId = fragPtr.p->fragmentId;
  Uint32 tableId = fragPtr.p->fragTableId;

  if (tabPtr.p->m_dropTable.m_file_type == 3) {
    DEB_DROP_TAB(("(%u) Failed to remove ctl file tab(%u,%u)", instance(),
                  tableId, fragId));
  }
#endif
  FsConf *conf = (FsConf *)signal->getDataPtrSend();
  conf->userPointer = userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  execFSREMOVECONF(signal);
}

void Dbtup::execFSREMOVECONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtrSend();

  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;

  tabPtr.i = conf->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  ndbrequire(tabPtr.p->m_dropTable.m_outstanding_ops > 0);
  tabPtr.p->m_dropTable.m_outstanding_ops--;
  if (tabPtr.p->m_dropTable.m_outstanding_ops > 0) {
    jam();
    return;
  }
  drop_fragment_fsremove(signal, tabPtr, fragPtr);
}

Uint32 Dbtup::get_max_lcp_record_size(Uint32 tableId) {
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  DEB_TUP_META_EXTRA(("(%u)LCP tab(%u) use total_rec_size = %u", instance(),
                      tableId, tabPtr.p->total_rec_size));

  return tabPtr.p->total_rec_size;
}
// End remove LCP

void Dbtup::start_restore_table(Uint32 tableId) {
  jam();
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  ndbassert(Uint16(tabPtr.p->m_attributes[DD].m_no_of_fixsize << 16) == 0);
  ndbassert(Uint16(tabPtr.p->m_attributes[DD].m_no_of_varsize << 16) == 0);

  Uint32 saveAttrCounts =
      (Uint32(tabPtr.p->m_attributes[DD].m_no_of_fixsize) << 16) |
      (Uint32(tabPtr.p->m_attributes[DD].m_no_of_varsize) << 0);

  tabPtr.p->m_dropTable.tabUserPtr = saveAttrCounts;
  tabPtr.p->m_dropTable.tabUserRef =
      (tabPtr.p->m_bits & Tablerec::TR_RowGCI) ? 1 : 0;
  tabPtr.p->m_createTable.defValLocation = tabPtr.p->m_default_value_location;

  Uint32 *tabDesc = (Uint32 *)(tableDescriptor + tabPtr.p->tabDescriptor);
  for (Uint32 i = 0; i < tabPtr.p->m_no_of_attributes; i++) {
    jam();
    Uint32 disk = AttributeDescriptor::getDiskBased(*tabDesc);
    Uint32 null = AttributeDescriptor::getNullable(*tabDesc);

    ndbrequire(tabPtr.p->notNullAttributeMask.get(i) != null);
    if (disk) {
      tabPtr.p->notNullAttributeMask.clear(i);
    }
    tabDesc += 2;
  }

  DEB_DISK(("(%u) start_restore_table Tab(%u) no of disk attr: %u", instance(),
            tabPtr.i, tabPtr.p->m_no_of_disk_attributes));

  tabPtr.p->m_no_of_disk_attributes = 0;
  tabPtr.p->m_attributes[DD].m_no_of_fixsize = 0;
  tabPtr.p->m_attributes[DD].m_no_of_varsize = 0;
  /* Avoid LQH trampling GCI restored in raw format */
  tabPtr.p->m_bits &= ~((Uint16)Tablerec::TR_RowGCI);
  tabPtr.p->m_default_value_location.setNull();
}

void Dbtup::complete_restore_table(Uint32 tableId) {
  jam();
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  Uint32 restoreAttrCounts = tabPtr.p->m_dropTable.tabUserPtr;

  tabPtr.p->m_attributes[DD].m_no_of_fixsize = restoreAttrCounts >> 16;
  tabPtr.p->m_attributes[DD].m_no_of_varsize = restoreAttrCounts & 0xffff;
  tabPtr.p->m_bits |=
      ((tabPtr.p->m_dropTable.tabUserRef & 1) ? Tablerec::TR_RowGCI : 0);

  tabPtr.p->m_no_of_disk_attributes =
      tabPtr.p->m_attributes[DD].m_no_of_fixsize +
      tabPtr.p->m_attributes[DD].m_no_of_varsize;
  tabPtr.p->m_default_value_location = tabPtr.p->m_createTable.defValLocation;

  DEB_DISK(("(%u) complete_restore_table Tab(%u) no of disk attr: %u",
            instance(), tabPtr.i, tabPtr.p->m_no_of_disk_attributes));

  Uint32 *tabDesc = (Uint32 *)(tableDescriptor + tabPtr.p->tabDescriptor);
  for (Uint32 i = 0; i < tabPtr.p->m_no_of_attributes; i++) {
    jam();
    Uint32 disk = AttributeDescriptor::getDiskBased(*tabDesc);
    Uint32 null = AttributeDescriptor::getNullable(*tabDesc);

    if (disk && !null) tabPtr.p->notNullAttributeMask.set(i);

    tabDesc += 2;
  }
}

void Dbtup::complete_restore_fragment(Signal *signal, Uint32 senderRef,
                                      Uint32 senderData, Uint32 restoredLcpId,
                                      Uint32 restoredLocalLcpId,
                                      Uint32 maxGciCompleted,
                                      Uint32 maxGciWritten, Uint32 tableId,
                                      Uint32 fragId) {
  /**
   * Rebuild free page list
   */
  Ptr<Fragoperrec> fragOpPtr;
  seizeFragoperrec(fragOpPtr);
  fragOpPtr.p->m_senderRef = senderRef;
  fragOpPtr.p->m_senderData = senderData;
  fragOpPtr.p->m_restoredLcpId = restoredLcpId;
  fragOpPtr.p->m_restoredLocalLcpId = restoredLocalLcpId;
  fragOpPtr.p->m_maxGciCompleted = maxGciCompleted;
  Ptr<Fragrecord> fragPtr;
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  getFragmentrec(fragPtr, fragId, tabPtr.p);
  /**
   * Restore will simply restore an LCP, no need to record rows
   * that was changed as part of this process. However any rows
   * changed by REDO log or other activity will be counted until
   * we start next LCP.
   *
   * See detailed comment on setting m_lcp_start_gci in
   * get_lcp_frag_stats function.
   *
   * maxGciCompleted == 0 indicates that no LCP was found to use
   * in restore so setting to ~0 to ensure that we will run next
   * LCP for this fragment and not use the idle LCP optimisation.
   *
   * We might have records written that have old_gci set up to
   * maxGciWritten. When these records are changed the first
   * time we also need to record it as a row change. So we need to
   * cater for that in the minimum old_gci to trigger an update of
   * the row change count.
   */
  Uint32 lcp_start_gci;
  if (maxGciCompleted == 0) {
    jam();
    lcp_start_gci = Uint32(~0);
  } else if (maxGciCompleted > maxGciWritten) {
    jam();
    lcp_start_gci = maxGciCompleted;
  } else {
    jam();
    lcp_start_gci = maxGciWritten;
  }
  fragPtr.p->m_lcp_changed_rows = 0;
  fragPtr.p->m_prev_row_count = fragPtr.p->m_row_count;
  set_lcp_start_gci(fragPtr.i, lcp_start_gci);

  fragOpPtr.p->fragPointer = fragPtr.i;
  fragPtr.p->m_free_page_id_list = FREE_PAGE_RNIL;

  signal->theData[0] = ZREBUILD_FREE_PAGE_LIST;
  signal->theData[1] = fragOpPtr.i;
  signal->theData[2] = 0;     // start page
  signal->theData[3] = RNIL;  // tail
  rebuild_page_free_list(signal);
}

bool Dbtup::get_frag_info(Uint32 tableId, Uint32 fragId, Uint32 *maxPage) {
  jamEntry();
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  getFragmentrec(fragPtr, fragId, tabPtr.p);

  if (maxPage) {
    *maxPage = fragPtr.p->m_max_page_cnt;
  }

  return true;
}

void Dbtup::set_lcp_start_gci(Uint32 fragPtrI, Uint32 startGci) {
  FragrecordPtr fragptr;
  fragptr.i = fragPtrI;
  ptrCheckGuard(fragptr, cnoOfFragrec, fragrecord);
  fragptr.p->m_lcp_start_gci = startGci;
}

const Dbtup::FragStats Dbtup::get_frag_stats(Uint32 fragId) const {
  Ptr<Fragrecord> fragptr;
  jam();
  ndbrequire(fragId < cnoOfFragrec);
  fragptr.i = fragId;
  ptrAss(fragptr, fragrecord);
  TablerecPtr tabPtr;
  tabPtr.i = fragptr.p->fragTableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  const Uint32 fixedWords = tabPtr.p->m_offsets[MM].m_fix_header_size;
  FragStats fs;
  fs.committedRowCount = fragptr.p->m_row_count;
  fs.committedChanges = fragptr.p->m_committed_changes;
  fs.fixedRecordBytes = static_cast<Uint32>(fixedWords * sizeof(Uint32));
  fs.pageSizeBytes = File_formats::NDB_PAGE_SIZE; /* 32768 */
  // Round downwards.
  fs.fixedSlotsPerPage = Tup_fixsize_page::DATA_WORDS / fixedWords;

  fs.fixedMemoryAllocPages = fragptr.p->noOfPages;
  fs.varMemoryAllocPages = fragptr.p->noOfVarPages;
  fs.varMemoryFreeBytes = fragptr.p->m_varWordsFree * sizeof(Uint32);
  // Amount of free memory should not exceed allocated memory.
  ndbassert(fs.varMemoryFreeBytes <=
            fs.varMemoryAllocPages * File_formats::NDB_PAGE_SIZE);
  fs.fixedElemCount = fragptr.p->m_fixedElemCount;
  // Memory in use should not exceed allocated memory.
  ndbassert(fs.fixedElemCount * fs.fixedRecordBytes <=
            fs.fixedMemoryAllocPages * File_formats::NDB_PAGE_SIZE);
  fs.varElemCount = fragptr.p->m_varElemCount;
  // Each row must has a fixed part and may have a var-sized part.
  ndbassert(fs.varElemCount <= fs.fixedElemCount);
  fs.logToPhysMapAllocBytes = fragptr.p->m_page_map.getByteSize();

  return fs;
}

Uint64 Dbtup::get_restore_row_count(Uint32 tableId, Uint32 fragId) {
  TablerecPtr tabPtr;
  Ptr<Fragrecord> fragPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  getFragmentrec(fragPtr, fragId, tabPtr.p);
  return fragPtr.p->m_row_count;
}

void Dbtup::get_lcp_frag_stats(Uint32 fragPtrI, Uint32 startGci,
                               Uint32 &maxPageCount, Uint64 &row_count,
                               Uint64 &prev_row_count, Uint64 &row_change_count,
                               Uint64 &memory_used_in_bytes, bool reset_flag) {
  /**
   * We want to count the number of rows, the number of changed rows
   * and the memory used in bytes as input to the Partial LCP
   * algorithm.
   *
   * Counting the rows is fairly straightforward, each insert of a row
   * will increment the row count and each free of a row of will decrement
   * the row count. The count will not be absolutely correct, there will
   * some differences due to commits and aborts, but it is detailed enough
   * to assist the Partial LCP algorithm.
   *
   * The row change count should count unique changed rows. So every
   * update should not be counted, only those updates that are new
   * since the last LCP. We don't have enough information to make this
   * count perfect and it is also not necessary to be absolutely
   * correct. It is however absolutely vital that we don't report 0
   * rows changed when actually some change have occurred.
   *
   * As an optimisation we don't count rows injected as part of the
   * restore of a fragment. This is inserting rows from an LCP that
   * exists and thus when the next LCP is to be executed something
   * more must have changed in order for row change count to be
   * larger than 0.
   *
   * During REDO log apply it is important to count the changes made
   * that wasn't part of the LCP. We know the Max Completed GCI of
   * each LCP, so if the row that is to be committed has a GCI which
   * is higher than this Max Completed GCI then we know that the
   * row have already been changed since we started the REDO log
   * execution and we can thus ignore the change when counting the
   * row change count.
   *
   * After REDO log execution we move onto the Copy Fragment part.
   * In this part the same principle still applies that if a row
   * that have its old GCI set higher than the Max Completed GCI
   * of the LCP restored then the row change can be ignored since
   * it must have been counted already when setting the GCI above
   * the Max Completed GCI before.
   *
   * One problem still is how to handle the cases when we have no
   * LCP to restore. In this case it doesn't really matter what we
   * do since the next LCP will be full LCP anyways. So here we
   * simply set the m_lcp_start_gci to ~0 which is the highest GCI
   * that could occur and thus all row changes will be counted.
   *
   * At start of LCP we gather the row count and the number of changed
   * rows to assist in deciding how to execute the LCP. We also set the
   * startGci, this is the highest GCI which have been set so far. So
   * if a row has a GCI higher than this, then we know that it has
   * been updated after this point and thus we need not count it as a
   * changed row anymore since it has already been counted.
   *
   * --------------------------------------------------------------
   * | Completed GCI | Highest seen GCI | Future GCIs             |
   * |               | == startGci      |                         |
   * --------------------------------------------------------------
   * So as we can see in this picture if old_gci belongs to completed
   * GCI, we are certain that it hasn't been updated before we came
   * here. If it is in future GCIs, we are certain that it has
   * already been updated since we came here. If it is equal to
   * startGci, then we don't know since we can have multiple updates
   * on the same row in one GCI. But we count all of those. So the
   * row change count might be a bit high, but it is a good
   * approximation to the number of changed rows since last LCP
   * started.
   */
  FragrecordPtr fragptr;
  fragptr.i = fragPtrI;
  ptrCheckGuard(fragptr, cnoOfFragrec, fragrecord);
  row_count = fragptr.p->m_row_count;
  prev_row_count = fragptr.p->m_prev_row_count;
  row_change_count = fragptr.p->m_lcp_changed_rows;
  maxPageCount = fragptr.p->m_max_page_cnt;

  if (reset_flag) {
    jam();
    if (fragptr.p->m_lcp_start_gci == Uint32(~0) && row_change_count == 0) {
      jam();
      /**
       * When no LCP existed before in restart we don't want to run the
       * next LCP as an IDLE LCP. It is most likely handled properly in
       * Backup, but for extra security we ensure that this optimisation
       * never happens when we restored a fragment from scratch. We do
       * this by signalling that one row changed and thus a proper LCP
       * is necessary.
       */
      row_change_count = 1;
    }
    fragptr.p->m_lcp_changed_rows = 0;
    fragptr.p->m_prev_row_count = row_count;
    fragptr.p->m_lcp_start_gci = startGci;
  }

  /**
   * Also calculate total amount of memory allocated to fragment.
   * This gives us an idea about the amount of data that needs to
   * be written in a fragment LCP for the memory part.
   */
  const Dbtup::FragStats fs = get_frag_stats(fragPtrI);
  Uint64 fixed_size = fs.fixedRecordBytes * fs.fixedElemCount;
  /* mem_size now contains memory in fixed part */
  Uint64 var_size_allocated =
      (fs.varMemoryAllocPages * (Tup_varsize_page::DATA_WORDS - 1) *
       sizeof(Uint32));
  Uint64 var_size_free = fs.varMemoryFreeBytes;
  ndbassert(var_size_allocated >= var_size_free);
  memory_used_in_bytes = fixed_size + (var_size_allocated - var_size_free);

  /**
   *  Varsize part is total allocated - area still free. We don't count the
   * varsize page headers. We only count the part which is actually
   * allocatable for rows. This means ignoring the header plus one word at the
   * end of the page which is used for list processing.
   */
  if (row_count != 0) {
    Uint64 average_row_size = memory_used_in_bytes / row_count;
    /* A simple safeguard */
    average_row_size = MAX(average_row_size, 32);
    fragptr.p->m_average_row_size = average_row_size;
  }
}

void Dbtup::execDROP_FRAG_REQ(Signal *signal) {
  jamEntry();
  if (ERROR_INSERTED(4013)) {
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    verifytabdes();
#endif
  }
  DropFragReq *req = (DropFragReq *)signal->getDataPtr();

  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  tabPtr.p->m_dropTable.tabUserRef = req->senderRef;
  tabPtr.p->m_dropTable.tabUserPtr = req->senderData;

  Uint32 fragIndex = get_frag_from_tab(tabPtr, req->fragId);
  if (fragIndex != RNIL) {
    jam();

    signal->theData[0] = ZUNMAP_PAGES;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = fragIndex;
    signal->theData[3] = 0;
    sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
    return;
  }

  DropFragConf *conf = (DropFragConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = tabPtr.p->m_dropTable.tabUserPtr;
  conf->tableId = tabPtr.i;
  sendSignal(tabPtr.p->m_dropTable.tabUserRef, GSN_DROP_FRAG_CONF, signal,
             DropFragConf::SignalLength, JBB);
}
