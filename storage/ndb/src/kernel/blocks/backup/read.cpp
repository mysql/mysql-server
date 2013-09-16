/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include "BackupFormat.hpp"
#include <AttributeHeader.hpp>
#include <SimpleProperties.hpp>
#include <ndb_version.h>
#include <util/ndbzio.h>

#define JAM_FILE_ID 476


bool readHeader(ndbzio_stream*, BackupFormat::FileHeader *);
bool readFragHeader(ndbzio_stream*, BackupFormat::DataFile::FragmentHeader *);
bool readFragFooter(ndbzio_stream*, BackupFormat::DataFile::FragmentFooter *);
Int32 readRecord(ndbzio_stream*, Uint32 **);

NdbOut & operator<<(NdbOut&, const BackupFormat::FileHeader &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::DataFile::FragmentHeader &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::DataFile::FragmentFooter &); 

bool readTableList(ndbzio_stream*, BackupFormat::CtlFile::TableList **);
bool readTableDesc(ndbzio_stream*, BackupFormat::CtlFile::TableDescription **);
bool readGCPEntry(ndbzio_stream*, BackupFormat::CtlFile::GCPEntry **);

NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::TableList &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::TableDescription &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::GCPEntry &); 

Int32 readLogEntry(ndbzio_stream*, Uint32**);

static Uint32 recNo;
static Uint32 logEntryNo;

int
main(int argc, const char * argv[]){

  ndb_init();
  if(argc <= 1){
    printf("Usage: %s <filename>\n", argv[0]);
    exit(1);
  }

  ndbzio_stream fo;
  bzero(&fo, sizeof(fo));
  int r= ndbzopen(&fo,argv[1], O_RDONLY);

  if(r != 1)
  {
    ndbout_c("Failed to open file '%s', error: %d",
             argv[1], r);
    exit(1);
  }

  ndbzio_stream* f = &fo;

  BackupFormat::FileHeader fileHeader;
  if(!readHeader(f, &fileHeader)){
    ndbout << "Invalid file!" << endl;
    exit(1);
  }	
  ndbout << fileHeader << endl;

  switch(fileHeader.FileType){
  case BackupFormat::DATA_FILE:
    while(f->z_eof){
      BackupFormat::DataFile::FragmentHeader fragHeader;
      if(!readFragHeader(f, &fragHeader))
	break;
      ndbout << fragHeader << endl;
      
      Uint32 len, * data;
      while((len = readRecord(f, &data)) > 0){
#if 0
	ndbout << "-> " << hex;
	for(Uint32 i = 0; i<len; i++){
	  ndbout << data[i] << " ";
	}
	ndbout << endl;
#endif
      }

      BackupFormat::DataFile::FragmentFooter fragFooter;
      if(!readFragFooter(f, &fragFooter))
	break;
      ndbout << fragFooter << endl;
    }
    break;
  case BackupFormat::CTL_FILE:{
    BackupFormat::CtlFile::TableList * tabList;
    if(!readTableList(f, &tabList)){
      ndbout << "Invalid file! No table list" << endl;
      break;
    }
    ndbout << (* tabList) << endl;

    const Uint32 noOfTables = tabList->SectionLength - 2;
    for(Uint32 i = 0; i<noOfTables; i++){
      BackupFormat::CtlFile::TableDescription * tabDesc;
      if(!readTableDesc(f, &tabDesc)){
	ndbout << "Invalid file missing table description" << endl;
	break;
      }
      ndbout << (* tabDesc) << endl;
    }

    BackupFormat::CtlFile::GCPEntry * gcpE;
    if(!readGCPEntry(f, &gcpE)){
      ndbout << "Invalid file! GCP ENtry" << endl;
      break;
    }
    ndbout << (* gcpE) << endl;
    
    break;
  }
  case BackupFormat::LOG_FILE:{
    logEntryNo = 0;

    typedef BackupFormat::LogFile::LogEntry LogEntry;

    Uint32 len, * data;
    while((len = readLogEntry(f, &data)) > 0){
      LogEntry * logEntry = (LogEntry *) data;
      /**
       * Log Entry
       */
      Uint32 event = ntohl(logEntry->TriggerEvent);
      bool gcp = (event & 0x10000) != 0;
      event &= 0xFFFF;
      if(gcp)
	len --;
      
      ndbout << "LogEntry Table: " << (Uint32)ntohl(logEntry->TableId) 
	     << " Event: " << event
	     << " Length: " << (len - 2);
      
      const Uint32 dataLen = len - 2;
#if 0
      Uint32 pos = 0;
      while(pos < dataLen){
	AttributeHeader * ah = (AttributeHeader*)&logEntry->Data[pos];
	ndbout_c(" Attribut: %d Size: %d",
		 ah->getAttributeId(),
		 ah->getDataSize());
	pos += ah->getDataSize() + 1;
      }
#endif
      if(gcp)
	  ndbout << " GCP: " << (Uint32)ntohl(logEntry->Data[dataLen]);
      ndbout << endl;
    }
    break;
  }
  case BackupFormat::LCP_FILE:
  {
    BackupFormat::CtlFile::TableList * tabList;
    if(!readTableList(f, &tabList)){
      ndbout << "Invalid file! No table list" << endl;
      break;
    }
    ndbout << (* tabList) << endl;
    
    const Uint32 noOfTables = tabList->SectionLength - 2;
    for(Uint32 i = 0; i<noOfTables; i++){
      BackupFormat::CtlFile::TableDescription * tabDesc;
      if(!readTableDesc(f, &tabDesc)){
	ndbout << "Invalid file missing table description" << endl;
	break;
      }
      ndbout << (* tabDesc) << endl;
    }

    while(!f->z_eof){
      BackupFormat::DataFile::FragmentHeader fragHeader;
      if(!readFragHeader(f, &fragHeader))
	break;
      ndbout << fragHeader << endl;
      
      Uint32 len, * data;
      while((len = readRecord(f, &data)) > 0){
#if 0
	ndbout << "-> " << hex;
	for(Uint32 i = 0; i<len; i++){
	  ndbout << data[i] << " ";
	}
	ndbout << endl;
#endif
      }
      
      BackupFormat::DataFile::FragmentFooter fragFooter;
      if(!readFragFooter(f, &fragFooter))
	break;
      ndbout << fragFooter << endl;
    }
    break;
  }
  default:
    ndbout << "Unsupported file type for printer: " 
	   << fileHeader.FileType << endl;
    break;
  }
  ndbzclose(f);
  return 0;
}

