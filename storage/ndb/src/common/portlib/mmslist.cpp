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

#include <ndb_common.h>

#include <NdbOut.hpp>
#include <NdbMain.h>

#include <ose.h>
#include <mms.sig>
#include <mms_err.h>
#include <NdbOut.hpp>

/**
 * NOTE: To use NdbMem from a OSE system ose_mms has to be defined 
 * as a "Required External Process"(see OSE Kernel User's Guide/R1.1(p. 148)),
 * like this:
 * EXT_PROC(ose_mms, ose_mms, 50000)
 * This will create a global variable ose_mms_ that is used from here.
 */

union SIGNAL
{
  SIGSELECT                       sigNo;
  struct MmsListDomainRequest     mmsListDomainRequest;
  struct MmsListDomainReply       mmsListDomainReply;
}; /* union SIGNAL */

extern PROCESS ose_mms_;

struct ARegion
{
   unsigned long int address;
   unsigned long int size;
   char name[32];

   U32         resident;            /* Boolean, nonzero if resident. */
   U32         access;              /* See values for AccessType (above) .*/
   U32         type;                /* either RAM-mem (1) or Io-mem (2) */
   U32         cache;               /* 0-copyback,1-writethrough, 2-CacheInhibit.*/
};

NDB_COMMAND(mmslist, "mmslist", "mmslist", "LIst the MMS memory segments", 4096){
  if (argc == 1){

  static SIGSELECT   allocate_sig[]  = {1,MMS_LIST_DOMAIN_REPLY};
  union SIGNAL           *sig;

  /* Send request to list all segments and regions. */
  sig = alloc(sizeof(struct MmsListDomainRequest),
              MMS_LIST_DOMAIN_REQUEST);
  send(&sig, ose_mms_);

  while (true){
    sig = receive(allocate_sig);    
    if (sig != NIL){
      if (sig->mmsListDomainReply.status == MMS_SUCCESS){
	/* Print domain info */
	ndbout << "=================================" << endl;
	ndbout << "domain: " << sig->mmsListDomainReply.domain << endl;
	ndbout << "name  : " << sig->mmsListDomainReply.name << endl;
	ndbout << "used  : " << sig->mmsListDomainReply.used << endl;
	ndbout << "lock  : " << sig->mmsListDomainReply.lock << endl;
	ndbout << "numOfRegions:" << sig->mmsListDomainReply.numOfRegions << endl;
	struct ARegion * tmp = (struct ARegion*)&sig->mmsListDomainReply.regions[0];
	for (int i = 0; i < sig->mmsListDomainReply.numOfRegions && i < 256; i++){	  
	  ndbout << i << ": adress=" << tmp->address <<
            ", size=" << tmp->size <<
	    ", name=" <<  tmp->name << 
	    ", resident=" <<  tmp->resident << 
	    ", access=" <<  tmp->access << 
	    ", type=" <<  tmp->type << 
            ", cache=" << tmp->cache << endl;
	  tmp++;
	}

	free_buf(&sig);
      }else{
	free_buf(&sig);
	break;
      }
    }
    
  }

  }else{
    ndbout << "Usage: mmslist" << endl;
  }
  return NULL;
}
