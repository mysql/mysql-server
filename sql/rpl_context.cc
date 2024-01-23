/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_context.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "mysql/binlog/event/compression/factory.h"
#include "mysql/binlog/event/control_events.h"  // Transaction_payload_event
#include "sql/binlog/group_commit/bgc_ticket_manager.h"  // Bgc_ticket_manager
#include "sql/binlog_ostream.h"
#include "sql/psi_memory_resource.h"  // memory_resource
#include "sql/rpl_gtid.h"             // Gtid_set
#include "sql/sql_class.h"            // THD
#include "sql/sql_lex.h"
#include "sql/system_variables.h"

Session_consistency_gtids_ctx::Session_consistency_gtids_ctx()
    : m_tsid_map(nullptr),
      m_gtid_set(nullptr),
      m_listener(nullptr),
      m_curr_session_track_gtids(SESSION_TRACK_GTIDS_OFF) {}

Session_consistency_gtids_ctx::~Session_consistency_gtids_ctx() {
  if (m_gtid_set) {
    delete m_gtid_set;
    m_gtid_set = nullptr;
  }

  if (m_tsid_map) {
    delete m_tsid_map;
    m_tsid_map = nullptr;
  }
}

inline bool Session_consistency_gtids_ctx::shall_collect(const THD *thd) {
  return /* Do not track OWN_GTID if session does not own a
            (non-anonymous) GTID. */
      (thd->owned_gtid.sidno > 0 ||
       m_curr_session_track_gtids == SESSION_TRACK_GTIDS_ALL_GTIDS) &&
      /* if there is no listener/tracker, then there is no reason to collect */
      m_listener != nullptr &&
      /* ROLLBACK statements may end up calling trans_commit_stmt */
      thd->lex->sql_command != SQLCOM_ROLLBACK &&
      thd->lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT;
}

bool Session_consistency_gtids_ctx::notify_after_transaction_commit(
    const THD *thd) {
  DBUG_TRACE;
  assert(thd);
  bool res = false;

  if (!shall_collect(thd)) return res;

  if (m_curr_session_track_gtids == SESSION_TRACK_GTIDS_ALL_GTIDS) {
    /*
     If one is configured to read all writes, we always collect
     the GTID_EXECUTED.

     NOTE: in the future optimize to collect deltas instead maybe.
    */
    global_tsid_lock->wrlock();
    res = m_gtid_set->add_gtid_set(gtid_state->get_executed_gtids()) !=
          RETURN_STATUS_OK;
    global_tsid_lock->unlock();

    if (!res) notify_ctx_change_listener();
  }

  return res;
}

bool Session_consistency_gtids_ctx::notify_after_gtid_executed_update(
    const THD *thd) {
  DBUG_TRACE;
  assert(thd);
  bool res = false;

  if (!shall_collect(thd)) return res;

  if (m_curr_session_track_gtids == SESSION_TRACK_GTIDS_OWN_GTID) {
    assert(global_gtid_mode.get() != Gtid_mode::OFF);
    assert(thd->owned_gtid.sidno > 0);
    const Gtid &gtid = thd->owned_gtid;
    if (gtid.sidno == -1)  // we need to add thd->owned_gtid_set
    {
      /* Caller must only call this function if the set was not empty. */
#ifdef HAVE_GTID_NEXT_LIST
      assert(!thd->owned_gtid_set.is_empty());
      res = m_gtid_set->add_gtid_set(&thd->owned_gtid_set) != RETURN_STATUS_OK;
#else
      assert(0);
#endif
    } else if (gtid.sidno > 0)  // only one gtid
    {
      /*
        Note that the interface is such that m_tsid_map must contain
        sidno before we add the gtid to m_gtid_set.

        Thus, to avoid relying on global_tsid_map and thus contributing
        to increased contention, we arrange for sidnos on the local
        sid map.
      */
      rpl_sidno local_set_sidno = m_tsid_map->add_tsid(thd->owned_tsid);

      assert(!m_gtid_set->contains_gtid(local_set_sidno, gtid.gno));
      res = m_gtid_set->ensure_sidno(local_set_sidno) != RETURN_STATUS_OK;
      if (!res) m_gtid_set->_add_gtid(local_set_sidno, gtid.gno);
    }

    if (!res) notify_ctx_change_listener();
  }
  return res;
}

void Session_consistency_gtids_ctx::
    update_tracking_activeness_from_session_variable(const THD *thd) {
  m_curr_session_track_gtids = thd->variables.session_track_gtids;
}

bool Session_consistency_gtids_ctx::notify_after_response_packet(
    const THD *thd) {
  int res = false;
  DBUG_TRACE;

  if (m_gtid_set && !m_gtid_set->is_empty()) m_gtid_set->clear();

  /*
   Every time we get a notification that a packet was sent, we update
   this value. It may have changed (the previous command may have been
   a SET SESSION session_track_gtids=...;).
   */
  update_tracking_activeness_from_session_variable(thd);
  return res;
}

void Session_consistency_gtids_ctx::register_ctx_change_listener(
    Session_consistency_gtids_ctx::Ctx_change_listener *listener, THD *thd) {
  assert(m_listener == nullptr || m_listener == listener);
  if (m_listener == nullptr) {
    assert(m_tsid_map == nullptr && m_gtid_set == nullptr);
    m_listener = listener;
    m_tsid_map = new Tsid_map(nullptr);
    m_gtid_set = new Gtid_set(m_tsid_map);

    /*
     Caches the value at startup if needed. This is called during THD::init,
     if the session_track_gtids value is set at startup time to anything
     different than OFF.
     */
    update_tracking_activeness_from_session_variable(thd);
  }
}