#define RETURN_FALSE() { ndbout_c("false: %d", __LINE__); abort(); return false; }

static bool endian = false;

static
inline
size_t
aread(void * buf, size_t sz, size_t n, ndbzio_stream* f)
{
  int error = 0;
  unsigned r = ndbzread(f, buf, (unsigned)(sz * n), &error);
  if (error || r != (sz * n))
  {
    printf("Failed to read!!");
    exit(1);
  }
  return r / sz;
}

bool 
readHeader(ndbzio_stream* f, BackupFormat::FileHeader * dst){
  if(aread(dst, 4, 3, f) != 3)
    RETURN_FALSE();

  if(memcmp(dst->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC)) != 0)
  {
    ndbout_c("Incorrect file-header!");
    printf("Found:  ");
    for (unsigned i = 0; i<sizeof(BACKUP_MAGIC); i++)
      printf("0x%.2x ", (Uint32)(Uint8)dst->Magic[i]);
    printf("\n");
    printf("Expect: ");
    for (unsigned i = 0; i<sizeof(BACKUP_MAGIC); i++)
      printf("0x%.2x ", (Uint32)(Uint8)BACKUP_MAGIC[i]);
    printf("\n");
    
    RETURN_FALSE();
  }

  dst->BackupVersion = ntohl(dst->BackupVersion);
  if(dst->BackupVersion >= NDB_VERSION)
  {
    printf("incorrect versions, file: 0x%x expect: 0x%x\n", dst->NdbVersion, NDB_VERSION);
    RETURN_FALSE();
  }

  if(aread(&dst->SectionType, 4, 2, f) != 2)
    RETURN_FALSE();
  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);

  if(dst->SectionType != BackupFormat::FILE_HEADER)
    RETURN_FALSE();

  if(dst->SectionLength != ((sizeof(BackupFormat::FileHeader) - 12) >> 2))
    RETURN_FALSE();

  if(aread(&dst->FileType, 4, dst->SectionLength - 2, f) != 
     (dst->SectionLength - 2))
    RETURN_FALSE();

  dst->FileType = ntohl(dst->FileType);
  dst->BackupId = ntohl(dst->BackupId);
  dst->BackupKey_0 = ntohl(dst->BackupKey_0);
  dst->BackupKey_1 = ntohl(dst->BackupKey_1);
  
  if(dst->ByteOrder != 0x12345678)
    endian = true;
  
  return true;
}

