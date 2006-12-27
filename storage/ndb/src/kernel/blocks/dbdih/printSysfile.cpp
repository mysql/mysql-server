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


#include <ndb_global.h>

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <Sysfile.hpp>

void 
usage(const char * prg){
  ndbout << "Usage " << prg 
	 << " P[0-1].sysfile" << endl;  
}

struct NSString {
  Sysfile::ActiveStatus NodeStatus;
  const char * desc;
};

static const
NSString NodeStatusStrings[] = {
  { Sysfile::NS_Active,                 "Active         " },
  { Sysfile::NS_ActiveMissed_1,         "Active missed 1" },
  { Sysfile::NS_ActiveMissed_2,         "Active missed 2" },
  { Sysfile::NS_ActiveMissed_3,         "Active missed 3" },
  { Sysfile::NS_HotSpare,               "Hot spare      " },
  { Sysfile::NS_NotActive_NotTakenOver, "Not active     " },
  { Sysfile::NS_TakeOver,               "Take over      " },
  { Sysfile::NS_NotActive_TakenOver,    "Taken over     " },
  { Sysfile::NS_NotDefined,             "Not defined    " },
  { Sysfile::NS_Standby,                "Stand by       " }
};

const
char * getNSString(Uint32 ns){
  for(Uint32 i = 0; i<(sizeof(NodeStatusStrings)/sizeof(NSString)); i++)
    if((Uint32)NodeStatusStrings[i].NodeStatus == ns)
      return NodeStatusStrings[i].desc;
  return "<Unknown state>";
}

void
fill(const char * buf, int mod){
  int len = strlen(buf)+1;
  ndbout << buf << " ";
  while((len % mod) != 0){
    ndbout << " ";
    len++;
  }
}

void 
print(const char * filename, const Sysfile * sysfile){
  char buf[255];
  ndbout << "----- Sysfile: " << filename 
	 << " seq: " << hex << sysfile->m_restart_seq
	 << " -----" << endl;
  ndbout << "Initial start ongoing: " 
	 << Sysfile::getInitialStartOngoing(sysfile->systemRestartBits) 
	 << ", ";

  ndbout << "Restart Ongoing: "
	 << Sysfile::getRestartOngoing(sysfile->systemRestartBits) 
	 << ", ";

  ndbout << "LCP Ongoing: "
	 << Sysfile::getLCPOngoing(sysfile->systemRestartBits) 
	 << endl;


  ndbout << "-- Global Checkpoint Identities: --" << endl;
  sprintf(buf, "keepGCI = %u", sysfile->keepGCI);
  fill(buf, 40); 
  ndbout << " -- Tail of REDO log" << endl;
  
  sprintf(buf, "oldestRestorableGCI = %u", sysfile->oldestRestorableGCI);
  fill(buf, 40);
  ndbout << " -- " << endl;

  sprintf(buf, "newestRestorableGCI = %u", sysfile->newestRestorableGCI);
  fill(buf, 40);
  ndbout << " -- " << endl;

  sprintf(buf, "latestLCP = %u", sysfile->latestLCP_ID);
  fill(buf, 40);
  ndbout << " -- " << endl;

  ndbout << "-- Node status: --" << endl;
  for(int i = 1; i < MAX_NDB_NODES; i++){
    if(Sysfile::getNodeStatus(i, sysfile->nodeStatus) !=Sysfile::NS_NotDefined){
      sprintf(buf, 
	      "Node %.2d -- %s GCP: %d, NodeGroup: %d, TakeOverNode: %d, "
	      "LCP Ongoing: %s",
	      i, 
	      getNSString(Sysfile::getNodeStatus(i,sysfile->nodeStatus)),
	      sysfile->lastCompletedGCI[i],
	      Sysfile::getNodeGroup(i, sysfile->nodeGroups),
	      Sysfile::getTakeOverNode(i, sysfile->takeOver),
	      BitmaskImpl::get(NdbNodeBitmask::Size, 
			       sysfile->lcpActive, i) != 0 ? "yes" : "no");
      ndbout << buf << endl;
    }
  }
}

NDB_COMMAND(printSysfile, 
	    "printSysfile", "printSysfile", "Prints a sysfile", 16384){ 
  if(argc < 2){
    usage(argv[0]);
    return 0;
  }

  for(int i = 1; i<argc; i++){
    const char * filename = argv[i];
    
    struct stat sbuf;
    const int res = stat(filename, &sbuf);
    if(res != 0){
      ndbout << "Could not find file: \"" << filename << "\"" << endl;
      continue;
    }
    const Uint32 bytes = sbuf.st_size;
    
    Uint32 * buf = new Uint32[bytes/4+1];
    
    FILE * f = fopen(filename, "rb");
    if(f == 0){
      ndbout << "Failed to open file" << endl;
      delete [] buf;
      continue;
    }
    Uint32 sz = fread(buf, 1, bytes, f);
    fclose(f);
    if(sz != bytes){
      ndbout << "Failure while reading file" << endl;
      delete [] buf;
      continue;
    }
    
    print(filename, (Sysfile *)&buf[0]);
    delete [] buf;
    continue;
  }
  return 0;
}
