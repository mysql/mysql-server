/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include "BackupFormat.hpp"
#include <AttributeHeader.hpp>
#include <SimpleProperties.hpp>

bool readHeader(FILE*, BackupFormat::FileHeader *);
bool readFragHeader(FILE*, BackupFormat::DataFile::FragmentHeader *);
bool readFragFooter(FILE*, BackupFormat::DataFile::FragmentFooter *);
Int32 readRecord(FILE*, Uint32 **);

NdbOut & operator<<(NdbOut&, const BackupFormat::FileHeader &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::DataFile::FragmentHeader &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::DataFile::FragmentFooter &); 

bool readTableList(FILE*, BackupFormat::CtlFile::TableList **);
bool readTableDesc(FILE*, BackupFormat::CtlFile::TableDescription **);
bool readGCPEntry(FILE*, BackupFormat::CtlFile::GCPEntry **);

NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::TableList &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::TableDescription &); 
NdbOut & operator<<(NdbOut&, const BackupFormat::CtlFile::GCPEntry &); 

Int32 readLogEntry(FILE*, Uint32**);

static Uint32 recNo;
static Uint32 logEntryNo;

int
main(int argc, const char * argv[]){

  ndb_init();
  if(argc <= 1){
    printf("Usage: %s <filename>", argv[0]);
    exit(1);
  }
  FILE * f = fopen(argv[1], "rb");
  if(!f){
    ndbout << "No such file!" << endl;
    exit(1);
  }

  BackupFormat::FileHeader fileHeader;
  if(!readHeader(f, &fileHeader)){
    ndbout << "Invalid file!" << endl;
    exit(1);
  }	
  ndbout << fileHeader << endl;

  switch(fileHeader.FileType){
  case BackupFormat::DATA_FILE:
    while(!feof(f)){
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
  default:
    ndbout << "Unsupported file type for printer: " 
	   << fileHeader.FileType << endl;
    break;
  }
  fclose(f);
  return 0;
}

#define RETURN_FALSE() { ndbout_c("false: %d", __LINE__); abort(); return false; }

static bool endian = false;

bool 
readHeader(FILE* f, BackupFormat::FileHeader * dst){
  if(fread(dst, 4, 3, f) != 3)
    RETURN_FALSE();

  if(memcmp(dst->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC)) != 0)
    RETURN_FALSE();

  dst->NdbVersion = ntohl(dst->NdbVersion);
  if(dst->NdbVersion != 210)
    RETURN_FALSE();

  if(fread(&dst->SectionType, 4, 2, f) != 2)
    RETURN_FALSE();
  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);

  if(dst->SectionType != BackupFormat::FILE_HEADER)
    RETURN_FALSE();

  if(dst->SectionLength != ((sizeof(BackupFormat::FileHeader) - 12) >> 2))
    RETURN_FALSE();

  if(fread(&dst->FileType, 4, dst->SectionLength - 2, f) != 
     (dst->SectionLength - 2))
    RETURN_FALSE();

  dst->FileType = ntohl(dst->FileType);
  dst->BackupId = ntohl(dst->BackupId);
  dst->BackupKey_0 = ntohl(dst->BackupKey_0);
  dst->BackupKey_1 = ntohl(dst->BackupKey_1);

  if(dst->FileType < BackupFormat::CTL_FILE || 
     dst->FileType > BackupFormat::DATA_FILE)
    RETURN_FALSE();
  
  if(dst->ByteOrder != 0x12345678)
    endian = true;
  
  return true;
}

bool 
readFragHeader(FILE* f, BackupFormat::DataFile::FragmentHeader * dst){
  if(fread(dst, 1, sizeof(* dst), f) != sizeof(* dst))
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
readFragFooter(FILE* f, BackupFormat::DataFile::FragmentFooter * dst){
  if(fread(dst, 1, sizeof(* dst), f) != sizeof(* dst))
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

static Uint32 buf[8192];

Int32
readRecord(FILE* f, Uint32 **dst){
  Uint32 len;
  if(fread(&len, 1, 4, f) != 4)
    RETURN_FALSE();

  len = ntohl(len);
  
  if(fread(buf, 4, len, f) != len)
    return -1;

  if(len > 0)
    recNo++;

  * dst = &buf[0];
  
  return len;
}

Int32
readLogEntry(FILE* f, Uint32 **dst){
  Uint32 len;
  if(fread(&len, 1, 4, f) != 4)
    RETURN_FALSE();
  
  len = ntohl(len);
  
  if(fread(&buf[1], 4, len, f) != len)
    return -1;
  
  buf[0] = len;
  
  if(len > 0)
    logEntryNo++;
  
  * dst = &buf[0];
  
  return len;
}


NdbOut & 
operator<<(NdbOut& ndbout, const BackupFormat::FileHeader & hf){
  
  char buf[9];
  memcpy(buf, hf.Magic, sizeof(hf.Magic));
  buf[8] = 0;

  ndbout << "-- FileHeader:" << endl;
  ndbout << "Magic: " << buf << endl;
  ndbout << "NdbVersion: " << hf.NdbVersion << endl;
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
readTableList(FILE* f, BackupFormat::CtlFile::TableList **ret){
  BackupFormat::CtlFile::TableList * dst = 
    (BackupFormat::CtlFile::TableList *)&buf[0];
  
  if(fread(dst, 4, 2, f) != 2)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::TABLE_LIST)
    RETURN_FALSE();
  
  const Uint32 len = dst->SectionLength - 2;
  if(fread(&dst->TableIds[0], 4, len, f) != len)
    RETURN_FALSE();

  for(Uint32 i = 0; i<len; i++){
    dst->TableIds[i] = ntohl(dst->TableIds[i]);
  }

  * ret = dst;

  return true;
}

bool 
readTableDesc(FILE* f, BackupFormat::CtlFile::TableDescription **ret){
  BackupFormat::CtlFile::TableDescription * dst = 
    (BackupFormat::CtlFile::TableDescription *)&buf[0];
  
  if(fread(dst, 4, 2, f) != 2)
    RETURN_FALSE();

  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::TABLE_DESCRIPTION)
    RETURN_FALSE();
  
  const Uint32 len = dst->SectionLength - 2;
  if(fread(&dst->DictTabInfo[0], 4, len, f) != len)
    RETURN_FALSE();
  
  * ret = dst;
  
  return true;
}

bool 
readGCPEntry(FILE* f, BackupFormat::CtlFile::GCPEntry **ret){
  BackupFormat::CtlFile::GCPEntry * dst = 
    (BackupFormat::CtlFile::GCPEntry *)&buf[0];
  
  if(fread(dst, 4, 4, f) != 4)
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

  SimplePropertiesLinearReader it(&hf.DictTabInfo[0],  hf.SectionLength - 2);
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

