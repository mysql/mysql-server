/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include "NdbMgmd.hpp"
#include <mgmapi.h>
#include <mgmapi_debug.h>
#include "../../src/mgmapi/mgmapi_internal.h"
#include <InputStream.hpp>
#include <signaldata/EventReport.hpp>
#include <NdbRestarter.hpp>
#include <random.h>

/*
  Tests that only need the mgmd(s) started

  Start ndb_mgmd and set NDB_CONNECTSTRING pointing
  to that/those ndb_mgmd(s), then run testMgm
 */


int runTestApiSession(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  Uint64 session_id= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());
  ndb_mgm_connect(h,0,0,0);
  ndb_native_socket_t s = ndb_mgm_get_fd(h);
  session_id= ndb_mgm_get_session_id(h);
  ndbout << "MGM Session id: " << session_id << endl;
  send(s,"get",3,0);
  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  struct NdbMgmSession sess;
  int slen= sizeof(struct NdbMgmSession);

  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());
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
  NdbMgmd mgmd;

  g_info << "Check connect works with timeout 3000" << endl;
  if (!mgmd.set_timeout(3000))
    return NDBT_FAILED;

  if (!mgmd.connect(0, 0, 0))
  {
    g_err << "Connect failed with timeout 3000" << endl;
    return NDBT_FAILED;
  }

  if (!mgmd.disconnect())
    return NDBT_FAILED;

  g_info << "Check connect to illegal host will timeout after 3000" << endl;
  if (!mgmd.set_timeout(3000))
    return NDBT_FAILED;
  mgmd.setConnectString("1.1.1.1");

  const Uint64 tstart= NdbTick_CurrentMillisecond();
  if (mgmd.connect(0, 0, 0))
  {
    g_err << "Connect to illegal host suceeded" << endl;
    return NDBT_FAILED;
  }

  const Uint64 msecs= NdbTick_CurrentMillisecond() - tstart;
  ndbout << "Took about " << msecs <<" milliseconds"<<endl;

  if(msecs > 6000)
  {
    g_err << "The connect to illegal host timedout after much longer "
          << "time than was expected, expected <= 6000, got " << msecs << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


int runTestApiTimeoutBasic(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int result= NDBT_FAILED;
  int cc= 0;
  int mgmd_nodeid= 0;
  ndb_mgm_reply reply;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

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
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

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
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

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
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

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

    ndb_native_socket_t fd= ndb_mgm_listen_event(h, filter);
    ndb_socket_t my_fd = ndb_socket_create_from_native(fd);

    if(!ndb_socket_valid(my_fd))
    {
      ndbout << "FAILED: could not listen to event" << endl;
      result= NDBT_FAILED;
    }

    union {
      Uint32 theData[25];
      EventReport repData;
    };
    EventReport *fake_event = &repData;
    fake_event->setEventType(NDB_LE_NDBStopForced);
    fake_event->setNodeId(42);
    theData[2]= 0;
    theData[3]= 0;
    theData[4]= 0;
    theData[5]= 0;

    ndb_mgm_report_event(h, theData, 6);

    char *tmp= 0;
    char buf[512];

    SocketInputStream in(my_fd,2000);
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
  NdbMgmd mgmd;
  int result= NDBT_OK;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

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
        union {
	  Uint32 theData[25];
	  EventReport repData;
	};
        EventReport *fake_event = &repData;
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

int runTestMgmApiReadErrorRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int mgmd_nodeid= 0;

  NdbMgmHandle h;
  h= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(h, mgmd.getConnectString());

  ndb_mgm_connect(h,0,0,0);

  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
                   0};

  NdbLogEventHandle le_handle= ndb_mgm_create_logevent_handle(h, filter);

  if(ndb_mgm_check_connection(h) < 0)
  {
    ndb_mgm_disconnect(h);
    ndb_mgm_destroy_handle(&h);

    return NDBT_FAILED;
  }

  mgmd_nodeid= ndb_mgm_get_mgmd_nodeid(h);
  if(mgmd_nodeid==0)
  {
    ndbout << "Failed to get mgmd node id" << endl;
    ndb_mgm_disconnect(h);
    ndb_mgm_destroy_handle(&h);

    return NDBT_FAILED;
  }

  ndb_mgm_reply reply;
  reply.return_code= 0;

  ndb_mgm_set_timeout(h,2500);

  struct ndb_logevent le;
  for(int i = 0; i < 100 ; i++)
  {
    union
    {
      Uint32 theData[25];
      EventReport repData;
    };
    EventReport *fake_event = &repData;
    fake_event->setEventType(NDB_LE_BackupAborted);

    fake_event->setNodeId(42);
    theData[2]= 0;
    theData[3]= 0;
    theData[4]= 0;
    theData[5]= 0;

    if(i <= 6 && i > 2)
    {
      if(ndb_mgm_report_event(h, theData, 6)) ndbout << "failed reporting event" << endl;
      ndbout << "Report event" << endl;
    }

    // Restart mgmd
    if(i == 10)
    {
      ndb_mgm_cluster_state *state = ndb_mgm_get_status(h);
      if(state == NULL)
      {
        ndbout_c("Could not get status");
      }
      int res = 0;
      int need_disconnect;
      const int list[]= {mgmd_nodeid};

      res = ndb_mgm_restart3(h, 1, list, false, false, false, &need_disconnect);

      if (res < 0)
      {
        ndbout << "Restart of NDB Cluster node(s) failed." << endl;
        return NDBT_FAILED;
      }

      ndbout << res << " NDB Cluster node(s) have restarted." << endl;

      if(need_disconnect)
      {
        ndbout << "Disconnecting to allow management server to restart."
               << endl << endl;
        ndb_mgm_disconnect(h);
      }
    }

    int r= ndb_logevent_get_next2(le_handle, &le, 2500);

    if(r > 0)
    {
      ndbout << "Received event of type: " << le.type << endl << endl;
    }
    else if(r < 0)
    {
      ndbout << "Error received: " << ndb_logevent_get_latest_error_msg(le_handle) << endl << endl;

      if(ndb_logevent_get_latest_error(le_handle) == NDB_LEH_READ_ERROR && i >= 10)
      {
        ndb_mgm_disconnect(h);
        ndb_mgm_destroy_handle(&h);

        return NDBT_OK;
      }
      else
      {
        ndbout << "FAILED: Unexpected error received" << endl;
        return NDBT_FAILED;
      }
    }
    else // no event
    {
      ndbout << "TIMED OUT READING EVENT at iteration " << i << endl << endl;
    }
  }

  /*
   * Should be disconnected.
   */
  if(!ndb_mgm_check_connection(h) || ndb_mgm_is_connected(h))
  {
    ndbout << "FAILED: is still connected after error" << endl;
  }

  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return NDBT_FAILED;
}

int runSetConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int loops= ctx->getNumLoops();
  for (int l= 0; l < loops; l++){
    g_info << l << ": ";

    struct ndb_mgm_configuration* conf=
      ndb_mgm_get_configuration(mgmd.handle(), 0);
    if (!conf)
    {
      g_err << "ndb_mgm_get_configuration failed, error: "
            << ndb_mgm_get_latest_error_msg(mgmd.handle()) << endl;
      return NDBT_FAILED;
    }

    int r= ndb_mgm_set_configuration(mgmd.handle(), conf);
    free(conf);

    if (r != 0)
    {
      g_err << "ndb_mgm_set_configuration failed, error: "
            << ndb_mgm_get_latest_error_msg(mgmd.handle()) << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}


int runSetConfigUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runSetConfig(ctx, step)) == NDBT_OK)
    ;
  return result;
}


int runGetConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int loops= ctx->getNumLoops();
  for (int l= 0; l < loops; l++){
    g_info << l << ": ";
    struct ndb_mgm_configuration* conf=
      ndb_mgm_get_configuration(mgmd.handle(), 0);
    if (!conf)
      return NDBT_FAILED;
    free(conf);
  }
  return NDBT_OK;
}


int runGetConfigUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runGetConfig(ctx, step)) == NDBT_OK)
    ;
  return result;
}


// Find a random node of a given type.

static bool
get_nodeid_of_type(NdbMgmd& mgmd, ndb_mgm_node_type type, int *nodeId)
{
  ndb_mgm_node_type
    node_types[2] = { type,
                      NDB_MGM_NODE_TYPE_UNKNOWN };

  ndb_mgm_cluster_state *cs = ndb_mgm_get_status2(mgmd.handle(), node_types);
  if (cs == NULL)
  {
    g_err << "ndb_mgm_get_status2 failed, error: "
            << ndb_mgm_get_latest_error(mgmd.handle()) << " "
            << ndb_mgm_get_latest_error_msg(mgmd.handle()) << endl;
    return false;
  }

  int noOfNodes = cs->no_of_nodes;
  int randomnode = myRandom48(noOfNodes);
  ndb_mgm_node_state *ns = cs->node_states + randomnode;
  require((Uint32)ns->node_type == (Uint32)type);
  require(ns->node_id != 0);

  *nodeId = ns->node_id;
  g_info << "Got node id " << *nodeId << " of type " << type << endl;

  free(cs);
  return true;
}


// Ensure getting config from an illegal node fails.
// Return true in that case.

static bool
get_config_from_illegal_node(NdbMgmd& mgmd, int nodeId)
{
  struct ndb_mgm_configuration* conf=
      ndb_mgm_get_configuration_from_node(mgmd.handle(), nodeId);

  // Get conf from an illegal node should fail.
  if (ndb_mgm_get_latest_error(mgmd.handle()) != NDB_MGM_GET_CONFIG_FAILED)
  {
      g_err << "ndb_mgm_get_configuration from illegal node "
            << nodeId << " not failed, error: "
            << ndb_mgm_get_latest_error(mgmd.handle()) << " "
            << ndb_mgm_get_latest_error_msg(mgmd.handle()) << endl;
      return false;
  }

  if (conf)
  {
    // Should not get a conf from an illegal node.
    g_err << "ndb_mgm_get_configuration from illegal node: "
          << nodeId << ", error: "
          << ndb_mgm_get_latest_error(mgmd.handle()) << " "
          << ndb_mgm_get_latest_error_msg(mgmd.handle()) << endl;
    free(conf);
    return false;
  }
  return true;
}


// Check get_config from a non-existing node fails.

static bool
check_get_config_illegal_node(NdbMgmd& mgmd)
{
  // Find a node that does not exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  int nodeId = 0;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) != 0){
      nodeId = i;
      break;
    }
  }
  if (nodeId == 0)
    return true; // All nodes probably defined

  return get_config_from_illegal_node(mgmd, nodeId);
}



// Check get_config from a non-NDB/MGM node type fails

static bool
check_get_config_wrong_type(NdbMgmd& mgmd)
{
  int myChoice = myRandom48(2);
  ndb_mgm_node_type randomAllowedType = (myChoice) ?
                                        NDB_MGM_NODE_TYPE_API :
                                        NDB_MGM_NODE_TYPE_MGM;
  int nodeId = 0;
  if (get_nodeid_of_type(mgmd, randomAllowedType, &nodeId))
  {
    return get_config_from_illegal_node(mgmd, nodeId);
  }
  // No API/MGM nodes found.
  return true;
}

