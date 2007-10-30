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
    if (!AttributeDescriptor::getDynamic(attrDescr)) {
      if (AttributeDescriptor::getArrayType(attrDescr) == NDB_ARRAYTYPE_FIXED){
        if (!AttributeDescriptor::getNullable(attrDescr)) {
          if (AttributeDescriptor::getSize(attrDescr) == 0){
            jam(); 
            regTabPtr->readFunctionArray[i] = &Dbtup::readBitsNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBitsNotNULL;
          } else if (AttributeDescriptor::getSizeInBytes(attrDescr) == 4) {
            jam();
            regTabPtr->readFunctionArray[i]=
	      &Dbtup::readFixedSizeTHOneWordNotNULL;
            regTabPtr->updateFunctionArray[i]=
	      &Dbtup::updateFixedSizeTHOneWordNotNULL;
          } else if (AttributeDescriptor::getSizeInBytes(attrDescr) == 8) {
            jam();
            regTabPtr->readFunctionArray[i]=
                    &Dbtup::readFixedSizeTHTwoWordNotNULL;
            regTabPtr->updateFunctionArray[i]=
                    &Dbtup::updateFixedSizeTHTwoWordNotNULL;
          } else {
            jam();
            regTabPtr->readFunctionArray[i]=
	      &Dbtup::readFixedSizeTHManyWordNotNULL;
            regTabPtr->updateFunctionArray[i]=
	      &Dbtup::updateFixedSizeTHManyWordNotNULL;
          }
          // replace functions for char attribute
          if (AttributeOffset::getCharsetFlag(attrOffset)) {
            jam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHManyWordNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNotNULL;
          }
        } else {
	  if (AttributeDescriptor::getSize(attrDescr) == 0){
            jam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBitsNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBitsNULLable;
	  } else if (AttributeDescriptor::getSizeInBytes(attrDescr) == 4){
            jam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHOneWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          } else if (AttributeDescriptor::getSizeInBytes(attrDescr) == 8) {
            jam();
            regTabPtr->readFunctionArray[i]=
                    &Dbtup::readFixedSizeTHTwoWordNULLable;
            regTabPtr->updateFunctionArray[i]=
                    &Dbtup::updateFixedSizeTHManyWordNULLable;
          } else {
            jam();
            regTabPtr->readFunctionArray[i]=
	      &Dbtup::readFixedSizeTHManyWordNULLable;
            regTabPtr->updateFunctionArray[i]=
	      &Dbtup::updateFixedSizeTHManyWordNULLable;
          }
          // replace functions for char attribute
          if (AttributeOffset::getCharsetFlag(attrOffset)) {
            jam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHManyWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          }
        }
      } else {
        if (!AttributeDescriptor::getNullable(attrDescr)) {
          regTabPtr->readFunctionArray[i]=
	    &Dbtup::readVarSizeNotNULL;
          regTabPtr->updateFunctionArray[i]=
	    &Dbtup::updateVarSizeNotNULL;
        } else {
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
	  AttributeDescriptor::getArrayType(attrDescr) == NDB_ARRAYTYPE_FIXED ? 2 : 4;
	
	if(AttributeDescriptor::getSize(attrDescr) == 0)
	  a= 0;
	
	Uint32 b= 
	  AttributeDescriptor::getNullable(attrDescr)? 1 : 0;
	regTabPtr->readFunctionArray[i]= r[a+b];
	regTabPtr->updateFunctionArray[i]= u[a+b];
      }
      } else {
      if (AttributeDescriptor::getArrayType(attrDescr) == NDB_ARRAYTYPE_FIXED){
        jam();
        regTabPtr->readFunctionArray[i]= &Dbtup::readDynFixedSize;
        regTabPtr->updateFunctionArray[i]= &Dbtup::updateDynFixedSize;
      } else {
        regTabPtr->readFunctionArray[i]= &Dbtup::readDynVarSize;
        regTabPtr->updateFunctionArray[i]= &Dbtup::updateDynVarSize;
      }
    }
  }
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
                          Uint32* outBuffer,
                          Uint32  maxRead,
                          bool    xfrm_flag)
{
  Uint32 attributeId, descr_index, tmpAttrBufIndex, inBufIndex;
  Uint32 attributeOffset;
  TableDescriptor* attr_descr;
  AttributeHeader* ahOut;

  Tablerec* const regTabPtr=  tabptr.p;
  Uint32 numAttributes= regTabPtr->m_no_of_attributes;

  inBufIndex= 0;
  req_struct->out_buf_index= 0;
  req_struct->max_read= maxRead;
  req_struct->xfrm_flag= xfrm_flag;
  while (inBufIndex < inBufLen) {
    tmpAttrBufIndex= req_struct->out_buf_index;
    AttributeHeader ahIn(inBuffer[inBufIndex]);
    inBufIndex++;
    attributeId= ahIn.getAttributeId();
    descr_index= attributeId << ZAD_LOG_SIZE;
    jam();

    AttributeHeader::init(&outBuffer[tmpAttrBufIndex], attributeId, 0);
    ahOut= (AttributeHeader*)&outBuffer[tmpAttrBufIndex];
    req_struct->out_buf_index= tmpAttrBufIndex + 1;
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
    } else if(attributeId & AttributeHeader::PSEUDO) {
      if (attributeId == AttributeHeader::ANY_VALUE)
      {
        jam();
        Uint32 RlogSize = req_struct->log_size;
        operPtr.p->m_any_value = inBuffer[inBufIndex];
        * (clogMemBuffer + RlogSize) = inBuffer[inBufIndex - 1];
        * (clogMemBuffer + RlogSize + 1) = inBuffer[inBufIndex];
        inBufIndex++;
        req_struct->out_buf_index = tmpAttrBufIndex;
        req_struct->log_size = RlogSize + 2;
        continue;
      }
      jam();
      Uint32 sz= read_pseudo(attributeId,
                             req_struct,
			     outBuffer+tmpAttrBufIndex+1);
      AttributeHeader::init(&outBuffer[tmpAttrBufIndex], attributeId, sz << 2);
      req_struct->out_buf_index= tmpAttrBufIndex + 1 + sz;
    } else {
      terrorCode = ZATTRIBUTE_ID_ERROR;
      return -1;
    }//if
  }//while
  return req_struct->out_buf_index;
}

