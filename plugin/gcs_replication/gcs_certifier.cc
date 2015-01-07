/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

using namespace std;

#include <signal.h>
#include "gcs_plugin.h"
#include "gcs_certifier.h"
#include <gcs_replication.h>

static void *launch_broadcast_thread(void* arg)
{
  Certifier_broadcast_thread *handler= (Certifier_broadcast_thread*) arg;
  handler->dispatcher();
  return 0;
}

Certifier_broadcast_thread::Certifier_broadcast_thread
                                      (Gcs_communication_interface* comm_intf,
                                       Gcs_control_interface* ctrl_intf,
                                       Cluster_member_info* local_node_info)
  :aborted(false), broadcast_pthd_running(false), gcs_communication(comm_intf),
   gcs_control(ctrl_intf), local_node(local_node_info)
{
  pthread_mutex_init(&broadcast_pthd_lock, NULL);
  pthread_cond_init(&broadcast_pthd_cond, NULL);
}

Certifier_broadcast_thread::~Certifier_broadcast_thread()
{
  pthread_mutex_destroy(&broadcast_pthd_lock);
  pthread_cond_destroy(&broadcast_pthd_cond);
}

int Certifier_broadcast_thread::initialize()
{
  DBUG_ENTER("Certifier_broadcast_thread::initialize");

  pthread_mutex_lock(&broadcast_pthd_lock);
  if (broadcast_pthd_running)
  {
    pthread_mutex_unlock(&broadcast_pthd_lock);
    DBUG_RETURN(0);
  }

  aborted= false;
  if (pthread_create(&broadcast_pthd, NULL, launch_broadcast_thread, (void*)this))
  {
    pthread_mutex_unlock(&broadcast_pthd_lock);
    DBUG_RETURN(1);
  }

  while (!broadcast_pthd_running)
  {
    DBUG_PRINT("sleep",("Waiting for certifier broadcast thread to start"));
    pthread_cond_wait(&broadcast_pthd_cond, &broadcast_pthd_lock);
  }
  pthread_mutex_unlock(&broadcast_pthd_lock);

  DBUG_RETURN(0);
}


int Certifier_broadcast_thread::terminate()
{
  DBUG_ENTER("Certifier_broadcast_thread::terminate");

  pthread_mutex_lock(&broadcast_pthd_lock);
  if (!broadcast_pthd_running)
  {
    pthread_mutex_unlock(&broadcast_pthd_lock);
    DBUG_RETURN(0);
  }

  aborted= true;
  while (broadcast_pthd_running)
  {
    DBUG_PRINT("loop", ("killing certifier broadcast thread"));
    pthread_kill(broadcast_pthd, SIGUSR1);
    pthread_cond_wait(&broadcast_pthd_cond, &broadcast_pthd_lock);
  }
  pthread_mutex_unlock(&broadcast_pthd_lock);

  DBUG_RETURN(0);
}


void Certifier_broadcast_thread::dispatcher()
{
  DBUG_ENTER("Certifier_broadcast_thread::dispatcher");

  pthread_mutex_lock(&broadcast_pthd_lock);
  broadcast_pthd_running= true;
  pthread_cond_broadcast(&broadcast_pthd_cond);
  pthread_mutex_unlock(&broadcast_pthd_lock);

  while (!aborted)
  {
    broadcast_gtid_executed();
    my_sleep(BROADCAST_PERIOD);
  }

  pthread_mutex_lock(&broadcast_pthd_lock);
  broadcast_pthd_running= false;
  pthread_cond_broadcast(&broadcast_pthd_cond);
  pthread_mutex_unlock(&broadcast_pthd_lock);

  DBUG_VOID_RETURN;
}


int Certifier_broadcast_thread::broadcast_gtid_executed()
{
  DBUG_ENTER("Certifier_broadcast_thread::broadcast_gtid_executed");

  if (this->local_node == NULL ||
      this->local_node->get_recovery_status() !=
                                        Cluster_member_info::MEMBER_ONLINE)
  {
    DBUG_RETURN(0);
  }


  int error= 0;
  uchar *encoded_gtid_executed= NULL;
  uint length;
  get_server_encoded_gtid_executed(&encoded_gtid_executed, &length);

  Gtid_Executed_Message gtid_executed_message;
  vector<uchar> encoded_gtid_executed_message;
  gtid_executed_message.append_gtid_executed(encoded_gtid_executed, length);
  gtid_executed_message.encode(&encoded_gtid_executed_message);

  Gcs_message msg(*gcs_control->get_local_information(),
                  *gcs_control->get_current_view()->get_group_id(),
                  UNIFORM);

  msg.append_to_payload(&encoded_gtid_executed_message.front(),
                        encoded_gtid_executed_message.size());

  if (gcs_communication->send_message(&msg))
  {
    log_message(MY_ERROR_LEVEL, "Unable to broadcast stable transactions set message");
    error= 1;
  }

#if !defined(DBUG_OFF)
  char *encoded_gtid_executed_string=
      encoded_gtid_set_to_string(encoded_gtid_executed, length);
  DBUG_PRINT("info", ("Certifier broadcast executed_set: %s", encoded_gtid_executed_string));
  my_free(encoded_gtid_executed_string);
#endif

  my_free(encoded_gtid_executed);
  DBUG_RETURN(error);
}