/* Find management node or a random data node, and get config from it.
 * Also ensure failure when getting config from
 * an illegal node (a non-NDB/MGM type, nodeid not defined,
 * or nodeid > MAX_NODES).
 */
int runGetConfigFromNode(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  if (!mgmd.connect())
    return NDBT_FAILED;

  if (!check_get_config_wrong_type(mgmd) ||
      !check_get_config_illegal_node(mgmd) ||
      !get_config_from_illegal_node(mgmd, MAX_NODES + 2))
  {
    return NDBT_FAILED;
  }

  int loops= ctx->getNumLoops();
  for (int l= 0; l < loops; l++)
  {
    /* Get config from a node of type: * NDB_MGM_NODE_TYPE_NDB
     */
    int nodeId = 0;
    if (get_nodeid_of_type(mgmd,  NDB_MGM_NODE_TYPE_NDB, &nodeId))
    {
      struct ndb_mgm_configuration* conf =
        ndb_mgm_get_configuration_from_node(mgmd.handle(), nodeId);
      if (!conf)
      {
        g_err << "ndb_mgm_get_configuration_from_node "
              << nodeId << " failed, error: "
              << ndb_mgm_get_latest_error(mgmd.handle()) << " "
              << ndb_mgm_get_latest_error_msg(mgmd.handle()) << endl;
        return NDBT_FAILED;
      }
      free(conf);
    }
    else
    {
      // ignore
    }
  }
  return NDBT_OK;
}


int runGetConfigFromNodeUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runGetConfigFromNode(ctx, step)) == NDBT_OK)
    ;
  return result;
}


int runTestStatus(NDBT_Context* ctx, NDBT_Step* step)
{
  ndb_mgm_node_type types[2] = {
    NDB_MGM_NODE_TYPE_NDB,
    NDB_MGM_NODE_TYPE_UNKNOWN
  };

  NdbMgmd mgmd;
  struct ndb_mgm_cluster_state *state;
  int iterations = ctx->getNumLoops();

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_OK;
  while (iterations-- != 0 && result == NDBT_OK)
  {
    state = ndb_mgm_get_status(mgmd.handle());
    if(state == NULL) {
      ndbout_c("Could not get status!");
      result= NDBT_FAILED;
      continue;
    }
    free(state);

    state = ndb_mgm_get_status2(mgmd.handle(), types);
    if(state == NULL){
      ndbout_c("Could not get status2!");
      result= NDBT_FAILED;
      continue;
    }
    free(state);

    state = ndb_mgm_get_status2(mgmd.handle(), 0);
    if(state == NULL){
      ndbout_c("Could not get status2 second time!");
      result= NDBT_FAILED;
      continue;
    }
    free(state);
  }
  return result;
}


int runTestStatusUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestStatus(ctx, step)) == NDBT_OK)
    ;
  return result;
}


static bool
get_nodeid(NdbMgmd& mgmd,
           const Properties& args,
           Properties& reply)
{
  // Fill in default values of other args
  Properties call_args(args);
  if (!call_args.contains("version"))
    call_args.put("version", 1);
  if (!call_args.contains("nodetype"))
    call_args.put("nodetype", 1);
  if (!call_args.contains("nodeid"))
    call_args.put("nodeid", 1);
  if (!call_args.contains("user"))
    call_args.put("user", "mysqld");
  if (!call_args.contains("password"))
    call_args.put("password", "mysqld");
  if (!call_args.contains("public key"))
  call_args.put("public key", "a public key");
  if (!call_args.contains("name"))
    call_args.put("name", "testMgm");
  if (!call_args.contains("log_event"))
    call_args.put("log_event", 1);
  if (!call_args.contains("timeout"))
    call_args.put("timeout", 100);

  if (!call_args.contains("endian"))
  {
    union { long l; char c[sizeof(long)]; } endian_check;
    endian_check.l = 1;
    call_args.put("endian", (endian_check.c[sizeof(long)-1])?"big":"little");
  }

  if (!mgmd.call("get nodeid", call_args,
                 "get nodeid reply", reply))
  {
    g_err << "get_nodeid: mgmd.call failed" << endl;
    return false;
  }

  // reply.print();
  return true;
}


static const char*
get_result(const Properties& reply)
{
  const char* result;
  if (!reply.get("result", &result)){
    ndbout_c("result: no 'result' found in reply");
    return NULL;
  }
  return result;
}


static bool result_contains(const Properties& reply,
                            const char* expected_result)
{
  BaseString result(get_result(reply));
  if (strstr(result.c_str(), expected_result) == NULL){
    ndbout_c("result_contains: result string '%s' "
             "didn't contain expected result '%s'",
             result.c_str(), expected_result);
    return false;
  }
  g_info << " result: " << result << endl;
  return true;
}


static bool ok(const Properties& reply)
{
  BaseString result(get_result(reply));
  if (result == "Ok")
    return true;
  return false;
}

static bool failed(const Properties& reply)
{
  BaseString result(get_result(reply));
  if (result == "Failed")
    return true;
  return false;
}

static const char*
get_message(const Properties& reply)
{
  const char* message;
  if (!reply.get("message", &message)){
    ndbout_c("message: no 'message' found in reply");
    return NULL;
  }
  return message;
}


static bool message_contains(const Properties& reply,
                            const char* expected_message)
{
  BaseString message(get_message(reply));
  if (strstr(message.c_str(), expected_message) == NULL){
    ndbout_c("message_contains: message string '%s' "
             "didn't contain expected message '%s'",
             message.c_str(), expected_message);
    return false;
  }
  g_info << " message: " << message << endl;
  return true;
}


static bool get_nodeid_result_contains(NdbMgmd& mgmd,
                                       const Properties& args,
                                       const char* expected_result)
{
  Properties reply;
  if (!get_nodeid(mgmd, args, reply))
    return false;
  return result_contains(reply, expected_result);
}



static bool
check_get_nodeid_invalid_endian1(NdbMgmd& mgmd)
{
  union { long l; char c[sizeof(long)]; } endian_check;
  endian_check.l = 1;
  Properties args;
  /* Set endian to opposite value */
  args.put("endian", (endian_check.c[sizeof(long)-1])?"little":"big");
  return get_nodeid_result_contains(mgmd, args,
                                    "Node does not have the same endian");
}


static bool
check_get_nodeid_invalid_endian2(NdbMgmd& mgmd)
{
  Properties args;
  /* Set endian to weird value */
  args.put("endian", "hepp");
  return get_nodeid_result_contains(mgmd, args,
                                    "Node does not have the same endian");
}


static bool
check_get_nodeid_invalid_nodetype1(NdbMgmd& mgmd)
{
  Properties args;
  args.put("nodetype", 37);
  return get_nodeid_result_contains(mgmd, args,
                                    "unknown nodetype 37");
}

static bool
check_get_nodeid_invalid_nodeid(NdbMgmd& mgmd)
{
  for (int nodeId = MAX_NODES; nodeId < MAX_NODES+2; nodeId++){
    g_info << "Testing invalid node " << nodeId << endl;;

    Properties args;
    args.put("nodeid", nodeId);
    BaseString expected;
    expected.assfmt("illegal nodeid %d", nodeId);
    if (!get_nodeid_result_contains(mgmd, args, expected.c_str()))
      return false;
  }
  return true;
}

static bool
check_get_nodeid_dynamic_nodeid(NdbMgmd& mgmd)
{
  bool result = true;
  Uint32 nodeId= 0; // Get dynamic node id
  for (int nodeType = NDB_MGM_NODE_TYPE_MIN;
       nodeType < NDB_MGM_NODE_TYPE_MAX; nodeType++){
    while(true)
    {
      g_info << "Testing dynamic nodeid " << nodeId
             << ", nodeType: " << nodeType << endl;

      Properties args;
      args.put("nodeid", nodeId);
      args.put("nodetype", nodeType);
      Properties reply;
      if (!get_nodeid(mgmd, args, reply))
        return false;

      /*
        Continue to get dynamic id's until
        an error "there is no more nodeid" occur
      */
      if (!ok(reply)){
        BaseString expected1;
        expected1.assfmt("No free node id found for %s",
                        NdbMgmd::NodeType(nodeType).c_str());
        BaseString expected2;
        expected2.assfmt("Connection done from wrong host");
        if (!(result_contains(reply, expected1.c_str()) ||
              result_contains(reply, expected2.c_str())))
          result= false; // Got wrong error message
        break;
      }
    }
  }
  return result;
}


static bool
check_get_nodeid_nonode(NdbMgmd& mgmd)
{
  // Find a node that does not exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId = 0;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) != 0){
      nodeId = i;
      break;
    }
  }
  if (nodeId == 0)
    return true; // All nodes probably defined

  g_info << "Testing nonexisting node " << nodeId << endl;;

  Properties args;
  args.put("nodeid", nodeId);
  BaseString expected;
  expected.assfmt("No node defined with id=%d", nodeId);
  return get_nodeid_result_contains(mgmd, args, expected.c_str());
}

#if 0
static bool
check_get_nodeid_nodeid1(NdbMgmd& mgmd)
{
  // Find a node that does exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId = 0;
  Uint32 nodeType = NDB_MGM_NODE_TYPE_UNKNOWN;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) == 0){
      nodeId = i;
      iter.get(CFG_TYPE_OF_SECTION, &nodeType);
      break;
    }
  }
  require(nodeId);
  require(nodeType != (Uint32)NDB_MGM_NODE_TYPE_UNKNOWN);

  Properties args, reply;
  args.put("nodeid",nodeId);
  args.put("nodetype",nodeType);
  if (!get_nodeid(mgmd, args, reply))
  {
    g_err << "check_get_nodeid_nodeid1: failed for "
          << "nodeid: " << nodeId << ", nodetype: " << nodeType << endl;
    return false;
  }
  reply.print();
  return ok(reply);
}
#endif

static bool
check_get_nodeid_wrong_nodetype(NdbMgmd& mgmd)
{
  // Find a node that does exist
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId = 0;
  Uint32 nodeType = NDB_MGM_NODE_TYPE_UNKNOWN;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) == 0){
      nodeId = i;
      iter.get(CFG_TYPE_OF_SECTION, &nodeType);
      break;
    }
  }
  require(nodeId);
  require(nodeType != (Uint32)NDB_MGM_NODE_TYPE_UNKNOWN);

  nodeType = (nodeType + 1) / NDB_MGM_NODE_TYPE_MAX;
  require((int)nodeType >= (int)NDB_MGM_NODE_TYPE_MIN &&
          (int)nodeType <= (int)NDB_MGM_NODE_TYPE_MAX);

  Properties args, reply;
  args.put("nodeid",nodeId);
  args.put("nodeid",nodeType);
  if (!get_nodeid(mgmd, args, reply))
  {
    g_err << "check_get_nodeid_nodeid1: failed for "
          << "nodeid: " << nodeId << ", nodetype: " << nodeType << endl;
    return false;
  }
  BaseString expected;
  expected.assfmt("Id %d configured as", nodeId);
  return result_contains(reply, expected.c_str());
}



