#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "my_config.h"
#include "undo_spaces_snapshot.h"


namespace undo {

class UndoSpacesSnapshotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_undo_spaces_snapshot = new undo::Undo_spaces_snapshot();
    for (auto i = 0u; i < 100; ++i) {
      m_tablespaces.push_back(new undo::Tablespace((i + 1) * 10));
    }
  }

  void TearDown() override {
    for (auto i = 0u; i < m_tablespaces.size(); ++i) {
      delete m_tablespaces[i];
    }
    delete m_undo_spaces_snapshot;
    m_undo_spaces_snapshot = nullptr;
  }

 protected:
  undo::Undo_spaces_snapshot *m_undo_spaces_snapshot;
  std::vector<undo::Tablespace *> m_tablespaces;
};

TEST_F(UndoSpacesSnapshotTest, DefaultConstructor) {
  ASSERT_NE(m_undo_spaces_snapshot, nullptr);
  ASSERT_EQ(m_undo_spaces_snapshot->get_unused_tickets_number(), 0);
  ASSERT_EQ(m_undo_spaces_snapshot->get_target_undo_tablespaces_size(), 0);
  ASSERT_EQ(m_undo_spaces_snapshot->at(0), nullptr);
}

void request_ticket(undo::Undo_spaces_snapshot *undo_spaces_snapshot,
                    std::atomic<unsigned int> &counter, std::mutex &mutex,
                    std::condition_variable &cv) {
  {
    std::unique_lock<std::mutex> thread_lock(mutex);
    cv.wait(thread_lock);
  }
  bool use_no_latch = undo_spaces_snapshot->request_ticket();
  if (use_no_latch) {
    counter++;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    undo_spaces_snapshot->return_ticket();
  }
}

TEST_F(UndoSpacesSnapshotTest, UseAllTickets) {
  constexpr unsigned int ticket_number = 100;
  m_undo_spaces_snapshot->reset(ticket_number, m_tablespaces);
  ASSERT_EQ(m_undo_spaces_snapshot->get_unused_tickets_number(), ticket_number);
  ASSERT_EQ(m_undo_spaces_snapshot->get_max_tickets_number(), ticket_number);
  ASSERT_EQ(m_undo_spaces_snapshot->get_target_undo_tablespaces_size(),
            m_tablespaces.size());
  ASSERT_NE(m_undo_spaces_snapshot->at(0), nullptr);
  ASSERT_EQ(m_undo_spaces_snapshot->at(m_tablespaces.size()), nullptr);

  std::vector<std::thread> sessions;
  std::atomic<unsigned int> success_requested(0);
  std::mutex mutex;
  std::condition_variable cv;
  for (auto i = 0u; i < ticket_number * 2; ++i) {
    sessions.push_back(std::thread(&request_ticket, m_undo_spaces_snapshot,
                                   std::ref(success_requested), std::ref(mutex),
                                   std::ref(cv)));
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_EQ(success_requested.load(), 0);
  cv.notify_all();
  for (auto i = 0u; i < sessions.size(); ++i) {
    sessions[i].join();
  }
  m_undo_spaces_snapshot->block_until_tickets_returned();
  ASSERT_EQ(success_requested.load(), ticket_number);
  ASSERT_EQ(m_undo_spaces_snapshot->get_unused_tickets_number(), 0);
}

TEST_F(UndoSpacesSnapshotTest, UseSomeTickets) {
  constexpr unsigned int ticket_number = 100;
  m_undo_spaces_snapshot->reset(ticket_number, m_tablespaces);
  ASSERT_EQ(m_undo_spaces_snapshot->get_unused_tickets_number(), ticket_number);
  ASSERT_EQ(m_undo_spaces_snapshot->get_max_tickets_number(), ticket_number);
  ASSERT_EQ(m_undo_spaces_snapshot->get_target_undo_tablespaces_size(),
            m_tablespaces.size());
  ASSERT_NE(m_undo_spaces_snapshot->at(0), nullptr);
  ASSERT_EQ(m_undo_spaces_snapshot->at(m_tablespaces.size()), nullptr);

  std::vector<std::thread> sessions;
  std::atomic<unsigned int> success_requested(0);
  std::mutex mutex;
  std::condition_variable cv;
  for (auto i = 0u; i < ticket_number / 2; ++i) {
    sessions.push_back(std::thread(&request_ticket, m_undo_spaces_snapshot,
                                   std::ref(success_requested), std::ref(mutex),
                                   std::ref(cv)));
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_EQ(success_requested.load(), 0);
  cv.notify_all();
  for (auto i = 0u; i < sessions.size(); ++i) {
    sessions[i].join();
  }
  m_undo_spaces_snapshot->block_until_tickets_returned();
  ASSERT_EQ(success_requested.load(), ticket_number / 2);
  ASSERT_EQ(m_undo_spaces_snapshot->get_unused_tickets_number(), 0);
}

}