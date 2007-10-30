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


#define DBTUP_C
#define DBTUP_ROUTINES_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>

void
Dbtup::setUpQueryRoutines(Tablerec *regTabPtr)
{
  Uint32 startDescriptor= regTabPtr->tabDescriptor;
  ndbrequire((startDescriptor + (regTabPtr->m_no_of_attributes << ZAD_LOG_SIZE)) 
	     <= cnoOfTabDescrRec);
  for (Uint32 i= 0; i < regTabPtr->m_no_of_attributes; i++) {
    Uint32 attrDescrStart= startDescriptor + (i << ZAD_LOG_SIZE);
    Uint32 attrDescr= tableDescriptor[attrDescrStart].tabDescr;
    Uint32 attrOffset= tableDescriptor[attrDescrStart + 1].tabDescr;

    Uint32 type = AttributeDescriptor::getType(attrDescr);
    Uint32 array = AttributeDescriptor::getArrayType(attrDescr);
    Uint32 charset = AttributeOffset::getCharsetFlag(attrOffset);
    Uint32 size = AttributeDescriptor::getSize(attrDescr);
    Uint32 bytes = AttributeDescriptor::getSizeInBytes(attrDescr);
    Uint32 words = AttributeDescriptor::getSizeInWords(attrDescr);
    Uint32 nullable = AttributeDescriptor::getNullable(attrDescr);
    Uint32 dynamic = AttributeDescriptor::getDynamic(attrDescr);

    if (!dynamic)
    {
      if (array  == NDB_ARRAYTYPE_FIXED)
      {
        if (!nullable)
        {
          switch(size){
          case DictTabInfo::aBit:
            jam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBitsNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBitsNotNULL;
            break;
          case DictTabInfo::an8Bit:
          case DictTabInfo::a16Bit:
            jam();
            regTabPtr->readFunctionArray[i]=
	      &Dbtup::readFixedSizeTHManyWordNotNULL;
            regTabPtr->updateFunctionArray[i]=
	      &Dbtup::updateFixedSizeTHManyWordNotNULL;
            break;
          case DictTabInfo::a32Bit:
          case DictTabInfo::a64Bit:
          case DictTabInfo::a128Bit:
          default:
            switch(bytes){
            case 4:
              jam();
              regTabPtr->readFunctionArray[i] = 
                &Dbtup::readFixedSizeTHOneWordNotNULL;
              regTabPtr->updateFunctionArray[i] = 
                &Dbtup::updateFixedSizeTHOneWordNotNULL;
              break;
            case 8:
              jam();
              regTabPtr->readFunctionArray[i] = 
                &Dbtup::readFixedSizeTHTwoWordNotNULL;
              regTabPtr->updateFunctionArray[i] = 
                &Dbtup::updateFixedSizeTHManyWordNotNULL;
              break;
            default:
              jam();
              regTabPtr->readFunctionArray[i] = 
                &Dbtup::readFixedSizeTHManyWordNotNULL;
              regTabPtr->updateFunctionArray[i] = 
                &Dbtup::updateFixedSizeTHManyWordNotNULL;
              break;
            }
          }
          if (charset)
          {
            jam();
            regTabPtr->readFunctionArray[i] = 
              &Dbtup::readFixedSizeTHManyWordNotNULL;
            regTabPtr->updateFunctionArray[i] = 
              &Dbtup::updateFixedSizeTHManyWordNotNULL;
          }
        }
        else // nullable
        {
          switch(size){
          case DictTabInfo::aBit:
            jam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBitsNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBitsNULLable;
            break;
          case DictTabInfo::an8Bit:
          case DictTabInfo::a16Bit:
            jam();
            regTabPtr->readFunctionArray[i]=
	      &Dbtup::readFixedSizeTHManyWordNULLable;
            regTabPtr->updateFunctionArray[i]=
	      &Dbtup::updateFixedSizeTHManyWordNULLable;
            break;
          case DictTabInfo::a32Bit:
          case DictTabInfo::a64Bit:
          case DictTabInfo::a128Bit:
          default:
            switch(bytes){
            case 4:
              jam();
              regTabPtr->readFunctionArray[i] = 
                &Dbtup::readFixedSizeTHOneWordNULLable;
              regTabPtr->updateFunctionArray[i] = 
                &Dbtup::updateFixedSizeTHManyWordNULLable;
              break;
            case 8:
              jam();
              regTabPtr->readFunctionArray[i] = 
                &Dbtup::readFixedSizeTHTwoWordNULLable;
              regTabPtr->updateFunctionArray[i] = 
                &Dbtup::updateFixedSizeTHManyWordNULLable;
              break;
            default:
              jam();
              regTabPtr->readFunctionArray[i] = 
                &Dbtup::readFixedSizeTHManyWordNULLable;
              regTabPtr->updateFunctionArray[i] = 
                &Dbtup::updateFixedSizeTHManyWordNULLable;
              break;
            }
          }
          if (charset)
          {
            jam();
            regTabPtr->readFunctionArray[i] = 
              &Dbtup::readFixedSizeTHManyWordNULLable;
            regTabPtr->updateFunctionArray[i] = 
              &Dbtup::updateFixedSizeTHManyWordNULLable;
          }
        }
      } else 
      {
        if (!nullable)
        {
          jam();
          regTabPtr->readFunctionArray[i]=
	    &Dbtup::readVarSizeNotNULL;
          regTabPtr->updateFunctionArray[i]=
	    &Dbtup::updateVarSizeNotNULL;
        } 
        else 
        {
          jam();
          regTabPtr->readFunctionArray[i]=
	    &Dbtup::readVarSizeNULLable;
          regTabPtr->updateFunctionArray[i]=
	    &Dbtup::updateVarSizeNULLable;
        }
      }
      if(AttributeDescriptor::getDiskBased(attrDescr))
      {
        // array initializer crashes gcc-2.95.3
	ReadFunction r[6];
        {
	  r[0] = &Dbtup::readDiskBitsNotNULL;
	  r[1] = &Dbtup::readDiskBitsNULLable;
	  r[2] = &Dbtup::readDiskFixedSizeNotNULL;
	  r[3] = &Dbtup::readDiskFixedSizeNULLable;
	  r[4] = &Dbtup::readDiskVarSizeNULLable;
	  r[5] = &Dbtup::readDiskVarSizeNotNULL;
        }
	UpdateFunction u[6];
        {
	  u[0] = &Dbtup::updateDiskBitsNotNULL;
	  u[1] = &Dbtup::updateDiskBitsNULLable;
	  u[2] = &Dbtup::updateDiskFixedSizeNotNULL;
	  u[3] = &Dbtup::updateDiskFixedSizeNULLable;
	  u[4] = &Dbtup::updateDiskVarSizeNULLable;
	  u[5] = &Dbtup::updateDiskVarSizeNotNULL;
        }
	Uint32 a= 
	  AttributeDescriptor::getArrayType(attrDescr) == NDB_ARRAYTYPE_FIXED 
          ? 2 : 4;
	
	if(AttributeDescriptor::getSize(attrDescr) == 0)
	  a= 0;
	
	Uint32 b= 
	  AttributeDescriptor::getNullable(attrDescr)? 1 : 0;
	regTabPtr->readFunctionArray[i]= r[a+b];
	regTabPtr->updateFunctionArray[i]= u[a+b];
      }
    }
    else // dynamic
    {
      if (nullable)
      {
        if (array == NDB_ARRAYTYPE_FIXED)
        {
          if (size == 0)
          {
            jam(); 
            regTabPtr->readFunctionArray[i]= &Dbtup::readDynBitsNULLable;
            regTabPtr->updateFunctionArray[i]= &Dbtup::updateDynBitsNULLable;
          } 
          else if (words > InternalMaxDynFix) 
          {
            jam();
            regTabPtr->readFunctionArray[i]= 
              &Dbtup::readDynBigFixedSizeNULLable;
            regTabPtr->updateFunctionArray[i]= 
              &Dbtup::updateDynBigFixedSizeNULLable;
          } 
          else 
          {
            jam();
            regTabPtr->readFunctionArray[i]= 
              &Dbtup::readDynFixedSizeNULLable;
            regTabPtr->updateFunctionArray[i]= 
              &Dbtup::updateDynFixedSizeNULLable;
          }
        } 
        else 
        {
          regTabPtr->readFunctionArray[i]= &Dbtup::readDynVarSizeNULLable;
          regTabPtr->updateFunctionArray[i]= &Dbtup::updateDynVarSizeNULLable;
        }
      } 
      else // nullable
      {
        if (array == NDB_ARRAYTYPE_FIXED)
        {
          if (size == 0)
          {
            jam(); 
            regTabPtr->readFunctionArray[i]= &Dbtup::readDynBitsNotNULL;
            regTabPtr->updateFunctionArray[i]= &Dbtup::updateDynBitsNotNULL;
          } 
          else if (words > InternalMaxDynFix) 
          {
            jam();
            regTabPtr->readFunctionArray[i]= 
              &Dbtup::readDynBigFixedSizeNotNULL;
            regTabPtr->updateFunctionArray[i]= 
              &Dbtup::updateDynBigFixedSizeNotNULL;
          } 
          else 
          {
            jam();
            regTabPtr->readFunctionArray[i]= 
              &Dbtup::readDynFixedSizeNotNULL;
            regTabPtr->updateFunctionArray[i]= 
              &Dbtup::updateDynFixedSizeNotNULL;
          }
        } 
        else 
        {
          jam();
	  regTabPtr->readFunctionArray[i]= &Dbtup::readDynVarSizeNotNULL;
          regTabPtr->updateFunctionArray[i]= &Dbtup::updateDynVarSizeNotNULL;
        }
      }
    }
  }
}

/* Dump a byte buffer, for debugging. */
static void dump_buf_hex(unsigned char *p, Uint32 bytes)
{
  char buf[3001];
  char *q= buf;
  buf[0]= '\0';

  for(Uint32 i=0; i<bytes; i++)
  {
    if(i==((sizeof(buf)/3)-1))
    {
      sprintf(q, "...");
      break;
    }
    sprintf(q+3*i, " %02X", p[i]);
  }
  ndbout_c("%8p: %s", p, buf);
}

static
inline
Uint32
pad32(Uint32 bytepos, Uint32 bitsused)
{
  if (bitsused)
  {
    assert((bytepos & 3) == 0);
  }
  Uint32 ret = 4 * ((bitsused + 31 >> 5)) +
    ((bytepos + 3) & ~(Uint32)3);
  return ret;
}

/* ---------------------------------------------------------------- */
/*       THIS ROUTINE IS USED TO READ A NUMBER OF ATTRIBUTES IN THE */
/*       DATABASE AND PLACE THE RESULT IN ATTRINFO RECORDS.         */
//
// In addition to the parameters used in the call it also relies on the
// following variables set-up properly.
//
// operPtr.p      Operation record pointer
// fragptr.p      Fragment record pointer
// tabptr.p       Table record pointer