int runTestGetNodeId(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_FAILED;
  if (
      check_get_nodeid_invalid_endian1(mgmd) &&
      check_get_nodeid_invalid_endian2(mgmd) &&
      check_get_nodeid_invalid_nodetype1(mgmd) &&
      check_get_nodeid_invalid_nodeid(mgmd) &&
      check_get_nodeid_dynamic_nodeid(mgmd) &&
      check_get_nodeid_nonode(mgmd) &&
//      check_get_nodeid_nodeid1(mgmd) &&
      check_get_nodeid_wrong_nodetype(mgmd) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}


int runTestGetNodeIdUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestGetNodeId(ctx, step)) == NDBT_OK)
    ;
  return result;
}


int runSleepAndStop(NDBT_Context* ctx, NDBT_Step* step)
{
  int counter= 3*ctx->getNumLoops();

  while(!ctx->isTestStopped() && counter--)
    NdbSleep_SecSleep(1);;
  ctx->stopTest();
  return NDBT_OK;
}


static bool
check_connection(NdbMgmd& mgmd)
{
  Properties args, reply;
  mgmd.verbose(false); // Verbose off
  bool result= mgmd.call("check connection", args,
                         "check connection reply", reply);
  mgmd.verbose(); // Verbose on
  return result;
}


static bool
check_transporter_connect(NdbMgmd& mgmd, const char * hello)
{
  SocketOutputStream out(mgmd.socket());

  // Call 'transporter connect'
  if (out.println("transporter connect\n"))
  {
    g_err << "Send failed" << endl;
    return false;
  }

  // Send the 'hello'
  g_info << "Client hello: '" << hello << "'" << endl;
  if (out.println("%s", hello))
  {
    g_err << "Send hello '" << hello << "' failed" << endl;
    return false;
  }

  // Should not be possible to read a reply now, socket
  // should have been closed
  if (check_connection(mgmd)){
    g_err << "not disconnected" << endl;
    return false;
  }

  // disconnect and connect again
  if (!mgmd.disconnect())
    return false;
  if (!mgmd.connect())
    return false;

  return true;
}


int runTestTransporterConnect(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result = NDBT_FAILED;
  if (
      // Junk hello strings
      check_transporter_connect(mgmd, "hello") &&
      check_transporter_connect(mgmd, "hello again") &&

      // "Blow" the buffer
      check_transporter_connect(mgmd, "string_longer_than_buf_1234567890") &&

      // Out of range nodeid
      check_transporter_connect(mgmd, "-1") &&
      check_transporter_connect(mgmd, "-2 2") &&
      check_transporter_connect(mgmd, "10000") &&
      check_transporter_connect(mgmd, "99999 8") &&

      // Valid nodeid, invalid transporter type
      // Valid nodeid and transporter type, state != CONNECTING
      // ^These are only possible to test by finding an existing
      //  NDB node that are not started and use its setting(s)

      true)
   result = NDBT_OK;

  return result;
}


static bool
show_config(NdbMgmd& mgmd,
            const Properties& args,
            Properties& reply)
{
  if (!mgmd.call("show config", args,
                 "show config reply", reply, NULL, false))
  {
    g_err << "show_config: mgmd.call failed" << endl;
    return false;
  }

  // reply.print();
  return true;
}


int runCheckConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  // Connect to any mgmd and get the config
  if (!mgmd.connect())
    return NDBT_FAILED;

  Properties args1;
  Properties config1;
  if (!show_config(mgmd, args1, config1))
    return NDBT_FAILED;

  // Get the binary config
  Config conf;
  if (!mgmd.get_config(conf))
    return NDBT_FAILED;

  // Extract list of connectstrings to each mgmd
  BaseString connectstring;
  conf.getConnectString(connectstring, ";");

  Vector<BaseString> mgmds;
  connectstring.split(mgmds, ";");

  // Connect to each mgmd and check
  // they all have the same config
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    NdbMgmd mgmd2;
    g_info << "Connecting to " << mgmds[i].c_str() << endl;
    if (!mgmd2.connect(mgmds[i].c_str()))
      return NDBT_FAILED;

    Properties args2;
    Properties config2;
    if (!show_config(mgmd, args2, config2))
      return NDBT_FAILED;

    // Compare config1 and config2 line by line
    Uint32 line = 1;
    const char* value1;
    const char* value2;
    while (true)
    {
      if (config1.get("line", line, &value1))
      {
        // config1 had line, so should config2
        if (config2.get("line", line, &value2))
        {
          // both configs had line, check they are equal
          if (strcmp(value1, value2) != 0)
          {
            g_err << "the value on line " << line << "didn't match!" << endl;
            g_err << "config1, value: " << value1 << endl;
            g_err << "config2, value: " << value2 << endl;
            return NDBT_FAILED;
          }
          // g_info << line << ": " << value1 << " = " << value2 << endl;
        }
        else
        {
          g_err << "config2 didn't have line " << line << "!" << endl;
          return NDBT_FAILED;
        }
      }
      else
      {
        // Make sure config2 does not have this line either and end loop
        if (config2.get("line", line, &value2))
        {
          g_err << "config2 had line " << line << " not in config1!" << endl;
          return NDBT_FAILED;
        }

        // End of loop
        g_info << "There was " << line << " lines in config" << endl;
        break;
      }
      line++;
    }
    if (line == 0)
    {
      g_err << "FAIL: config should have lines!" << endl;
      return NDBT_FAILED;
    }

    // Compare the binary config
    Config conf2;
    if (!mgmd.get_config(conf2))
      return NDBT_FAILED;

    if (!conf.equal(&conf2))
    {
      g_err << "The binary config was different! host: " << mgmds[i] << endl;
      return NDBT_FAILED;
    }

  }

  return NDBT_OK;
}


static bool
reload_config(NdbMgmd& mgmd,
              const Properties& args,
              Properties& reply)
{
  if (!mgmd.call("reload config", args,
                 "reload config reply", reply))
  {
    g_err << "reload config: mgmd.call failed" << endl;
    return false;
  }

  //reply.print();
  return true;
}


static bool reload_config_result_contains(NdbMgmd& mgmd,
                                          const Properties& args,
                                          const char* expected_result)
{
  Properties reply;
  if (!reload_config(mgmd, args, reply))
    return false;
  return result_contains(reply, expected_result);
}


static bool
check_reload_config_both_config_and_mycnf(NdbMgmd& mgmd)
{
  Properties args;
  // Send reload command with both config_filename and mycnf set
  args.put("config_filename", "some filename");
  args.put("mycnf", 1);
  return reload_config_result_contains(mgmd, args,
                                       "ERROR: Both mycnf and config_filename");
}


static bool
show_variables(NdbMgmd& mgmd, Properties& reply)
{
  if (!mgmd.call("show variables", "",
                 "show variables reply", reply))
  {
    g_err << "show_variables: mgmd.call failed" << endl;
    return false;
  }
  return true;
}


static bool
check_reload_config_invalid_config_filename(NdbMgmd& mgmd, bool mycnf)
{

  BaseString expected("Could not load configuration from 'nonexisting_file");
  if (mycnf)
  {
    // Differing error message if started from my.cnf
    expected.assign("Can't switch to use config.ini 'nonexisting_file' "
                    "when node was started from my.cnf");
  }

  Properties args;
  // Send reload command with an invalid config_filename
  args.put("config_filename", "nonexisting_file");
  return reload_config_result_contains(mgmd, args, expected.c_str());
}


int runTestReloadConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  Properties variables;
  if (!show_variables(mgmd, variables))
    return NDBT_FAILED;

  variables.print();

  const char* mycnf_str;
  if (!variables.get("mycnf", &mycnf_str))
    abort();
  bool uses_mycnf = (strcmp(mycnf_str, "yes") == 0);

  int result= NDBT_FAILED;
  if (
      check_reload_config_both_config_and_mycnf(mgmd) &&
      check_reload_config_invalid_config_filename(mgmd, uses_mycnf) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}


static bool
set_config(NdbMgmd& mgmd,
           const Properties& args,
           BaseString encoded_config,
           Properties& reply)
{

  // Fill in default values of other args
  Properties call_args(args);
  if (!call_args.contains("Content-Type"))
    call_args.put("Content-Type", "ndbconfig/octet-stream");
  if (!call_args.contains("Content-Transfer-Encoding"))
    call_args.put("Content-Transfer-Encoding", "base64");
  if (!call_args.contains("Content-Length"))
    call_args.put("Content-Length",
                  encoded_config.length() ? encoded_config.length() - 1 : 1);

  if (!mgmd.call("set config", call_args,
                 "set config reply", reply,
                 encoded_config.c_str()))
  {
    g_err << "set config: mgmd.call failed" << endl;
    return false;
  }

  //reply.print();
  return true;
}


static bool set_config_result_contains(NdbMgmd& mgmd,
                                       const Properties& args,
                                       const BaseString& encoded_config,
                                       const char* expected_result)
{
  Properties reply;
  if (!set_config(mgmd, args, encoded_config, reply))
    return false;
  return result_contains(reply, expected_result);
}


static bool set_config_result_contains(NdbMgmd& mgmd,
                                       const Config& conf,
                                       const char* expected_result)
{
  Properties reply;
  Properties args;

  BaseString encoded_config;
  if (!conf.pack64(encoded_config))
    return false;

  if (!set_config(mgmd, args, encoded_config, reply))
    return false;
  return result_contains(reply, expected_result);
}


static bool
check_set_config_invalid_content_type(NdbMgmd& mgmd)
{
  Properties args;
  args.put("Content-Type", "illegal type");
  return set_config_result_contains(mgmd, args, BaseString(""),
                                    "Unhandled content type 'illegal type'");
}

static bool
check_set_config_invalid_content_encoding(NdbMgmd& mgmd)
{
  Properties args;
  args.put("Content-Transfer-Encoding", "illegal encoding");
  return set_config_result_contains(mgmd, args, BaseString(""),
                                    "Unhandled content encoding "
                                    "'illegal encoding'");
}

static bool
check_set_config_too_large_content_length(NdbMgmd& mgmd)
{
  Properties args;
  args.put("Content-Length", 1024*1024 + 1);
  return set_config_result_contains(mgmd, args, BaseString(""),
                                    "Illegal config length size 1048577");
}

static bool
check_set_config_too_small_content_length(NdbMgmd& mgmd)
{
  Properties args;
  args.put("Content-Length", (Uint32)0);
  return set_config_result_contains(mgmd, args, BaseString(""),
                                    "Illegal config length size 0");
}

static bool
check_set_config_wrong_config_length(NdbMgmd& mgmd)
{

  // Get the binary config
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  BaseString encoded_config;
  if (!conf.pack64(encoded_config))
    return false;

  Properties args;
  args.put("Content-Length", encoded_config.length() - 20);
  bool res = set_config_result_contains(mgmd, args, encoded_config,
                                        "Failed to unpack config");

  if (res){
    /*
      There are now additional 20 bytes of junk that has been
      sent to mgmd, reconnect to get rid of it
    */
    if (!mgmd.disconnect())
      return false;
    if (!mgmd.connect())
       return false;
  }
  return res;
}

