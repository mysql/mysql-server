# Copyright (c)2011, Oracle and/or its affiliates. All rights
# reserved.
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301  USA

# pmpstack.awk

# Analyze memcached stack traces obtained from "Poor Man's Profiler" 

function label(name)  {
   if(thr == "event") event[name] += $1;
   else if(thr == "commit") commit[name] += $1;
   else if(thr == "broker") broker[name] += $1;
   else if(thr == "send")   send[name]   += $1;
   else if(thr == "poll")   poll[name]   += $1;
}   


### Which thread is running? 
                                       { thr="x" }   # undetermined

/event_base_loop,worker_libevent/      { event["total"]  += $1; thr="event" } 
/run_commit_thread/                    { commit["total"] += $1; thr="commit" }
/run_ndb_commit_thread/                { commit["total"] += $1; thr="commit" }
/run_broker_thread/                    { broker["total"] += $1; thr="broker" }
/run_ndb_send_thread/                  { send["total"] += $1  ; thr="send"   }
/run_ndb_poll_thread/                  { poll["total"] += $1  ; thr="poll"   }

                                       { if(thr == "x") x["total"] += $1 }

### What is the thread doing? 
### Patterns higher up in this file take precedence over lower ones

$2 ~ /^epoll_wait,TransporterRegistry::po/ { label("epoll_wait_transporter_recv"); next }
                                             
$2 ~ /^epoll_wait/                         { label("epoll_wait"); next }

$2 ~ /^writev,TCP_Transporter::doSend/     { label("sending_to_ndb"); next }

$2 ~ /^recv,TCP_Transporter/               { label("tcp_recv_from_ndb"); next }

$2 ~ /^sendmsg,conn_mwrite/                { label("sending_to_client"); next }

/pthread_mutex_unlock/                     { label("releasing_locks"); next }

/pthread_mutex_lock,Ndb::sendPrepared/     { label("lock_Ndb_impl"); next }

/_mutex_lock/ && /TransporterFacade/       { label("lock_transporter_facade_mutex"); next }

/pthread_cond_timedwait/ && /ollNdb/       { label("wait_poll_ndb"); next }
                                        
/workqueue_consumer_wait/                  { label("workqueue_idle_wait"); next }

/pthread_mutex_lock,notify_io_complete/    { label("memcached_lock_notify"); next }
                                                    
/pthread_cond_signal/                      { label("signaling_condition_var"); next }

/Ndb::computeHash/                         { label("ndb_compute_hash"); next }

/sleep/                                    { label("sleep"); next }


# Print final summary

END { 
      for(i in event) if (i != "total")
       printf("%s\t%.2f%%\t%s\n", 
              "Event", (event[i] / event["total"]) * 100, i)
      printf("\n");

      for(i in broker) if (i != "total")
       printf("%s\t%.2f%%\t%s\n", 
              "Broker", (broker[i] / broker["total"]) * 100, i)
      if(broker["total"]) printf("\n");

      for(i in commit) if(i != "total")
       printf("%s\t%.2f%%\t%s\n", 
              "Commit", (commit[i] / commit["total"]) * 100, i)
      if(commit["total"]) printf("\n");

      for(i in send) if(i != "total")
       printf("%s\t%.2f%%\t%s\n", 
              "Send", (send[i] / send["total"]) * 100, i)
      if(send["total"]) printf("\n");

      for(i in poll) if(i != "total")
       printf("%s\t%.2f%%\t%s\n", 
              "Poll", (poll[i] / poll["total"]) * 100, i)
      if(poll["total"]) printf("\n");

      for(i in x) if(i != "total")
       printf("%s\t%.2f%%\t%s\n", 
              "Unidentified", (x[i] / x["total"]) * 100, i)
    }
