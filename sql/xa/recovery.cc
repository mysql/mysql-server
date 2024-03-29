/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/xa/recovery.h"

#include "my_loglevel.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/mysqld.h"  // tc_heuristic_recover

namespace {  // Compilation unit local types and functions
/**
  Pair of tuples used to store the information about success and failures
  of committing, rolling back or preparing transactions. Counters are
  organized as follows

   {
     {Failed commits, Failed rollbacks, Failed prepares},
     {Successful commits, Successful rallbacks, Successful prepares}
   }
 */
using recovery_statistics = std::pair<std::tuple<size_t, size_t, size_t>,
                                      std::tuple<size_t, size_t, size_t>>;
constexpr size_t STATS_FAILURE = 0,    // The failure tuple
    STATS_SUCCESS = 1;                 // The success tuple
constexpr size_t STATS_COMMITTED = 0,  // The committed counter
    STATS_ROLLEDBACK = 1,              // The rolled back counter
    STATS_PREPARED = 2;                // The preapred counter

/**
  Processes an internally coordinated transaction against the transaction
  coordinator internal state.

  Searches the TC information on committed transactions,
  `xarecover_st::commit_list` for the XID of the transaction:

  1. If found and the TC recovery heuristics is
     `TC_HEURISTIC_RECOVER_COMMIT`, commits the transaction in the storage
     engine.
  2. If not, rolls it back in the storage engine.

  @param info TC internal state w.r.t transaction state
  @param ht The plugin interface for the storage engine to recover the
            transaction for
  @param xa_trx The information about the transaction to be recovered
  @param xid The internal XID of the transaction to be recovered
  @param stats Repository of statistical information about transaction
               recovery success and failures
 */
void recover_one_internal_trx(xarecover_st const &info, handlerton &ht,
                              XA_recover_txn const &xa_trx, my_xid xid,
                              ::recovery_statistics &stats);
/**
  Processes an externally coordinated transaction against the transaction
  coordinator internal state.

  Searches the TC information on externally coordinated transactions,
  `xarecover_st::xa_list` for the XID of the transaction:

  1. If found, checks the state of the transaction:
     a. If the state is `COMMITTED`, the transaction is committed in the
        storage engine.
     b. If the state is `ROLLEDBACK`, the transaction is rolled back in the
        storage engine.
     c. If the state is `PREPARED`, the transaction is kept in prepared
        state, added to the list of recovered transactions, visible with
        `XA RECOVER` and storage engines are instructed to move the
        transaction to the `PREPARED_IN_TC` state.
  2. If not, rolls it back in the storage engine.

  @param info TC internal state w.r.t to transaction state
  @param ht The plugin interface for the storage engine to recover the
            transaction for
  @param xa_trx The information about the transaction to be recovered
  @param stats Repository of statistical information about transaction
               recovery success and failures
 */
void recover_one_external_trx(xarecover_st const &info, handlerton &ht,
                              XA_recover_txn const &xa_trx,
                              ::recovery_statistics &stats);
/**
  Changes the given stats object by adding 1 to the given counter `counter`
  in the tuple `state`.

  @param stats The statistics object to change the counter for.
 */
template <size_t state, size_t counter>
void add_to_stats(::recovery_statistics &stats);
/**
  Checks if there are any non-zero failure counters in the fiven statistics
  object.

  @param stats The statistics object to check for non-zero failure
               counters.

  @return true if any non-zero failure counters were found, false otherwise.
 */
bool has_failures(::recovery_statistics const &stats);
/**
  Composes a string presenting the statistics for the given objects.

  @param internal_stats The object containing statistics for internally
                        coordinated transactions.
  @param external_stats The object containing statistics for externally
                        transactions.

  @return a string with the textual representation of the given statistics.
 */
std::string print_stats(::recovery_statistics const &internal_stats,
                        ::recovery_statistics const &external_stats);
/**
  Prints to the given string stream the textual representation of the
  statistics stored in the given object.

  @param stats The object containing the statistics.
  @param trx_type A string containing the textual description of the type
                  of transaction the statistics refer to.
  @param oss The string stream to print the textual representation to.

  @return true if any non-zero counter was found and something was written
          to the string stream, false otherwise.
 */
bool print_stat(::recovery_statistics const &stats, std::string const &trx_type,
                std::ostringstream &oss);
/**
  Logs a message to the error log about failure to commit, rollback or
  prepare a transaction. The severity level used is INFORMATION_LEVEL.

  @param error The error to be reported, one of
                ER_BINLOG_CRASH_RECOVERY_COMMIT_FAILED,
                ER_BINLOG_CRASH_RECOVERY_ROLLBACK_FAILED,
                ER_BINLOG_CRASH_RECOVERY_PREPARE_FAILED
  @param id The identifier of the transaction. A templated type is used to
             allow to print either the internal ID for internally
             coordinated transactions or the XID for externally coordinated
             transactions.
  @param ht The handlerton for the storage engine that failed to complete
            the action.
  @param failure_code The `xa_status_code` failure code returned by the
                      SE. If no `xa_status_code` is returned from SE, XA_OK
                      should passed to avoing adding the code to the
                      message.
  @param is_xa Whether or not the `id` refers to an XA transaction.
 */
template <typename ID>
void report_trx_recovery_error(int error, ID const &id, handlerton const &ht,
                               enum xa_status_code failure_code,
                               bool is_xa = false);
/**
  Returns an XA status code according to active debug symbols. If none of
  the targeted debug symbols are active, will return XA_OK.

  @return one of XAER_ASYNC, XAER_RMERR, XAER_NOTA, XAER_INVAL, XAER_PROTO,
          XAER_RMFAIL, XAER_DUPID, XAER_OUTSIDE if associated debug symbol
          is active, XA_OK otherwise.
 */
enum xa_status_code generate_xa_recovery_error();
}  // namespace