static bool
check_set_config_any_node(NDBT_Context* ctx, NDBT_Step* step, NdbMgmd& mgmd)
{

  // Get the binary config
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  // Extract list of connectstrings to each mgmd
  BaseString connectstring;
  conf.getConnectString(connectstring, ";");

  Vector<BaseString> mgmds;
  connectstring.split(mgmds, ";");

  // Connect to each mgmd and check
  // they all have the same config
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    NdbMgmd mgmd2;
    g_info << "Connecting to " << mgmds[i].c_str() << endl;
    if (!mgmd2.connect(mgmds[i].c_str()))
      return false;

    // Get the binary config
    Config conf2;
    if (!mgmd2.get_config(conf2))
      return false;

    // Set the modified config
    if (!mgmd2.set_config(conf2))
      return false;

    // Check that all mgmds now have the new config
    if (runCheckConfig(ctx, step) != NDBT_OK)
      return false;

  }

  return true;
}

static bool
check_set_config_fail_wrong_generation(NdbMgmd& mgmd)
{
  // Get the binary config
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  // Change generation
  if (!conf.setGeneration(conf.getGeneration() + 10))
    return false;

  // Set the modified config
  return set_config_result_contains(mgmd, conf,
                                    "Invalid generation in");
}

static bool
check_set_config_fail_wrong_name(NdbMgmd& mgmd)
{
  // Get the binary config
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  // Change name
  if (!conf.setName("NEWNAME"))
    return false;

  // Set the modified config
  return set_config_result_contains(mgmd, conf,
                                    "Invalid configuration name");
}

static bool
check_set_config_fail_wrong_primary(NdbMgmd& mgmd)
{
  // Get the binary config
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  // Change primary and thus make this configuration invalid
  if (!conf.setPrimaryMgmNode(conf.getPrimaryMgmNode()+10))
    return false;

  // Set the modified config
  return set_config_result_contains(mgmd, conf,
                                    "Not primary mgm node");
}

int runTestSetConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_FAILED;
  if (
      check_set_config_invalid_content_type(mgmd) &&
      check_set_config_invalid_content_encoding(mgmd) &&
      check_set_config_too_large_content_length(mgmd) &&
      check_set_config_too_small_content_length(mgmd) &&
      check_set_config_wrong_config_length(mgmd) &&
      check_set_config_any_node(ctx, step, mgmd) &&
      check_set_config_fail_wrong_generation(mgmd) &&
      check_set_config_fail_wrong_name(mgmd) &&
      check_set_config_fail_wrong_primary(mgmd) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}

int runTestSetConfigParallel(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int sucessful = 0;

  int invalid_generation = 0, config_change_ongoing = 0;

  /*
    continue looping until "loops" number of successful
    changes have been made from this thread
  */
  while (sucessful < loops &&
         !ctx->isTestStopped() &&
         result == NDBT_OK)
  {
    // Get the binary config
    Config conf;
    if (!mgmd.get_config(conf))
      return NDBT_FAILED;

    /* Set the config and check for valid errors */
    mgmd.verbose(false);
    if (mgmd.set_config(conf))
    {
      /* Config change suceeded */
      sucessful++;
    }
    else
    {
      /* Config change failed */
      if (mgmd.last_error() != NDB_MGM_CONFIG_CHANGE_FAILED)
      {
        g_err << "Config change failed with unexpected error: "
              << mgmd.last_error() << endl;
        result = NDBT_FAILED;
        continue;
      }

      BaseString error(mgmd.last_error_message());
      if (error == "Invalid generation in configuration")
        invalid_generation++;
      else
      if (error == "Config change ongoing")
        config_change_ongoing++;
      else
      {
        g_err << "Config change failed with unexpected error: '"
              << error << "'" << endl;
        result = NDBT_FAILED;

      }
    }
  }

  ndbout << "Thread " << step->getStepNo()
         << ", sucess: " << sucessful
         << ", ongoing: " << config_change_ongoing
         << ", invalid_generation: " << invalid_generation << endl;
  return result;
}

int runTestSetConfigParallelUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestSetConfigParallel(ctx, step)) == NDBT_OK)
    ;
  return result;
}



static bool
get_connection_parameter(NdbMgmd& mgmd,
                         const Properties& args,
                         Properties& reply)
{

  // Fill in default values of other args
  Properties call_args(args);
  if (!call_args.contains("node1"))
    call_args.put("node1", 1);
  if (!call_args.contains("node2"))
    call_args.put("node2", 1);
  if (!call_args.contains("param"))
    call_args.put("param", CFG_CONNECTION_SERVER_PORT);

  if (!mgmd.call("get connection parameter", call_args,
                 "get connection parameter reply", reply))
  {
    g_err << "get_connection_parameter: mgmd.call failed" << endl;
    return false;
  }
  return true;
}


static bool
set_connection_parameter(NdbMgmd& mgmd,
                         const Properties& args,
                         Properties& reply)
{

  // Fill in default values of other args
  Properties call_args(args);
  if (!call_args.contains("node1"))
    call_args.put("node1", 1);
  if (!call_args.contains("node2"))
    call_args.put("node2", 1);
  if (!call_args.contains("param"))
    call_args.put("param", CFG_CONNECTION_SERVER_PORT);
 if (!call_args.contains("value"))
    call_args.put("value", 37);

  if (!mgmd.call("set connection parameter", call_args,
                 "set connection parameter reply", reply))
  {
    g_err << "set_connection_parameter: mgmd.call failed" << endl;
    return false;
  }
  return true;
}


static bool
check_connection_parameter_invalid_nodeid(NdbMgmd& mgmd)
{
  for (int nodeId = MAX_NODES; nodeId < MAX_NODES+2; nodeId++){
    g_info << "Testing invalid node " << nodeId << endl;;

    Properties args;
    args.put("node1", nodeId);
    args.put("node2", nodeId);

    Properties get_result;
    if (!get_connection_parameter(mgmd, args, get_result))
      return false;

    if (!result_contains(get_result,
                         "Unable to find connection between nodes"))
        return false;

    Properties set_result;
    if (!set_connection_parameter(mgmd, args, set_result))
      return false;

    if (!failed(set_result))
        return false;

    if (!message_contains(set_result,
                          "Unable to find connection between nodes"))
        return false;
  }
  return true;
}


static bool
check_connection_parameter(NdbMgmd& mgmd)
{
  // Find a NDB node with dynamic port
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId1 = 0;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    Uint32 nodeType;
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) == 0 &&
        iter.get(CFG_TYPE_OF_SECTION, &nodeType) == 0 &&
        nodeType == NDB_MGM_NODE_TYPE_NDB){
      nodeId1 = i;
      break;
    }
  }

  NodeId otherNodeId = 0;
  BaseString original_value;

  // Get current value of first connection between mgmd and other node
  for (int nodeId = 1; nodeId < MAX_NODES; nodeId++){

    g_info << "Checking if connection between " << nodeId1
           << " and " << nodeId << " exists" << endl;

    Properties args;
    args.put("node1", nodeId1);
    args.put("node2", nodeId);

    Properties result;
    if (!get_connection_parameter(mgmd, args, result))
      return false;

    if (!ok(result))
      continue;

    result.print();
    // Get the nodeid
    otherNodeId = nodeId;

    // Get original value
    if (!result.get("value", original_value))
    {
      g_err << "Failed to get original value" << endl;
      return false;
    }
    break; // Done with the loop
  }

  if (otherNodeId == 0)
  {
    g_err << "Could not find a suitable connection for test" << endl;
    return false;
  }

  Properties get_args;
  get_args.put("node1", nodeId1);
  get_args.put("node2", otherNodeId);

  {
    g_info <<  "Set new value(37 by default)" << endl;

    Properties set_args(get_args);
    Properties set_result;
    if (!set_connection_parameter(mgmd, set_args, set_result))
      return false;

    if (!ok(set_result))
      return false;
  }

  {
    g_info << "Check new value" << endl;

    Properties get_result;
    if (!get_connection_parameter(mgmd, get_args, get_result))
      return false;

    if (!ok(get_result))
      return false;

    BaseString new_value;
    if (!get_result.get("value", new_value))
    {
      g_err << "Failed to get new value" << endl;
      return false;
    }

    g_info << "new_value: " << new_value << endl;
    if (new_value != "37")
    {
      g_err << "New value was not correct, expected 37, got "
            << new_value << endl;
      return false;
    }
  }

  {
    g_info << "Restore old value" << endl;

    Properties set_args(get_args);
    if (!set_args.put("value", original_value.c_str()))
    {
      g_err << "Failed to put original_value" << endl;
      return false;
    }

    Properties set_result;
    if (!set_connection_parameter(mgmd, set_args, set_result))
      return false;

    if (!ok(set_result))
      return false;
  }

  {
    g_info << "Check restored value" << endl;
    Properties get_result;
    if (!get_connection_parameter(mgmd, get_args, get_result))
      return false;

    if (!ok(get_result))
      return false;

    BaseString restored_value;
    if (!get_result.get("value", restored_value))
    {
      g_err << "Failed to get restored value" << endl;
      return false;
    }

    if (restored_value != original_value)
    {
      g_err << "Restored value was not correct, expected "
            << original_value << ", got "
            << restored_value << endl;
      return false;
    }
    g_info << "restored_value: " << restored_value << endl;
  }

  return true;

}


int runTestConnectionParameter(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_FAILED;
  if (
      check_connection_parameter(mgmd) &&
      check_connection_parameter_invalid_nodeid(mgmd) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}


int runTestConnectionParameterUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestConnectionParameter(ctx, step)) == NDBT_OK)
    ;

  return result;
}


static bool
set_ports(NdbMgmd& mgmd,
          const Properties& args, const char* bulk_arg,
          Properties& reply)
{
  if (!mgmd.call("set ports", args,
                 "set ports reply", reply, bulk_arg))
  {
    g_err << "set_ports: mgmd.call failed" << endl;
    return false;
  }
  return true;
}

static bool
check_set_ports_invalid_nodeid(NdbMgmd& mgmd)
{
  for (int nodeId = MAX_NODES; nodeId < MAX_NODES+2; nodeId++)
  {
    g_err << "Testing invalid node " << nodeId << endl;

    Properties args;
    args.put("node", nodeId);
    args.put("num_ports", 2);

    Properties set_result;
    if (!set_ports(mgmd, args, "", set_result))
      return false;

    if (ok(set_result))
      return false;

    if (!result_contains(set_result, "Illegal value for argument node"))
      return false;
  }
  return true;
}

static bool
check_set_ports_invalid_num_ports(NdbMgmd& mgmd)
{
  g_err << "Testing invalid number of ports "<< endl;

  Properties args;
  args.put("node", 1);
  args.put("num_ports", MAX_NODES + 37);

  Properties set_result;
  if (!set_ports(mgmd, args, "", set_result))
    return false;

  if (ok(set_result))
    return false;

  if (!result_contains(set_result, "Illegal value for argument num_ports"))
    return false;

  return true;
}