bool
Dbtup::readFixedSizeTHOneWordNotNULL(Uint32* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2)
{
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 const wordRead= tuple_header[readOffset];
  Uint32 newIndexBuf= indexBuf + 1;
  Uint32 maxRead= req_struct->max_read;

  ndbrequire(readOffset < req_struct->check_offset[MM]);
  if (newIndexBuf <= maxRead) {
    jam();
    outBuffer[indexBuf]= wordRead;
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
Dbtup::readFixedSizeTHTwoWordNotNULL(Uint32* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2)
{
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 const wordReadFirst= tuple_header[readOffset];
  Uint32 const wordReadSecond= tuple_header[readOffset + 1];
  Uint32 newIndexBuf= indexBuf + 2;
  Uint32 maxRead= req_struct->max_read;

  ndbrequire(readOffset + 1 < req_struct->check_offset[MM]);
  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize(2);
    outBuffer[indexBuf]= wordReadFirst;
    outBuffer[indexBuf + 1]= wordReadSecond;
    req_struct->out_buf_index= newIndexBuf;
    return true;
  } else {
    jam();
    terrorCode= ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }
}

bool
Dbtup::readFixedSizeTHManyWordNotNULL(Uint32* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 attrNoOfWords= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 maxRead= req_struct->max_read;
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);

  ndbrequire((readOffset + attrNoOfWords - 1) < req_struct->check_offset[MM]);
  if (! charsetFlag || ! req_struct->xfrm_flag) {
    Uint32 newIndexBuf = indexBuf + attrNoOfWords;
    if (newIndexBuf <= maxRead) {
      jam();
      ahOut->setByteSize(AttributeDescriptor::getSizeInBytes(attrDescriptor));
      MEMCOPY_NO_WORDS(&outBuffer[indexBuf],
                       &tuple_header[readOffset],
                       attrNoOfWords);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    } else {
      jam();
      terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    }//if
  } else {
    jam();
    Tablerec* regTabPtr = tabptr.p;
    Uint32 srcBytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
    uchar* dstPtr = (uchar*)&outBuffer[indexBuf];
    const uchar* srcPtr = (uchar*)&tuple_header[readOffset];
    Uint32 i = AttributeOffset::getCharsetPos(attrDes2);
    ndbrequire(i < regTabPtr->noOfCharsets);
    CHARSET_INFO* cs = regTabPtr->charsetArray[i];
    Uint32 typeId = AttributeDescriptor::getType(attrDescriptor);
    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
    Uint32 xmul = cs->strxfrm_multiply;
    if (xmul == 0)
      xmul = 1;
    Uint32 dstLen = xmul * (srcBytes - lb);
    Uint32 maxIndexBuf = indexBuf + (dstLen >> 2);
    if (maxIndexBuf <= maxRead && ok) {
      jam();
      int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, srcPtr + lb, len);
      ndbrequire(n != -1);
      int m = n;
      while ((m & 3) != 0) {
        dstPtr[m++] = 0;
      }
      ahOut->setByteSize(n);
      Uint32 newIndexBuf = indexBuf + (m >> 2);
      ndbrequire(newIndexBuf <= maxRead);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    } else {
      jam();
      terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    }
  } 
  return false;
}//Dbtup::readFixedSizeTHManyWordNotNULL()

