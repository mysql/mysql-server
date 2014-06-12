/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "sql_class.h"
#include "rpl_context.h"

Session_gtids_ctx::Session_gtids_ctx() : m_sid_map(NULL),
        m_gtid_set(NULL), listener(NULL) { }

Session_gtids_ctx::~Session_gtids_ctx()
{
  if (m_gtid_set)
  {
    delete m_gtid_set;
    m_gtid_set= NULL;
  }

  if (m_sid_map)
  {
    delete m_sid_map;
    m_sid_map= NULL;
  }
}

inline bool Session_gtids_ctx::shall_collect(THD* thd)
{
  return  gtid_mode == GTID_MODE_ON &&
          // if there is no listener/tracker, then there is no reason to collect
          this->listener != NULL &&
          /* ROLLBACK statements may end up calling trans_commit_stmt */
          thd->lex->sql_command != SQLCOM_ROLLBACK &&
          thd->lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT;
}

bool Session_gtids_ctx::notify_after_transaction_commit(THD* thd)
{
  DBUG_ENTER("Rpl_consistency_ctx::notify_after_transaction_commit");
  DBUG_ASSERT(thd);
  bool res= false;

  if (!shall_collect(thd))
    DBUG_RETURN(res);

  if (thd->variables.session_track_gtids == ALL_GTIDS)
  {
    /*
     If one is configured to read all writes, we always collect
     the GTID_EXECUTED.

     TODO: in the future optimize to collect deltas instead maybe.
    */
    global_sid_lock->rdlock();
    res= m_gtid_set->add_gtid_set(gtid_state->get_executed_gtids()) != RETURN_STATUS_OK;
    global_sid_lock->unlock();

    if (!res)
      this->notify_ctx_change_listener();
  }

  DBUG_RETURN(res);
}

bool Session_gtids_ctx::notify_after_transaction_replicated(THD *thd)
{
  DBUG_ENTER("Rpl_consistency_ctx::notify_after_transaction_replicated");
  DBUG_ASSERT(thd);
  bool res= false;

  if (!shall_collect(thd))
    DBUG_RETURN(res);

  const Gtid& gtid= thd->owned_gtid;
  if (thd->variables.session_track_gtids == OWN_GTID &&
      gtid.sidno >= 1)
  {
    DBUG_ASSERT(!m_gtid_set->contains_gtid(gtid.sidno, gtid.gno));
    res= (m_gtid_set->ensure_sidno(gtid.sidno) != RETURN_STATUS_OK ||
          m_gtid_set->_add_gtid(gtid.sidno, gtid.gno) != RETURN_STATUS_OK);

    if (!res)
      this->notify_ctx_change_listener();
  }
  DBUG_RETURN(res);
}

bool Session_gtids_ctx::notify_after_response_packet(THD *thd)
{
  int res= false;
  DBUG_ENTER("Rpl_consistency_ctx::notify_after_response_packet");
  if (gtid_mode != GTID_MODE_ON)
    DBUG_RETURN(res);

  if (m_gtid_set && !m_gtid_set->is_empty())
    m_gtid_set->clear();

  DBUG_RETURN(res);
}

void
Session_gtids_ctx::register_ctx_change_listener(
              Session_gtids_ctx::Ctx_change_listener* listener)
{
  DBUG_ASSERT(this->listener == NULL || this->listener == listener);
  if (this->listener == NULL)
  {
    DBUG_ASSERT(m_sid_map == NULL && m_gtid_set == NULL);
    this->listener= listener;
    m_sid_map= new Sid_map(NULL);
    m_gtid_set= new Gtid_set(m_sid_map);
  }
}

void Session_gtids_ctx::unregister_ctx_change_listener(
             Session_gtids_ctx::Ctx_change_listener* listener)
{
  DBUG_ASSERT(this->listener == listener || this->listener == NULL);

  if (m_gtid_set)
    delete m_gtid_set;

  if (m_sid_map)
    delete m_sid_map;

  this->listener= NULL;
  m_gtid_set= NULL;
  m_sid_map= NULL;
}