static bool
check_set_ports_invalid_mismatch_num_port_1(NdbMgmd& mgmd)
{
  g_err << "Testing invalid num port 1"<< endl;

  Properties args;
  args.put("node", 1);
  args.put("num_ports", 1);
  // Intend to send 1   ^ but passes two below

  Properties set_result;
  if (!set_ports(mgmd, args, "1=-37\n2=-38\n", set_result))
    return false;

  if (ok(set_result))
    return false;
  set_result.print();

  if (!result_contains(set_result, "expected empty line"))
    return false;

  return true;
}

static bool
check_set_ports_invalid_mismatch_num_port_2(NdbMgmd& mgmd)
{
  g_err << "Testing invalid num port 2"<< endl;

  Properties args;
  args.put("node", 1);
  args.put("num_ports", 2);
  // Intend to send 2   ^ but pass only one line below

  Properties set_result;
  if (!set_ports(mgmd, args, "1=-37\n", set_result))
    return false;

  if (ok(set_result))
    return false;
  set_result.print();

  if (!result_contains(set_result, "expected name=value pair"))
    return false;

  return true;
}


static bool
check_set_ports_invalid_port_list(NdbMgmd& mgmd)
{
  g_err << "Testing invalid port list"<< endl;

  Properties args;
  args.put("node", 1);
  // No connection from 1 -> 1 exist
  args.put("num_ports", 1);

  Properties set_result;
  if (!set_ports(mgmd, args, "1=-37\n", set_result))
    return false;
  set_result.print();

  if (ok(set_result))
    return false;

  if (!result_contains(set_result,
                       "Unable to find connection between nodes 1 -> 1"))
    return false;

  return true;
}

static bool check_mgmapi_err(NdbMgmd& mgmd,
                             int return_code,
                             int expected_error,
                             const char* expected_message)
{
  if (return_code != -1)
  {
    ndbout_c("check_mgmapi_error: unexpected return code: %d", return_code);
    return false;
  }
  if (mgmd.last_error() != expected_error)
  {
     ndbout_c("check_mgmapi_error: unexpected error code: %d "
              "expected %d", mgmd.last_error(), expected_error);
    return false;
  }
  if (strstr(mgmd.last_error_message(), expected_message) == NULL)
  {
    ndbout_c("check_mgmapi_error: last_error_message '%s' "
             "didn't contain expected message '%s'",
             mgmd.last_error_message(), expected_message);
    return false;
  }
  return true;

}

static bool
check_set_ports_mgmapi(NdbMgmd& mgmd)
{
  g_err << "Testing mgmapi"<< endl;

  int ret;
  int nodeid = 1;
  unsigned num_ports = 1;
  ndb_mgm_dynamic_port ports[MAX_NODES * 10];
  static_assert(MAX_NODES < NDB_ARRAY_SIZE(ports), "");
  ports[0].nodeid = 1;
  ports[0].port = -1;

  {
    ndbout_c("No handle");
    NdbMgmd no_handle;
    ret = ndb_mgm_set_dynamic_ports(no_handle.handle(), nodeid,
                                    ports, num_ports);
    if (ret != -1)
      return false;
  }
  {
    ndbout_c("Not connected");
    NdbMgmd no_con;
    no_con.verbose(false);
    if (no_con.connect("no_such_host:12345", 0, 1))
    {
      // Connect should not suceed!
      return false;
    }

    ret = ndb_mgm_set_dynamic_ports(no_con.handle(), nodeid,
                                    ports, num_ports);
    if (!check_mgmapi_err(no_con, ret, NDB_MGM_SERVER_NOT_CONNECTED, ""))
      return false;
  }

  ndbout_c("Invalid number of ports");
  num_ports = 0; // <<
  ret = ndb_mgm_set_dynamic_ports(mgmd.handle(), nodeid,
                                  ports, num_ports);
  if (!check_mgmapi_err(mgmd, ret, NDB_MGM_USAGE_ERROR,
                        "Illegal number of dynamic ports"))
    return false;

  ndbout_c("Invalid nodeid");
  nodeid = 0; // <<
  num_ports = 1;
  ret = ndb_mgm_set_dynamic_ports(mgmd.handle(), nodeid,
                                  ports, num_ports);
  if (!check_mgmapi_err(mgmd, ret, NDB_MGM_USAGE_ERROR,
                        "Illegal value for argument node: 0"))
    return false;

  ndbout_c("Invalid port in list");
  nodeid = 1;
  ports[0].nodeid = 1;
  ports[0].port = 1; // <<
  ret = ndb_mgm_set_dynamic_ports(mgmd.handle(), nodeid,
                                  ports, num_ports);
  if (!check_mgmapi_err(mgmd, ret, NDB_MGM_USAGE_ERROR,
                        "Illegal port specfied in ports array"))
    return false;


  ndbout_c("Invalid nodeid in list");
  nodeid = 1;
  ports[0].nodeid = 0; // <<
  ports[0].port = -11;
  ret = ndb_mgm_set_dynamic_ports(mgmd.handle(), nodeid,
                                  ports, num_ports);
  if (!check_mgmapi_err(mgmd, ret, NDB_MGM_USAGE_ERROR,
                        "Illegal nodeid specfied in ports array"))
    return false;

  ndbout_c("Max number of ports exceeded");
  nodeid = 1;
  num_ports = MAX_NODES; // <<
  for (unsigned i = 0; i < num_ports; i++)
  {
    ports[i].nodeid = i+1;
    ports[i].port = -37;
  }
  ret = ndb_mgm_set_dynamic_ports(mgmd.handle(), nodeid,
                                  ports, num_ports);
  if (!check_mgmapi_err(mgmd, ret, NDB_MGM_USAGE_ERROR,
                        "Illegal value for argument num_ports"))
    return false;

  ndbout_c("Many many ports");
  nodeid = 1;
  num_ports = NDB_ARRAY_SIZE(ports); // <<
  for (unsigned i = 0; i < num_ports; i++)
  {
    ports[i].nodeid = i+1;
    ports[i].port = -37;
  }
  ret = ndb_mgm_set_dynamic_ports(mgmd.handle(), nodeid,
                                  ports, num_ports);
  if (!check_mgmapi_err(mgmd, ret, NDB_MGM_USAGE_ERROR,
                        "Illegal value for argument num_ports"))
    return false;

  return true;
}

// Return name value pair of nodeid/ports which can be sent
// verbatim back to ndb_mgmd
static bool
get_all_ports(NdbMgmd& mgmd, Uint32 nodeId1, BaseString& values)
{
  for (int nodeId = 1; nodeId < MAX_NODES; nodeId++)
  {
    Properties args;
    args.put("node1", nodeId1);
    args.put("node2", nodeId);

    Properties result;
    if (!get_connection_parameter(mgmd, args, result))
      return false;

    if (!ok(result))
      continue;

    // Get value
    BaseString value;
    if (!result.get("value", value))
    {
      g_err << "Failed to get value" << endl;
      return false;
    }
    values.appfmt("%d=%s\n", nodeId, value.c_str());
  }
  return true;
}


static bool
check_set_ports(NdbMgmd& mgmd)
{
  // Find a NDB node with dynamic port
  Config conf;
  if (!mgmd.get_config(conf))
    return false;

  Uint32 nodeId1 = 0;
  for(Uint32 i= 1; i < MAX_NODES; i++){
    Uint32 nodeType;
    ConfigIter iter(&conf, CFG_SECTION_NODE);
    if (iter.find(CFG_NODE_ID, i) == 0 &&
        iter.get(CFG_TYPE_OF_SECTION, &nodeType) == 0 &&
        nodeType == NDB_MGM_NODE_TYPE_NDB){
      nodeId1 = i;
      break;
    }
  }

  g_err << "Using NDB node with id: " << nodeId1 << endl;

  g_err << "Get original values of dynamic ports" << endl;
  BaseString original_values;
  if (!get_all_ports(mgmd, nodeId1, original_values))
  {
    g_err << "Failed to get all original values" << endl;
    return false;
  }
  ndbout_c("original values: %s", original_values.c_str());

  g_err << "Set new values for all dynamic ports" << endl;
  BaseString new_values;
  {
    Vector<BaseString> port_pairs;
    original_values.split(port_pairs, "\n");
    // Remove last empty line
    require(port_pairs[port_pairs.size()-1] == "");
    port_pairs.erase(port_pairs.size()-1);

    // Generate new portnumbers
    for (unsigned i = 0; i < port_pairs.size(); i++)
    {
      int nodeid, port;
      if (sscanf(port_pairs[i].c_str(), "%d=%d", &nodeid, &port) != 2)
      {
        g_err << "Failed to parse port_pairs[" << i << "]: '"
              << port_pairs[i] << "'" << endl;
        return false;
      }
      const int new_port = -(int)(i + 37);
      new_values.appfmt("%d=%d\n", nodeid, new_port);
    }

    Properties args;
    args.put("node", nodeId1);
    args.put("num_ports", port_pairs.size());

    Properties set_result;
    if (!set_ports(mgmd, args, new_values.c_str(), set_result))
      return false;

    if (!ok(set_result))
    {
      g_err << "Unexpected result received from set_ports" << endl;
      set_result.print();
      return false;
    }
  }

  g_err << "Compare new values of dynamic ports" << endl;
  {
    BaseString current_values;
    if (!get_all_ports(mgmd, nodeId1, current_values))
    {
      g_err << "Failed to get all current values" << endl;
      return false;
    }
    ndbout_c("current values: %s", current_values.c_str());

    if (current_values != new_values)
    {
      g_err << "Set values was not correct, expected "
            << new_values << ", got "
            << current_values << endl;
      return false;
    }
  }

  g_err << "Restore old values" << endl;
  {
    Vector<BaseString> port_pairs;
    original_values.split(port_pairs, "\n");
    // Remove last empty line
    require(port_pairs[port_pairs.size()-1] == "");
    port_pairs.erase(port_pairs.size()-1);

    Properties args;
    args.put("node", nodeId1);
    args.put("num_ports", port_pairs.size());

    Properties set_result;
    if (!set_ports(mgmd, args, original_values.c_str(), set_result))
      return false;

    if (!ok(set_result))
    {
      g_err << "Unexpected result received from set_ports" << endl;
      set_result.print();
      return false;
    }
  }

  g_err << "Check restored values" << endl;
  {
    BaseString current_values;
    if (!get_all_ports(mgmd, nodeId1, current_values))
    {
      g_err << "Failed to get all current values" << endl;
      return false;
    }
    ndbout_c("current values: %s", current_values.c_str());

    if (current_values != original_values)
    {
      g_err << "Restored values was not correct, expected "
            << original_values << ", got "
            << current_values << endl;
      return false;
    }
  }

  return true;
}


