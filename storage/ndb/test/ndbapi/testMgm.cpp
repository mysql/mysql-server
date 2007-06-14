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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include <random.h>
#include <mgmapi.h>
#include <mgmapi_debug.h>
#include <ndb_logevent.h>
#include <InputStream.hpp>
#include <signaldata/EventReport.hpp>

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


int create_index_on_pk(Ndb* pNdb, const char* tabName){
  int result  = NDBT_OK;

  const NdbDictionary::Table * tab = NDBT_Table::discoverTableFromDb(pNdb,
								     tabName);

  // Create index      
  const char* idxName = "IDX_ON_PK";
  ndbout << "Create: " <<idxName << "( ";
  NdbDictionary::Index pIdx(idxName);
  pIdx.setTable(tabName);
  pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c< tab->getNoOfPrimaryKeys(); c++){    
    pIdx.addIndexColumn(tab->getPrimaryKey(c));
    ndbout << tab->getPrimaryKey(c)<<" ";
  }
  
  ndbout << ") ";
  if (pNdb->getDictionary()->createIndex(pIdx) != 0){
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    ERR(err);
    result = NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  return result;
}

int drop_index_on_pk(Ndb* pNdb, const char* tabName){
  int result = NDBT_OK;
  const char* idxName = "IDX_ON_PK";
  ndbout << "Drop: " << idxName;
  if (pNdb->getDictionary()->dropIndex(idxName, tabName) != 0){
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    ERR(err);
    result = NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  return result;
}


#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int runTestSingleUserMode(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  NdbRestarter restarter;
  char tabName[255];
  strncpy(tabName, ctx->getTab()->getName(), 255);
  ndbout << "tabName="<<tabName<<endl;
  
  int i = 0;
  int count;
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (i<loops && result == NDBT_OK) {
    g_info << i << ": ";
    int timeout = 120;
    // Test that the single user mode api can do everything     
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0); 
    CHECK(hugoTrans.loadTable(pNdb, records, 128) == 0);
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));
    CHECK(utilTrans.clearTable(pNdb, records/2) == 0);
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0); 

    // Test create index in single user mode
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0); 
    CHECK(create_index_on_pk(pNdb, tabName) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 128) == 0);
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(drop_index_on_pk(pNdb, tabName) == 0);	  
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0); 

    // Test recreate index in single user mode
    CHECK(create_index_on_pk(pNdb, tabName) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 128) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0); 
    CHECK(drop_index_on_pk(pNdb, tabName) == 0);	
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(create_index_on_pk(pNdb, tabName) == 0);
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0); 
    CHECK(drop_index_on_pk(pNdb, tabName) == 0);

    CHECK(utilTrans.clearTable(GETNDB(step),  records) == 0);

    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    i++;

  }
  return result;
}

int runTestApiSession(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  Uint64 session_id= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);
  ndb_mgm_connect(h,0,0,0);
  int s= ndb_mgm_get_fd(h);
  session_id= ndb_mgm_get_session_id(h);
  ndbout << "MGM Session id: " << session_id << endl;
  write(s,"get",3);
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  struct NdbMgmSession sess;
  int slen= sizeof(struct NdbMgmSession);

  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);
  ndb_mgm_connect(h,0,0,0);

  NdbSleep_SecSleep(1);

  if(ndb_mgm_get_session(h,session_id,&sess,&slen))
  {
    ndbout << "Failed, session still exists" << endl;
    ndb_mgm_disconnect(h);
    ndb_mgm_destroy_handle(&h);
    return NDBT_FAILED;
  }
  else
  {
    ndbout << "SUCCESS: session is gone" << endl;
    ndb_mgm_disconnect(h);
    ndb_mgm_destroy_handle(&h);
    return NDBT_OK;
  }
}

int runTestApiConnectTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  int result= NDBT_FAILED;
  int cc= 0;
  int mgmd_nodeid= 0;
  ndb_mgm_reply reply;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  ndbout << "TEST connect timeout" << endl;

  ndb_mgm_set_timeout(h, 3000);

  struct timeval tstart, tend;
  int secs;
  timerclear(&tstart);
  timerclear(&tend);
  gettimeofday(&tstart,NULL);

  ndb_mgm_connect(h,0,0,0);

  gettimeofday(&tend,NULL);

  secs= tend.tv_sec - tstart.tv_sec;
  ndbout << "Took about: " << secs <<" seconds"<<endl;

  if(secs < 4)
    result= NDBT_OK;
  else
    goto done;

  ndb_mgm_set_connectstring(h, mgm);

  ndbout << "TEST connect timeout" << endl;

  ndb_mgm_destroy_handle(&h);

  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, "1.1.1.1");

  ndbout << "TEST connect timeout (invalid host)" << endl;

  ndb_mgm_set_timeout(h, 3000);

  timerclear(&tstart);
  timerclear(&tend);
  gettimeofday(&tstart,NULL);

  ndb_mgm_connect(h,0,0,0);

  gettimeofday(&tend,NULL);

  secs= tend.tv_sec - tstart.tv_sec;
  ndbout << "Took about: " << secs <<" seconds"<<endl;

  if(secs < 4)
    result= NDBT_OK;
  else
    result= NDBT_FAILED;

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestApiTimeoutBasic(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  int result= NDBT_FAILED;
  int cc= 0;
  int mgmd_nodeid= 0;
  ndb_mgm_reply reply;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  ndbout << "TEST timout check_connection" << endl;
  int errs[] = { 1, 2, 3, -1};

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndbout << "trying error " << error_ins << endl;
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_set_timeout(h,2500);

    cc= ndb_mgm_check_connection(h);
    if(cc < 0)
      result= NDBT_OK;
    else
      result= NDBT_FAILED;

    if(ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: still connected" << endl;
      result= NDBT_FAILED;
    }
  }

  ndbout << "TEST get_mgmd_nodeid" << endl;
  ndb_mgm_connect(h,0,0,0);

  if(ndb_mgm_insert_error(h, mgmd_nodeid, 0, &reply)< 0)
  {
    ndbout << "failed to remove inserted error " << endl;
    result= NDBT_FAILED;
    goto done;
  }

  cc= ndb_mgm_get_mgmd_nodeid(h);
  ndbout << "got node id: " << cc << endl;
  if(cc==0)
  {
    ndbout << "FAILED: didn't get node id" << endl;
    result= NDBT_FAILED;
  }
  else
    result= NDBT_OK;

  ndbout << "TEST end_session" << endl;
  ndb_mgm_connect(h,0,0,0);

  if(ndb_mgm_insert_error(h, mgmd_nodeid, 4, &reply)< 0)
  {
    ndbout << "FAILED: insert error 1" << endl;
    result= NDBT_FAILED;
    goto done;
  }

  cc= ndb_mgm_end_session(h);
  if(cc==0)
  {
    ndbout << "FAILED: success in calling end_session" << endl;
    result= NDBT_FAILED;
  }
  else if(ndb_mgm_get_latest_error(h)!=ETIMEDOUT)
  {
    ndbout << "FAILED: Incorrect error code (" << ndb_mgm_get_latest_error(h)
           << " != expected " << ETIMEDOUT << ") desc: "
           << ndb_mgm_get_latest_error_desc(h)
           << " line: " << ndb_mgm_get_latest_error_line(h)
           << " msg: " << ndb_mgm_get_latest_error_msg(h)
           << endl;
    result= NDBT_FAILED;
  }
  else
    result= NDBT_OK;

  if(ndb_mgm_is_connected(h))
  {
    ndbout << "FAILED: is still connected after error" << endl;
    result= NDBT_FAILED;
  }