bool
Dbtup::readFixedSizeTHOneWordNULLable(Uint32* outBuffer,
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
Dbtup::readFixedSizeTHTwoWordNULLable(Uint32* outBuffer,
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
Dbtup::readFixedSizeTHManyWordNULLable(Uint32* outBuffer,
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
Dbtup::readFixedSizeTHZeroWordNULLable(Uint32* outBuffer,
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

bool
Dbtup::readVarSizeNotNULL(Uint32* out_buffer,
                          KeyReqStruct *req_struct,
                          AttributeHeader* ah_out,
                          Uint32  attr_des2)
{
  Uint32 attr_descriptor, index_buf, var_index;
  Uint32 vsize_in_bytes, vsize_in_words, new_index, max_var_size;
  Uint32 var_attr_pos, max_read;

  Uint32 idx= req_struct->m_var_data[MM].m_var_len_offset;
  var_index= AttributeOffset::getOffset(attr_des2);
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attr_des2);
  var_attr_pos= req_struct->m_var_data[MM].m_offset_array_ptr[var_index];
  vsize_in_bytes= req_struct->m_var_data[MM].m_offset_array_ptr[var_index+idx] - var_attr_pos;
  attr_descriptor= req_struct->attr_descriptor;
  index_buf= req_struct->out_buf_index;
  max_var_size= AttributeDescriptor::getSizeInWords(attr_descriptor);
  max_read= req_struct->max_read;
  vsize_in_words= convert_byte_to_word_size(vsize_in_bytes);
  new_index= index_buf + vsize_in_words;

  ndbrequire(vsize_in_words <= max_var_size);
  if (! charsetFlag || ! req_struct->xfrm_flag)
  {
    if (new_index <= max_read) {
      jam();
      ah_out->setByteSize(vsize_in_bytes);
      out_buffer[index_buf + (vsize_in_bytes >> 2)] = 0;
      memcpy(out_buffer+index_buf,
	     req_struct->m_var_data[MM].m_data_ptr+var_attr_pos,
	     vsize_in_bytes);
      req_struct->out_buf_index= new_index;
      return true;
    }
  }
  else
  {
    jam();
    Tablerec* regTabPtr = tabptr.p;
    Uint32 maxBytes = AttributeDescriptor::getSizeInBytes(attr_descriptor);
    Uint32 srcBytes = vsize_in_bytes;
    uchar* dstPtr = (uchar*)(out_buffer+index_buf);
    const uchar* srcPtr = (uchar*)(req_struct->m_var_data[MM].m_data_ptr+var_attr_pos);
    Uint32 i = AttributeOffset::getCharsetPos(attr_des2);
    ndbrequire(i < regTabPtr->noOfCharsets);
    CHARSET_INFO* cs = regTabPtr->charsetArray[i];
    Uint32 typeId = AttributeDescriptor::getType(attr_descriptor);
    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
    Uint32 xmul = cs->strxfrm_multiply;
    if (xmul == 0)
      xmul = 1;
    // see comment in DbtcMain.cpp
    Uint32 dstLen = xmul * (maxBytes - lb);
    Uint32 maxIndexBuf = index_buf + (dstLen >> 2);
    if (maxIndexBuf <= max_read && ok) {
      jam();
      int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, srcPtr + lb, len);
      ndbrequire(n != -1);
      int m = n;
      while ((m & 3) != 0) {
	dstPtr[m++] = 0;
      }
      ah_out->setByteSize(n);
      Uint32 newIndexBuf = index_buf + (m >> 2);
      ndbrequire(newIndexBuf <= max_read);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    }
  }
  jam();
  terrorCode= ZTRY_TO_READ_TOO_MUCH_ERROR;
  return false;
}

bool
Dbtup::readVarSizeNULLable(Uint32* outBuffer,
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
Dbtup::readDynFixedSize(Uint32* outBuffer,
                        KeyReqStruct *req_struct,
                        AttributeHeader* ahOut,
                        Uint32  attrDes2)
{
  jam();
  terrorCode= ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}

bool
Dbtup::readDynVarSize(Uint32* outBuffer,
                      KeyReqStruct *req_struct,
                      AttributeHeader* ahOut,
                      Uint32  attrDes2)
{
  jam();
  terrorCode= ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readDynBigVarSize()

bool
Dbtup::readDiskFixedSizeNotNULL(Uint32* outBuffer,
				KeyReqStruct *req_struct,
				AttributeHeader* ahOut,
				Uint32  attrDes2)
{
  Uint32 attrDescriptor= req_struct->attr_descriptor;
  Uint32 *tuple_header= req_struct->m_disk_ptr->m_data;
  Uint32 indexBuf= req_struct->out_buf_index;
  Uint32 readOffset= AttributeOffset::getOffset(attrDes2);
  Uint32 attrNoOfWords= AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 maxRead= req_struct->max_read;
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);

  ndbrequire((readOffset + attrNoOfWords - 1) < req_struct->check_offset[DD]);
  if (! charsetFlag || ! req_struct->xfrm_flag) {
    Uint32 newIndexBuf = indexBuf + attrNoOfWords;
    if (newIndexBuf <= maxRead) {
      jam();
      ahOut->setByteSize(AttributeDescriptor::getSizeInBytes(attrDescriptor));
      MEMCOPY_NO_WORDS(&outBuffer[indexBuf],
                       &tuple_header[readOffset],
                       attrNoOfWords);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    } else {
      jam();
      terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    }//if
  } else {
    jam();
    Tablerec* regTabPtr = tabptr.p;
    Uint32 srcBytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
    uchar* dstPtr = (uchar*)&outBuffer[indexBuf];
    const uchar* srcPtr = (uchar*)&tuple_header[readOffset];
    Uint32 i = AttributeOffset::getCharsetPos(attrDes2);
    ndbrequire(i < regTabPtr->noOfCharsets);
    CHARSET_INFO* cs = regTabPtr->charsetArray[i];
    Uint32 typeId = AttributeDescriptor::getType(attrDescriptor);
    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
    Uint32 xmul = cs->strxfrm_multiply;
    if (xmul == 0)
      xmul = 1;
    Uint32 dstLen = xmul * (srcBytes - lb);
    Uint32 maxIndexBuf = indexBuf + (dstLen >> 2);
    if (maxIndexBuf <= maxRead && ok) {
      jam();
      int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, srcPtr + lb, len);
      ndbrequire(n != -1);
      int m = n;
      while ((m & 3) != 0) {
        dstPtr[m++] = 0;
      }
      ahOut->setByteSize(n);
      Uint32 newIndexBuf = indexBuf + (m >> 2);
      ndbrequire(newIndexBuf <= maxRead);
      req_struct->out_buf_index = newIndexBuf;
      return true;
    } else {
      jam();
      terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    }
  } 
  return false;
}

bool
Dbtup::readDiskFixedSizeNULLable(Uint32* outBuffer,
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
Dbtup::readDiskVarSizeNotNULL(Uint32* out_buffer,
			      KeyReqStruct *req_struct,
			      AttributeHeader* ah_out,
			      Uint32  attr_des2)
{
  Uint32 attr_descriptor, index_buf, var_index;
  Uint32 vsize_in_bytes, vsize_in_words, new_index, max_var_size;
  Uint32 var_attr_pos, max_read;

  Uint32 idx= req_struct->m_var_data[DD].m_var_len_offset;
  var_index= AttributeOffset::getOffset(attr_des2);
  var_attr_pos= req_struct->m_var_data[DD].m_offset_array_ptr[var_index];
  vsize_in_bytes= req_struct->m_var_data[DD].m_offset_array_ptr[var_index+idx] - var_attr_pos;
  attr_descriptor= req_struct->attr_descriptor;
  index_buf= req_struct->out_buf_index;
  max_var_size= AttributeDescriptor::getSizeInWords(attr_descriptor);
  max_read= req_struct->max_read;
  vsize_in_words= convert_byte_to_word_size(vsize_in_bytes);
  new_index= index_buf + vsize_in_words;

  ndbrequire(vsize_in_words <= max_var_size);
  if (new_index <= max_read) {
    jam();
    ah_out->setByteSize(vsize_in_bytes);
    memcpy(out_buffer+index_buf,
	   req_struct->m_var_data[DD].m_data_ptr+var_attr_pos,
	   vsize_in_bytes);
    req_struct->out_buf_index= new_index;
    return true;
  } else {
    jam();
    terrorCode= ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }
}

bool
Dbtup::readDiskVarSizeNULLable(Uint32* outBuffer,
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
  req_struct->max_read = MAX_KEY_SIZE_IN_WORDS;
  req_struct->attr_descriptor = attrDescriptor;

  bool tmp = req_struct->xfrm_flag;
  req_struct->xfrm_flag = true;
  ndbrequire((this->*f)(&keyReadBuffer[0],
                        req_struct,
                        &attributeHeader,
                        attributeOffset));
  req_struct->xfrm_flag = tmp;
  
  ndbrequire(req_struct->out_buf_index == attributeHeader.getDataSize());
  if (ahIn.getDataSize() != attributeHeader.getDataSize()) {
    jam();
    return true;
  }
  if (memcmp(&keyReadBuffer[0],
             &updateBuffer[1],
             req_struct->out_buf_index << 2) != 0) {
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
Dbtup::updateFixedSizeTHManyWordNotNULL(Uint32* inBuffer,
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
  Uint32 *tuple_header= req_struct->m_tuple_ptr->m_data;
  ndbrequire((updateOffset + noOfWords - 1) < req_struct->check_offset[MM]);

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
  vpos_array= req_struct->m_var_data[MM].m_offset_array_ptr;
  Uint32 idx= req_struct->m_var_data[MM].m_var_len_offset;
  Uint32 check_offset= req_struct->m_var_data[MM].m_max_var_offset;
  
  if (new_index <= in_buf_len && vsize_in_words <= max_var_size) {
    if (!null_ind) {
      jam();
      var_attr_pos= vpos_array[var_index];
      var_data_start= req_struct->m_var_data[MM].m_data_ptr;
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
Dbtup::updateDynFixedSize(Uint32* inBuffer,
                          KeyReqStruct *req_struct,
                          Uint32  attrDes2)
{
  jam();
  terrorCode= ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}

bool
Dbtup::updateDynVarSize(Uint32* inBuffer,
                        KeyReqStruct *req_struct,
                        Uint32  attrDes2)
{
  jam();
  terrorCode= ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}

Uint32 
Dbtup::read_pseudo(Uint32 attrId,
                   KeyReqStruct *req_struct,
                   Uint32* outBuffer)
{
  Uint32 tmp[sizeof(SignalHeader)+25];
  Signal * signal = (Signal*)&tmp;
  switch(attrId){
  case AttributeHeader::FRAGMENT:
    * outBuffer = fragptr.p->fragmentId;
    return 1;
  case AttributeHeader::FRAGMENT_FIXED_MEMORY:
  {
    Uint64 tmp= fragptr.p->noOfPages;
    tmp*= 32768;
    memcpy(outBuffer,&tmp,8);
  }
  return 2;
  case AttributeHeader::FRAGMENT_VARSIZED_MEMORY:
  {
    Uint64 tmp= fragptr.p->noOfVarPages;
    tmp*= 32768;
    memcpy(outBuffer,&tmp,8);
  }
  return 2;
  case AttributeHeader::ROW_SIZE:
    * outBuffer = tabptr.p->m_offsets[MM].m_fix_header_size << 2;
    return 1;
  case AttributeHeader::ROW_COUNT:
  case AttributeHeader::COMMIT_COUNT:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSEUDO_REQ, signal, 2);
    outBuffer[0] = signal->theData[0];
    outBuffer[1] = signal->theData[1];
    return 2;
  case AttributeHeader::RANGE_NO:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSEUDO_REQ, signal, 2);
    outBuffer[0] = signal->theData[0];
    return 1;
  case AttributeHeader::DISK_REF:
  {
    Uint32 *ref= req_struct->m_tuple_ptr->get_disk_ref_ptr(tabptr.p);
    outBuffer[0] = ref[0];
    outBuffer[1] = ref[1];
    return 2;
  }
  case AttributeHeader::RECORDS_IN_RANGE:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSEUDO_REQ, signal, 2);
    outBuffer[0] = signal->theData[0];
    outBuffer[1] = signal->theData[1];
    outBuffer[2] = signal->theData[2];
    outBuffer[3] = signal->theData[3];
    return 4;
  case AttributeHeader::ROWID:
    outBuffer[0] = req_struct->frag_page_id;
    outBuffer[1] = operPtr.p->m_tuple_location.m_page_idx;
    return 2;
  case AttributeHeader::ROW_GCI:
    if (tabptr.p->m_bits & Tablerec::TR_RowGCI)
    {
      Uint64 tmp = * req_struct->m_tuple_ptr->get_mm_gci(tabptr.p);
      memcpy(outBuffer, &tmp, sizeof(tmp));
      return 2;
    }
    return 0;
  case AttributeHeader::COPY_ROWID:
    outBuffer[0] = operPtr.p->m_copy_tuple_location.m_page_no;
    outBuffer[1] = operPtr.p->m_copy_tuple_location.m_page_idx;
    return 2;
  default:
    return 0;
  }
}

bool
Dbtup::readBitsNotNULL(Uint32* outBuffer,
		       KeyReqStruct* req_struct,
		       AttributeHeader* ahOut,
		       Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 indexBuf = req_struct->out_buf_index;
  Uint32 newIndexBuf = indexBuf + ((bitCount + 31) >> 5);
  Uint32 maxRead = req_struct->max_read;
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize((bitCount + 31) >> 5);
    req_struct->out_buf_index = newIndexBuf;
    
    BitmaskImpl::getField(regTabPtr->m_offsets[MM].m_null_words, bits, pos, 
			  bitCount, outBuffer+indexBuf);
    
    return true;
  } else {
    jam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}

bool
Dbtup::readBitsNULLable(Uint32* outBuffer,
			KeyReqStruct* req_struct,
			AttributeHeader* ahOut,
			Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  
  Uint32 indexBuf = req_struct->out_buf_index;
  Uint32 newIndexBuf = indexBuf + ((bitCount + 31) >> 5);
  Uint32 maxRead = req_struct->max_read;
  Uint32 *bits= req_struct->m_tuple_ptr->get_null_bits(regTabPtr);
  
  if(BitmaskImpl::get(regTabPtr->m_offsets[MM].m_null_words, bits, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize((bitCount + 31) >> 5);
    req_struct->out_buf_index = newIndexBuf;
    BitmaskImpl::getField(regTabPtr->m_offsets[MM].m_null_words, bits, pos+1, 
			  bitCount, outBuffer+indexBuf);
    return true;
  } else {
    jam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
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
Dbtup::readDiskBitsNotNULL(Uint32* outBuffer,
			   KeyReqStruct* req_struct,
			   AttributeHeader* ahOut,
			   Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  Uint32 indexBuf = req_struct->out_buf_index;
  Uint32 newIndexBuf = indexBuf + ((bitCount + 31) >> 5);
  Uint32 maxRead = req_struct->max_read;
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize((bitCount + 31) >> 5);
    req_struct->out_buf_index = newIndexBuf;
    
    BitmaskImpl::getField(regTabPtr->m_offsets[DD].m_null_words, bits, pos, 
			  bitCount, outBuffer+indexBuf);
    
    return true;
  } else {
    jam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}

bool
Dbtup::readDiskBitsNULLable(Uint32* outBuffer,
			    KeyReqStruct* req_struct,
			    AttributeHeader* ahOut,
			    Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = 
    AttributeDescriptor::getArraySize(req_struct->attr_descriptor);
  
  Uint32 indexBuf = req_struct->out_buf_index;
  Uint32 newIndexBuf = indexBuf + ((bitCount + 31) >> 5);
  Uint32 maxRead = req_struct->max_read;
  Uint32 *bits= req_struct->m_disk_ptr->get_null_bits(regTabPtr, DD);
  
  if(BitmaskImpl::get(regTabPtr->m_offsets[DD].m_null_words, bits, pos))
  {
    jam();
    ahOut->setNULL();
    return true;
  }

  if (newIndexBuf <= maxRead) {
    jam();
    ahOut->setDataSize((bitCount + 31) >> 5);
    req_struct->out_buf_index = newIndexBuf;
    BitmaskImpl::getField(regTabPtr->m_offsets[DD].m_null_words, bits, pos+1, 
			  bitCount, outBuffer+indexBuf);
    return true;
  } else {
    jam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
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

