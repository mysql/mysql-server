/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <signal.h>

#include "certifier.h"
#include "plugin.h"
#include "plugin_log.h"


static void *launch_broadcast_thread(void* arg)
{
  Certifier_broadcast_thread *handler= (Certifier_broadcast_thread*) arg;
  handler->dispatcher();
  return 0;
}

Certifier_broadcast_thread::
Certifier_broadcast_thread(Gcs_communication_interface* comm_intf,
                           Gcs_control_interface* ctrl_intf,
                           Group_member_info* local_member_info)
  :aborted(false), broadcast_pthd_running(false), gcs_communication(comm_intf),
   gcs_control(ctrl_intf), local_member_info(local_member_info)
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

  if (this->local_member_info == NULL ||
      this->local_member_info->get_recovery_status() !=
                                        Group_member_info::MEMBER_ONLINE)
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
  :gcs_communication(NULL), gcs_control(NULL), local_member_info(NULL),
   initialized(false), next_seqno(1), positive_cert(0), negative_cert(0)
{
  certification_info_sid_map= new Sid_map(NULL);
  incoming= new Synchronized_queue<Data_packet*>();
  stable_sid_map= new Sid_map(NULL);
  stable_gtid_set= new Gtid_set(stable_sid_map, NULL);
  broadcast_thread= new Certifier_broadcast_thread(gcs_communication,
                                                   gcs_control,
                                                   local_member_info);

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_info certifier_mutexes[]=
  {
    { &key_LOCK_certification_info, "LOCK_certification_info", 0}
  };
  register_group_replication_psi_keys(certifier_mutexes, 1, NULL, 0);
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(key_LOCK_certification_info, &LOCK_certification_info,
                   MY_MUTEX_INIT_FAST);
}


Certifier::~Certifier()
{
  clear_certification_info();
  delete certification_info_sid_map;

  delete stable_gtid_set;
  delete stable_sid_map;
  delete broadcast_thread;

  clear_incoming();
  delete incoming;

  mysql_mutex_destroy(&LOCK_certification_info);
}