bool 
readFragHeader(ndbzio_stream* f, BackupFormat::DataFile::FragmentHeader * dst){
  if(aread(dst, 1, sizeof(* dst), f) != sizeof(* dst))
    return false;

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  dst->TableId = ntohl(dst->TableId);
  dst->FragmentNo = ntohl(dst->FragmentNo);
  dst->ChecksumType = ntohl(dst->ChecksumType);

  if(dst->SectionLength != (sizeof(* dst) >> 2))
    RETURN_FALSE();
  
  if(dst->SectionType != BackupFormat::FRAGMENT_HEADER)
    RETURN_FALSE();

  recNo = 0;

  return true;
}

bool 
readFragFooter(ndbzio_stream* f, BackupFormat::DataFile::FragmentFooter * dst){
  if(aread(dst, 1, sizeof(* dst), f) != sizeof(* dst))
    RETURN_FALSE();
  
  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  dst->TableId = ntohl(dst->TableId);
  dst->FragmentNo = ntohl(dst->FragmentNo);
  dst->NoOfRecords = ntohl(dst->NoOfRecords);
  dst->Checksum = ntohl(dst->Checksum);
  
  if(dst->SectionLength != (sizeof(* dst) >> 2))
    RETURN_FALSE();
  
  if(dst->SectionType != BackupFormat::FRAGMENT_FOOTER)
    RETURN_FALSE();
  return true;
}


static union {
  Uint32 buf[8192];
  BackupFormat::CtlFile::TableList TableList;
  BackupFormat::CtlFile::GCPEntry GcpEntry;
  BackupFormat::CtlFile::TableDescription TableDescription;
  BackupFormat::LogFile::LogEntry LogEntry;
} theData;

Int32
readRecord(ndbzio_stream* f, Uint32 **dst){
  Uint32 len;
  if(aread(&len, 1, 4, f) != 4)
    RETURN_FALSE();

  len = ntohl(len);
  
  if(aread(theData.buf, 4, len, f) != len)
  {
    return -1;
  }

  if(len > 0)
    recNo++;
  else
    ndbout_c("Found %d records", recNo);
  
  * dst = &theData.buf[0];

  
  return len;
}

Int32
readLogEntry(ndbzio_stream* f, Uint32 **dst){
  Uint32 len;
  if(aread(&len, 1, 4, f) != 4)
    RETURN_FALSE();
  
  len = ntohl(len);
  
  if(aread(&theData.buf[1], 4, len, f) != len)
    return -1;
  
  theData.buf[0] = len;
  
  if(len > 0)
    logEntryNo++;
  
  * dst = &theData.buf[0];
  
  return len;
}


NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::FileHeader & hf){
  
  char buf[9];
  memcpy(buf, hf.Magic, sizeof(hf.Magic));
  buf[8] = 0;

  ndbout << "-- FileHeader:" << endl;
  ndbout << "Magic: " << buf << endl;
  ndbout << "BackupVersion: " << hex << hf.BackupVersion << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "FileType: " << hf.FileType << endl;
  ndbout << "BackupId: " << hf.BackupId << endl;
  ndbout << "BackupKey: [ " << hex << hf.BackupKey_0 
	 << " "<< hf.BackupKey_1 << " ]" << endl;
  ndbout << "ByteOrder: " << hex << hf.ByteOrder << endl;
  return ndbout;
} 