// It requires the following fields in KeyReqStruct to be properly 
// filled in:
// tuple_header Reference to the tuple
// check_offset Record size
// attr_descr   Reference to the Table Descriptor for the table
//
// The read functions in addition expects that the following fields in
// KeyReqStruct is set up:
// out_buf_index Index for output buffer
// max_read      Size of output buffer
// attr_descriptor Attribute Descriptor from where attribute size
//                 can be read
/* ---------------------------------------------------------------- */
int Dbtup::readAttributes(KeyReqStruct *req_struct,
                          const Uint32* inBuffer,
                          Uint32  inBufLen,
                          Uint32* outBuf,
                          Uint32  maxRead,
                          bool    xfrm_flag)
{
  Uint32 attributeId, descr_index, tmpAttrBufIndex, tmpAttrBufBits, inBufIndex;
  Uint32 attributeOffset;
  TableDescriptor* attr_descr;
  AttributeHeader* ahOut;

  Tablerec* const regTabPtr=  tabptr.p;
  Uint32 numAttributes= regTabPtr->m_no_of_attributes;

  inBufIndex= 0;
  req_struct->out_buf_index= 0;
  req_struct->out_buf_bits = 0;
  req_struct->max_read= 4*maxRead;
  req_struct->xfrm_flag= xfrm_flag;
  Uint8*outBuffer = (Uint8*)outBuf;
  while (inBufIndex < inBufLen) {
    tmpAttrBufIndex= req_struct->out_buf_index;
    tmpAttrBufBits = req_struct->out_buf_bits;
    AttributeHeader ahIn(inBuffer[inBufIndex]);
    inBufIndex++;
    attributeId= ahIn.getAttributeId();
    descr_index= attributeId << ZAD_LOG_SIZE;
    jam();

    tmpAttrBufIndex = pad32(tmpAttrBufIndex, tmpAttrBufBits);
    AttributeHeader::init((Uint32 *)&outBuffer[tmpAttrBufIndex],
			  attributeId, 0);
    ahOut= (AttributeHeader*)&outBuffer[tmpAttrBufIndex];
    req_struct->out_buf_index= tmpAttrBufIndex + 4;
    req_struct->out_buf_bits = 0;
    attr_descr= req_struct->attr_descr;
    if (attributeId < numAttributes) {
      attributeOffset= attr_descr[descr_index + 1].tabDescr;
      ReadFunction f= regTabPtr->readFunctionArray[attributeId];
      req_struct->attr_descriptor= attr_descr[descr_index].tabDescr;
      if ((this->*f)(outBuffer,
                     req_struct,
                     ahOut,
                     attributeOffset)) {
        continue;
      } else {
        return -1;
      }
    } 
    else if(attributeId & AttributeHeader::PSEUDO) 
    {
      Uint32 sz = read_pseudo(inBuffer, inBufIndex,
                              req_struct,
                              (Uint32*)outBuffer);
      inBufIndex += sz;
    } 
    else 
    {
      terrorCode = ZATTRIBUTE_ID_ERROR;
      return -1;
    }//if
  }//while
  return pad32(req_struct->out_buf_index, req_struct->out_buf_bits) >> 2;
}

bool
Dbtup::readFixedSizeTHOneWordNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2)
{
  ndbassert((req_struct->out_buf_index & 3) == 0);
  ndbassert(req_struct->out_buf_bits == 0);

  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 const wordRead= tuple_header[readOffset];

  Uint32 newIndexBuf = indexBuf + 4;
  Uint32* dst = (Uint32*)(outBuffer + indexBuf);
  Uint32 maxRead= req_struct->max_read;

  ndbrequire(readOffset < req_struct->check_offset[MM]);
  if (newIndexBuf <= maxRead) {
    jam();
    dst[0] = wordRead;
    ahOut->setDataSize(1);
    req_struct->out_buf_index= newIndexBuf;
    return true;
  } else {
    jam();
    terrorCode= ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }
}

bool
Dbtup::readFixedSizeTHTwoWordNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2)
{
  ndbassert((req_struct->out_buf_index & 3) == 0);
  ndbassert(req_struct->out_buf_bits == 0);

  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 const wordReadFirst= tuple_header[readOffset];
  Uint32 const wordReadSecond= tuple_header[readOffset + 1];
  Uint32 maxRead= req_struct->max_read;

  Uint32 newIndexBuf = indexBuf + 8;
  Uint32* dst = (Uint32*)(outBuffer + indexBuf);

  ndbrequire(readOffset + 1 < req_struct->check_offset[MM]);
  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize(2);
    dst[0] = wordReadFirst;
    dst[1] = wordReadSecond;
    req_struct->out_buf_index= newIndexBuf;
    return true;
  } else {
    jam();
    terrorCode= ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }
}

static
inline
void
zero32(Uint8* dstPtr, Uint32 len)
{
  while ((len & 3) != 0) 
  {
    dstPtr[len++] = 0;
  }
}

bool
Dbtup::readFixedSizeTHManyWordNotNULL(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2)
{
  ndbassert(req_struct->out_buf_bits == 0);

  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 srcBytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
  Uint32 attrNoOfWords= (srcBytes + 3) >> 2;
  Uint32 maxRead= req_struct->max_read;
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);

  Uint32 newIndexBuf = indexBuf + srcBytes;
  Uint8* dst = (outBuffer + indexBuf);
  const Uint8* src = (Uint8*)(tuple_header + readOffset);

  ndbrequire((readOffset + attrNoOfWords - 1) < req_struct->check_offset[MM]);
  if (! charsetFlag || ! req_struct->xfrm_flag) {
    if (newIndexBuf <= maxRead) {
      jam();
      ahOut->setByteSize(srcBytes);
      memcpy(dst, src, srcBytes);
      zero32(dst, srcBytes);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    } else {
      jam();
      terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
      return false;
    }//if
  } 
  else 
  {
    return xfrm_reader(dst, req_struct, ahOut, attrDes2, src, srcBytes);
  }
}//Dbtup::readFixedSizeTHManyWordNotNULL()

bool
Dbtup::readFixedSizeTHOneWordNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2)
{
  if (!nullFlagCheck(req_struct, attrDes2)) {
    jam();
    return readFixedSizeTHOneWordNotNULL(outBuffer,
                                         req_struct,
                                         ahOut,
                                         attrDes2);
  } else {
    jam();
    ahOut->setNULL();
    return true;
  }
}

bool
Dbtup::readFixedSizeTHTwoWordNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2)
{
  if (!nullFlagCheck(req_struct, attrDes2)) {
    jam();
    return readFixedSizeTHTwoWordNotNULL(outBuffer,
                                         req_struct,
                                         ahOut,
                                         attrDes2);
  } else {
    jam();
    ahOut->setNULL();
    return true;
  }
}

bool
Dbtup::readFixedSizeTHManyWordNULLable(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2)
{
  if (!nullFlagCheck(req_struct, attrDes2)) {
    jam();
    return readFixedSizeTHManyWordNotNULL(outBuffer,
                                          req_struct,
                                          ahOut,
                                          attrDes2);
  } else {
    jam();
    ahOut->setNULL();
    return true;
  }
}

bool
Dbtup::readFixedSizeTHZeroWordNULLable(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2)
{
  jam();
  if (nullFlagCheck(req_struct, attrDes2)) {
    jam();
    ahOut->setNULL();
  }
  return true;
}

bool
Dbtup::nullFlagCheck(KeyReqStruct *req_struct, Uint32  attrDes2)
{
  Tablerec* const regTabPtr= tabptr.p;
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  
  return BitmaskImpl::get(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
}

bool
Dbtup::disk_nullFlagCheck(KeyReqStruct *req_struct, Uint32  attrDes2)
{
  Tablerec* const regTabPtr= tabptr.p;
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  
  return BitmaskImpl::get(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
}

/* Shared code for reading static varsize and expanded dynamic attributes. */
bool
Dbtup::varsize_reader(Uint8* outBuffer,
                      KeyReqStruct *req_struct,
                      AttributeHeader* ah_out,
                      Uint32  attr_des2,
                      const void * srcPtr,
                      Uint32 srcBytes)
{
  ndbassert(req_struct->out_buf_bits == 0);

  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attr_des2);
  Uint32 attr_descriptor= req_struct->attr_descriptor;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 max_var_size= AttributeDescriptor::getSizeInBytes(attr_descriptor);
  Uint32 max_read= req_struct->max_read;

  Uint32 newIndexBuf = indexBuf + srcBytes;
  Uint8* dst = (outBuffer + indexBuf);

  ndbrequire(srcBytes <= max_var_size);
  if (! charsetFlag || ! req_struct->xfrm_flag)
  {
    if (newIndexBuf <= max_read) {
      jam();
      ah_out->setByteSize(srcBytes);
      memcpy(dst, srcPtr, srcBytes);
      zero32(dst, srcBytes);
      req_struct->out_buf_index= newIndexBuf;
      return true;
    }
  }
  else
  {
    return xfrm_reader(dst, req_struct, ah_out, attr_des2, srcPtr, srcBytes);
  }
  
  jam();
  terrorCode= ZTRY_TO_READ_TOO_MUCH_ERROR;
  return false;
}

bool
Dbtup::xfrm_reader(Uint8* dstPtr,  
                   KeyReqStruct* req_struct, 
                   AttributeHeader* ahOut,
                   Uint32 attrDes2,
                   const void* srcPtr, Uint32 srcBytes)
{
  jam();
  ndbassert(req_struct->out_buf_bits == 0);

  Tablerec* regTabPtr = tabptr.p;
  Uint32 attrDes1 = req_struct->attr_descriptor;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 maxRead= req_struct->max_read;
  Uint32 i = AttributeOffset::getCharsetPos(attrDes2);  
  Uint32 typeId = AttributeDescriptor::getType(attrDes1);
  Uint32 maxBytes = AttributeDescriptor::getSizeInBytes(attrDes1);

  ndbrequire(i < regTabPtr->noOfCharsets);
  CHARSET_INFO* cs = regTabPtr->charsetArray[i];

  Uint32 lb, len;
  bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
  Uint32 xmul = cs->strxfrm_multiply;
  if (xmul == 0)
    xmul = 1;
  Uint32 dstLen = xmul * (maxBytes - lb);
  Uint32 maxIndexBuf = indexBuf + (dstLen >> 2);
  if (maxIndexBuf <= maxRead && ok) 
  {
    jam();
    int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, 
                                         (const Uint8*)srcPtr + lb, len);
    ndbrequire(n != -1);
    zero32(dstPtr, n);
    ahOut->setByteSize(n);
    Uint32 newIndexBuf = indexBuf + n;
    ndbrequire(newIndexBuf <= maxRead);
    req_struct->out_buf_index = newIndexBuf;
    return true;
  } else {
    jam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
  }
  return false;
}        