Certifier::Certifier()
  :gcs_communication(NULL), gcs_control(NULL), local_node(NULL),
   initialized(false), next_seqno(1), positive_cert(0), negative_cert(0)
{
  incoming= new Synchronized_queue<Data_packet*>();
  stable_sid_map= new Sid_map(NULL);
  stable_gtid_set= new Gtid_set(stable_sid_map, NULL);
  broadcast_thread= new Certifier_broadcast_thread(gcs_communication,
                                                   gcs_control,
                                                   local_node);

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_info certifier_mutexes[]=
  {
    { &key_LOCK_certifier_map, "LOCK_certifier_map", 0}
  };
  register_gcs_psi_keys(certifier_mutexes, 1, NULL, 0);
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(key_LOCK_certifier_map, &LOCK_certifier_map,
                   MY_MUTEX_INIT_FAST);
}


Certifier::~Certifier()
{
  delete stable_gtid_set;
  delete stable_sid_map;
  delete broadcast_thread;

  clear_incoming();
  delete incoming;

  mysql_mutex_destroy(&LOCK_certifier_map);
}


void Certifier::clear_incoming()
{
  DBUG_ENTER("Certifier::clear_incoming");
  while (!this->incoming->empty())
  {
    Data_packet *packet= NULL;
    this->incoming->pop(&packet);
    delete packet;
  }
  DBUG_VOID_RETURN;
}


int Certifier::initialize(rpl_gno last_delivered_gno)
{
  DBUG_ENTER("Certifier::initialize");

  if (is_initialized())
    DBUG_RETURN(1);

  rpl_gno last_executed_gno= get_last_executed_gno(gcs_cluster_sidno);
  if (last_delivered_gno < 0)
    DBUG_RETURN(1);

  next_seqno= 1 + std::max<rpl_gno>(last_executed_gno, last_delivered_gno);
  DBUG_PRINT("info",
             ("Certifier next sequence number: %lld; last_executed_gno: %lld; last_delivered_gno: %lld",
             next_seqno, last_executed_gno, last_delivered_gno));
  DBUG_EXECUTE_IF("gcs_assert_next_seqno_equal_3",
                  DBUG_ASSERT(next_seqno == 3 &&
                              last_delivered_gno == 2 &&
                              last_executed_gno == 0););
  DBUG_EXECUTE_IF("gcs_assert_next_seqno_equal_4",
                  DBUG_ASSERT(next_seqno == 4 &&
                              last_delivered_gno == 2 &&
                              last_executed_gno == 3););

  int error= broadcast_thread->initialize();
  initialized= !error;

  DBUG_RETURN(error);
}


int Certifier::terminate()
{
  DBUG_ENTER("Certifier::terminate");
  int error= 0;

  if (is_initialized())
    error= broadcast_thread->terminate();

  DBUG_RETURN(error);
}


rpl_gno Certifier::certify(rpl_gno snapshot_timestamp,
                           std::list<const char*> *write_set)
{
  DBUG_ENTER("Certifier::certify");
  rpl_gno result= 0;

  if (!is_initialized())
    DBUG_RETURN(-1);

  if( write_set == NULL)
    DBUG_RETURN(-1);

  mysql_mutex_lock(&LOCK_certifier_map);
  DBUG_EXECUTE_IF("gcs_force_1_negative_certification", {
                  DBUG_SET("-d,gcs_force_1_negative_certification");
                  goto end;});

  for (std::list<const char*>::iterator it= write_set->begin();
       it != write_set->end();
       ++it)
  {
    rpl_gno seq_no= get_seqno(*it);
    DBUG_PRINT("info", ("sequence number in certifier: %llu", seq_no));
    DBUG_PRINT("info", ("snapshot timestamp in certifier: %llu", snapshot_timestamp));
    /*
      If certification DB contains a greater sequence number for the
      transaction write-set, that is the transaction that is being
      certified was executed on outdated data, this transaction will
      be negatively certified.
    */
    if(seq_no > snapshot_timestamp)
      goto end;
  }
  /*
    If the sequence number of the transaction that is being certified
    is the greatest then we increment certifier sequence number.
  */
  result= next_seqno;
  next_seqno++;

  /*
    Add the transaction's write set to certification DB and the
    transaction will be positively certified.
  */
  for(std::list<const char*>::iterator it= write_set->begin();
      it != write_set->end();
      ++it)
  {
    add_item(*it, (result));
  }

end:
  update_certified_transaction_count(result>0);

  mysql_mutex_unlock(&LOCK_certifier_map);
  DBUG_RETURN(result);
}


