/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_CERTIFIER
#define GCS_CERTIFIER

#include "../gcs_plugin_utils.h"
#include <replication.h>
#include <log_event.h>
#include <log.h>
#include <map>
#include <string>
#include <list>

/**
  This class is a core component of the database state machine
  replication protocol. It implements conflict detection based
  on a certification procedure.

  Snapshot Isolation is based on assigning logical timestamp to optimistic
  transactions, i.e. the ones which successfully meet certification and
  are good to commit on all nodes in the group. This timestamp is a
  monotonically increasing counter, and is same across all nodes in the group.

  This timestamp is further used to update the certification Database, which
  maps the items in a transaction to the last optimistic Transaction Id
  that modified the particular item.
  The items here are extracted as part of the write-sets of a transaction.

  For the incoming transaction, if the items in its writeset are modified
  by any transaction which was optimistically certified to commit has a
  sequence number greater than the timestamp seen by the incoming transaction
  then it is not certified to commit. Otherwise, this transaction is marked
  certified and is later written to the Relay log of the participating node.

*/

typedef std::map<std::string, rpl_gno> cert_db;
typedef std::list<const char*> item_list;

class Certifier
{

public:

  Certifier();
  /**
    This member function SHALL certify the set of items against transactions
    that have already passed the certification test.

    @param tcle     The incoming transaction context log event to be certified.
    @returns        sequence number if successfully certified, else returns
                    zero.
   */
  rpl_gno certify(Transaction_context_log_event* tcle);

  cert_db::iterator begin()
  {
    return item_to_seqno_map.begin();
  }

  cert_db::iterator end()
  {
    return item_to_seqno_map.end();
  }

  /**
    Retrieves the current certification db and sequence number.

     @note if concurrent access is introduce to these variables,
     locking is needed in this method

     @param[out] cert_db     a pointer to retrieve the certification database
     @param[out] seq_number  a pointer to retrieve the sequence number
  */
  void get_certification_info(cert_db **cert_db, rpl_gno *seq_number)
  {
    *cert_db= &item_to_seqno_map;
    *seq_number= next_seqno;
  }

  /**
    Sets the certification db and sequence number according to the given values.

    @note if concurrent access is introduce to these variables,
    locking is needed in this method

    @param cert_db
    @param sequence_number
  */
  void set_certification_info(std::map<std::string, rpl_gno> *cert_db,
                              rpl_gno sequence_number)
  {
    item_to_seqno_map.clear();

    DBUG_ASSERT(cert_db != NULL);

    std::map<std::string, rpl_gno>::iterator iter;

    for (iter= cert_db->begin(); iter!= cert_db->end(); iter++)
    {
      const char* key= iter->first.c_str();
      add_item(key, iter->second);
    }

    next_seqno= sequence_number;
  }

private:

  /**
    Adds an item from transaction writeset to the certification DB.
    @param[in]   item       item in the writeset to be added to the
                            Certification DB.
    @param[in]  seq_no      Sequence number of the incoming transaction
                            which modified the above mentioned item.
    @return
    @retval      TRUE       succesfully added to the map.
  */
  bool add_item(const char* item, rpl_gno seq_no);
  /**
    Find the seq_no object corresponding to an item. Return if
    it exists, other wise return NULL;

    @param[in]  item          item for the seq_no object.
    @retval                   seq_no object if exists in the map.
                              Otherwise, NULL;
  */
  rpl_gno get_seqno(const char* item);
  rpl_gno next_seqno;
  cert_db item_to_seqno_map;

};

#endif /* GCS_CERTIFIER */