int runTestSetPorts(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_FAILED;
  if (
      check_set_ports(mgmd) &&
      check_set_ports_invalid_nodeid(mgmd) &&
      check_set_ports_invalid_num_ports(mgmd) &&
      check_set_ports_invalid_mismatch_num_port_1(mgmd) &&
      check_set_ports_invalid_mismatch_num_port_2(mgmd) &&
      check_set_ports_invalid_port_list(mgmd) &&
      check_set_ports_mgmapi(mgmd) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}


#ifdef NOT_YET
static bool
check_restart_connected(NdbMgmd& mgmd)
{
  if (!mgmd.restart())
    return false;
  return true;
 }

int runTestRestartMgmd(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int result= NDBT_FAILED;
  if (
      check_restart_connected(mgmd) &&
      true)
    result= NDBT_OK;

  if (!mgmd.end_session())
    result= NDBT_FAILED;

  return result;
}
#endif


static bool
set_logfilter(NdbMgmd& mgmd,
              enum ndb_mgm_event_severity severity,
              int enable)
{
  struct ndb_mgm_reply reply;
  if (ndb_mgm_set_clusterlog_severity_filter(mgmd.handle(),
					     severity,
					     enable,
                                             &reply
                                             ) == -1)
  {
    g_err << "set_logfilter: ndb_mgm_set_clusterlog_severity_filter failed"
          << endl;
    return false;
  }
  return true;
}

static bool
get_logfilter(NdbMgmd& mgmd,
              enum ndb_mgm_event_severity severity,
              unsigned int* value)
{

  struct ndb_mgm_severity severity_struct;
  severity_struct.category = severity;
  if (ndb_mgm_get_clusterlog_severity_filter(mgmd.handle(),
					     &severity_struct,
					     1) != 1)
  {
    g_err << "get_logfilter: ndb_mgm_get_clusterlog_severity_filter failed"
          << endl;
    return false;
  }

  require(value);
  *value = severity_struct.value;

  return true;
}


int runTestSetLogFilter(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  for (int i = 0; i < (int)NDB_MGM_EVENT_SEVERITY_ALL; i++)
  {
    g_info << "severity: " << i << endl;
    ndb_mgm_event_severity severity = (ndb_mgm_event_severity)i;

    // Get initial value of level
    unsigned int initial_value;
    if (!get_logfilter(mgmd, severity, &initial_value))
      return NDBT_FAILED;

    // Turn level off
    if (!set_logfilter(mgmd, severity, 0))
      return NDBT_FAILED;

    // Check it's off
    unsigned int curr_value;
    if (!get_logfilter(mgmd, severity, &curr_value))
      return NDBT_FAILED;

    if (curr_value != 0)
    {
      g_err << "Failed to turn off severity: "  << severity << endl;
      return NDBT_FAILED;
    }

    // Turn level on
    if (!set_logfilter(mgmd, severity, 1))
      return NDBT_FAILED;

    // Check it's on
    if (!get_logfilter(mgmd, severity, &curr_value))
      return NDBT_FAILED;

    if (curr_value == 0)
    {
      g_err << "Filed to turn on severity: "  << severity << endl;
      return NDBT_FAILED;
    }

    // Toggle, ie. turn off
    if (!set_logfilter(mgmd, severity, -1))
      return NDBT_FAILED;

    // Check it's off
    if (!get_logfilter(mgmd, severity, &curr_value))
      return NDBT_FAILED;

    if (curr_value != 0)
    {
      g_err << "Failed to toggle severity : "  << severity << endl;
      return NDBT_FAILED;
    }

    // Set back initial value
    if (!set_logfilter(mgmd, severity, initial_value))
      return NDBT_FAILED;

  }

  return NDBT_OK;
}


int runTestBug40922(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int filter[] = {
    15, NDB_MGM_EVENT_CATEGORY_BACKUP,
    1, NDB_MGM_EVENT_CATEGORY_STARTUP,
    0
  };
  NdbLogEventHandle le_handle =
    ndb_mgm_create_logevent_handle(mgmd.handle(), filter);
  if (!le_handle)
    return NDBT_FAILED;

  g_info << "Calling ndb_log_event_get_next" << endl;

  struct ndb_logevent le_event;
  int r = ndb_logevent_get_next(le_handle,
                                &le_event,
                                2000);
  g_info << "ndb_log_event_get_next returned " << r << endl;

  int result = NDBT_FAILED;
  if (r == 0)
  {
    // Got timeout
    g_info << "ndb_logevent_get_next returned timeout" << endl;
    result = NDBT_OK;
  }
  else
  {
    if(r>0)
      g_err << "ERROR: Receieved unexpected event: "
            << le_event.type << endl;
    if(r<0)
      g_err << "ERROR: ndb_logevent_get_next returned error: "
            << r << endl;
  }

  ndb_mgm_destroy_logevent_handle(&le_handle);

  return result;
}


int runTestBug45497(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  Vector<NdbMgmd*> mgmds;

  while(true)
  {
    NdbMgmd* mgmd = new NdbMgmd();

    // Set quite short timeout
    if (!mgmd->set_timeout(1000))
    {
      result = NDBT_FAILED;
      break;
    }

    if (mgmd->connect())
    {
      mgmds.push_back(mgmd);
      g_info << "connections: " << mgmds.size() << endl;
      continue;
    }

    g_err << "Failed to make another connection, connections: "
          << mgmds.size() << endl;


    // Disconnect some connections
    int to_disconnect = 10;
    while(mgmds.size() && to_disconnect--)
    {
      g_info << "disconnnect, connections: " << mgmds.size() << endl;
      NdbMgmd* mgmd = mgmds[0];
      mgmds.erase(0);
      delete mgmd;
    }

    if (loops-- == 0)
      break;
  }

  while(mgmds.size())
  {
    NdbMgmd* mgmd = mgmds[0];
    mgmds.erase(0);
    delete mgmd;
  }

  return result;
}


bool isCategoryValid(struct ndb_logevent* le)
{
  switch (le->category)
  {
  case NDB_MGM_EVENT_CATEGORY_BACKUP:
  case NDB_MGM_EVENT_CATEGORY_STARTUP:
  case NDB_MGM_EVENT_CATEGORY_NODE_RESTART:
  case NDB_MGM_EVENT_CATEGORY_CONNECTION:
  case NDB_MGM_EVENT_CATEGORY_STATISTIC:
  case NDB_MGM_EVENT_CATEGORY_CHECKPOINT:
    return true;
  default:
    return false;
  }
}

int runTestBug16723708(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int loops = ctx->getNumLoops();
  int result = NDBT_FAILED;

  if (!mgmd.connect())
    return NDBT_FAILED;

  int filter[] = {
    15, NDB_MGM_EVENT_CATEGORY_BACKUP,
    15, NDB_MGM_EVENT_CATEGORY_STARTUP,
    15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
    15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
    15, NDB_MGM_EVENT_CATEGORY_STATISTIC,
    15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT,
    0
  };
  NdbLogEventHandle le_handle =
    ndb_mgm_create_logevent_handle(mgmd.handle(), filter);
  if (!le_handle)
    return NDBT_FAILED;
  NdbLogEventHandle le_handle2 = 
    ndb_mgm_create_logevent_handle(mgmd.handle(), filter);
  if (!le_handle2)
    return NDBT_FAILED;
 
  for(int l=0; l<loops; l++)
  {
    g_info << "Calling ndb_log_event_get_next" << endl;
  
    struct ndb_logevent le_event;
    int r = ndb_logevent_get_next(le_handle,
                                  &le_event,
                                  2000);
    g_info << "ndb_log_event_get_next returned " << r << endl;
    
    struct ndb_logevent le_event2;
    int r2 = ndb_logevent_get_next2(le_handle2,
                                    &le_event2,
                                    2000);
    g_info << "ndb_log_event_get_next2 returned " << r2 << endl;
    
    result = NDBT_OK;
    if ((r == 0) || (r2 == 0))
    {
      // Got timeout
      g_info << "ndb_logevent_get_next[2] returned timeout" << endl;
    }
    else
    {
      if(r>0)
      {
        g_info << "next() ndb_logevent type : " << le_event.type 
               << " category : " << le_event.category 
               << " " << ndb_mgm_get_event_category_string(le_event.category)
               << endl;
        if (isCategoryValid(&le_event))
        {
          g_err << "ERROR: ndb_logevent_get_next() returned valid category! "
                << le_event.category << endl;
          result = NDBT_FAILED;
        }
      }
      else
      {
        g_err << "ERROR: ndb_logevent_get_next returned error: "
              << r << endl;
      }
      
      if(r2>0)
      {        
        g_info << "next2() ndb_logevent type : " << le_event2.type 
               << " category : " << le_event2.category 
               << " " << ndb_mgm_get_event_category_string(le_event2.category)
               << endl;

        if (!isCategoryValid(&le_event2))
        {
          g_err << "ERROR: ndb_logevent_get_next2() returned invalid category! "
                << le_event2.category << endl;
          result = NDBT_FAILED;
        }
      }
      else
      {
        g_err << "ERROR: ndb_logevent_get_next2 returned error: "
              << r << endl;
        result = NDBT_FAILED;
      }
    }
    if(result == NDBT_FAILED)
      break;
  }
  ndb_mgm_destroy_logevent_handle(&le_handle2);
  ndb_mgm_destroy_logevent_handle(&le_handle);

  return result;
}


static int
runTestGetVersion(NDBT_Context* ctx, NDBT_Step* step)
{

  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  char verStr[64];
  int major, minor, build;
  if (ndb_mgm_get_version(mgmd.handle(),
                          &major, &minor, &build,
                          sizeof(verStr), verStr) != 1)
  {
    g_err << "ndb_mgm_get_version failed,"
          << "error: " << ndb_mgm_get_latest_error_msg(mgmd.handle())
          << "desc: " << ndb_mgm_get_latest_error_desc(mgmd.handle()) << endl;
    return NDBT_FAILED;
  }

  g_info << "Using major: " << major
         << " minor: " << minor
         << " build: " << build
         << " string: " << verStr << endl;

  int l = 0;
  int loops = ctx->getNumLoops();
  while(l < loops)
  {
    char verStr2[64];
    int major2, minor2, build2;
    if (ndb_mgm_get_version(mgmd.handle(),
                            &major2, &minor2, &build2,
                            sizeof(verStr2), verStr2) != 1)
    {
      g_err << "ndb_mgm_get_version failed,"
            << "error: " << ndb_mgm_get_latest_error_msg(mgmd.handle())
            << "desc: " << ndb_mgm_get_latest_error_desc(mgmd.handle()) << endl;
      return NDBT_FAILED;
    }

    if (major != major2)
    {
      g_err << "Got different major: " << major2
            << " excpected: " << major << endl;
      return NDBT_FAILED;
    }

    if (minor != minor2)
    {
      g_err << "Got different minor: " << minor2
            << " excpected: " << minor << endl;
      return NDBT_FAILED;
    }

    if (build != build2)
    {
      g_err << "Got different build: " << build2
            << " excpected: " << build << endl;
      return NDBT_FAILED;
    }

    if (strcmp(verStr, verStr2) != 0)
    {
      g_err << "Got different verStr: " << verStr2
            << " excpected: " << verStr << endl;
      return NDBT_FAILED;
    }

    l++;
  }

  return NDBT_OK;
}


static int
runTestGetVersionUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result= NDBT_OK;
  while(!ctx->isTestStopped() &&
        (result= runTestGetVersion(ctx, step)) == NDBT_OK)
    ;
  return result;
}