bool
Dbtup::bits_reader(Uint8* outBuffer,
                   KeyReqStruct *req_struct,
                   AttributeHeader* ahOut,
                   const Uint32* bmptr, Uint32 bmlen,
                   Uint32 bitPos, Uint32 bitCount)
{
  ndbassert((req_struct->out_buf_index & 3) == 0);

  Uint32 indexBuf = req_struct->out_buf_index;
  Uint32 indexBits = req_struct->out_buf_bits; 
  Uint32 maxRead = req_struct->max_read;

  Uint32 sz32 = (bitCount + 31) >> 5;
  Uint32 newIndexBuf = indexBuf + 4 * ((indexBits + bitCount) >> 5);
  Uint32 newIndexBits = (indexBits + bitCount) & 31;

  Uint32* dst = (Uint32*)(outBuffer + indexBuf);
  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize(sz32);
    req_struct->out_buf_index = newIndexBuf;
    req_struct->out_buf_bits = newIndexBits;

    if (bitCount == 1)
    {
      * dst &= (1 << indexBits) - 1;
      BitmaskImpl::set(1, dst, indexBits, 
                       BitmaskImpl::get(bmlen, bmptr, bitPos));
    }
    else if (indexBits == 0)
    {
      BitmaskImpl::getField(bmlen, bmptr, bitPos, bitCount, dst);
    }
    else
    {
      BitmaskImpl::getField(bmlen, bmptr, bitPos, bitCount, dst + 2);
      BitmaskImpl::setField(1+sz32, dst, indexBits, bitCount, dst + 2);
    }
    
    return true;
  } else {
    jam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}

bool
Dbtup::readVarSizeNotNULL(Uint8* out_buffer,
                          KeyReqStruct *req_struct,
                          AttributeHeader* ah_out,
                          Uint32  attr_des2)
{
  Uint32 var_idx= AttributeOffset::getOffset(attr_des2);
  Uint32 var_attr_pos= req_struct->m_var_data[MM].m_offset_array_ptr[var_idx];
  Uint32 idx= req_struct->m_var_data[MM].m_var_len_offset;
  Uint32 srcBytes =
    req_struct->m_var_data[MM].m_offset_array_ptr[var_idx+idx] - var_attr_pos;
  const char* src_ptr= req_struct->m_var_data[MM].m_data_ptr+var_attr_pos;

  return varsize_reader(out_buffer, req_struct, ah_out, attr_des2,
                        src_ptr, srcBytes);
}

bool
Dbtup::readVarSizeNULLable(Uint8* outBuffer,
                           KeyReqStruct *req_struct,
                           AttributeHeader* ahOut,
                           Uint32  attrDes2)
{
  if (!nullFlagCheck(req_struct, attrDes2)) {
    jam();
    return readVarSizeNotNULL(outBuffer,
                              req_struct,
                              ahOut,
                              attrDes2);
  } else {
    jam();
    ahOut->setNULL();
    return true;
  }
}

bool
Dbtup::readDynFixedSizeNotNULL(Uint8* outBuffer,
                               KeyReqStruct *req_struct,
                               AttributeHeader* ahOut,
                               Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynFixedSizeExpandedNotNULL(outBuffer, req_struct,
                                           ahOut, attrDes2);
  else
    return readDynFixedSizeShrunkenNotNULL(outBuffer, req_struct,
                                           ahOut, attrDes2);
}

bool
Dbtup::readDynFixedSizeNULLable(Uint8* outBuffer,
                                KeyReqStruct *req_struct,
                                AttributeHeader* ahOut,
                                Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynFixedSizeExpandedNULLable(outBuffer, req_struct,
                                            ahOut, attrDes2);
  else
    return readDynFixedSizeShrunkenNULLable(outBuffer, req_struct,
                                            ahOut, attrDes2);
}

bool
Dbtup::readDynFixedSizeExpandedNotNULL(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2)
{
  /*
    In the expanded format, we share the read code with static varsized, just
    using different data base pointer and offset/lenght arrays.
  */
  jam();
  char *src_ptr= req_struct->m_var_data[MM].m_dyn_data_ptr;
  Uint32 var_index= AttributeOffset::getOffset(attrDes2);
  Uint16* off_arr= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint32 var_attr_pos= off_arr[var_index];
  Uint32 vsize_in_bytes=
    AttributeDescriptor::getSizeInBytes(req_struct->attr_descriptor);
  return varsize_reader(outBuffer, req_struct, ahOut, attrDes2,
                        src_ptr + var_attr_pos, vsize_in_bytes);
}

