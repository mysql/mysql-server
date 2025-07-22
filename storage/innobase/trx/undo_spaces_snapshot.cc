#include "undo_spaces_snapshot.h"
#include <chrono>
#include <thread>
#include "trx0purge.h"

namespace undo {
Undo_spaces_snapshot::Undo_spaces_snapshot()
    : m_max_tickets(0),
      m_unused_tickets(0),
      m_used_tickets(0),
      m_tablespaces(),
      m_is_depriving(false) {}

Undo_spaces_snapshot::~Undo_spaces_snapshot() {}

void Undo_spaces_snapshot::reset(const unsigned int max_tickets,
                                 const std::vector<Tablespace *> &tablespaces) {
  m_used_tickets.store(0u, std::memory_order_release);
  m_tablespaces = tablespaces;
  m_max_tickets = max_tickets;
  m_is_depriving.store(false, std::memory_order_release);
  // Set m_unused_tickets in the end, which request_ticket depends on
  m_unused_tickets.store(m_max_tickets, std::memory_order_release);
}

std::size_t Undo_spaces_snapshot::get_target_undo_tablespaces_size() const {
  return m_tablespaces.size();
}

unsigned int Undo_spaces_snapshot::get_unused_tickets_number() const {
  return m_unused_tickets.load(std::memory_order_acquire);
}

bool Undo_spaces_snapshot::request_ticket() {
  unsigned int current_tickets_number = 0;
  while (m_is_depriving.load(std::memory_order_acquire) == false &&
         (current_tickets_number = get_unused_tickets_number()) > 0) {
    MONITOR_INC_VALUE(MONITOR_UNDO_TRUNCATE_SNAPSHOT_TICKET_TRY_COUNT, 1);
    if (m_unused_tickets.compare_exchange_strong(current_tickets_number,
                                                 current_tickets_number - 1,
                                                 std::memory_order_acq_rel)) {
      MONITOR_INC_VALUE(MONITOR_UNDO_TRUNCATE_SNAPSHOT_TICKET_GRANT_COUNT, 1);
      return true;
    }
  }
  return false;
}

void Undo_spaces_snapshot::return_ticket() { m_used_tickets.fetch_add(1); }

void Undo_spaces_snapshot::block_until_tickets_returned() {
  unsigned int deprived_tickets_number = 0;
  bool tickets_cleaned = false;
  m_is_depriving.store(true, std::memory_order_release);
  while (!tickets_cleaned) {
    deprived_tickets_number = get_unused_tickets_number();
    tickets_cleaned = m_unused_tickets.compare_exchange_strong(
        deprived_tickets_number, 0, std::memory_order_acq_rel);
  }
  while (m_used_tickets.load(std::memory_order_acquire) +
             deprived_tickets_number <
         m_max_tickets) {
    MONITOR_INC_VALUE(MONITOR_UNDO_TRUNCATE_SNAPSHOT_TICKET_WAIT_COUNT, 1)
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}

unsigned int Undo_spaces_snapshot::get_max_tickets_number() const {
  return m_max_tickets;
}

undo::Tablespace *Undo_spaces_snapshot::at(size_t pos) const {
  if (pos >= m_tablespaces.size()) {
    return nullptr;
  }
  return m_tablespaces[pos];
}

bool Undo_spaces_snapshot::is_active_no_latch_for_undo_space(size_t pos) const {
  auto undo_space = this->at(pos);
  if (undo_space == nullptr) {
    return false;
  }
  return undo_space->is_active_no_latch();
}

ulint Undo_spaces_snapshot::get_rsegs_size_for_undo_space(size_t pos) const {
  auto undo_space = this->at(pos);
  if (undo_space == nullptr) {
    return 0;
  }
  return undo_space->rsegs()->size();
}

trx_rseg_t *Undo_spaces_snapshot::get_active_for_undo_space(
    size_t pos, ulint rseg_slot) const {
  auto undo_space = this->at(pos);
  if (undo_space == nullptr) {
    return nullptr;
  }
  return undo_space->get_active(rseg_slot);
}

} /* namespace undo */