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

#ifndef TransporterInternalDefinitions_H
#define TransporterInternalDefinitions_H

#if defined DEBUG_TRANSPORTER || defined VM_TRACE
#include <NdbOut.hpp>
#endif

#define NDB_TCP_TRANSPORTER

#ifdef HAVE_NDB_SHM
#define NDB_SHM_TRANSPORTER
#endif

#ifdef HAVE_NDB_SCI
#define NDB_SCI_TRANSPORTER
#endif

#ifdef DEBUG_TRANSPORTER
#define DEBUG(x) ndbout << x << endl
#else
#define DEBUG(x)
#endif

#if defined VM_TRACE || defined DEBUG_TRANSPORTER
#define WARNING(X) ndbout << X << endl;
#else
#define WARNING(X)
#endif

// Calculate a checksum
inline
Uint32
computeChecksum(const Uint32 * const startOfData, int nWords) {
  Uint32 chksum = startOfData[0];
  for (int i=1; i < nWords; i++)
    chksum ^= startOfData[i];
  return chksum;
}

struct Protocol6 {
  Uint32 word1;
  Uint32 word2;
  Uint32 word3;
  
/**
 *
 * b = Byte order           -  4 Bits (Note 1 significant bit)
 * g = GSN                  - 16 Bits
 * p = Prio                 -  2 Bits
 * c = Checksum included    -  1 Bit
 * z = Compression          -  1 Bit
 * v = Version id           -  4 Bits
 * i = Signal id included   -  1 Bit
 * m = Message length       - 16 Bits (0-65536) (In word -> 0-256k bytes)
 * d = Signal data length   -  5 Bits (0-31)
 * t = trace                -  6 Bits (0-63)
 * r = Recievers block no   - 16 Bits
 * s = Senders block no     - 16 Bits
 * u = Unused               -  7 Bits
 * f = FragmentInfo1        -  1 Bit
 * h = FragmentInfo2        -  1 bit
 * n = No of segments       -  2 Bits

 * Word 1
 *
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * bfizcppbmmmmmmmmmmmmmmmmbhdddddb

 **
 * Word 2
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * ggggggggggggggggvvvvttttttnn  
 
 **
 * Word 3
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * rrrrrrrrrrrrrrrrssssssssssssssss

 **
 * Word 4 (Optional Signal Id)
 */
  
  /**
   * 0 = Big endian (Sparc), 1 = Little endian (Intel)
   */
  static Uint32 getByteOrder       (const Uint32 & word1);
  static Uint32 getCompressed      (const Uint32 & word1);
  static Uint32 getSignalIdIncluded(const Uint32 & word1);
  static Uint32 getCheckSumIncluded(const Uint32 & word1);
  static Uint32 getPrio            (const Uint32 & word1);
  static Uint32 getMessageLength   (const Uint32 & word1);

  static void setByteOrder       (Uint32 & word1, Uint32 byteOrder);
  static void setCompressed      (Uint32 & word1, Uint32 compressed);
  static void setSignalIdIncluded(Uint32 & word1, Uint32 signalId);
  static void setCheckSumIncluded(Uint32 & word1, Uint32 checkSum);
  static void setPrio            (Uint32 & word1, Uint32 prio);
  static void setMessageLength   (Uint32 & word1, Uint32 messageLen);
  
  static void createSignalHeader(SignalHeader * const dst,
				 const Uint32 & word1, 
				 const Uint32 & word2, 
				 const Uint32 & word3);
  
  static void createProtocol6Header(Uint32 & word1, 
				    Uint32 & word2, 
				    Uint32 & word3,
				    const SignalHeader * const src);
};  

#define WORD1_BYTEORDER_MASK   (0x81000081)
#define WORD1_SIGNALID_MASK    (0x00000004)
#define WORD1_COMPRESSED_MASK  (0x00000008)
#define WORD1_CHECKSUM_MASK    (0x00000010)
#define WORD1_PRIO_MASK        (0x00000060)
#define WORD1_MESSAGELEN_MASK  (0x00FFFF00)
#define WORD1_SIGNAL_LEN_MASK  (0x7C000000)
#define WORD1_FRAG_INF_MASK    (0x00000002)
#define WORD1_FRAG_INF2_MASK   (0x02000000)

#define WORD1_FRAG_INF_SHIFT   (1)
#define WORD1_SIGNALID_SHIFT   (2)
#define WORD1_COMPRESSED_SHIFT (3)
#define WORD1_CHECKSUM_SHIFT   (4)
#define WORD1_PRIO_SHIFT       (5)
#define WORD1_MESSAGELEN_SHIFT (8)
#define WORD1_FRAG_INF2_SHIFT  (25)
#define WORD1_SIGNAL_LEN_SHIFT (26)

#define WORD2_VERID_GSN_MASK   (0x000FFFFF)
#define WORD2_TRACE_MASK       (0x03f00000)
#define WORD2_SEC_COUNT_MASK   (0x0c000000)

#define WORD2_TRACE_SHIFT      (20)
#define WORD2_SEC_COUNT_SHIFT  (26)

#define WORD3_SENDER_MASK      (0x0000FFFF)
#define WORD3_RECEIVER_MASK    (0xFFFF0000)

#define WORD3_RECEIVER_SHIFT   (16)

inline
Uint32
Protocol6::getByteOrder(const Uint32 & word1){
  return word1 & 1;
}

inline
Uint32
Protocol6::getCompressed(const Uint32 & word1){
  return (word1 & WORD1_COMPRESSED_MASK) >> WORD1_COMPRESSED_SHIFT;
}