void Session_consistency_gtids_ctx::unregister_ctx_change_listener(
    Session_consistency_gtids_ctx::Ctx_change_listener *listener
    [[maybe_unused]]) {
  assert(m_listener == listener || m_listener == nullptr);

  if (m_gtid_set) delete m_gtid_set;

  if (m_tsid_map) delete m_tsid_map;

  m_listener = nullptr;
  m_gtid_set = nullptr;
  m_tsid_map = nullptr;
}

Last_used_gtid_tracker_ctx::Last_used_gtid_tracker_ctx() {
  m_last_used_gtid = std::unique_ptr<Gtid>(new Gtid{0, 0});
}

Last_used_gtid_tracker_ctx::~Last_used_gtid_tracker_ctx() = default;

void Last_used_gtid_tracker_ctx::set_last_used_gtid(
    const Gtid &gtid, const mysql::gtid::Tsid &tsid) {
  (*m_last_used_gtid).set(gtid.sidno, gtid.gno);
  m_last_used_tsid = tsid;
}

void Last_used_gtid_tracker_ctx::get_last_used_gtid(Gtid &gtid) {
  gtid.sidno = (*m_last_used_gtid).sidno;
  gtid.gno = (*m_last_used_gtid).gno;
}

void Last_used_gtid_tracker_ctx::get_last_used_tsid(mysql::gtid::Tsid &tsid) {
  tsid = m_last_used_tsid;
}

Transaction_compression_ctx::Transaction_compression_ctx(PSI_memory_key key)
    : m_managed_buffer_memory_resource(psi_memory_resource(key)),
      m_managed_buffer_sequence(Grow_calculator_t(),
                                m_managed_buffer_memory_resource) {}

Transaction_compression_ctx::Compressor_ptr_t
Transaction_compression_ctx::get_compressor(THD *thd) {
  auto ctype = (mysql::binlog::event::compression::type)
                   thd->variables.binlog_trx_compression_type;

  if (m_compressor == nullptr || (m_compressor->get_type_code() != ctype)) {
    m_compressor = Compressor_ptr_t(
        Factory_t::build_compressor(ctype, m_managed_buffer_memory_resource));
  }
  return m_compressor;
}

Transaction_compression_ctx::Managed_buffer_sequence_t &
Transaction_compression_ctx::managed_buffer_sequence() {
  return m_managed_buffer_sequence;
}

binlog::BgcTicket Binlog_group_commit_ctx::get_session_ticket() {
  return this->m_session_ticket;
}

void Binlog_group_commit_ctx::set_session_ticket(binlog::BgcTicket ticket) {
  if (Binlog_group_commit_ctx::manual_ticket_setting()->load()) {
    assert(this->m_session_ticket.is_set() == false);
    this->m_session_ticket = ticket;
  }
}

void Binlog_group_commit_ctx::assign_ticket() {
  if (this->m_session_ticket.is_set()) {
    return;
  }
  auto ticket_opaque =
      binlog::Bgc_ticket_manager::instance().assign_session_to_ticket();
  this->m_session_ticket = ticket_opaque;
}

bool Binlog_group_commit_ctx::has_waited() { return this->m_has_waited; }

void Binlog_group_commit_ctx::mark_as_already_waited() {
  this->m_has_waited = true;
}

void Binlog_group_commit_ctx::reset() {
  this->m_session_ticket = binlog::BgcTicket(binlog::BgcTicket::kTicketUnset);
  this->m_has_waited = false;
  m_max_size_exceeded = false;
  m_force_rotate = false;
}

std::string Binlog_group_commit_ctx::to_string() const {
  std::ostringstream oss;
  this->format(oss);
  return oss.str();
}

void Binlog_group_commit_ctx::format(std::ostream &out) const {
  out << "Binlog_group_commit_ctx (" << std::hex << this << std::dec
      << "):" << std::endl
      << " · m_session_ticket: " << this->m_session_ticket << std::endl
      << " · m_has_waited: " << this->m_has_waited << std::endl
      << " · manual_ticket_setting(): " << manual_ticket_setting()->load()
      << std::flush;
}

memory::Aligned_atomic<bool> &Binlog_group_commit_ctx::manual_ticket_setting() {
  static memory::Aligned_atomic<bool> flag{false};
  return flag;
}

std::pair<bool, bool> Binlog_group_commit_ctx::aggregate_rotate_settings(
    THD *queue) {
  bool exceeded = false;
  bool force_rotate = false;
  for (THD *thd = queue; thd; thd = thd->next_to_commit) {
    exceeded |= thd->rpl_thd_ctx.binlog_group_commit_ctx().m_max_size_exceeded;
    force_rotate |= thd->rpl_thd_ctx.binlog_group_commit_ctx().m_force_rotate;
  }
  return {exceeded, force_rotate};
}

void Rpl_thd_context::init() {
  m_tx_rpl_delegate_stage_status = TX_RPL_STAGE_INIT;
}

void Rpl_thd_context::set_tx_rpl_delegate_stage_status(
    enum_transaction_rpl_delegate_status status) {
  m_tx_rpl_delegate_stage_status = status;
}

Rpl_thd_context::enum_transaction_rpl_delegate_status
Rpl_thd_context::get_tx_rpl_delegate_stage_status() {
  return m_tx_rpl_delegate_stage_status;
}

Binlog_group_commit_ctx &Rpl_thd_context::binlog_group_commit_ctx() {
  return this->m_binlog_group_commit_ctx;
}