NdbOut & operator<<(NdbOut& ndbout, 
		    const BackupFormat::DataFile::FragmentHeader & hf){

  ndbout << "-- Fragment header:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "TableId: " << hf.TableId << endl;
  ndbout << "FragmentNo: " << hf.FragmentNo << endl;
  ndbout << "ChecksumType: " << hf.ChecksumType << endl;
  
  return ndbout;
} 
NdbOut & operator<<(NdbOut& ndbout, 
		    const BackupFormat::DataFile::FragmentFooter & hf){
  
  ndbout << "-- Fragment footer:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "TableId: " << hf.TableId << endl;
  ndbout << "FragmentNo: " << hf.FragmentNo << endl;
  ndbout << "NoOfRecords: " << hf.NoOfRecords << endl;
  ndbout << "Checksum: " << hf.Checksum << endl;
  
  return ndbout;
} 

bool 
readTableList(ndbzio_stream* f, BackupFormat::CtlFile::TableList **ret){
  BackupFormat::CtlFile::TableList * dst = &theData.TableList;
  
  if(aread(dst, 4, 2, f) != 2)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::TABLE_LIST)
    RETURN_FALSE();
  
  const Uint32 len = dst->SectionLength - 2;
  if(aread(&dst->TableIds[0], 4, len, f) != len)
    RETURN_FALSE();

  for(Uint32 i = 0; i<len; i++){
    dst->TableIds[i] = ntohl(dst->TableIds[i]);
  }

  * ret = dst;

  return true;
}

bool 
readTableDesc(ndbzio_stream* f, BackupFormat::CtlFile::TableDescription **ret){
  BackupFormat::CtlFile::TableDescription * dst = &theData.TableDescription;
  
  if(aread(dst, 4, 3, f) != 3)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  dst->TableType = ntohl(dst->TableType);

  if(dst->SectionType != BackupFormat::TABLE_DESCRIPTION)
    RETURN_FALSE();
  
  const Uint32 len = dst->SectionLength - 3;
  if(aread(&dst->DictTabInfo[0], 4, len, f) != len)
    RETURN_FALSE();
  
  * ret = dst;
  
  return true;
}

bool 
readGCPEntry(ndbzio_stream* f, BackupFormat::CtlFile::GCPEntry **ret){
  BackupFormat::CtlFile::GCPEntry * dst = &theData.GcpEntry;
  
  if(aread(dst, 4, 4, f) != 4)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::GCP_ENTRY)
    RETURN_FALSE();
  
  dst->StartGCP = ntohl(dst->StartGCP);
  dst->StopGCP = ntohl(dst->StopGCP);

  * ret = dst;
  
  return true;
}


NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::CtlFile::TableList & hf) {
  ndbout << "-- Table List:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  for(Uint32 i = 0; i < hf.SectionLength - 2; i++){
    ndbout << hf.TableIds[i] << " ";
    if((i + 1) % 16 == 0)
      ndbout << endl;
  }
  return ndbout;
}

NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::CtlFile::TableDescription & hf){
  ndbout << "-- Table Description:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "TableType: " << hf.TableType << endl;

  SimplePropertiesLinearReader it(&hf.DictTabInfo[0],  hf.SectionLength - 3);
  char buf[1024];
  for(it.first(); it.valid(); it.next()){
    switch(it.getValueType()){
    case SimpleProperties::Uint32Value:
      ndbout << "Key: " << it.getKey()
	     << " value(" << it.getValueLen() << ") : " 
	     << it.getUint32() << endl;
      break;
    case SimpleProperties::StringValue:
      if(it.getValueLen() < sizeof(buf)){
	it.getString(buf);
	ndbout << "Key: " << it.getKey()
	       << " value(" << it.getValueLen() << ") : " 
	       << "\"" << buf << "\"" << endl;
      } else {
	ndbout << "Key: " << it.getKey()
	       << " value(" << it.getValueLen() << ") : " 
	       << "\"" << "<TOO LONG>" << "\"" << endl;
	
      }
      break;
    default:
      ndbout << "Unknown type for key: " << it.getKey() 
	     << " type: " << it.getValueType() << endl;
    }
  }
  
  return ndbout;
} 

NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::CtlFile::GCPEntry & hf) {
  ndbout << "-- GCP Entry:" << endl;
  ndbout << "SectionType: " << hf.SectionType << endl;
  ndbout << "SectionLength: " << hf.SectionLength << endl;
  ndbout << "Start GCP: " << hf.StartGCP << endl;
  ndbout << "Stop GCP: " << hf.StopGCP << endl;
  
  return ndbout;
}