inline
Uint32
Protocol6::getSignalIdIncluded(const Uint32 & word1){
  return (word1 & WORD1_SIGNALID_MASK) >> WORD1_SIGNALID_SHIFT;
}

inline
Uint32
Protocol6::getCheckSumIncluded(const Uint32 & word1){
  return (word1 & WORD1_CHECKSUM_MASK) >> WORD1_CHECKSUM_SHIFT;
}

inline
Uint32
Protocol6::getMessageLength(const Uint32 & word1){
  return (word1 & WORD1_MESSAGELEN_MASK) >> WORD1_MESSAGELEN_SHIFT;
}

inline
Uint32
Protocol6::getPrio(const Uint32 & word1){
  return (word1 & WORD1_PRIO_MASK) >> WORD1_PRIO_SHIFT;
}

inline
void
Protocol6::setByteOrder(Uint32 & word1, Uint32 byteOrder){
  Uint32 tmp = byteOrder;
  tmp |= (tmp << 7);
  tmp |= (tmp << 24);
  word1 |= (tmp & WORD1_BYTEORDER_MASK);
}

inline
void
Protocol6::setCompressed(Uint32 & word1, Uint32 compressed){
  word1 |= ((compressed << WORD1_COMPRESSED_SHIFT) & WORD1_COMPRESSED_MASK);
}

inline
void
Protocol6::setSignalIdIncluded(Uint32 & word1, Uint32 signalId){
  word1 |= ((signalId << WORD1_SIGNALID_SHIFT) & WORD1_SIGNALID_MASK);
}

inline
void
Protocol6::setCheckSumIncluded(Uint32 & word1, Uint32 checkSum){
  word1 |= ((checkSum << WORD1_CHECKSUM_SHIFT) & WORD1_CHECKSUM_MASK);
}

inline
void
Protocol6::setMessageLength(Uint32 & word1, Uint32 messageLen){
  word1 |= ((messageLen << WORD1_MESSAGELEN_SHIFT) & WORD1_MESSAGELEN_MASK);
}

inline
void
Protocol6::setPrio(Uint32 & word1, Uint32 prio){
  word1 |= ((prio << WORD1_PRIO_SHIFT) & WORD1_PRIO_MASK);
}

inline
void
Protocol6::createSignalHeader(SignalHeader * const dst,
			      const Uint32 & word1, 
			      const Uint32 & word2, 
			      const Uint32 & word3){
  
  Uint32 signal_len = (word1 & WORD1_SIGNAL_LEN_MASK)>> WORD1_SIGNAL_LEN_SHIFT;
  Uint32 fragInfo1  = (word1 & WORD1_FRAG_INF_MASK) >> (WORD1_FRAG_INF_SHIFT-1);
  Uint32 fragInfo2  = (word1 & WORD1_FRAG_INF2_MASK) >> (WORD1_FRAG_INF2_SHIFT);
  Uint32 trace      = (word2 & WORD2_TRACE_MASK) >> WORD2_TRACE_SHIFT;
  Uint32 verid_gsn  = (word2 & WORD2_VERID_GSN_MASK);
  Uint32 secCount   = (word2 & WORD2_SEC_COUNT_MASK) >> WORD2_SEC_COUNT_SHIFT;
  
  dst->theTrace              = trace;
  dst->m_noOfSections        = secCount;
  dst->m_fragmentInfo        = fragInfo1 | fragInfo2;
  
  dst->theLength             = signal_len;
  dst->theVerId_signalNumber = verid_gsn;
  
  Uint32 sBlockNum  = (word3 & WORD3_SENDER_MASK);
  Uint32 rBlockNum  = (word3 & WORD3_RECEIVER_MASK) >> WORD3_RECEIVER_SHIFT;
  
  dst->theSendersBlockRef      = sBlockNum;
  dst->theReceiversBlockNumber = rBlockNum;
}

inline
void
Protocol6::createProtocol6Header(Uint32 & word1, 
				 Uint32 & word2, 
				 Uint32 & word3,
				 const SignalHeader * const src){
  const Uint32 signal_len = src->theLength;
  const Uint32 fragInfo   = src->m_fragmentInfo;
  const Uint32 fragInfo1 = (fragInfo & 2);
  const Uint32 fragInfo2 = (fragInfo & 1);
  
  const Uint32 trace      = src->theTrace;
  const Uint32 verid_gsn  = src->theVerId_signalNumber;
  const Uint32 secCount   = src->m_noOfSections;
  
  word1 |= ((signal_len << WORD1_SIGNAL_LEN_SHIFT) & WORD1_SIGNAL_LEN_MASK);
  word1 |= ((fragInfo1 << (WORD1_FRAG_INF_SHIFT-1)) & WORD1_FRAG_INF_MASK);
  word1 |= ((fragInfo2 << WORD1_FRAG_INF2_SHIFT) & WORD1_FRAG_INF2_MASK);

  word2 |= ((trace << WORD2_TRACE_SHIFT) & WORD2_TRACE_MASK);
  word2 |= (verid_gsn & WORD2_VERID_GSN_MASK);
  word2 |= ((secCount << WORD2_SEC_COUNT_SHIFT) & WORD2_SEC_COUNT_MASK);
  
  Uint32 sBlockNum  = src->theSendersBlockRef ;
  Uint32 rBlockNum  = src->theReceiversBlockNumber ;
  
  word3 |= (sBlockNum & WORD3_SENDER_MASK);
  word3 |= ((rBlockNum << WORD3_RECEIVER_SHIFT) & WORD3_RECEIVER_MASK);
}

// Define of TransporterInternalDefinitions_H
#endif
