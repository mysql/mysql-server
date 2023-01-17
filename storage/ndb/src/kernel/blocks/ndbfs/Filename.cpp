/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include "Filename.hpp"
#include "ErrorHandlingMacros.hpp"
#include "RefConvert.hpp"
#include "DebuggerNames.hpp"
#include "Ndbfs.hpp"

#include <signaldata/FsOpenReq.hpp>

#define JAM_FILE_ID 383


static const char* fileExtension[] = {
  ".Data",
  ".FragLog",
  ".LocLog",
  ".FragList",
  ".TableList",
  ".SchemaLog",
  ".sysfile",
  ".log",
  ".ctl"
};

static const Uint32 noOfExtensions = sizeof(fileExtension)/sizeof(char*);

Filename::Filename()
{
}

Filename::~Filename(){
}

void 
Filename::set(Ndbfs* fs,
              BlockReference blockReference,
              const Uint32 filenumber[4], bool dir,
              SegmentedSectionPtr ptr)
{
  m_base_path_spec = FsOpenReq::BP_MAX;

  char buf[PATH_MAX];

  const Uint32 type = FsOpenReq::getSuffix(filenumber);
  const Uint32 version = FsOpenReq::getVersion(filenumber);

  size_t sz;
  if (version == 2)
  {
    m_base_path_spec = FsOpenReq::BP_BACKUP;
    sz = BaseString::snprintf(theName, sizeof(theName), "%s", 
                              fs->get_base_path(FsOpenReq::BP_BACKUP).c_str());
    m_base_name = theName + fs->get_base_path(FsOpenReq::BP_BACKUP).length();
  }
  else
  {
    m_base_path_spec = FsOpenReq::BP_FS;
    sz = BaseString::snprintf(theName, sizeof(theName), "%s", 
                              fs->get_base_path(FsOpenReq::BP_FS).c_str());
    m_base_name = theName + fs->get_base_path(FsOpenReq::BP_FS).length();
  }
  
  switch(version){
  case 1 :{
    const Uint32 diskNo = FsOpenReq::v1_getDisk(filenumber);
    const Uint32 table  = FsOpenReq::v1_getTable(filenumber);
    const Uint32 frag   = FsOpenReq::v1_getFragment(filenumber);
    const Uint32 S_val  = FsOpenReq::v1_getS(filenumber);
    const Uint32 P_val  = FsOpenReq::v1_getP(filenumber);

    if (diskNo < 0xff){	  
      BaseString::snprintf(buf, sizeof(buf), "D%d%s", diskNo, DIR_SEPARATOR);
      strcat(theName, buf);
    }
    
    {
      const char* blockName = getBlockName( refToMain(blockReference) );
      if (blockName == NULL){
	ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"","No Block Name");
	return;
      }
      BaseString::snprintf(buf, sizeof(buf), "%s%s", blockName, DIR_SEPARATOR);
      strcat(theName, buf);
    }
    
    if (table < 0xffffffff){
      BaseString::snprintf(buf, sizeof(buf), "T%d%s", table, DIR_SEPARATOR);
      strcat(theName, buf);
    }
    
    if (frag < 0xffffffff){
      BaseString::snprintf(buf, sizeof(buf), "F%d%s", frag, DIR_SEPARATOR);
      strcat(theName, buf);
    }
    
    
    if (S_val < 0xffffffff){
      BaseString::snprintf(buf, sizeof(buf), "S%d", S_val);
      strcat(theName, buf);
    }

    if (P_val < 0xff){
      BaseString::snprintf(buf, sizeof(buf), "P%d", P_val);
      strcat(theName, buf);
    }
    
  }
  break;
  case 2:{
    const Uint32 seq = FsOpenReq::v2_getSequence(filenumber);
    const Uint32 nodeId = FsOpenReq::v2_getNodeId(filenumber);
    const Uint32 partNum =  FsOpenReq::v2_getPartNum(filenumber);
    const Uint32 totalParts =  FsOpenReq::v2_getTotalParts(filenumber);
    const Uint32 count = FsOpenReq::v2_getCount(filenumber);

    if(partNum == 0)
      BaseString::snprintf(buf, sizeof(buf), "BACKUP%sBACKUP-%u%s",
            DIR_SEPARATOR, seq, DIR_SEPARATOR);
    else
      BaseString::snprintf(buf, sizeof(buf), "BACKUP%sBACKUP-%u%sBACKUP-%u-PART-%u-OF-%u%s",
             DIR_SEPARATOR, seq, DIR_SEPARATOR, seq, partNum, totalParts, DIR_SEPARATOR);

    strcat(theName, buf);
    if(count == 0xffff) {
      BaseString::snprintf(buf, sizeof(buf), "BACKUP-%u.%u",
	       seq, nodeId); strcat(theName, buf);
    } else {
      BaseString::snprintf(buf, sizeof(buf), "BACKUP-%u-%u.%u",
	       seq, count, nodeId); strcat(theName, buf);
    }
    break;
  }
  break;
  case 3:{
    const Uint32 diskNo = FsOpenReq::v1_getDisk(filenumber);

    if(diskNo == 0xFF){
      ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"","Invalid disk specification");
    }

    BaseString::snprintf(buf, sizeof(buf), "D%d%s", diskNo, DIR_SEPARATOR);
    strcat(theName, buf);
  }
    break;
  case 4:
  {
    const unsigned ptr_sz_bytes = ptr.sz * 4;
    if (ptr_sz_bytes > PATH_MAX) {
      ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"",
                "File name is too long");
      return;
    }
    if (ptr_sz_bytes == 0) {
      ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"",
                "File name is empty");
      return;
    }

    char buf[PATH_MAX];
    copy((Uint32*)&buf[0], ptr);

    const bool nul_terminated = ((buf[ptr_sz_bytes-1] == '\0') ||
                                 (buf[ptr_sz_bytes-2] == '\0') ||
                                 (buf[ptr_sz_bytes-3] == '\0') ||
                                 (buf[ptr_sz_bytes-4] == '\0'));
    if (!nul_terminated) {
      ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"",
                "File name is not NUL-terminated");
      return;
    }
    if(buf[0] == '\0')
    {
      ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"",
                "File name is not given");
      return;
    }

    const unsigned theName_sz = sizeof(theName);
    if(buf[0] == DIR_SEPARATOR[0])
    {
      BaseString::snprintf(theName, theName_sz, "%s", buf);
      m_base_name = theName;
    }
    else
    {
#ifdef _WIN32
      char* b= buf;
      while((b= strchr(b, '/')) && b)
      {
        *b= '\\';
      }
#endif
      Uint32 bp = FsOpenReq::v4_getBasePath(filenumber);
      m_base_path_spec = bp;
      const Uint32 base_path_len = fs->get_base_path(bp).length();
      const size_t concat_sz = BaseString::snprintf(theName, theName_sz, "%s%s",
                                fs->get_base_path(bp).c_str(), buf);
      if (concat_sz >= theName_sz) {
        // File path name is truncated
        ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"",
                  "File path name is too long");
        return;
      }
      m_base_name = theName + base_path_len;
    }
    return; // No extension
  }
  case 5:
  {
    Uint32 tableId = FsOpenReq::v5_getTableId(filenumber);
    Uint32 lcpNo = FsOpenReq::v5_getLcpNo(filenumber);
    Uint32 fragId = FsOpenReq::v5_getFragmentId(filenumber);
    BaseString::snprintf(buf, sizeof(buf), "LCP%s%d%sT%dF%d", DIR_SEPARATOR, lcpNo, DIR_SEPARATOR, tableId, fragId);
    strcat(theName, buf);
    break;
  }
  case 6:
  {
    Uint32 bp = FsOpenReq::v5_getLcpNo(filenumber);
    m_base_path_spec = bp;
    sz = BaseString::snprintf(theName, sizeof(theName), "%s",
                              fs->get_base_path(bp).c_str());
    break;
  }
  default:
    ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"","Wrong version");
  }
  if (type >= noOfExtensions){
    ERROR_SET(ecError, NDBD_EXIT_AFS_PARAMETER,"","File Type doesn't exist");
    return;
  }
  strcat(theName, fileExtension[type]);
  
  if(dir == true){
    for(int l = (int)strlen(theName) - 1; l >= 0; l--){
      if(theName[l] == DIR_SEPARATOR[0]){
	theName[l] = 0;
	break;
      }
    }
  }
}
