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


#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>
#include <HugoTransactions.hpp>
#include <getarg.h>


int 
main(int argc, const char** argv){
  ndb_init();

  
  int _help = 0;
  const char* db = 0;

  struct getargs args[] = {
    { "database", 'd', arg_string, &db, "Database", "" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0, i;
  char desc[] = 
    "<tabname>+ \nThis program listen to events on specified tables\n";
  
  if(getarg(args, num_args, argc, argv, &optind) ||
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  // Connect to Ndb
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb( &con, db ? db : "TEST_DB" );

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  int result = 0;
  Uint64 last_gci= 0, cnt= 0;
  
  NdbDictionary::Dictionary *myDict = MyNdb.getDictionary();
  Vector<NdbDictionary::Event*> events;
  Vector<NdbEventOperation*> event_ops;
  for(i= optind; i<argc; i++)
  {
    const NdbDictionary::Table* table= myDict->getTable(argv[i]);
    if(!table)
    {
      ndbout_c("Could not find table: %s, skipping", argv[i]);
      continue;
    }

    BaseString name;
    name.appfmt("EV-%s", argv[i]);
    NdbDictionary::Event *myEvent= new NdbDictionary::Event(name.c_str());
    myEvent->setTable(table->getName());
    myEvent->addTableEvent(NdbDictionary::Event::TE_ALL); 
    for(int a = 0; a < table->getNoOfColumns(); a++){
      myEvent->addEventColumn(a);
    }

    if (myDict->createEvent(* myEvent))
    {
      if(myDict->getNdbError().classification == NdbError::SchemaObjectExists) 
      {
	g_info << "Event creation failed event exists\n";
	if (myDict->dropEvent(name.c_str()))
	{
	  g_err << "Failed to drop event: " << myDict->getNdbError() << endl;
	  result = 1;
	  goto end;
	}
	// try again
	if (myDict->createEvent(* myEvent)) 
	{
	  g_err << "Failed to create event: " << myDict->getNdbError() << endl;
	  result = 1;
	  goto end;
	}
      }
      else
      {
	g_err << "Failed to create event: " << myDict->getNdbError() << endl;
	result = 1;
	goto end;
      }
    }
    
    events.push_back(myEvent);

    NdbEventOperation* pOp = MyNdb.createEventOperation(name.c_str());
    if ( pOp == NULL ) {
      g_err << "Event operation creation failed" << endl;
      result = 1;
      goto end;
    }

    for (int a = 0; a < table->getNoOfColumns(); a++) 
    {
      pOp->getValue(table->getColumn(a)->getName());
      pOp->getPreValue(table->getColumn(a)->getName());
    }
    event_ops.push_back(pOp);
  }

  for(i= 0; i<(int)event_ops.size(); i++)
  {
    if (event_ops[i]->execute())
    { 
      g_err << "operation execution failed: " << event_ops[i]->getNdbError()
	    << endl;
      result = 1;
      goto end;
    }
  }

  while(true)
  {
    while(MyNdb.pollEvents(100) == 0);
    
    NdbEventOperation* pOp;
    while((pOp= MyNdb.nextEvent()) != 0)
    {
      if(pOp->getGCI() != last_gci)
      {
	if(cnt) ndbout_c("GCI: %lld events: %lld", last_gci, cnt);
	cnt= 1;
	last_gci= pOp->getGCI();
      }
      else
      {
	cnt++;
      }
    }
  }
end:
  return NDBT_ProgramExit(NDBT_OK);
}

template class Vector<NdbDictionary::Event*>;
template class Vector<NdbEventOperation*>;