bool
Dbtup::readDynFixedSizeExpandedNULLable(Uint8* outBuffer,
                                        KeyReqStruct *req_struct,
                                        AttributeHeader* ahOut,
                                        Uint32  attrDes2)
{
  /*
    Check for NULL. In the expanded format, the bitmap is guaranteed
    to be stored in full length.
  */
  Uint32 *src_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(!BitmaskImpl::get((* src_ptr) & DYN_BM_LEN_MASK, src_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynFixedSizeExpandedNotNULL(outBuffer, req_struct,
                                         ahOut, attrDes2);
}

bool
Dbtup::readDynFixedSizeShrunkenNotNULL(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  ndbrequire(dyn_len != 0);
  Uint32 bm_len= (* bm_ptr) & DYN_BM_LEN_MASK; // In 32-bit words
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  ndbrequire(BitmaskImpl::get(bm_len, bm_ptr, pos));

  /*
    The attribute is not NULL. Now to get the data offset, we count the number
    of bits set in the bitmap for fixed-size dynamic attributes prior to this
    attribute. Since there is one bit for each word of fixed-size attribute,
    and since fixed-size attributes are stored word-aligned backwards from the
    end of the row, this gives the distance in words from the row end to the
    end of the data for this attribute.

    We use a pre-computed bitmask to mask away all bits for fixed-sized
    dynamic attributes, and we also mask away the initial bitmap length byte and
    any trailing non-bitmap bytes to save a few conditionals.
  */
  jam();
  Tablerec* regTabPtr = tabptr.p;
  Uint32 *bm_mask_ptr= regTabPtr->dynFixSizeMask;
  Uint32 bm_pos= AttributeOffset::getNullFlagOffset(attrDes2);
  Uint32 prevMask= (1 << (pos & 31)) - 1;
  Uint32 bit_count= count_bits(prevMask & bm_mask_ptr[bm_pos] & bm_ptr[bm_pos]);
  for(Uint32 i=0; i<bm_pos; i++)
    bit_count+= count_bits(bm_mask_ptr[i] & bm_ptr[i]);

  /* Now compute the data pointer from the row length. */
  Uint32 attr_descriptor= req_struct->attr_descriptor;
  Uint32 vsize_in_bytes= AttributeDescriptor::getSizeInBytes(attr_descriptor);
  Uint32 vsize_in_words= (vsize_in_bytes+3)>>2;
  Uint32 *data_ptr= bm_ptr + dyn_len - bit_count - vsize_in_words;

  return varsize_reader(outBuffer, req_struct, ahOut, attrDes2,
                        (Uint8 *)data_ptr, vsize_in_bytes);
}

static
inline
bool
dynCheckNull(Uint32 totlen, Uint32 bm_len, const Uint32* bm_ptr, Uint32 pos)
{
  return  totlen == 0 || !(bm_len > (pos >> 5)) || 
    !BitmaskImpl::get(bm_len, bm_ptr, pos);
}

bool
Dbtup::readDynFixedSizeShrunkenNULLable(Uint8* outBuffer,
                                        KeyReqStruct *req_struct,
                                        AttributeHeader* ahOut,
                                        Uint32  attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  /* Check for NULL (including the case of an empty bitmap). */
  if(dyn_len == 0 || dynCheckNull(dyn_len, (* bm_ptr) & DYN_BM_LEN_MASK,
                                  bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynFixedSizeShrunkenNotNULL(outBuffer, req_struct,
                                         ahOut, attrDes2);
}

bool
Dbtup::readDynBigFixedSizeNotNULL(Uint8* outBuffer,
                                  KeyReqStruct *req_struct,
                                  AttributeHeader* ahOut,
                                  Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynBigFixedSizeExpandedNotNULL(outBuffer, req_struct,
                                         ahOut, attrDes2);
  else
    return readDynBigFixedSizeShrunkenNotNULL(outBuffer, req_struct,
                                         ahOut, attrDes2);
}//Dbtup::readDynBigVarSize()

bool
Dbtup::readDynBigFixedSizeNULLable(Uint8* outBuffer,
                                   KeyReqStruct *req_struct,
                                   AttributeHeader* ahOut,
                                   Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynBigFixedSizeExpandedNULLable(outBuffer, req_struct,
                                          ahOut, attrDes2);
  else
    return readDynBigFixedSizeShrunkenNULLable(outBuffer, req_struct,
                                          ahOut, attrDes2);
}//Dbtup::readDynBigVarSize()

bool
Dbtup::readDynBigFixedSizeExpandedNotNULL(Uint8* outBuffer,
                                          KeyReqStruct *req_struct,
                                          AttributeHeader* ahOut,
                                          Uint32  attrDes2)
{
  /*
    In the expanded format, we share the read code with static varsized, just
    using different data base pointer and offset/lenght arrays.
  */
  jam();
  char *src_ptr= req_struct->m_var_data[MM].m_dyn_data_ptr;
  Uint32 var_index= AttributeOffset::getOffset(attrDes2);
  Uint16* off_arr= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint32 var_attr_pos= off_arr[var_index];
  Uint32 vsize_in_bytes=
    AttributeDescriptor::getSizeInBytes(req_struct->attr_descriptor);
  Uint32 idx= req_struct->m_var_data[MM].m_dyn_len_offset;
  ndbrequire(vsize_in_bytes <= off_arr[var_index+idx] - var_attr_pos);
  return varsize_reader(outBuffer, req_struct, ahOut, attrDes2,
                        src_ptr + var_attr_pos, vsize_in_bytes);
}

bool
Dbtup::readDynBigFixedSizeExpandedNULLable(Uint8* outBuffer,
                                           KeyReqStruct *req_struct,
                                           AttributeHeader* ahOut,
                                           Uint32  attrDes2)
{
  /*
    Check for NULL. In the expanded format, the bitmap is guaranteed
    to be stored in full length.
  */
  Uint32 *src_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(!BitmaskImpl::get((* src_ptr) & DYN_BM_LEN_MASK, src_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynBigFixedSizeExpandedNotNULL(outBuffer, req_struct,
                                       ahOut, attrDes2);
}

bool
Dbtup::readDynBigFixedSizeShrunkenNotNULL(Uint8* outBuffer,
                                          KeyReqStruct *req_struct,
                                          AttributeHeader* ahOut,
                                          Uint32  attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  ndbrequire(dyn_len!=0);
  Uint32 bm_len = (* bm_ptr) & DYN_BM_LEN_MASK;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  ndbrequire(BitmaskImpl::get(bm_len, bm_ptr, pos));

  /*
    The attribute is not NULL. Now to get the data offset, we count the number
    of varsize dynamic attributes prior to this one that are not NULL.

    We use a pre-computed bitmask to mask away all bits for fixed-sized
    dynamic attributes, and we also mask away the initial bitmap length byte and
    any trailing non-bitmap bytes to save a few conditionals.
  */
  Tablerec* regTabPtr = tabptr.p;
  Uint32 *bm_mask_ptr= regTabPtr->dynVarSizeMask;
  Uint32 bm_pos= AttributeOffset::getNullFlagOffset(attrDes2);
  Uint32 prevMask= (1 << (pos & 31)) - 1;
  Uint32 bit_count= count_bits(prevMask & bm_mask_ptr[bm_pos] & bm_ptr[bm_pos]);
  for(Uint32 i=0; i<bm_pos; i++)
    bit_count+= count_bits(bm_mask_ptr[i] & bm_ptr[i]);

  /* Now find the data pointer and length from the offset array. */
  Uint32 attr_descriptor= req_struct->attr_descriptor;
  Uint32 vsize_in_bytes= AttributeDescriptor::getSizeInBytes(attr_descriptor);
  //Uint16 *offset_array= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint16* offset_array = (Uint16*)(bm_ptr + bm_len);
  Uint16 data_offset= offset_array[bit_count];
  ndbrequire(vsize_in_bytes <= Uint32(offset_array[bit_count+1]- data_offset));
  
  /*
    In the expanded format, we share the read code with static varsized, just
    using different data base pointer and offset/lenght arrays.
  */
  jam();
  return varsize_reader(outBuffer, req_struct, ahOut, attrDes2,
                        ((char *)offset_array) + data_offset, vsize_in_bytes);
}

bool
Dbtup::readDynBigFixedSizeShrunkenNULLable(Uint8* outBuffer,
                                           KeyReqStruct *req_struct,
                                           AttributeHeader* ahOut,
                                           Uint32  attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  /* Check for NULL (including the case of an empty bitmap). */
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(dyn_len == 0 || dynCheckNull(dyn_len, (* bm_ptr) & DYN_BM_LEN_MASK,
                                  bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynBigFixedSizeShrunkenNotNULL(outBuffer, req_struct,
                                       ahOut, attrDes2);
}

bool
Dbtup::readDynBitsNotNULL(Uint8* outBuffer,
                          KeyReqStruct *req_struct,
                          AttributeHeader* ahOut,
                          Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynBitsExpandedNotNULL(outBuffer, req_struct, ahOut, attrDes2);
  else
    return readDynBitsShrunkenNotNULL(outBuffer, req_struct, ahOut, attrDes2);
}

bool
Dbtup::readDynBitsNULLable(Uint8* outBuffer,
                           KeyReqStruct *req_struct,
                           AttributeHeader* ahOut,
                           Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynBitsExpandedNULLable(outBuffer, req_struct, ahOut, attrDes2);
  else
    return readDynBitsShrunkenNULLable(outBuffer, req_struct, ahOut, attrDes2);
}

bool
Dbtup::readDynBitsShrunkenNotNULL(Uint8* outBuffer,
                                  KeyReqStruct* req_struct,
                                  AttributeHeader* ahOut,
                                  Uint32 attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  ndbrequire(dyn_len != 0);
  Uint32 bm_len = (* bm_ptr) & DYN_BM_LEN_MASK;
  Uint32 bitCount =
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  /* Make sure we have sufficient data in the row. */
  ndbrequire((pos>>5)<bm_len);
  /* The bit data is stored just before the NULL bit. */
  ndbassert(pos>bitCount);
  pos-= bitCount;

  return bits_reader(outBuffer, req_struct, ahOut,
                     bm_ptr, bm_len,
                     pos, bitCount);
}

bool
Dbtup::readDynBitsShrunkenNULLable(Uint8* outBuffer,
                                   KeyReqStruct* req_struct,
                                   AttributeHeader* ahOut,
                                   Uint32 attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  /* Check for NULL (including the case of an empty bitmap). */
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(dyn_len == 0 || dynCheckNull(dyn_len, (* bm_ptr) & DYN_BM_LEN_MASK,
                                  bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynBitsShrunkenNotNULL(outBuffer, req_struct, ahOut, attrDes2);
}

bool
Dbtup::readDynBitsExpandedNotNULL(Uint8* outBuffer,
                                  KeyReqStruct* req_struct,
                                  AttributeHeader* ahOut,
                                  Uint32 attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 bm_len = (* bm_ptr) & DYN_BM_LEN_MASK;
  Uint32 bitCount =
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  /* The bit data is stored just before the NULL bit. */
  ndbassert(pos>bitCount);
  pos-= bitCount;

  return bits_reader(outBuffer, req_struct, ahOut,
                     bm_ptr, bm_len,
                     pos, bitCount);
}

bool
Dbtup::readDynBitsExpandedNULLable(Uint8* outBuffer,
                                   KeyReqStruct* req_struct,
                                   AttributeHeader* ahOut,
                                   Uint32 attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(!BitmaskImpl::get((* bm_ptr) & DYN_BM_LEN_MASK, bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynBitsExpandedNotNULL(outBuffer, req_struct, ahOut, attrDes2);
}

bool
Dbtup::readDynVarSizeNotNULL(Uint8* outBuffer,
                             KeyReqStruct *req_struct,
                             AttributeHeader* ahOut,
                             Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynVarSizeExpandedNotNULL(outBuffer, req_struct,
                                         ahOut, attrDes2);
  else
    return readDynVarSizeShrunkenNotNULL(outBuffer, req_struct,
                                         ahOut, attrDes2);
}//Dbtup::readDynBigVarSize()

bool
Dbtup::readDynVarSizeNULLable(Uint8* outBuffer,
                              KeyReqStruct *req_struct,
                              AttributeHeader* ahOut,
                              Uint32  attrDes2)
{
  jam();
  if(req_struct->is_expanded)
    return readDynVarSizeExpandedNULLable(outBuffer, req_struct,
                                          ahOut, attrDes2);
  else
    return readDynVarSizeShrunkenNULLable(outBuffer, req_struct,
                                          ahOut, attrDes2);
}//Dbtup::readDynBigVarSize()

bool
Dbtup::readDynVarSizeExpandedNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2)
{
  /*
    In the expanded format, we share the read code with static varsized, just
    using different data base pointer and offset/lenght arrays.
  */
  jam();
  char *src_ptr= req_struct->m_var_data[MM].m_dyn_data_ptr;
  Uint32 var_index= AttributeOffset::getOffset(attrDes2);
  Uint16* off_arr= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint32 var_attr_pos= off_arr[var_index];
  Uint32 idx= req_struct->m_var_data[MM].m_dyn_len_offset;
  Uint32 vsize_in_bytes= off_arr[var_index+idx] - var_attr_pos;
  return varsize_reader(outBuffer, req_struct, ahOut, attrDes2,
                        src_ptr + var_attr_pos, vsize_in_bytes);
}

bool
Dbtup::readDynVarSizeExpandedNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2)
{
  /*
    Check for NULL. In the expanded format, the bitmap is guaranteed
    to be stored in full length.
  */
  Uint32 *src_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(!BitmaskImpl::get((* src_ptr) & DYN_BM_LEN_MASK, src_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynVarSizeExpandedNotNULL(outBuffer, req_struct,
                                       ahOut, attrDes2);
}

bool
Dbtup::readDynVarSizeShrunkenNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  ndbrequire(dyn_len!=0);
  Uint32 bm_len = (* bm_ptr) & DYN_BM_LEN_MASK;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  ndbrequire(BitmaskImpl::get(bm_len, bm_ptr, pos));

  /*
    The attribute is not NULL. Now to get the data offset, we count the number
    of varsize dynamic attributes prior to this one that are not NULL.

    We use a pre-computed bitmask to mask away all bits for fixed-sized
    dynamic attributes, and we also mask away the initial bitmap length byte and
    any trailing non-bitmap bytes to save a few conditionals.
  */
  Tablerec* regTabPtr = tabptr.p;
  Uint32 *bm_mask_ptr= regTabPtr->dynVarSizeMask;
  Uint32 bm_pos= AttributeOffset::getNullFlagOffset(attrDes2);
  Uint32 prevMask= (1 << (pos & 31)) - 1;
  Uint32 bit_count= count_bits(prevMask & bm_mask_ptr[bm_pos] & bm_ptr[bm_pos]);
  for(Uint32 i=0; i<bm_pos; i++)
    bit_count+= count_bits(bm_mask_ptr[i] & bm_ptr[i]);

  /* Now find the data pointer and length from the offset array. */
  //Uint16* offset_array = req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint16* offset_array = (Uint16*)(bm_ptr + bm_len);
  Uint16 data_offset= offset_array[bit_count];
  Uint32 vsize_in_bytes= offset_array[bit_count+1] - data_offset;

  /*
    In the expanded format, we share the read code with static varsized, just
    using different data base pointer and offset/lenght arrays.
  */
  jam();
  return varsize_reader(outBuffer, req_struct, ahOut, attrDes2,
                        ((char *)offset_array) + data_offset, vsize_in_bytes);
}

bool
Dbtup::readDynVarSizeShrunkenNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2)
{
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 dyn_len= req_struct->m_var_data[MM].m_dyn_part_len;
  /* Check for NULL (including the case of an empty bitmap). */
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  if(dyn_len == 0 || dynCheckNull(dyn_len, (* bm_ptr) & DYN_BM_LEN_MASK,
                                  bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return readDynVarSizeShrunkenNotNULL(outBuffer, req_struct,
                                       ahOut, attrDes2);
}

bool
Dbtup::readDiskFixedSizeNotNULL(Uint8* outBuffer,
				KeyReqStruct *req_struct,
				AttributeHeader* ahOut,
				Uint32  attrDes2)
{
  ndbassert(req_struct->out_buf_bits == 0);

  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 *tuple_header= req_struct->m_disk_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 srcBytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
  Uint32 attrNoOfWords= (srcBytes + 3) >> 2;
  Uint32 maxRead= req_struct->max_read;
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);
  Uint32 newIndexBuf = indexBuf + srcBytes;
  Uint8* dst = (outBuffer + indexBuf);
  const Uint8* src = (Uint8*)(tuple_header+readOffset);

  ndbrequire((readOffset + attrNoOfWords - 1) < req_struct->check_offset[DD]);
  if (! charsetFlag || ! req_struct->xfrm_flag) 
  {
    if (newIndexBuf <= maxRead) 
    {
      jam();
      ahOut->setByteSize(srcBytes);
      memcpy(dst, src, srcBytes);
      zero32(dst, srcBytes);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    }
  } 
  else 
  {
    return xfrm_reader(dst, req_struct, ahOut, attrDes2, src, srcBytes);
  } 
  
  jam();
  terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
  return false;
}

bool
Dbtup::readDiskFixedSizeNULLable(Uint8* outBuffer,
				 KeyReqStruct *req_struct,
				 AttributeHeader* ahOut,
				 Uint32  attrDes2)
{
  if (!disk_nullFlagCheck(req_struct, attrDes2)) {
    jam();
    return readDiskFixedSizeNotNULL(outBuffer,
				    req_struct,
				    ahOut,
				    attrDes2);
  } else {
    jam();
    ahOut->setNULL();
    return true;
  }
}

bool
Dbtup::readDiskVarSizeNotNULL(Uint8* out_buffer,
			      KeyReqStruct *req_struct,
			      AttributeHeader* ah_out,
			      Uint32  attr_des2)
{
  ndbrequire(false);
  return 0;
}

bool
Dbtup::readDiskVarSizeNULLable(Uint8* outBuffer,
			       KeyReqStruct *req_struct,
			       AttributeHeader* ahOut,
			       Uint32  attrDes2)
{
  if (!disk_nullFlagCheck(req_struct, attrDes2)) {
    jam();
    return readDiskVarSizeNotNULL(outBuffer,
				  req_struct,
				  ahOut,
				  attrDes2);
  } else {
    jam();
    ahOut->setNULL();
    return true;
  }
}

bool
Dbtup::readDiskBitsNotNULL(Uint8* outBuffer,
			   KeyReqStruct* req_struct,
			   AttributeHeader* ahOut,
			   Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 bm_len = regTabPtr->m_offsets[DD].m_null_words;
  Uint32* bm_ptr = req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  
  return bits_reader(outBuffer, req_struct, ahOut,
                     bm_ptr, bm_len,
                     pos, bitCount);

}

bool
Dbtup::readDiskBitsNULLable(Uint8* outBuffer,
			    KeyReqStruct* req_struct,
			    AttributeHeader* ahOut,
			    Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  
  Uint32 bm_len = regTabPtr->m_offsets[DD].m_null_words;
  Uint32 *bm_ptr= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  
  if(BitmaskImpl::get(bm_len, bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }
  
  return bits_reader(outBuffer, req_struct, ahOut,
                     bm_ptr, bm_len,
                     pos+1, bitCount);
}



/* ---------------------------------------------------------------------- */
/*       THIS ROUTINE IS USED TO UPDATE A NUMBER OF ATTRIBUTES. IT IS     */
/*       USED BY THE INSERT ROUTINE, THE UPDATE ROUTINE AND IT CAN BE     */
/*       CALLED SEVERAL TIMES FROM THE INTERPRETER.                       */
// In addition to the parameters used in the call it also relies on the
// following variables set-up properly.
//
// operPtr.p      Operation record pointer
// tabptr.p       Table record pointer
/* ---------------------------------------------------------------------- */
int Dbtup::updateAttributes(KeyReqStruct *req_struct,
                            Uint32* inBuffer,
                            Uint32 inBufLen)
{
  Tablerec* const regTabPtr=  tabptr.p;
  Operationrec* const regOperPtr= operPtr.p;
  Uint32 numAttributes= regTabPtr->m_no_of_attributes;
  TableDescriptor *attr_descr= req_struct->attr_descr;

  Uint32 inBufIndex= 0;
  req_struct->in_buf_index= 0;
  req_struct->in_buf_len= inBufLen;

  while (inBufIndex < inBufLen) {
    AttributeHeader ahIn(inBuffer[inBufIndex]);
    Uint32 attributeId= ahIn.getAttributeId();
    Uint32 attrDescriptorIndex= attributeId << ZAD_LOG_SIZE;
    if (likely(attributeId < numAttributes)) {
      Uint32 attrDescriptor= attr_descr[attrDescriptorIndex].tabDescr;
      Uint32 attributeOffset= attr_descr[attrDescriptorIndex + 1].tabDescr;
      if ((AttributeDescriptor::getPrimaryKey(attrDescriptor)) &&
          (regOperPtr->op_struct.op_type != ZINSERT)) {
        if (checkUpdateOfPrimaryKey(req_struct,
                                    &inBuffer[inBufIndex],
                                    regTabPtr)) {
          jam();
          terrorCode= ZTRY_UPDATE_PRIMARY_KEY;
          return -1;
        }
      }
      UpdateFunction f= regTabPtr->updateFunctionArray[attributeId];
      jam();
      req_struct->attr_descriptor= attrDescriptor;
      req_struct->changeMask.set(attributeId);
      if (attributeId >= 64) {
        if (req_struct->max_attr_id_updated < attributeId) {
          Uint32 no_changed_attrs= req_struct->no_changed_attrs;
          req_struct->max_attr_id_updated= attributeId;
          req_struct->no_changed_attrs= no_changed_attrs + 1;
        }
      }
      if ((this->*f)(inBuffer,
                     req_struct,
                     attributeOffset)) {
        inBufIndex= req_struct->in_buf_index;
        continue;
      } else {
        jam();
        return -1;
      }
    } 
    else if(attributeId == AttributeHeader::DISK_REF)
    {
      jam();
      Uint32 sz= ahIn.getDataSize();
      ndbrequire(sz == 2);
      req_struct->m_tuple_ptr->m_header_bits |= Tuple_header::DISK_PART;
      memcpy(req_struct->m_tuple_ptr->get_disk_ref_ptr(regTabPtr),
	     inBuffer+inBufIndex+1, sz << 2);
      inBufIndex += 1 + sz;
      req_struct->in_buf_index = inBufIndex;
    }
    else if(attributeId == AttributeHeader::ANY_VALUE)
    {
      jam();
      Uint32 sz= ahIn.getDataSize();
      ndbrequire(sz == 1);
      regOperPtr->m_any_value = * (inBuffer + inBufIndex + 1);
      inBufIndex += 1 + sz;
      req_struct->in_buf_index = inBufIndex;
    }
    else
    {
      jam();
      terrorCode= ZATTRIBUTE_ID_ERROR;
      return -1;
    }
  }
  return 0;
}

bool
Dbtup::checkUpdateOfPrimaryKey(KeyReqStruct* req_struct,
                               Uint32* updateBuffer,
                               Tablerec* const regTabPtr)
{
  Uint32 keyReadBuffer[MAX_KEY_SIZE_IN_WORDS];
  TableDescriptor* attr_descr = req_struct->attr_descr;
  AttributeHeader ahIn(*updateBuffer);
  Uint32 attributeId = ahIn.getAttributeId();
  Uint32 attrDescriptorIndex = attributeId << ZAD_LOG_SIZE;
  Uint32 attrDescriptor = attr_descr[attrDescriptorIndex].tabDescr;
  Uint32 attributeOffset = attr_descr[attrDescriptorIndex + 1].tabDescr;

  Uint32 xfrmBuffer[1 + MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attributeOffset);
  if (charsetFlag) {
    Uint32 csIndex = AttributeOffset::getCharsetPos(attributeOffset);
    CHARSET_INFO* cs = regTabPtr->charsetArray[csIndex];
    Uint32 srcPos = 0;
    Uint32 dstPos = 0;
    xfrm_attr(attrDescriptor, cs, &updateBuffer[1], srcPos,
              &xfrmBuffer[1], dstPos, MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY);
    ahIn.setDataSize(dstPos);
    xfrmBuffer[0] = ahIn.m_value;
    updateBuffer = xfrmBuffer;
  }

  ReadFunction f = regTabPtr->readFunctionArray[attributeId];

  AttributeHeader attributeHeader(attributeId, 0);
  req_struct->out_buf_index = 0;
  req_struct->out_buf_bits = 0;
  req_struct->max_read = sizeof(keyReadBuffer);
  req_struct->attr_descriptor = attrDescriptor;
  
  bool tmp = req_struct->xfrm_flag;
  req_struct->xfrm_flag = true;
  ndbrequire((this->*f)((Uint8*)keyReadBuffer,
                        req_struct,
                        &attributeHeader,
                        attributeOffset));
  req_struct->xfrm_flag = tmp;
  
  ndbrequire(req_struct->out_buf_index == attributeHeader.getByteSize());
  if (ahIn.getDataSize() != attributeHeader.getDataSize()) {
    jam();
    return true;
  }
  if (memcmp(&keyReadBuffer[0], 
             &updateBuffer[1],
             req_struct->out_buf_index) != 0) {
    jam();
    return true;
  }
  return false;
}

bool
Dbtup::updateFixedSizeTHOneWordNotNULL(Uint32* inBuffer,
                                       KeyReqStruct *req_struct,
                                       Uint32  attrDes2)
{
  Uint32 indexBuf= req_struct->in_buf_index;
  Uint32 inBufLen= req_struct->in_buf_len;
  Uint32 updateOffset= AttributeOffset::getOffset(attrDes2);
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 newIndex= indexBuf + 2;
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  ndbrequire(updateOffset < req_struct->check_offset[MM]);

  if (newIndex <= inBufLen) {
    Uint32 updateWord= inBuffer[indexBuf + 1];
    if (!nullIndicator) {
      jam();
      req_struct->in_buf_index= newIndex;
      tuple_header[updateOffset]= updateWord;
      return true;
    } else {
      jam();
      terrorCode= ZNOT_NULL_ATTR;
      return false;
    }
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
  return true;
}

bool
Dbtup::updateFixedSizeTHTwoWordNotNULL(Uint32* inBuffer,
                                       KeyReqStruct *req_struct,
                                       Uint32  attrDes2)
{
  Uint32 indexBuf= req_struct->in_buf_index;
  Uint32 inBufLen= req_struct->in_buf_len;
  Uint32 updateOffset= AttributeOffset::getOffset(attrDes2);
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 newIndex= indexBuf + 3;
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  ndbrequire((updateOffset + 1) < req_struct->check_offset[MM]);

  if (newIndex <= inBufLen) {
    Uint32 updateWord1= inBuffer[indexBuf + 1];
    Uint32 updateWord2= inBuffer[indexBuf + 2];
    if (!nullIndicator) {
      jam();
      req_struct->in_buf_index= newIndex;
      tuple_header[updateOffset]= updateWord1;
      tuple_header[updateOffset + 1]= updateWord2;
      return true;
    } else {
      jam();
      terrorCode= ZNOT_NULL_ATTR;
      return false;
    }
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

bool
Dbtup::fixsize_updater(Uint32* inBuffer,
                       KeyReqStruct *req_struct,
                       Uint32  attrDes2,
                       Uint32 *dst_ptr,
                       Uint32 updateOffset,
                       Uint32 checkOffset)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 indexBuf= req_struct->in_buf_index;
  Uint32 inBufLen= req_struct->in_buf_len;
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);
  
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 noOfWords= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 newIndex= indexBuf + noOfWords + 1;
  ndbrequire((updateOffset + noOfWords - 1) < checkOffset);

  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      jam();
      if (charsetFlag) {
        jam();
        Tablerec* regTabPtr = tabptr.p;
	Uint32 typeId = AttributeDescriptor::getType(attrDescriptor);
        Uint32 bytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
        Uint32 i = AttributeOffset::getCharsetPos(attrDes2);
        ndbrequire(i < regTabPtr->noOfCharsets);
        // not const in MySQL
        CHARSET_INFO* cs = regTabPtr->charsetArray[i];
        int not_used;
        const char* ssrc = (const char*)&inBuffer[indexBuf + 1];
        Uint32 lb, len;
        if (! NdbSqlUtil::get_var_length(typeId, ssrc, bytes, lb, len)) {
          jam();
          terrorCode = ZINVALID_CHAR_FORMAT;
          return false;
        }
	// fast fix bug#7340
        if (typeId != NDB_TYPE_TEXT &&
	    (*cs->cset->well_formed_len)(cs, ssrc + lb, ssrc + lb + len, ZNIL, &not_used) != len) {
          jam();
          terrorCode = ZINVALID_CHAR_FORMAT;
          return false;
        }
      }
      req_struct->in_buf_index= newIndex;
      MEMCOPY_NO_WORDS(&(dst_ptr[updateOffset]),
                       &inBuffer[indexBuf + 1],
                       noOfWords);
      
      return true;
    } else {
      jam();
      terrorCode= ZNOT_NULL_ATTR;
      return false;
    }
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

bool
Dbtup::updateFixedSizeTHManyWordNotNULL(Uint32* inBuffer,
                                        KeyReqStruct *req_struct,
                                        Uint32  attrDes2)
{
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 updateOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 checkOffset= req_struct->check_offset[MM];
  return fixsize_updater(inBuffer, req_struct, attrDes2, tuple_header,
                         updateOffset, checkOffset);
}

bool
Dbtup::updateFixedSizeTHManyWordNULLable(Uint32* inBuffer,
                                         KeyReqStruct *req_struct,
                                         Uint32  attrDes2)
{
  Tablerec* const regTabPtr=  tabptr.p;
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  
  if (!nullIndicator) {
    jam();
    BitmaskImpl::clear(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
    return updateFixedSizeTHManyWordNotNULL(inBuffer,
                                            req_struct,
                                            attrDes2);
  } else {
    Uint32 newIndex= req_struct->in_buf_index + 1;
    if (newIndex <= req_struct->in_buf_len) {
      BitmaskImpl::set(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
      jam();
      req_struct->in_buf_index= newIndex;
      return true;
    } else {
      jam();
      terrorCode= ZAI_INCONSISTENCY_ERROR;
      return false;
    }
  }
}

bool
Dbtup::updateVarSizeNotNULL(Uint32* in_buffer,
                            KeyReqStruct *req_struct,
                            Uint32 attr_des2)
{
  Uint32 var_index;
  char *var_data_start= req_struct->m_var_data[MM].m_data_ptr;
  var_index= AttributeOffset::getOffset(attr_des2);
  Uint32 idx= req_struct->m_var_data[MM].m_var_len_offset;
  Uint16 *vpos_array= req_struct->m_var_data[MM].m_offset_array_ptr;
  Uint16 offset= vpos_array[var_index];
  Uint16 *len_offset_ptr= &(vpos_array[var_index+idx]);
  return varsize_updater(in_buffer, req_struct, var_data_start,
                         offset, len_offset_ptr,
                         req_struct->m_var_data[MM].m_max_var_offset);
}
bool
Dbtup::varsize_updater(Uint32* in_buffer,
                       KeyReqStruct *req_struct,
                       char *var_data_start,
                       Uint32 var_attr_pos,
                       Uint16 *len_offset_ptr,
                       Uint32 check_offset)
{
  Uint32 attr_descriptor, index_buf, in_buf_len, null_ind;
  Uint32 vsize_in_words, new_index, max_var_size;

  attr_descriptor= req_struct->attr_descriptor;
  index_buf= req_struct->in_buf_index;
  in_buf_len= req_struct->in_buf_len;
  AttributeHeader ahIn(in_buffer[index_buf]);
  null_ind= ahIn.isNULL();
  Uint32 size_in_bytes = ahIn.getByteSize();
  vsize_in_words= (size_in_bytes + 3) >> 2;
  max_var_size= AttributeDescriptor::getSizeInBytes(attr_descriptor);
  new_index= index_buf + vsize_in_words + 1;
  
  if (new_index <= in_buf_len && vsize_in_words <= max_var_size) {
    if (!null_ind) {
      jam();
      *len_offset_ptr= var_attr_pos+size_in_bytes;
      req_struct->in_buf_index= new_index;
      
      ndbrequire(var_attr_pos+size_in_bytes <= check_offset);
      memcpy(var_data_start+var_attr_pos, &in_buffer[index_buf + 1],
	     size_in_bytes);
      return true;
    }

    jam();
    terrorCode= ZNOT_NULL_ATTR;
    return false;
  }

  jam();
  terrorCode= ZAI_INCONSISTENCY_ERROR;
  return false;
}

bool
Dbtup::updateVarSizeNULLable(Uint32* inBuffer,
                             KeyReqStruct *req_struct,
                             Uint32  attrDes2)
{
  Tablerec* const regTabPtr=  tabptr.p;
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  Uint32 idx= req_struct->m_var_data[MM].m_var_len_offset;
  
  if (!nullIndicator) {
    jam();
    BitmaskImpl::clear(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
    return updateVarSizeNotNULL(inBuffer,
                                req_struct,
                                attrDes2);
  } else {
    Uint32 newIndex= req_struct->in_buf_index + 1;
    Uint32 var_index= AttributeOffset::getOffset(attrDes2);
    Uint32 var_pos= req_struct->var_pos_array[var_index];
    if (newIndex <= req_struct->in_buf_len) {
      jam();
      BitmaskImpl::set(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
      req_struct->var_pos_array[var_index+idx]= var_pos;
      req_struct->in_buf_index= newIndex;
      return true;
    } else {
      jam();
      terrorCode= ZAI_INCONSISTENCY_ERROR;
      return false;
    }
  }
}

bool
Dbtup::updateDynFixedSizeNotNULL(Uint32* inBuffer,
                                 KeyReqStruct *req_struct,
                                 Uint32  attrDes2)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 nullbits= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);

  ndbassert(nullbits && nullbits <= 16);

  /*
    Compute two 16-bit bitmasks and a 16-bit aligned bitmap offset for setting
    all the null bits for the fixed-size dynamic attribute.
    There are at most 16 bits (corresponding to 64 bytes fixsize; longer
    attributes are stored more efficiently as varsize internally anyway).
  */

  Uint32 bm_idx= (pos >> 5);
  /* Store bits in little-endian so fit with length byte and trailing padding*/
  Uint64 bm_mask = ((Uint64(1) << nullbits) - 1) << (pos & 31);
  Uint32 bm_mask1 = bm_mask & 0xFFFFFFFF;
  Uint32 bm_mask2 = bm_mask >> 32;

  jam();
  /* Set all the bits in the NULL bitmap. */
  bm_ptr[bm_idx]|= bm_mask1;
  /*
    It is possible that bm_ptr[bm_idx+1] points off the end of the
    bitmap. But in that case, we are merely ANDing all ones into the offset
    array (no-op), cheaper than a conditional.
  */
  bm_ptr[bm_idx+1]|= bm_mask2;

  /* Compute the data and offset location and write the actual data. */
  Uint32 off_index= AttributeOffset::getOffset(attrDes2);
  Uint16* off_arr= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint32 offset= off_arr[off_index];
  Uint32 *dst_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 check_offset= req_struct->m_var_data[MM].m_max_dyn_offset;

  ndbassert((offset&3)==0);
  ndbassert((check_offset&3)==0);
  bool result= fixsize_updater(inBuffer, req_struct, attrDes2, dst_ptr,
                               (offset>>2), (check_offset>>2));
  return result; 
}

bool
Dbtup::updateDynFixedSizeNULLable(Uint32* inBuffer,
                                  KeyReqStruct *req_struct,
                                  Uint32  attrDes2)
{
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();

  if(!nullIndicator)
    return updateDynFixedSizeNotNULL(inBuffer, req_struct, attrDes2);

  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 nullbits= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);

  ndbassert(nullbits && nullbits <= 16);

  /*
    Compute two 16-bit bitmasks and a 16-bit aligned bitmap offset for
    clearing all the null bits for the fixed-size dynamic attribute.
    There are at most 16 bits (corresponding to 64 bytes fixsize; longer
    attributes are stored more efficiently as varsize internally anyway).
  */

  Uint32 bm_idx= (pos >> 5);
  /* Store bits in little-endian so fit with length byte and trailing padding*/
  Uint64 bm_mask = ~(((Uint64(1) << nullbits) - 1) << (pos & 31));
  Uint32 bm_mask1 = bm_mask & 0xFFFFFFFF;
  Uint32 bm_mask2 = bm_mask >> 32;
  
  Uint32 newIndex= req_struct->in_buf_index + 1;
  if (newIndex <= req_struct->in_buf_len) {
    jam();
    /* Clear the bits in the NULL bitmap. */
    bm_ptr[bm_idx] &= bm_mask1;
    bm_ptr[bm_idx+1] &= bm_mask2;
    req_struct->in_buf_index= newIndex;
    return true;
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

/* Update a big dynamic fixed-size column, stored internally as varsize. */
bool
Dbtup::updateDynBigFixedSizeNotNULL(Uint32* inBuffer,
                                  KeyReqStruct *req_struct,
                                  Uint32  attrDes2)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  
  jam();
  BitmaskImpl::set((* bm_ptr) & DYN_BM_LEN_MASK, bm_ptr, pos);
  /* Compute the data and offset location and write the actual data. */
  Uint32 off_index= AttributeOffset::getOffset(attrDes2);
  Uint32 noOfWords= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint16* off_arr= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint32 offset= off_arr[off_index];
  Uint32 idx= req_struct->m_var_data[MM].m_dyn_len_offset;

  ndbassert((offset&3)==0);
  bool res= fixsize_updater(inBuffer,
                            req_struct,
                            attrDes2,
                            bm_ptr,
                            offset>>2,
                            req_struct->m_var_data[MM].m_max_dyn_offset);
  /* Set the correct size for fixsize data. */
  off_arr[off_index+idx]= offset+(noOfWords<<2);
  return res;
}

bool
Dbtup::updateDynBigFixedSizeNULLable(Uint32* inBuffer,
                                   KeyReqStruct *req_struct,
                                   Uint32  attrDes2)
{
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bm_ptr= (Uint32*)req_struct->m_var_data[MM].m_dyn_data_ptr;
  
  if (!nullIndicator)
    return updateDynBigFixedSizeNotNULL(inBuffer, req_struct, attrDes2);

  Uint32 newIndex= req_struct->in_buf_index + 1;
  if (newIndex <= req_struct->in_buf_len) {
    jam();
    BitmaskImpl::clear((* bm_ptr) & DYN_BM_LEN_MASK, bm_ptr, pos);
    req_struct->in_buf_index= newIndex;
    return true;
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

bool
Dbtup::updateDynBitsNotNULL(Uint32* inBuffer,
                            KeyReqStruct *req_struct,
                            Uint32  attrDes2)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
  Uint32 *bm_ptr= (Uint32 *)(req_struct->m_var_data[MM].m_dyn_data_ptr);
  Uint32 bm_len = (* bm_ptr) & DYN_BM_LEN_MASK;
  jam();
  BitmaskImpl::set(bm_len, bm_ptr, pos);

  Uint32 indexBuf= req_struct->in_buf_index;
  Uint32 inBufLen= req_struct->in_buf_len;
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);

  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      ndbassert(pos>=bitCount);
      BitmaskImpl::setField(bm_len, bm_ptr, pos-bitCount, bitCount, 
			    inBuffer+indexBuf+1);
      req_struct->in_buf_index= newIndex;
      return true;
    } else {
      jam();
      terrorCode= ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
  return true;
}

bool
Dbtup::updateDynBitsNULLable(Uint32* inBuffer,
                             KeyReqStruct *req_struct,
                             Uint32  attrDes2)
{
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();

  if(!nullIndicator)
    return updateDynBitsNotNULL(inBuffer, req_struct, attrDes2);

  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bm_ptr= (Uint32*)req_struct->m_var_data[MM].m_dyn_data_ptr;

  Uint32 newIndex= req_struct->in_buf_index + 1;
  if (newIndex <= req_struct->in_buf_len) {
    jam();
    BitmaskImpl::clear((* bm_ptr) & DYN_BM_LEN_MASK, bm_ptr, pos);
    req_struct->in_buf_index= newIndex;
    return true;
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

bool
Dbtup::updateDynVarSizeNotNULL(Uint32* inBuffer,
                               KeyReqStruct *req_struct,
                               Uint32  attrDes2)
{
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bm_ptr= (Uint32*)req_struct->m_var_data[MM].m_dyn_data_ptr;
  
  jam();
  BitmaskImpl::set((* bm_ptr) & DYN_BM_LEN_MASK, bm_ptr, pos);
  /* Compute the data and offset location and write the actual data. */
  Uint32 off_index= AttributeOffset::getOffset(attrDes2);
  Uint16* off_arr= req_struct->m_var_data[MM].m_dyn_offset_arr_ptr;
  Uint32 offset= off_arr[off_index];
  Uint32 idx= req_struct->m_var_data[MM].m_dyn_len_offset;

  bool res= varsize_updater(inBuffer,
                            req_struct,
                            (char*)bm_ptr,
                            offset,
                            &(off_arr[off_index+idx]),
                            req_struct->m_var_data[MM].m_max_dyn_offset);
  return res;
}

bool
Dbtup::updateDynVarSizeNULLable(Uint32* inBuffer,
                                KeyReqStruct *req_struct,
                                Uint32  attrDes2)
{
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bm_ptr= (Uint32*)req_struct->m_var_data[MM].m_dyn_data_ptr;
  
  if (!nullIndicator)
    return updateDynVarSizeNotNULL(inBuffer, req_struct, attrDes2);

  Uint32 newIndex= req_struct->in_buf_index + 1;
  if (newIndex <= req_struct->in_buf_len) {
    jam();
    BitmaskImpl::clear((* bm_ptr) & DYN_BM_LEN_MASK, bm_ptr, pos);
    req_struct->in_buf_index= newIndex;
    return true;
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

Uint32 
Dbtup::read_pseudo(const Uint32 * inBuffer, Uint32 inPos,
                   KeyReqStruct *req_struct,
                   Uint32* outBuf)
{
  ndbassert(inPos);
  ndbassert(req_struct->out_buf_index);
  ndbassert(req_struct->out_buf_bits == 0);
  ndbassert((req_struct->out_buf_index & 3) == 0);
  
  Uint32 attrId = (* (inBuffer + inPos - 1)) >> 16;
  Uint32 outPos = req_struct->out_buf_index;
  Uint32* outBuffer = outBuf + ((outPos - 1) >> 2);
  
  Uint32 sz;
  Uint32 tmp[sizeof(SignalHeader)+25];
  Signal * signal = (Signal*)&tmp;
  switch(attrId){
  case AttributeHeader::READ_PACKED:
  case AttributeHeader::READ_ALL:
    return read_packed(inBuffer, inPos, req_struct, outBuf);
  case AttributeHeader::FRAGMENT:
    outBuffer[1] = fragptr.p->fragmentId;
    sz = 1;
    break;
  case AttributeHeader::FRAGMENT_FIXED_MEMORY:
  {
    Uint64 tmp= fragptr.p->noOfPages;
    tmp*= 32768;
    memcpy(outBuffer + 1, &tmp, 8);
    sz = 2;
    break;
  }
  case AttributeHeader::FRAGMENT_VARSIZED_MEMORY:
  {
    Uint64 tmp= fragptr.p->noOfVarPages;
    tmp*= 32768;
    memcpy(outBuffer + 1, &tmp, 8);
    sz = 2;
    break;
  }
  case AttributeHeader::ROW_SIZE:
    outBuffer[1] = tabptr.p->m_offsets[MM].m_fix_header_size << 2;
    sz = 1;
    break;
  case AttributeHeader::ROW_COUNT:
  case AttributeHeader::COMMIT_COUNT:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSEUDO_REQ, signal, 2);
    outBuffer[1] = signal->theData[0];
    outBuffer[2] = signal->theData[1];
    sz = 2;
    break;
  case AttributeHeader::RANGE_NO:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSEUDO_REQ, signal, 2);
    outBuffer[1] = signal->theData[0];
    sz = 1;
    break;
  case AttributeHeader::DISK_REF:
  {
    Uint32 *ref= req_struct->m_tuple_ptr->get_disk_ref_ptr(tabptr.p);
    outBuffer[1] = ref[0];
    outBuffer[2] = ref[1];
    sz = 2;
    break;
  }
  case AttributeHeader::RECORDS_IN_RANGE:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSEUDO_REQ, signal, 2);
    outBuffer[1] = signal->theData[0];
    outBuffer[2] = signal->theData[1];
    outBuffer[3] = signal->theData[2];
    outBuffer[4] = signal->theData[3];
    sz = 4;
    break;
  case AttributeHeader::ROWID:
    outBuffer[1] = req_struct->frag_page_id;
    outBuffer[2] = operPtr.p->m_tuple_location.m_page_idx;
    sz = 2;
    break;
  case AttributeHeader::ROW_GCI:
    sz = 0;
    if (tabptr.p->m_bits & Tablerec::TR_RowGCI)
    {
      Uint64 tmp = * req_struct->m_tuple_ptr->get_mm_gci(tabptr.p);
      memcpy(outBuffer + 1, &tmp, sizeof(tmp));
      sz = 2;
    }
    break;
  case AttributeHeader::ANY_VALUE:
  {
    /**
     * Read ANY_VALUE does not actually read anything
     *   but...sets operPtr.p->m_any_value and
     *   and puts it into clogMemBuffer so that it's also sent
     *   to backup replica(s)
     *
     *   This nifty features is used for delete+read with circ. replication
     */
    jam();
    Uint32 RlogSize = req_struct->log_size;
    operPtr.p->m_any_value = inBuffer[inPos];
    * (clogMemBuffer + RlogSize) = inBuffer[inPos - 1];
    * (clogMemBuffer + RlogSize + 1) = inBuffer[inPos];
    req_struct->out_buf_index = outPos - 4;
    req_struct->log_size = RlogSize + 2;
    return 1;
  }
  case AttributeHeader::COPY_ROWID:
    sz = 2;
    outBuffer[1] = operPtr.p->m_copy_tuple_location.m_page_no;
    outBuffer[2] = operPtr.p->m_copy_tuple_location.m_page_idx;
    break;
  default:
    return 0;
  }
  
  AttributeHeader::init(outBuffer, attrId, sz << 2);
  req_struct->out_buf_index = outPos + 4*sz;
  return 0;
}

Uint32
Dbtup::read_packed(const Uint32* inBuf, Uint32 inPos,
                   KeyReqStruct *req_struct,
                   Uint32* outBuffer)
{
  ndbassert(req_struct->out_buf_index >= 4);
  ndbassert((req_struct->out_buf_index & 3) == 0);
  ndbassert(req_struct->out_buf_bits == 0);
  
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 outPos = req_struct->out_buf_index;
  Uint32 outBits = req_struct->out_buf_bits;
  Uint32 maxRead = req_struct->max_read;

  Uint32 cnt;
  Uint32 numAttributes = regTabPtr->m_no_of_attributes;
  Uint32 attrDescriptorStart = regTabPtr->tabDescriptor;
  Uint32 attrId =  (* (inBuf + inPos - 1)) >> 16;
  Uint32 bmlen32 = ((* (inBuf + inPos - 1)) & 0xFFFF);

  Bitmask<MAXNROFATTRIBUTESINWORDS> mask;
  if (attrId == AttributeHeader::READ_ALL)
  {
    cnt = bmlen32;
    for (Uint32 i = 0; i<cnt; i++)
      mask.set(i);
    bmlen32 = 0;
  }
  else
  {
    bmlen32 /= 4;
    cnt = 32*bmlen32 <= numAttributes ? 32*bmlen32 : numAttributes;
    mask.assign(bmlen32, inBuf + inPos);
  }
  
  // Compute result bitmap len
  Bitmask<MAXNROFATTRIBUTESINWORDS> nullable = mask;
  nullable.bitANDC(regTabPtr->notNullAttributeMask);
  Uint32 nullcnt = nullable.count();
  Uint32 masksz = (cnt + nullcnt + 31) >> 5;
    
  Uint32* dst = (Uint32*)(outBuffer + ((outPos - 4) >> 2));
  Uint32* dstmask = dst + 1;
  AttributeHeader::init(dst, AttributeHeader::READ_PACKED, 4*masksz);
  bzero(dstmask, 4*masksz);
    
  AttributeHeader ahOut;
  Uint8* outBuf = (Uint8*)outBuffer;
  outPos += 4*masksz;
  if (likely(outPos <= maxRead))
  {
    jam();
    for (Uint32 attrId = 0, maskpos = 0; attrId<cnt; attrId++, maskpos++)
    {
      jam();
      if (mask.get(attrId))
      {
        jam();
        Uint32 attrDescrIdx = attrDescriptorStart + (attrId << ZAD_LOG_SIZE);
        Uint32 attrDesc1 = tableDescriptor[attrDescrIdx].tabDescr;
        Uint32 attrDesc2 = tableDescriptor[attrDescrIdx + 1].tabDescr;
        ReadFunction f = regTabPtr->readFunctionArray[attrId];

        if (outBits)
        {
          ndbassert((outPos & 3) == 0);
        }
        
        Uint32 save[2] = { outPos, outBits };
        switch(AttributeDescriptor::getSize(attrDesc1)){
        case DictTabInfo::aBit: // bit
          outPos = (outPos + 3) & ~(Uint32)3;
          break;
        case DictTabInfo::an8Bit: // char
        case DictTabInfo::a16Bit: // uint16
          outPos = outPos + 4 * ((outBits + 31) >> 5);
          outBits = 0;
          break;
        case DictTabInfo::a32Bit: // uint32
        case DictTabInfo::a64Bit: // uint64
        case DictTabInfo::a128Bit:
          outPos = ((outPos + 3) & ~(Uint32)3) + 4 * ((outBits + 31) >> 5);
          outBits = 0;
          break;
#ifdef VM_TRACE
        default:
          ndbrequire(false);
#endif
        }
        
        req_struct->out_buf_index = outPos;
        req_struct->out_buf_bits = outBits;
        req_struct->attr_descriptor = attrDesc1;
        if ((this->*f)(outBuf, req_struct, &ahOut, attrDesc2))
        {
          jam();
          BitmaskImpl::set(masksz, dstmask, maskpos);

          outPos = req_struct->out_buf_index;
          outBits = req_struct->out_buf_bits;

          if (nullable.get(attrId))
          {
            jam();
            maskpos++;
            if (ahOut.isNULL())
            {
              jam();
              BitmaskImpl::set(masksz, dstmask, maskpos);
              outPos = save[0];
              outBits = save[1];
            }
          }
          continue;
        } else {
          goto error;
        }//if
      }
    }
    
    req_struct->out_buf_index = pad32(outPos, outBits);
    req_struct->out_buf_bits = 0;
    return bmlen32;
  }
  
error:  
  ndbrequire(false);
  return 0;
}

bool
Dbtup::readBitsNotNULL(Uint8* outBuffer,
		       KeyReqStruct* req_struct,
		       AttributeHeader* ahOut,
		       Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 *bmptr= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  Uint32 bmlen = regTabPtr->m_offsets[MM].m_null_words;

  return bits_reader(outBuffer,
                     req_struct,
                     ahOut,
                     bmptr, bmlen,
                     pos, bitCount);
}

bool
Dbtup::readBitsNULLable(Uint8* outBuffer,
			KeyReqStruct* req_struct,
			AttributeHeader* ahOut,
			Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  
  Uint32 *bm_ptr= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  Uint32 bm_len = regTabPtr->m_offsets[MM].m_null_words;
  
  if(BitmaskImpl::get(bm_len, bm_ptr, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  return bits_reader(outBuffer, req_struct, ahOut,
                     bm_ptr, bm_len,
                     pos+1, bitCount);
}

bool
Dbtup::updateBitsNotNULL(Uint32* inBuffer,
			 KeyReqStruct* req_struct,
			 Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 indexBuf = req_struct->in_buf_index;
  Uint32 inBufLen = req_struct->in_buf_len;
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  
  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      BitmaskImpl::setField(regTabPtr->m_offsets[MM].m_null_words, bits, pos, 
			    bitCount, inBuffer+indexBuf+1);
      req_struct->in_buf_index = newIndex;
      return true;
    } else {
      jam();
      terrorCode = ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    jam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
  return true;
}

bool
Dbtup::updateBitsNULLable(Uint32* inBuffer,
			  KeyReqStruct* req_struct,
			  Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 indexBuf = req_struct->in_buf_index;
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  
  if (!nullIndicator) {
    BitmaskImpl::clear(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
    BitmaskImpl::setField(regTabPtr->m_offsets[MM].m_null_words, bits, pos+1, 
			  bitCount, inBuffer+indexBuf+1);
    
    Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);
    req_struct->in_buf_index = newIndex;
    return true;
  } else {
    Uint32 newIndex = indexBuf + 1;
    if (newIndex <= req_struct->in_buf_len)
    {
      jam();
      BitmaskImpl::set(regTabPtr->m_offsets[MM].m_null_words, bits, pos);
      
      req_struct->in_buf_index = newIndex;
      return true;
    } else {
      jam();
      terrorCode = ZAI_INCONSISTENCY_ERROR;
      return false;
    }//if
  }//if
}

bool
Dbtup::updateDiskFixedSizeNotNULL(Uint32* inBuffer,
				  KeyReqStruct *req_struct,
				  Uint32  attrDes2)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 indexBuf= req_struct->in_buf_index;
  Uint32 inBufLen= req_struct->in_buf_len;
  Uint32 updateOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);
  
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 noOfWords= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 newIndex= indexBuf + noOfWords + 1;
  Uint32 *tuple_header= req_struct->m_disk_ptr->m_data;
  ndbrequire((updateOffset + noOfWords - 1) < req_struct->check_offset[DD]);

  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      jam();
      if (charsetFlag) {
        jam();
        Tablerec* regTabPtr = tabptr.p;
	Uint32 typeId = AttributeDescriptor::getType(attrDescriptor);
        Uint32 bytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
        Uint32 i = AttributeOffset::getCharsetPos(attrDes2);
        ndbrequire(i < regTabPtr->noOfCharsets);
        // not const in MySQL
        CHARSET_INFO* cs = regTabPtr->charsetArray[i];
	int not_used;
        const char* ssrc = (const char*)&inBuffer[indexBuf + 1];
        Uint32 lb, len;
        if (! NdbSqlUtil::get_var_length(typeId, ssrc, bytes, lb, len)) {
          jam();
          terrorCode = ZINVALID_CHAR_FORMAT;
          return false;
        }
	// fast fix bug#7340
        if (typeId != NDB_TYPE_TEXT &&
	    (*cs->cset->well_formed_len)(cs, ssrc + lb, ssrc + lb + len, ZNIL, &not_used) != len) {
          jam();
          terrorCode = ZINVALID_CHAR_FORMAT;
          return false;
        }
      }
      req_struct->in_buf_index= newIndex;
      MEMCOPY_NO_WORDS(&tuple_header[updateOffset],
                       &inBuffer[indexBuf + 1],
                       noOfWords);
      return true;
    } else {
      jam();
      terrorCode= ZNOT_NULL_ATTR;
      return false;
    }
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
}

bool
Dbtup::updateDiskFixedSizeNULLable(Uint32* inBuffer,
				   KeyReqStruct *req_struct,
				   Uint32  attrDes2)
{
  Tablerec* const regTabPtr=  tabptr.p;
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  
  if (!nullIndicator) {
    jam();
    BitmaskImpl::clear(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
    return updateDiskFixedSizeNotNULL(inBuffer,
				      req_struct,
				      attrDes2);
  } else {
    Uint32 newIndex= req_struct->in_buf_index + 1;
    if (newIndex <= req_struct->in_buf_len) {
      BitmaskImpl::set(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
      jam();
      req_struct->in_buf_index= newIndex;
      return true;
    } else {
      jam();
      terrorCode= ZAI_INCONSISTENCY_ERROR;
      return false;
    }
  }
}

bool
Dbtup::updateDiskVarSizeNotNULL(Uint32* in_buffer,
                            KeyReqStruct *req_struct,
                            Uint32 attr_des2)
{
  Uint32 attr_descriptor, index_buf, in_buf_len, var_index, null_ind;
  Uint32 vsize_in_words, new_index, max_var_size;
  Uint32 var_attr_pos;
  char *var_data_start;
  Uint16 *vpos_array;

  attr_descriptor= req_struct->attr_descriptor;
  index_buf= req_struct->in_buf_index;
  in_buf_len= req_struct->in_buf_len;
  var_index= AttributeOffset::getOffset(attr_des2);
  AttributeHeader ahIn(in_buffer[index_buf]);
  null_ind= ahIn.isNULL();
  Uint32 size_in_bytes = ahIn.getByteSize();
  vsize_in_words= (size_in_bytes + 3) >> 2;
  max_var_size= AttributeDescriptor::getSizeInBytes(attr_descriptor);
  new_index= index_buf + vsize_in_words + 1;
  vpos_array= req_struct->m_var_data[DD].m_offset_array_ptr;
  Uint32 idx= req_struct->m_var_data[DD].m_var_len_offset;
  Uint32 check_offset= req_struct->m_var_data[DD].m_max_var_offset;
  
  if (new_index <= in_buf_len && vsize_in_words <= max_var_size) {
    if (!null_ind) {
      jam();
      var_attr_pos= vpos_array[var_index];
      var_data_start= req_struct->m_var_data[DD].m_data_ptr;
      vpos_array[var_index+idx]= var_attr_pos+size_in_bytes;
      req_struct->in_buf_index= new_index;
      
      ndbrequire(var_attr_pos+size_in_bytes <= check_offset);
      memcpy(var_data_start+var_attr_pos, &in_buffer[index_buf + 1],
	     size_in_bytes);
      return true;
    } else {
      jam();
      terrorCode= ZNOT_NULL_ATTR;
      return false;
    }
  } else {
    jam();
    terrorCode= ZAI_INCONSISTENCY_ERROR;
    return false;
  }
  return false;
}

bool
Dbtup::updateDiskVarSizeNULLable(Uint32* inBuffer,
				 KeyReqStruct *req_struct,
				 Uint32  attrDes2)
{
  Tablerec* const regTabPtr=  tabptr.p;
  AttributeHeader ahIn(inBuffer[req_struct->in_buf_index]);
  Uint32 nullIndicator= ahIn.isNULL();
  Uint32 pos= AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  Uint32 idx= req_struct->m_var_data[DD].m_var_len_offset;
  
  if (!nullIndicator) {
    jam();
    BitmaskImpl::clear(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
    return updateDiskVarSizeNotNULL(inBuffer,
				    req_struct,
				    attrDes2);
  } else {
    Uint32 newIndex= req_struct->in_buf_index + 1;
    Uint32 var_index= AttributeOffset::getOffset(attrDes2);
    Uint32 var_pos= req_struct->var_pos_array[var_index];
    if (newIndex <= req_struct->in_buf_len) {
      jam();
      BitmaskImpl::set(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
      req_struct->var_pos_array[var_index+idx]= var_pos;
      req_struct->in_buf_index= newIndex;
      return true;
    } else {
      jam();
      terrorCode= ZAI_INCONSISTENCY_ERROR;
      return false;
    }
  }
}

bool
Dbtup::updateDiskBitsNotNULL(Uint32* inBuffer,
			     KeyReqStruct* req_struct,
			     Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 indexBuf = req_struct->in_buf_index;
  Uint32 inBufLen = req_struct->in_buf_len;
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  
  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      BitmaskImpl::setField(regTabPtr->m_offsets[DD].m_null_words, bits, pos, 
			    bitCount, inBuffer+indexBuf+1);
      req_struct->in_buf_index = newIndex;
      return true;
    } else {
      jam();
      terrorCode = ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    jam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
  return true;
}

bool
Dbtup::updateDiskBitsNULLable(Uint32* inBuffer,
			      KeyReqStruct* req_struct,
			      Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 indexBuf = req_struct->in_buf_index;
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  
  if (!nullIndicator) {
    BitmaskImpl::clear(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
    BitmaskImpl::setField(regTabPtr->m_offsets[DD].m_null_words, bits, pos+1, 
			  bitCount, inBuffer+indexBuf+1);
    
    Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);
    req_struct->in_buf_index = newIndex;
    return true;
  } else {
    Uint32 newIndex = indexBuf + 1;
    if (newIndex <= req_struct->in_buf_len)
    {
      jam();
      BitmaskImpl::set(regTabPtr->m_offsets[DD].m_null_words, bits, pos);
      
      req_struct->in_buf_index = newIndex;
      return true;
    } else {
      jam();
      terrorCode = ZAI_INCONSISTENCY_ERROR;
      return false;
    }//if
  }//if
}

