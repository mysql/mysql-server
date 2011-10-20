/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include "Filename.hpp"
#include "ErrorHandlingMacros.hpp"
#include "RefConvert.hpp"
#include "DebuggerNames.hpp"
#include "Ndbfs.hpp"

#include <signaldata/FsOpenReq.hpp>

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
  char buf[PATH_MAX];

  const Uint32 type = FsOpenReq::getSuffix(filenumber);
  const Uint32 version = FsOpenReq::getVersion(filenumber);

  size_t sz;
  if (version == 2)
  {
    sz = BaseString::snprintf(theName, sizeof(theName), "%s", 
                              fs->get_base_path(FsOpenReq::BP_BACKUP).c_str());
    m_base_name = theName + fs->get_base_path(FsOpenReq::BP_BACKUP).length();
  }
  else
  {
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
    const Uint32 count = FsOpenReq::v2_getCount(filenumber);
    
    BaseString::snprintf(buf, sizeof(buf), "BACKUP%sBACKUP-%u%s",
	     DIR_SEPARATOR, seq, DIR_SEPARATOR); 
    strcat(theName, buf);
    if(count == 0xffffffff) {
      BaseString::snprintf(buf, sizeof(buf), "BACKUP-%u.%d",
	       seq, nodeId); strcat(theName, buf);
    } else {
      BaseString::snprintf(buf, sizeof(buf), "BACKUP-%u-%d.%d",
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
    char buf[PATH_MAX];
    copy((Uint32*)&buf[0], ptr);
    if(buf[0] == DIR_SEPARATOR[0])
    {
      strncpy(theName, buf, PATH_MAX);
      m_base_name = theName;
    }
    else
    {
#ifdef NDB_WIN32
      char* b= buf;
      while((b= strchr(b, '/')) && b)
      {
        *b= '\\';
      }
#endif
      Uint32 bp = FsOpenReq::v4_getBasePath(filenumber);
      BaseString::snprintf(theName, sizeof(theName), "%s%s",
               fs->get_base_path(bp).c_str(), buf);
      m_base_name = theName + fs->get_base_path(bp).length();
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