bool Certifier::add_item(const char* item, rpl_gno seq_no)
{
  DBUG_ENTER("Certifier::add_item");

  cert_db::iterator it;
  pair<cert_db::iterator, bool> ret;

  if(!item)
    DBUG_RETURN(true);

  /* convert item to string for persistence in map */
  string item_str(item);

  it= item_to_seqno_map.find(item_str);
  if(it == item_to_seqno_map.end())
  {
    ret= item_to_seqno_map.insert(pair<string, rpl_gno >(item_str, seq_no));
    DBUG_RETURN(!ret.second);
  }
  else
  {
    it->second= seq_no;
    DBUG_RETURN(false);
  }
}


rpl_gno Certifier::get_seqno(const char* item)
{
  DBUG_ENTER("Certifier::get_seqno");

  if (!is_initialized())
    DBUG_RETURN(0);

  if (!item)
    DBUG_RETURN(0);

  cert_db::iterator it;
  string item_str(item);

  it= item_to_seqno_map.find(item_str);

  if (it == item_to_seqno_map.end())
    DBUG_RETURN(0);
  else
    DBUG_RETURN(it->second);
}


cert_db::iterator Certifier::begin()
{
  return item_to_seqno_map.begin();
}


cert_db::iterator Certifier::end()
{
  return item_to_seqno_map.end();
}


Gtid_set* Certifier::get_group_stable_transactions_set()
{
  DBUG_ENTER("Certifier::get_group_stable_transactions_set");
  DBUG_RETURN(stable_gtid_set);
}


bool Certifier::set_group_stable_transactions_set(Gtid_set* executed_gtid_set)
{
  DBUG_ENTER("Certifier::set_group_stable_transactions_set");

  if (!is_initialized())
    DBUG_RETURN(true);

  if (executed_gtid_set == NULL)
  {
    log_message(MY_ERROR_LEVEL, "Invalid stable transactions set");
    DBUG_RETURN(true);
  }

  if (stable_gtid_set->add_gtid_set(executed_gtid_set) != RETURN_STATUS_OK)
  {
    log_message(MY_ERROR_LEVEL, "Error updating stable transactions set");
    DBUG_RETURN(true);
  }

  garbage_collect();

  DBUG_RETURN(false);
}

void
Certifier::set_gcs_interfaces(Gcs_communication_interface* comm_if,
                              Gcs_control_interface* ctrl_if)
{
  DBUG_ENTER("Certifier::set_gcs_interfaces");

  this->gcs_communication= comm_if;
  this->gcs_control= ctrl_if;

  broadcast_thread->set_gcs_communication(comm_if, ctrl_if);

  DBUG_VOID_RETURN;
}

void
Certifier::set_local_node_info(Cluster_member_info* local_info)
{
  DBUG_ENTER("Certifier::set_local_node_info");

  this->local_node= local_info;

  broadcast_thread->set_local_node_info(local_info);

  DBUG_VOID_RETURN;
}

void Certifier::garbage_collect()
{
  DBUG_ENTER("Certifier::garbage_collect");
  mysql_mutex_lock(&LOCK_certifier_map);

  /*
    When a given transaction is applied on all nodes, we won't need
    more it's certification sequence number to certify new transactions
    that update the same row(s), since all nodes have the same data.
    So we can iterate through certification DB and remove the already
    applied on all nodes transactions data.
  */
  cert_db::iterator it= item_to_seqno_map.begin();
  while (it != item_to_seqno_map.end())
  {
    if(stable_gtid_set->contains_gtid(gcs_cluster_sidno, it->second))
      item_to_seqno_map.erase(it++);
    else
      ++it;
  }

  mysql_mutex_unlock(&LOCK_certifier_map);
  DBUG_VOID_RETURN;
}


int Certifier::handle_certifier_data(uchar *data, uint len)
{
  DBUG_ENTER("Certifier::handle_certifier_data");

  if (!is_initialized())
    DBUG_RETURN(1);

  this->incoming->push(new Data_packet(data, len));

  if (get_gcs_members_number() == this->incoming->size())
    DBUG_RETURN(stable_set_handle());
  DBUG_RETURN(0);
}