bool xa::recovery::recover_prepared_in_tc_one_ht(THD *, plugin_ref plugin,
                                                 void *arg) {
  handlerton *ht = plugin_data<handlerton *>(plugin);
  xarecover_st *info = static_cast<struct xarecover_st *>(arg);

  if (ht->state == SHOW_OPTION_YES && ht->recover_prepared_in_tc) {
    assert(info->xa_list != nullptr);
    return ht->recover_prepared_in_tc(ht, *info->xa_list);
  }
  return false;
}

bool xa::recovery::recover_one_ht(THD *, plugin_ref plugin, void *arg) {
  handlerton *ht = plugin_data<handlerton *>(plugin);
  xarecover_st *info = static_cast<struct xarecover_st *>(arg);
  int got;

  if (ht->state == SHOW_OPTION_YES && ht->recover) {
    ::recovery_statistics external_stats{{0, 0, 0}, {0, 0, 0}};
    ::recovery_statistics internal_stats{{0, 0, 0}, {0, 0, 0}};
    while (
        (got = ht->recover(
             ht, info->list, info->len,
             Recovered_xa_transactions::instance().get_allocated_memroot())) >
        0) {
      assert(got <= info->len);
      LogErr(INFORMATION_LEVEL, ER_XA_RECOVER_FOUND_TRX_IN_SE, got,
             ha_resolve_storage_engine_name(ht));

      for (int i = 0; i < got; ++i) {
        auto &xa_trx = info->list[i];
        my_xid xid = xa_trx.id.get_my_xid();

        if (!xid) {  // Externally coordinated transaction
          ::recover_one_external_trx(*info, *ht, xa_trx, external_stats);
          ++info->found_foreign_xids;
          continue;
        }

        if (info->dry_run) {  // No information provided w.r.t TC state so,
                              // nothing to do in regards to internally
                              // coordinated transactions
          ++info->found_my_xids;
          continue;
        }

        // Internally coordinated transaction
        ::recover_one_internal_trx(*info, *ht, xa_trx, xid, internal_stats);
      }
      if (got < info->len) break;
    }
    bool has_failures =
        ::has_failures(internal_stats) || ::has_failures(external_stats);
    LogErr(has_failures ? ERROR_LEVEL : INFORMATION_LEVEL,
           ER_BINLOG_CRASH_RECOVERY_ENGINE_RESULTS,
           ha_resolve_storage_engine_name(ht),
           ::print_stats(internal_stats, external_stats).data());
    DBUG_EXECUTE_IF("xa_recovery_error_reporting", return has_failures;);
  }
  return false;
}

