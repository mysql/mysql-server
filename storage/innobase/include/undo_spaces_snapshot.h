#ifndef UNDO_SPACES_SNAPSHOT_H
#define UNDO_SPACES_SNAPSHOT_H

#include <atomic>
#include <vector>
#include "trx0purge.h"

#ifdef GMOCK_FOUND
#include "gtest/gtest_prod.h"
#endif

namespace undo {

class Undo_spaces_snapshot {
 public:
  Undo_spaces_snapshot();
  Undo_spaces_snapshot(const Undo_spaces_snapshot &) = delete;
  Undo_spaces_snapshot &operator=(const Undo_spaces_snapshot &) = delete;
  Undo_spaces_snapshot(Undo_spaces_snapshot &&) = delete;
  Undo_spaces_snapshot &operator=(Undo_spaces_snapshot &&) = delete;
  ~Undo_spaces_snapshot();
  /* Reset the snapshot and tickets. The method should never be called when
   * block_until_tickets_returned is being executed. */
  void reset(const unsigned int max_tickets,
             const std::vector<Tablespace *> &tablespaces);
  /* Return the undo tablespaces size in the snapshot. */
  std::size_t get_target_undo_tablespaces_size() const;
  /* Return the unused tickets number. */
  unsigned int get_unused_tickets_number() const;
  /* Request a ticket. It will repeatedly call try_request_ticket until either
  it succeeds or unused_tickets_number equals 0. 
  Return true if it succeeds.
  Return false if there is no unused ticket. */
  bool request_ticket();
  /* Return a ticket requested earlier. */
  void return_ticket();
  /* Block the caller until all used tickets have been returned. */
  void block_until_tickets_returned();
  /* Return the max tickets number. */
  unsigned int get_max_tickets_number() const;
  /* Wrapper */
  bool is_active_no_latch_for_undo_space(size_t pos) const;
  /* Wrapper */
  ulint get_rsegs_size_for_undo_space(size_t pos) const;
  /* Wrapper */
  trx_rseg_t *get_active_for_undo_space(size_t pos, ulint rseg_slot) const;

 private:
  /* Return the tablespace pointer at pos, or nullptr if pos is out of bound. */
  Tablespace *at(size_t pos) const;

 private:
  unsigned int m_max_tickets;
  std::atomic<unsigned int> m_unused_tickets;
  std::atomic<unsigned int> m_used_tickets;
  std::vector<Tablespace *> m_tablespaces;
  std::atomic<bool> m_is_depriving;

#ifdef FRIEND_TEST
  FRIEND_TEST(UndoSpacesSnapshotTest, DefaultConstructor);
  FRIEND_TEST(UndoSpacesSnapshotTest, UseAllTickets);
  FRIEND_TEST(UndoSpacesSnapshotTest, UseSomeTickets);
#endif
}; /* class Undo_spaces_snapshot */
} /* namespace undo */

#endif  // UNDO_SPACES_SNAPSHOT_H