int runTestDumpEvents(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  // Test with unsupported logevent_type
  {
    const Ndb_logevent_type unsupported = NDB_LE_NDBStopForced;
    g_info << "ndb_mgm_dump_events(" << unsupported << ")" << endl;

    const struct ndb_mgm_events* events =
      ndb_mgm_dump_events(mgmd.handle(), unsupported, 0, 0);
    if (events != NULL)
    {
      g_err << "ndb_mgm_dump_events returned events "
            << "for unsupported Ndb_logevent_type" << endl;
      return NDBT_FAILED;
    }

    if (ndb_mgm_get_latest_error(mgmd.handle()) != NDB_MGM_USAGE_ERROR ||
        strcmp("ndb_logevent_type 59 not supported",
               ndb_mgm_get_latest_error_desc(mgmd.handle())))
    {
      g_err << "Unexpected error for unsupported logevent type, "
            << ndb_mgm_get_latest_error(mgmd.handle())
            << ", desc: " << ndb_mgm_get_latest_error_desc(mgmd.handle())
            << endl;
      return NDBT_FAILED;
    }
  }

  // Test with nodes >= MAX_NDB_NODES
  for (int i = MAX_NDB_NODES; i < MAX_NDB_NODES + 3; i++)
  {
    g_info << "ndb_mgm_dump_events(NDB_LE_MemoryUsage, 1, "
           << i << ")" << endl;

    const struct ndb_mgm_events* events =
      ndb_mgm_dump_events(mgmd.handle(), NDB_LE_MemoryUsage, 1, &i);
    if (events != NULL)
    {
      g_err << "ndb_mgm_dump_events returned events "
            << "for too large nodeid" << endl;
      return NDBT_FAILED;
    }

    int invalid_nodeid;
    if (ndb_mgm_get_latest_error(mgmd.handle()) != NDB_MGM_USAGE_ERROR ||
        sscanf(ndb_mgm_get_latest_error_desc(mgmd.handle()),
               "invalid nodes: '%d'", &invalid_nodeid) != 1 ||
        invalid_nodeid != i)
    {
      g_err << "Unexpected error for too large nodeid, "
            << ndb_mgm_get_latest_error(mgmd.handle())
            << ", desc: " << ndb_mgm_get_latest_error_desc(mgmd.handle())
            << endl;
      return NDBT_FAILED;
    }

  }

  int l = 0;
  int loops = ctx->getNumLoops();
  while (l<loops)
  {
    const Ndb_logevent_type supported[] =
      {
        NDB_LE_MemoryUsage,
        NDB_LE_BackupStatus,
        (Ndb_logevent_type)0
      };

    // Test with supported logevent_type
    for (int i = 0; supported[i]; i++)
    {
      g_info << "ndb_mgm_dump_events(" << supported[i] << ")" << endl;

      struct ndb_mgm_events* events =
        ndb_mgm_dump_events(mgmd.handle(), supported[i], 0, 0);
      if (events == NULL)
      {
        g_err << "ndb_mgm_dump_events failed, type: " << supported[i]
              << ", error: " << ndb_mgm_get_latest_error(mgmd.handle())
              << ", msg: " << ndb_mgm_get_latest_error_msg(mgmd.handle())
              << endl;
        return NDBT_FAILED;
      }

      if (events->no_of_events < 0)
      {
        g_err << "ndb_mgm_dump_events returned a negative number of events: "
              << events->no_of_events << endl;
        free(events);
        return NDBT_FAILED;
      }

      g_info << "Got " << events->no_of_events << " events" << endl;
      free(events);
    }

    l++;
  }

  return NDBT_OK;
}

int runTestStatusAfterStop(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  ndb_mgm_node_type
    node_types[2] = { NDB_MGM_NODE_TYPE_NDB,
                      NDB_MGM_NODE_TYPE_UNKNOWN };

  // Test: get status, stop node, get status again
  printf("Getting status\n");
  ndb_mgm_cluster_state *cs = ndb_mgm_get_status2(mgmd.handle(), node_types);
  if (cs == NULL)
  {
    printf("%s (%d)\n", ndb_mgm_get_latest_error_msg(mgmd.handle()),
           ndb_mgm_get_latest_error(mgmd.handle()));
    return NDBT_FAILED;
  }

  int nodeId = 0;
  for(int i=0; i < cs->no_of_nodes; i++ )
  {
    ndb_mgm_node_state *ns = cs->node_states + i;
    printf("Node ID: %d  status:%d\n", ns->node_id, ns->node_status);
    if (nodeId == 0 && ns->node_type == NDB_MGM_NODE_TYPE_NDB)
      nodeId = ns->node_id;
  }
  free(cs);
  cs = NULL;

  printf("Stopping data node\n");
  // We only stop 1 data node, in this case NodeId=2
  int nodes[1] =  { nodeId };
  int stopped = ndb_mgm_restart2(mgmd.handle(), NDB_ARRAY_SIZE(nodes), nodes, 
                                 0, 0, 1);
  if (stopped < 0)
  {
    printf("ndb_mgm_stop failed, '%s' (%d)\n",
           ndb_mgm_get_latest_error_msg(mgmd.handle()),
           ndb_mgm_get_latest_error(mgmd.handle()));
    return NDBT_FAILED;
  }

  printf("Stopped %d data node(s)\n", stopped);

  printf("Getting status\n");
  cs = ndb_mgm_get_status2(mgmd.handle(), node_types);
  if (cs == NULL)
  {
    printf("%s (%d)\n", ndb_mgm_get_latest_error_msg(mgmd.handle()),
           ndb_mgm_get_latest_error(mgmd.handle()));
    return NDBT_FAILED;
  }
  for(int i=0; i < cs->no_of_nodes; i++ )
  {
    ndb_mgm_node_state *ns = cs->node_states + i;
    printf("Node ID: %d  status:%d\n", ns->node_id, ns->node_status);
  }
  free(cs);

  NdbRestarter res;
  res.startAll();
  res.waitClusterStarted();

  return NDBT_OK;
}

int
sort_ng(const void * _e0, const void * _e1)
{
  const struct ndb_mgm_node_state * e0 = (const struct ndb_mgm_node_state*)_e0;
  const struct ndb_mgm_node_state * e1 = (const struct ndb_mgm_node_state*)_e1;
  if (e0->node_group != e1->node_group)
    return e0->node_group - e1->node_group;

  return e0->node_id - e1->node_id;
}

int
runBug12928429(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
  {
    return NDBT_FAILED;
  }

  ndb_mgm_node_type node_types[2] =
    { NDB_MGM_NODE_TYPE_NDB, NDB_MGM_NODE_TYPE_UNKNOWN };

  ndb_mgm_cluster_state * cs = ndb_mgm_get_status2(mgmd.handle(), node_types);
  if (cs == NULL)
  {
    printf("%s (%d)\n", ndb_mgm_get_latest_error_msg(mgmd.handle()),
           ndb_mgm_get_latest_error(mgmd.handle()));
    return NDBT_FAILED;
  }

  /**
   * sort according to node-group
   */
  qsort(cs->node_states, cs->no_of_nodes,
        sizeof(cs->node_states[0]), sort_ng);

  int ng = cs->node_states[0].node_group;
  int replicas = 1;
  for (int i = 1; i < cs->no_of_nodes; i++)
  {
    if (cs->node_states[i].node_status != NDB_MGM_NODE_STATUS_STARTED)
    {
      ndbout_c("node %u is not started!!!", cs->node_states[i].node_id);
      free(cs);
      return NDBT_OK;
    }
    if (cs->node_states[i].node_group == ng)
    {
      replicas++;
    }
    else
    {
      break;
    }
  }

  if (replicas == 1)
  {
    free(cs);
    return NDBT_OK;
  }

  int nodes[MAX_NODES];
  int cnt = 0;
  for (int i = 0; i < cs->no_of_nodes; i += replicas)
  {
    printf("%u ", cs->node_states[i].node_id);
    nodes[cnt++] = cs->node_states[i].node_id;
  }
  printf("\n");

  int initial = 0;
  int nostart = 1;
  int abort = 0;
  int force = 1;
  int disconnnect = 0;

  /**
   * restart half of the node...should be only restart half of the nodes
   */
  int res = ndb_mgm_restart4(mgmd.handle(), cnt, nodes,
                             initial, nostart, abort, force, &disconnnect);

  if (res == -1)
  {
    ndbout_c("%u res: %u ndb_mgm_get_latest_error: %u line: %u msg: %s",
             __LINE__,
             res,
             ndb_mgm_get_latest_error(mgmd.handle()),
             ndb_mgm_get_latest_error_line(mgmd.handle()),
             ndb_mgm_get_latest_error_msg(mgmd.handle()));
    return NDBT_FAILED;
  }

  {
    ndb_mgm_cluster_state * cs2 = ndb_mgm_get_status2(mgmd.handle(),node_types);
    if (cs2 == NULL)
    {
      printf("%s (%d)\n", ndb_mgm_get_latest_error_msg(mgmd.handle()),
             ndb_mgm_get_latest_error(mgmd.handle()));
      return NDBT_FAILED;
    }

    for (int i = 0; i < cs2->no_of_nodes; i++)
    {
      int node_id = cs2->node_states[i].node_id;
      int expect = NDB_MGM_NODE_STATUS_STARTED;
      for (int c = 0; c < cnt; c++)
      {
        if (node_id == nodes[c])
        {
          expect = NDB_MGM_NODE_STATUS_NOT_STARTED;
          break;
        }
      }
      if (cs2->node_states[i].node_status != expect)
      {
        ndbout_c("%u node %u expect: %u found: %u",
                 __LINE__,
                 cs2->node_states[i].node_id,
                 expect,
                 cs2->node_states[i].node_status);
        return NDBT_FAILED;
      }
    }
    free(cs2);
  }

  NdbRestarter restarter;
  restarter.startAll();
  restarter.waitClusterStarted();

  /**
   * restart half of the node...and all nodes in one node group
   *   should restart cluster
   */
  cnt = 0;
  for (int i = 0; i < replicas; i++)
  {
    printf("%u ", cs->node_states[i].node_id);
    nodes[cnt++] = cs->node_states[i].node_id;
  }
  for (int i = replicas; i < cs->no_of_nodes; i += replicas)
  {
    printf("%u ", cs->node_states[i].node_id);
    nodes[cnt++] = cs->node_states[i].node_id;
  }
  printf("\n");

  res = ndb_mgm_restart4(mgmd.handle(), cnt, nodes,
                         initial, nostart, abort, force, &disconnnect);

  if (res == -1)
  {
    ndbout_c("%u res: %u ndb_mgm_get_latest_error: %u line: %u msg: %s",
             __LINE__,
             res,
             ndb_mgm_get_latest_error(mgmd.handle()),
             ndb_mgm_get_latest_error_line(mgmd.handle()),
             ndb_mgm_get_latest_error_msg(mgmd.handle()));
    return NDBT_FAILED;
  }

  {
    ndb_mgm_cluster_state * cs2 = ndb_mgm_get_status2(mgmd.handle(),node_types);
    if (cs2 == NULL)
    {
      printf("%s (%d)\n", ndb_mgm_get_latest_error_msg(mgmd.handle()),
             ndb_mgm_get_latest_error(mgmd.handle()));
      return NDBT_FAILED;
    }

    for (int i = 0; i < cs2->no_of_nodes; i++)
    {
      int expect = NDB_MGM_NODE_STATUS_NOT_STARTED;
      if (cs2->node_states[i].node_status != expect)
      {
        ndbout_c("%u node %u expect: %u found: %u",
                 __LINE__,
                 cs2->node_states[i].node_id,
                 expect,
                 cs2->node_states[i].node_status);
        return NDBT_FAILED;
      }
    }
    free(cs2);
  }

  restarter.startAll();
  restarter.waitClusterStarted();

  free(cs);
  return NDBT_OK;
}