namespace {
void recover_one_internal_trx(xarecover_st const &info, handlerton &ht,
                              XA_recover_txn const &xa_trx, my_xid xid,
                              ::recovery_statistics &stats) {
  if (info.commit_list ? info.commit_list->count(xid) != 0
                       : tc_heuristic_recover == TC_HEURISTIC_RECOVER_COMMIT) {
    enum xa_status_code exec_status;
    if (DBUG_EVALUATE_IF("xa_recovery_error_reporting", true, false))
      exec_status = ::generate_xa_recovery_error();
    else
      exec_status = ht.commit_by_xid(&ht, const_cast<XID *>(&xa_trx.id));

    if (exec_status == XA_OK)
      ::add_to_stats<STATS_SUCCESS, STATS_COMMITTED>(stats);
    else {
      ::add_to_stats<STATS_FAILURE, STATS_COMMITTED>(stats);
      ::report_trx_recovery_error(ER_BINLOG_CRASH_RECOVERY_COMMIT_FAILED, xid,
                                  ht, exec_status);
    }
  } else {
    enum xa_status_code exec_status;
    if (DBUG_EVALUATE_IF("xa_recovery_error_reporting", true, false))
      exec_status = ::generate_xa_recovery_error();
    else
      exec_status = ht.rollback_by_xid(&ht, const_cast<XID *>(&xa_trx.id));

    if (exec_status == XA_OK)
      ::add_to_stats<STATS_SUCCESS, STATS_ROLLEDBACK>(stats);
    else {
      ::add_to_stats<STATS_FAILURE, STATS_ROLLEDBACK>(stats);
      ::report_trx_recovery_error(ER_BINLOG_CRASH_RECOVERY_ROLLBACK_FAILED, xid,
                                  ht, exec_status);
    }
  }
}

void recover_one_external_trx(xarecover_st const &info, handlerton &ht,
                              XA_recover_txn const &xa_trx,
                              ::recovery_statistics &stats) {
  auto state{enum_ha_recover_xa_state::NOT_FOUND};

  if (info.xa_list != nullptr) {
    state = info.xa_list->find(xa_trx.id);
  }

  switch (state) {
    case enum_ha_recover_xa_state::COMMITTED_WITH_ONEPHASE:
    case enum_ha_recover_xa_state::COMMITTED: {
      if (ht.commit_by_xid != nullptr) {
        enum xa_status_code exec_status;
        if (DBUG_EVALUATE_IF("xa_recovery_error_reporting", true, false))
          exec_status = ::generate_xa_recovery_error();
        else
          exec_status = ht.commit_by_xid(&ht, const_cast<XID *>(&xa_trx.id));

        if (exec_status == XA_OK) {
          ::add_to_stats<STATS_SUCCESS, STATS_COMMITTED>(stats);
          break;
        } else
          ::report_trx_recovery_error(ER_BINLOG_CRASH_RECOVERY_COMMIT_FAILED,
                                      xa_trx.id, ht, exec_status,
                                      /*is_xa*/ true);
      }
      ::add_to_stats<STATS_FAILURE, STATS_COMMITTED>(stats);
      break;
    }
    case enum_ha_recover_xa_state::NOT_FOUND:
    case enum_ha_recover_xa_state::PREPARED_IN_SE:
    case enum_ha_recover_xa_state::ROLLEDBACK: {
      if (ht.rollback_by_xid != nullptr) {
        enum xa_status_code exec_status;
        if (DBUG_EVALUATE_IF("xa_recovery_error_reporting", true, false))
          exec_status = ::generate_xa_recovery_error();
        else
          exec_status = ht.rollback_by_xid(&ht, const_cast<XID *>(&xa_trx.id));

        if (exec_status == XA_OK) {
          ::add_to_stats<STATS_SUCCESS, STATS_ROLLEDBACK>(stats);
          break;
        } else
          ::report_trx_recovery_error(ER_BINLOG_CRASH_RECOVERY_ROLLBACK_FAILED,
                                      xa_trx.id, ht, exec_status,
                                      /*is_xa*/ true);
      }
      ::add_to_stats<STATS_FAILURE, STATS_ROLLEDBACK>(stats);
      break;
    }
    case enum_ha_recover_xa_state::PREPARED_IN_TC: {
      if (!Recovered_xa_transactions::instance().add_prepared_xa_transaction(
              &xa_trx)) {
        if (ht.set_prepared_in_tc_by_xid != nullptr) {
          enum xa_status_code exec_status;
          if (DBUG_EVALUATE_IF("xa_recovery_error_reporting", true, false))
            exec_status = ::generate_xa_recovery_error();
          else
            exec_status = ht.set_prepared_in_tc_by_xid(
                &ht, const_cast<XID *>(&xa_trx.id));

          if (exec_status == XA_OK) {
            ::add_to_stats<STATS_SUCCESS, STATS_PREPARED>(stats);
            break;
          } else
            ::report_trx_recovery_error(ER_BINLOG_CRASH_RECOVERY_PREPARE_FAILED,
                                        xa_trx.id, ht, exec_status,
                                        /*is_xa*/ true);
        }
      }
      ::add_to_stats<STATS_FAILURE, STATS_PREPARED>(stats);
      break;
    }
  }
}

template <size_t state, size_t counter>
void add_to_stats(::recovery_statistics &stats) {
  ++std::get<counter>(std::get<state>(stats));
}

bool has_failures(::recovery_statistics const &stats) {
  auto &failure = std::get<STATS_FAILURE>(stats);
  auto [fail_committed, fail_rolledback, fail_prepared] = failure;
  return fail_committed + fail_rolledback + fail_prepared != 0;
}

std::string print_stats(::recovery_statistics const &internal_stats,
                        ::recovery_statistics const &external_stats) {
  std::ostringstream oss;
  auto has_metrics =
      ::print_stat(internal_stats, "internal transaction(s)", oss);
  has_metrics =
      ::print_stat(external_stats, "XA transaction(s)", oss) || has_metrics;
  if (!has_metrics)
    oss << "No attempts to commit, rollback or prepare any transactions."
        << std::flush;
  return oss.str();
}

bool print_stat(::recovery_statistics const &stats, std::string const &trx_type,
                std::ostringstream &oss) {
  bool has_metrics{false};
  auto &[failure, success] = stats;

  auto [s_committed, s_rolledback, s_prepared] = success;
  if ((s_committed + s_rolledback + s_prepared) != 0) {
    has_metrics = true;
    oss << "Successfully" << std::flush;

    if (s_committed != 0) oss << " committed " << s_committed;
    if (s_rolledback != 0)
      oss << (s_committed != 0 ? "," : "") << " rolled back " << s_rolledback;
    if (s_prepared != 0)
      oss << (s_committed + s_rolledback != 0 ? "," : "") << " prepared "
          << s_prepared;

    oss << " " << trx_type << ". " << std::flush;
  }

  auto [f_committed, f_rolledback, f_prepared] = failure;
  if ((f_committed + f_rolledback + f_prepared) != 0) {
    has_metrics = true;
    oss << "Failed to" << std::flush;

    if (f_committed != 0) oss << " commit " << f_committed;
    if (f_rolledback != 0)
      oss << (f_committed != 0 ? "," : "") << " rollback " << f_rolledback;
    if (f_prepared != 0)
      oss << (f_committed + f_rolledback != 0 ? "," : "") << " prepare "
          << f_prepared;

    oss << " " << trx_type << "." << std::flush;
  }

  return has_metrics;
}

template <typename ID>
void report_trx_recovery_error(int error, ID const &xid, handlerton const &ht,
                               enum xa_status_code failure_code, bool is_xa) {
  assert(error == ER_BINLOG_CRASH_RECOVERY_COMMIT_FAILED ||
         error == ER_BINLOG_CRASH_RECOVERY_ROLLBACK_FAILED ||
         error == ER_BINLOG_CRASH_RECOVERY_PREPARE_FAILED);