done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestApiGetStatusTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  int result= NDBT_OK;
  int cc= 0;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  int errs[] = { 0, 5, 6, 7, 8, 9, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    struct ndb_mgm_cluster_state *cl= ndb_mgm_get_status(h);

    if(cl!=NULL)
      free(cl);

    /*
     * For whatever strange reason,
     * get_status is okay with not having the last enter there.
     * instead of "fixing" the api, let's have a special case
     * so we don't break any behaviour
     */

    if(error_ins!=0 && error_ins!=9 && cl!=NULL)
    {
      ndbout << "FAILED: got a ndb_mgm_cluster_state back" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && error_ins!=9 && ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && error_ins!=9 && ndb_mgm_get_latest_error(h)!=ETIMEDOUT)
    {
      ndbout << "FAILED: Incorrect error code (" << ndb_mgm_get_latest_error(h)
             << " != expected " << ETIMEDOUT << ") desc: "
             << ndb_mgm_get_latest_error_desc(h)
             << " line: " << ndb_mgm_get_latest_error_line(h)
             << " msg: " << ndb_mgm_get_latest_error_msg(h)
             << endl;
      result= NDBT_FAILED;
    }
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestMgmApiGetConfigTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  int errs[] = { 0, 1, 2, 3, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    struct ndb_mgm_configuration *c= ndb_mgm_get_configuration(h,0);

    if(c!=NULL)
      free(c);

    if(error_ins!=0 && c!=NULL)
    {
      ndbout << "FAILED: got a ndb_mgm_configuration back" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    if(error_ins!=0 && ndb_mgm_get_latest_error(h)!=ETIMEDOUT)
    {
      ndbout << "FAILED: Incorrect error code (" << ndb_mgm_get_latest_error(h)
             << " != expected " << ETIMEDOUT << ") desc: "
             << ndb_mgm_get_latest_error_desc(h)
             << " line: " << ndb_mgm_get_latest_error_line(h)
             << " msg: " << ndb_mgm_get_latest_error_msg(h)
             << endl;
      result= NDBT_FAILED;
    }
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestMgmApiEventTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  int errs[] = { 10000, 0, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                     1, NDB_MGM_EVENT_CATEGORY_STARTUP,
                     0 };
    int fd= ndb_mgm_listen_event(h, filter);

    if(fd==NDB_INVALID_SOCKET)
    {
      ndbout << "FAILED: could not listen to event" << endl;
      result= NDBT_FAILED;
    }

    Uint32 theData[25];
    EventReport *fake_event = (EventReport*)theData;
    fake_event->setEventType(NDB_LE_NDBStopForced);
    fake_event->setNodeId(42);
    theData[2]= 0;
    theData[3]= 0;
    theData[4]= 0;
    theData[5]= 0;

    ndb_mgm_report_event(h, theData, 6);

    char *tmp= 0;
    char buf[512];
    SocketInputStream in(fd,2000);
    for(int i=0; i<20; i++)
    {
      if((tmp = in.gets(buf, sizeof(buf))))
      {
//        const char ping_token[]="<PING>";
//        if(memcmp(ping_token,tmp,sizeof(ping_token)-1))
          if(tmp && strlen(tmp))
            ndbout << tmp;
      }
      else
      {
        if(in.timedout())
        {
          ndbout << "TIMED OUT READING EVENT at iteration " << i << endl;
          break;
        }
      }
    }

    /*
     * events go through a *DIFFERENT* socket than the NdbMgmHandle
     * so we should still be connected (and be able to check_connection)
     *
     */

    if(ndb_mgm_check_connection(h) && !ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    ndb_mgm_disconnect(h);
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

int runTestMgmApiStructEventTimeout(NDBT_Context* ctx, NDBT_Step* step)
{
  char *mgm= ctx->getRemoteMgm();
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgm);

  int errs[] = { 10000, 0, -1 };

  for(int error_ins_no=0; errs[error_ins_no]!=-1; error_ins_no++)
  {
    int error_ins= errs[error_ins_no];
    ndb_mgm_connect(h,0,0,0);

    if(ndb_mgm_check_connection(h) < 0)
    {
      result= NDBT_FAILED;
      goto done;
    }

    mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
    if(mgmd_nodeid==0)
    {
      ndbout << "Failed to get mgmd node id to insert error" << endl;
      result= NDBT_FAILED;
      goto done;
    }

    ndb_mgm_reply reply;
    reply.return_code= 0;

    if(ndb_mgm_insert_error(h, mgmd_nodeid, error_ins, &reply)< 0)
    {
      ndbout << "failed to insert error " << error_ins << endl;
      result= NDBT_FAILED;
    }

    ndbout << "trying error: " << error_ins << endl;

    ndb_mgm_set_timeout(h,2500);

    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                     1, NDB_MGM_EVENT_CATEGORY_STARTUP,
                     0 };
    NdbLogEventHandle le_handle= ndb_mgm_create_logevent_handle(h, filter);

    struct ndb_logevent le;
    for(int i=0; i<20; i++)
    {
      if(error_ins==0 || (error_ins!=0 && i<5))
      {
        Uint32 theData[25];
        EventReport *fake_event = (EventReport*)theData;
        fake_event->setEventType(NDB_LE_NDBStopForced);
        fake_event->setNodeId(42);
        theData[2]= 0;
        theData[3]= 0;
        theData[4]= 0;
        theData[5]= 0;

        ndb_mgm_report_event(h, theData, 6);
      }
      int r= ndb_logevent_get_next(le_handle, &le, 2500);
      if(r>0)
      {
        ndbout << "Receieved event" << endl;
      }
      else if(r<0)
      {
        ndbout << "ERROR" << endl;
      }
      else // no event
      {
        ndbout << "TIMED OUT READING EVENT at iteration " << i << endl;
        if(error_ins==0)
          result= NDBT_FAILED;
        else
          result= NDBT_OK;
        break;
      }
    }

    /*
     * events go through a *DIFFERENT* socket than the NdbMgmHandle
     * so we should still be connected (and be able to check_connection)
     *
     */

    if(ndb_mgm_check_connection(h) && !ndb_mgm_is_connected(h))
    {
      ndbout << "FAILED: is still connected after error" << endl;
      result= NDBT_FAILED;
    }

    ndb_mgm_disconnect(h);
  }

done:
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return result;
}

NDBT_TESTSUITE(testMgm);
TESTCASE("SingleUserMode", 
	 "Test single user mode"){
  INITIALIZER(runTestSingleUserMode);
  FINALIZER(runClearTable);
}
TESTCASE("ApiSessionFailure",
	 "Test failures in MGMAPI session"){
  INITIALIZER(runTestApiSession);

}
TESTCASE("ApiConnectTimeout",
	 "Connect timeout tests for MGMAPI"){
  INITIALIZER(runTestApiConnectTimeout);

}
TESTCASE("ApiTimeoutBasic",
	 "Basic timeout tests for MGMAPI"){
  INITIALIZER(runTestApiTimeoutBasic);

}
TESTCASE("ApiGetStatusTimeout",
	 "Test timeout for MGMAPI getStatus"){
  INITIALIZER(runTestApiGetStatusTimeout);

}
TESTCASE("ApiGetConfigTimeout",
	 "Test timeouts for mgmapi get_configuration"){
  INITIALIZER(runTestMgmApiGetConfigTimeout);

}
TESTCASE("ApiMgmEventTimeout",
	 "Test timeouts for mgmapi get_configuration"){
  INITIALIZER(runTestMgmApiEventTimeout);

}
TESTCASE("ApiMgmStructEventTimeout",
	 "Test timeouts for mgmapi get_configuration"){
  INITIALIZER(runTestMgmApiStructEventTimeout);

}
NDBT_TESTSUITE_END(testMgm);

int main(int argc, const char** argv){
  ndb_init();
  myRandom48Init(NdbTick_CurrentMillisecond());
  return testMgm.execute(argc, argv);
}