int Certifier::stable_set_handle()
{
  DBUG_ENTER("Certifier:stable_set_handle");

  Data_packet *packet= NULL;
  int error= 0;

  Sid_map sid_map(NULL);
  Gtid_set executed_set(&sid_map, NULL);

  /*
    Compute intersection between all received sets.
  */
  while(!error && !this->incoming->empty())
  {
    if ((error= this->incoming->pop(&packet)))
    {
      log_message(MY_ERROR_LEVEL, "Error reading certifier's queue");
      break;
    }

    if (packet == NULL)
    {
      log_message(MY_ERROR_LEVEL, "Null packet on certifier's queue");
      error= 1;
      break;
    }

    uchar* payload= packet->payload;
    Gtid_set member_set(&sid_map, NULL);
    Gtid_set intersection_result(&sid_map, NULL);

    if (member_set.add_gtid_encoding(payload, packet->len) != RETURN_STATUS_OK)
    {
      log_message(MY_ERROR_LEVEL, "Error reading GTIDs from the message");
      error= 1;
    }
    else
    {
      /*
        First member set? If so we only need to add it to executed set.
      */
      if (executed_set.is_empty())
      {
        if (executed_set.add_gtid_set(&member_set))
        {
          log_message(MY_ERROR_LEVEL, "Error processing stable transactions set");
          error= 1;
        }
      }
      else
      {
        /*
          We have three sets:
            member_set:          the one sent from a given member;
            executed_set:        the one that contains the intersection of
                                 the computed sets until now;
            intersection_result: the intersection between set and
                                 intersection_result.
          So we compute the intersection between set and executed_set, and
          set that value to executed_set to be used on the next intersection.
        */
        if (member_set.intersection(&executed_set, &intersection_result) != RETURN_STATUS_OK)
        {
          log_message(MY_ERROR_LEVEL, "Error processing intersection of stable transactions set");
          error= 1;
        }
        else
        {
          executed_set.clear();
          if (executed_set.add_gtid_set(&intersection_result) != RETURN_STATUS_OK)
          {
            log_message(MY_ERROR_LEVEL, "Error processing stable transactions set");
            error= 1;
          }
        }
      }
    }

    delete packet;
  }

  if (!error && set_group_stable_transactions_set(&executed_set))
  {
    log_message(MY_ERROR_LEVEL, "Error setting stable transactions set");
    error= 1;
  }

#if !defined(DBUG_OFF)
  char *executed_set_string= executed_set.to_string();
  DBUG_PRINT("info", ("Certifier stable_set_handle: executed_set: %s", executed_set_string));
  my_free(executed_set_string);
#endif

  DBUG_RETURN(error);
}

void Certifier::handle_view_change()
{
  DBUG_ENTER("Certifier::handle_view_change");
  clear_incoming();
  DBUG_VOID_RETURN;
}


void Certifier::get_certification_info(cert_db *cert_db,
                                       rpl_gno *seq_number)
{
  DBUG_ENTER("Certifier::get_certification_info");
  mysql_mutex_lock(&LOCK_certifier_map);

  (*cert_db).insert(begin(), end());
  *seq_number= next_seqno;

  mysql_mutex_unlock(&LOCK_certifier_map);
  DBUG_VOID_RETURN;
}


void Certifier::set_certification_info(std::map<std::string,
                                       rpl_gno> *cert_db,
                                       rpl_gno sequence_number)
{
  DBUG_ENTER("Certifier::set_certification_info");
  DBUG_ASSERT(cert_db != NULL);
  mysql_mutex_lock(&LOCK_certifier_map);

  item_to_seqno_map.clear();
  item_to_seqno_map.insert(cert_db->begin(), cert_db->end());
  next_seqno= sequence_number;

  mysql_mutex_unlock(&LOCK_certifier_map);
  DBUG_VOID_RETURN;
}

void Certifier::update_certified_transaction_count(bool result)
{
  if(result)
    positive_cert++;
  else
    negative_cert++;
}

ulonglong Certifier::get_positive_certified()
{
  return positive_cert;
}

ulonglong Certifier::get_negative_certified()
{
  return negative_cert;
}

ulonglong Certifier::get_cert_db_size()
{
  return item_to_seqno_map.size();
}

rpl_gno Certifier::get_last_sequence_number()
{
  return next_seqno-1;
}

/*
  Gtid_Executed_Message implementation
 */

Gtid_Executed_Message::Gtid_Executed_Message()
                      : Gcs_plugin_message(PAYLOAD_CERTIFICATION_EVENT)
{
}

Gtid_Executed_Message::~Gtid_Executed_Message()
{
}

void
Gtid_Executed_Message::append_gtid_executed(uchar* gtid_data, size_t len)
{
  data.insert(data.end(), gtid_data, gtid_data+len);
}

void
Gtid_Executed_Message::encode_message(vector<uchar>* buf)
{
  buf->insert(buf->end(), data.begin(), data.end());
}

void
Gtid_Executed_Message::decode_message(uchar* buf, size_t len)
{
  data.insert(data.end(), buf, buf+len);
}