  std::ostringstream oss;
  oss << (is_xa ? "XA " : "") << "transaction " << xid << std::flush;

  std::string failure;
  switch (failure_code) {
    case XAER_ASYNC: {
      failure.assign("XAER_ASYNC");
      break;
    }
    case XAER_RMERR: {
      failure.assign("XAER_RMERR");
      break;
    }
    case XAER_NOTA: {
      failure.assign("XAER_NOTA");
      break;
    }
    case XAER_INVAL: {
      failure.assign("XAER_INVAL");
      break;
    }
    case XAER_PROTO: {
      failure.assign("XAER_PROTO");
      break;
    }
    case XAER_RMFAIL: {
      failure.assign("XAER_RMFAIL");
      break;
    }
    case XAER_DUPID: {
      failure.assign("XAER_DUPID");
      break;
    }
    case XAER_OUTSIDE: {
      failure.assign("XAER_OUTSIDE");
      break;
    }
    case XA_OK: {
      assert(false);
      break;
    }
  }
  LogErr(INFORMATION_LEVEL, error, oss.str().data(),
         ha_resolve_storage_engine_name(&ht), failure.data());
}

enum xa_status_code generate_xa_recovery_error() {
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_async", return XAER_ASYNC;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_rmerr", return XAER_RMERR;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_nota", return XAER_NOTA;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_inval", return XAER_INVAL;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_proto", return XAER_PROTO;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_rmfail", return XAER_RMFAIL;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_dupid", return XAER_DUPID;);
  DBUG_EXECUTE_IF("xa_recovery_error_xaer_outside", return XAER_OUTSIDE;);
  return XA_OK;
}
}  // namespace