void Certifier::clear_certification_info()
{
  for (Certification_info::iterator it= certification_info.begin();
       it != certification_info.end();
       ++it)
    delete it->second;

  certification_info.clear();
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

  rpl_gno last_executed_gno= get_last_executed_gno(group_sidno);
  if (last_delivered_gno < 0)
    DBUG_RETURN(1);

  next_seqno= 1 + std::max<rpl_gno>(last_executed_gno, last_delivered_gno);
  DBUG_PRINT("info",
             ("Certifier next sequence number: %lld; last_executed_gno: %lld; last_delivered_gno: %lld",
             next_seqno, last_executed_gno, last_delivered_gno));
  DBUG_EXECUTE_IF("certifier_assert_next_seqno_equal_3",
                  DBUG_ASSERT(next_seqno == 3 &&
                              last_delivered_gno == 2 &&
                              last_executed_gno == 0););
  DBUG_EXECUTE_IF("certifier_assert_next_seqno_equal_4",
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


rpl_gno Certifier::certify(const Gtid_set *snapshot_version,
                           std::list<const char*> *write_set,
                           bool generate_group_id)
{
  DBUG_ENTER("Certifier::certify");
  rpl_gno result= 0;

  if (!is_initialized())
    DBUG_RETURN(-1);

  if (write_set == NULL)
    DBUG_RETURN(-1);

  mysql_mutex_lock(&LOCK_certification_info);
  DBUG_EXECUTE_IF("certifier_force_1_negative_certification", {
                  DBUG_SET("-d,certifier_force_1_negative_certification");
                  goto end;});

  for (std::list<const char*>::iterator it= write_set->begin();
       it != write_set->end();
       ++it)
  {
    Gtid_set *certified_write_set_snapshot_version=
        get_certified_write_set_snapshot_version(*it);

    /*
      If the incoming transaction snapshot version is a subset of a
      previous certified transaction for the same write set, the current
      transaction was executed on top of outdated data, so it will be
      negatively certified. Otherwise, this transaction is marked
      certified and goes into applier.
    */
    if (certified_write_set_snapshot_version != NULL &&
        snapshot_version->is_subset(certified_write_set_snapshot_version))
      goto end;
  }

  /*
    If the current transaction doesn't have a specified GTID, one
    for group UUID will be generated.
    This situation happens when transactions are executed with
    GTID_NEXT equal to AUTOMATIC_GROUP (the default case).
  */
  if (generate_group_id)
  {
    result= next_seqno;
    next_seqno++;
    DBUG_PRINT("info",
               ("Group replication Certifier: generated sequence number: %llu",
                result));
  }
  else
  {
    result= 1;
    DBUG_PRINT("info",
               ("Group replication Certifier: there was no sequence number "
                "generated since transaction already had a GTID specified"));
  }

  /*
    Add the transaction's write set to certification DB and the
    transaction will be positively certified.
  */
  for(std::list<const char*>::iterator it= write_set->begin();
      it != write_set->end();
      ++it)
  {
    add_item(*it, snapshot_version);
  }

end:
  update_certified_transaction_count(result>0);

  mysql_mutex_unlock(&LOCK_certification_info);
  DBUG_PRINT("info", ("Group replication Certifier: certification result: %llu",
                      result));
  DBUG_RETURN(result);
}


bool Certifier::add_item(const char* item, const Gtid_set *snapshot_version)
{
  DBUG_ENTER("Certifier::add_item");
  bool error= true;
  std::string key;
  Gtid_set *value= NULL;
  Certification_info::iterator it;

  if(!item)
    goto end;

  /* Convert key and value for persistence in map. */
  key.assign(item);
  value = new Gtid_set(certification_info_sid_map);
  if (value->add_gtid_set(snapshot_version) != RETURN_STATUS_OK)
    goto end;

  it= certification_info.find(key);
  if(it == certification_info.end())
  {
    std::pair<Certification_info::iterator, bool> ret=
        certification_info.insert(std::pair<std::string, Gtid_set*>(key, value));
    error= !ret.second;
  }
  else
  {
    delete it->second;
    it->second= value;
    error= false;
  }

end:
  if (error)
    delete value;
  DBUG_RETURN(error);
}


Gtid_set *Certifier::get_certified_write_set_snapshot_version(const char* item)
{
  DBUG_ENTER("Certifier::get_certified_write_set_snapshot_version");

  if (!is_initialized())
    DBUG_RETURN(NULL);

  if (!item)
    DBUG_RETURN(NULL);

  Certification_info::iterator it;
  std::string item_str(item);

  it= certification_info.find(item_str);

  if (it == certification_info.end())
    DBUG_RETURN(NULL);
  else
    DBUG_RETURN(it->second);
}


Certification_info::iterator Certifier::begin()
{
  return certification_info.begin();
}


Certification_info::iterator Certifier::end()
{
  return certification_info.end();
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
Certifier::set_local_member_info(Group_member_info* local_info)
{
  DBUG_ENTER("Certifier::set_local_member_info");

  this->local_member_info= local_info;

  broadcast_thread->set_local_member_info(local_info);

  DBUG_VOID_RETURN;
}

void Certifier::garbage_collect()
{
  DBUG_ENTER("Certifier::garbage_collect");
  mysql_mutex_lock(&LOCK_certification_info);

  /*
    When a transaction "t" is applied to all group members and for all
    ongoing, i.e., not yet committed or aborted transactions,
    "t" was already committed when they executed (thus "t"
    precedes them), then "t" is stable and can be removed from
    the certification info.
  */
  Certification_info::iterator it= certification_info.begin();
  while (it != certification_info.end())
  {
    if (it->second->is_subset_not_equals(stable_gtid_set))
    {
      delete it->second;
      certification_info.erase(it++);
    }
    else
      ++it;
  }

  mysql_mutex_unlock(&LOCK_certification_info);
  DBUG_VOID_RETURN;
}


int Certifier::handle_certifier_data(uchar *data, uint len)
{
  DBUG_ENTER("Certifier::handle_certifier_data");

  if (!is_initialized())
    DBUG_RETURN(1);

  this->incoming->push(new Data_packet(data, len));

  if (plugin_get_group_members_number() == this->incoming->size())
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


void Certifier::get_certification_info(std::map<std::string, std::string> *cert_info,
                                       rpl_gno *sequence_number)
{
  DBUG_ENTER("Certifier::get_certification_info");
  mysql_mutex_lock(&LOCK_certification_info);

  for(Certification_info::iterator it = certification_info.begin();
      it != certification_info.end(); ++it)
  {
    std::string key= it->first;

    size_t len= it->second->get_encoded_length();
    uchar* buf= (uchar *)my_malloc(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                                   PSI_NOT_INSTRUMENTED,
#endif
                                   len, MYF(0));
    it->second->encode(buf);
    std::string value(reinterpret_cast<const char*>(buf), len);
    my_free(buf);

    (*cert_info).insert(std::pair<std::string, std::string>(key, value));
  }
  *sequence_number= next_seqno;

  mysql_mutex_unlock(&LOCK_certification_info);
  DBUG_VOID_RETURN;
}


void Certifier::set_certification_info(std::map<std::string, std::string> *cert_info,
                                       rpl_gno sequence_number)
{
  DBUG_ENTER("Certifier::set_certification_info");
  DBUG_ASSERT(cert_info != NULL);
  mysql_mutex_lock(&LOCK_certification_info);

  clear_certification_info();
  for(std::map<std::string, std::string>::iterator it = cert_info->begin();
      it != cert_info->end(); ++it)
  {
    std::string key= it->first;
    Gtid_set *value = new Gtid_set(certification_info_sid_map);
    value->add_gtid_encoding((const uchar*) it->second.c_str(), it->second.length());
    certification_info.insert(std::pair<std::string, Gtid_set*>(key, value));
  }
  next_seqno= sequence_number;

  mysql_mutex_unlock(&LOCK_certification_info);
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

ulonglong Certifier::get_certification_info_size()
{
  return certification_info.size();
}

rpl_gno Certifier::get_last_sequence_number()
{
  return next_seqno-1;
}

/*
  Gtid_Executed_Message implementation
 */

Gtid_Executed_Message::Gtid_Executed_Message()
                      : Plugin_gcs_message(PAYLOAD_CERTIFICATION_EVENT)
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