int runTestNdbApiConfig(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;

  if (!mgmd.connect())
    return NDBT_FAILED;

  struct test_parameter
  {
    Uint32 key;
    Uint32 NdbApiConfig::*ptr;
    Uint32 values[2];
  } parameters[] =
  {
    { CFG_MAX_SCAN_BATCH_SIZE,                   &NdbApiConfig::m_scan_batch_size, { 10, 1000 } },
    { CFG_BATCH_BYTE_SIZE,                       &NdbApiConfig::m_batch_byte_size, { 10, 1000 } },
    { CFG_BATCH_SIZE,                            &NdbApiConfig::m_batch_size,      { 10, 1000 } },
    // Skip test of m_waitfor_timeout since it is not configurable in API-section
    { CFG_DEFAULT_OPERATION_REDO_PROBLEM_ACTION, &NdbApiConfig::m_default_queue_option,
      { OPERATION_REDO_PROBLEM_ACTION_ABORT, OPERATION_REDO_PROBLEM_ACTION_QUEUE } },
    { CFG_DEFAULT_HASHMAP_SIZE,                  &NdbApiConfig::m_default_hashmap_size, { 240, 3840 } },
  };
  // Catch if new members are added to NdbApiConfig,
  // if so add tests and adjust expected size
  NDB_STATIC_ASSERT(sizeof(NdbApiConfig) == 7 * sizeof(Uint32));

  Config savedconf;
  if (!mgmd.get_config(savedconf))
    return NDBT_FAILED;

  for (size_t i = 0; i < NDB_ARRAY_SIZE(parameters[0].values) ; i ++)
  {
    /**
     * Setup configuration
     */

    // Get the binary config
    Config conf;
    if (!mgmd.get_config(conf))
      return NDBT_FAILED;

    ConfigValues::Iterator iter(conf.m_configValues->m_config);
    for (Uint32 nodeid = 1; nodeid < MAX_NODES; nodeid ++)
    {
      Uint32 type;
      if (!iter.openSection(CFG_SECTION_NODE, nodeid))
        continue;

      if (iter.get(CFG_TYPE_OF_SECTION, &type) &&
          type == NDB_MGM_NODE_TYPE_API)
      {
        for (size_t param = 0; param < NDB_ARRAY_SIZE(parameters) ; param ++)
        {
          iter.set(parameters[param].key, parameters[param].values[i]);
        }
      }

      iter.closeSection();
    }

    // Set the modified config
    if (!mgmd.set_config(conf))
      return NDBT_FAILED;

    /**
     * Connect api
     */

    Ndb_cluster_connection con(mgmd.getConnectString());

    const int retries = 12;
    const int retry_delay = 5;
    const int verbose = 1;
    if (con.connect(retries, retry_delay, verbose) != 0)
    {
      g_err << "Ndb_cluster_connection.connect failed" << endl;
      return NDBT_FAILED;
    }

    /**
     * Check api configuration
     */

    NDBT_Context conctx(con);
    int failures = 0;

    for (size_t param = 0; param < NDB_ARRAY_SIZE(parameters) ; param ++)
    {
      Uint32 expected = parameters[param].values[i];
      Uint32 got = conctx.getConfig().*parameters[param].ptr;
      if (got != expected)
      {
        int j;
        for(j = 0; j < ConfigInfo::m_NoOfParams ; j ++)
        {
          if (ConfigInfo::m_ParamInfo[j]._paramId == parameters[param].key)
            break;
        }
        g_err << "Parameter ";
        if (j < ConfigInfo::m_NoOfParams)
          g_err << ConfigInfo::m_ParamInfo[j]._fname << " (" << parameters[param].key << ")";
        else
          g_err << "Unknown (" << parameters[param].key << ")";
        g_err << ": Expected " << expected << " got " << got << endl;
        failures++;
      }
      if (failures > 0)
        return NDBT_FAILED;
    }
  }

  // Restore conf after upgrading config generation
  Config conf;
  if (!mgmd.get_config(conf))
    return NDBT_FAILED;

  savedconf.setGeneration(conf.getGeneration());

  if (!mgmd.set_config(savedconf))
  {
    g_err << "Failed to restore config." << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}


static
int runTestCreateLogEvent(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbMgmd mgmd;
  int loops = ctx->getNumLoops();

  if (!mgmd.connect())
    return NDBT_FAILED;

  int filter[] = {
    15, NDB_MGM_EVENT_CATEGORY_BACKUP,
    0
  };

  for(int l=0; l<loops; l++)
  {
    g_info << "Creating log event handle " << l << endl;
    NdbLogEventHandle le_handle =
        ndb_mgm_create_logevent_handle(mgmd.handle(), filter);
    if (!le_handle)
      return NDBT_FAILED;

    ndb_mgm_destroy_logevent_handle(&le_handle);
  }
  return NDBT_OK;
}

NDBT_TESTSUITE(testMgm);
DRIVER(DummyDriver); /* turn off use of NdbApi */
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
TESTCASE("SetConfig",
	 "Tests the ndb_mgm_set_configuration function"){
  INITIALIZER(runSetConfig);
}
TESTCASE("CheckConfig",
	 "Connect to each ndb_mgmd and check they have the same configuration"){
  INITIALIZER(runCheckConfig);
}
TESTCASE("TestReloadConfig",
	 "Test of 'reload config'"){
  INITIALIZER(runTestReloadConfig);
}
TESTCASE("TestSetConfig",
	 "Test of 'set config'"){
  INITIALIZER(runTestSetConfig);
}
TESTCASE("TestSetConfigParallel",
	 "Test of 'set config' from 5 threads"){
  STEPS(runTestSetConfigParallel, 5);
}
TESTCASE("GetConfig", "Run ndb_mgm_get_configuration in parallel"){
  STEPS(runGetConfig, 64);
}
TESTCASE("TestStatus",
	 "Test status and status2"){
  INITIALIZER(runTestStatus);

}
TESTCASE("TestStatusMultiple",
	 "Test status and status2 with 64 threads"){
  /**
   * For this and other tests we are limited in how much TCP backlog
   * the MGM server socket has. It is currently set to a maximum of
   * 64, so if we need to test more than 64 threads in parallel we
   * need to introduce some sort of wait state to ensure that we
   * don't get all threads sending TCP connect at the same time.
   */
  STEPS(runTestStatus, 64);

}
TESTCASE("TestGetNodeId",
	 "Test 'get nodeid'"){
  INITIALIZER(runTestGetNodeId);
}
TESTCASE("TestGetVersion",
 	 "Test 'get version' and 'ndb_mgm_get_version'"){
  STEPS(runTestGetVersion, 20);
}
TESTCASE("TestTransporterConnect",
	 "Test 'transporter connect'"){
  INITIALIZER(runTestTransporterConnect);
}
TESTCASE("TestConnectionParameter",
	 "Test 'get/set connection parameter'"){
  INITIALIZER(runTestConnectionParameter);
}
TESTCASE("TestSetLogFilter",
	 "Test 'set logfilter' and 'get info clusterlog'"){
  INITIALIZER(runTestSetLogFilter);
}
#ifdef NOT_YET
TESTCASE("TestRestartMgmd",
        "Test restart of ndb_mgmd(s)"){
  INITIALIZER(runTestRestartMgmd);
}
#endif
TESTCASE("Bug40922",
	 "Make sure that ndb_logevent_get_next returns when "
         "called with a timeout"){
  INITIALIZER(runTestBug40922);
}
TESTCASE("Bug16723708",
	 "Check that ndb_logevent_get_next returns events "
         "which have valid category values"){
  INITIALIZER(runTestBug16723708);
}
TESTCASE("Stress",
	 "Run everything while changing config"){
  STEP(runTestGetNodeIdUntilStopped);
  STEP(runSetConfigUntilStopped);
  STEPS(runGetConfigUntilStopped, 10);
  STEPS(runGetConfigFromNodeUntilStopped, 10);
  STEPS(runTestStatusUntilStopped, 10);
  STEPS(runTestGetVersionUntilStopped, 5);
  STEP(runSleepAndStop);
}
TESTCASE("Stress2",
	 "Run everything while changing config in parallel"){
  STEP(runTestGetNodeIdUntilStopped);
  STEPS(runTestSetConfigParallelUntilStopped, 5);
  STEPS(runGetConfigUntilStopped, 10);
  STEPS(runGetConfigFromNodeUntilStopped, 10);
  STEPS(runTestStatusUntilStopped, 10);
  STEPS(runTestGetVersionUntilStopped, 5);
  STEP(runSleepAndStop);
}
X_TESTCASE("Bug45497",
         "Connect to ndb_mgmd until it can't handle more connections"){
  STEP(runTestBug45497);
}
TESTCASE("TestGetVersion",
 	 "Test 'get version' and 'ndb_mgm_get_version'"){
  STEPS(runTestGetVersion, 20);
}
TESTCASE("TestDumpEvents",
 	 "Test 'dump events'"){
  STEPS(runTestDumpEvents, 1);
}
TESTCASE("TestStatusAfterStop",
 	 "Test get status after stop "){
  STEPS(runTestStatusAfterStop, 1);
}
TESTCASE("Bug12928429", "")
{
  STEP(runBug12928429);
}
TESTCASE("TestNdbApiConfig", "")
{
  STEP(runTestNdbApiConfig);
}
TESTCASE("TestSetPorts",
         "Test 'set ports'"){
  INITIALIZER(runTestSetPorts);
}
TESTCASE("TestCreateLogEvent", "Test ndb_mgm_create_log_event_handle"){
  STEPS(runTestCreateLogEvent, 5);
}
TESTCASE("TestConnectionFailure",
         "Test if Read Error is received after mgmd is restarted"){
  INITIALIZER(runTestMgmApiReadErrorRestart);
}
NDBT_TESTSUITE_END(testMgm);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testMgm);
  testMgm.setCreateTable(false);
  testMgm.setRunAllTables(true);
  return testMgm.execute(argc, argv);
}

template class Vector<NdbMgmd*>;
