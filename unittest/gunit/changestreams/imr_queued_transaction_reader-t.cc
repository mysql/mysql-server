/* Copyright (c) 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>

#include "mysql/scheduler/statistics_map.h"
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_memory.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_reader.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/log_event.h"
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {

/// A fake encoded event: it hands back a preset, already-decoded Log_event when
/// the byte source asks it to decode(). This lets a single event be streamed
/// into the enqueue-created sink so the Job_binlog ctor's first-event peek
/// (wait_next()) returns immediately instead of blocking on an empty stream.
class Fake_reader_event : public IReader_event {
 public:
  explicit Fake_reader_event(std::shared_ptr<Log_event> decoded)
      : m_decoded(std::move(decoded)) {}

  std::shared_ptr<Log_event> decode() override { return m_decoded; }

  // Never re-read with reset_events=true in these tests, so a no-op suffices.
  void reset(const Format_description_log_event *) override {}

 private:
  std::shared_ptr<Log_event> m_decoded;
};

/// A body event served by the stream. A non-GTID event is fine:
/// Job_binlog::restart_internal dynamic_casts to Gtid_log_event*, gets nullptr,
/// and simply skips setting the gtid — no crash. Allocated with
/// Log_event::operator new (my_malloc), as a decoded event is: fetch_next()
/// hands its PSI memory ownership over, and my_claim() needs that header.
std::shared_ptr<Log_event> make_event() {
  return std::shared_ptr<Log_event>(new Format_description_log_event());
}

/// Wrap a decoded Log_event into a fake encoded IReader_event entry.
IReader_event_ptr make_fake(std::shared_ptr<Log_event> decoded) {
  return std::make_shared<Fake_reader_event>(std::move(decoded));
}

/// Serialize a real Format_description_log_event, as the receiver's network
/// bytes, for the byte-oriented append_event() of the memory sink.
std::vector<unsigned char> serialize_event() {
  Format_description_log_event ev;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing the FDE must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

/// Arm both roles, as an attached receiver and applier do.
void resume_both_roles(Trx_envelope_queue &queue) {
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.resume(Trx_envelope_queue::Scope::APPLIER);
}

}  // namespace

// Generous per-channel bounds so enqueue() always takes the MEMORY path and
// never blocks in acquire_admission().
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 20;  // 1 MiB
constexpr std::size_t kTrxLength = 128;            // fits the memory path

/// Build the active FDE that every enqueue() must be handed. enqueue() takes
/// the precise std::shared_ptr<Format_description_log_event> (the exact type
/// of Imr_runtime::m_fde_snapshot), so no upcast is
/// needed at the call site.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// @brief Fixture that exercises the REAL Queued_transaction_reader::read()
/// path through the queue-only test constructor.
///
/// This class is declared a friend of Queued_transaction_reader, so its member
/// functions (and, through make_reader(), the TEST_F bodies) may construct a
/// reader wired to nothing but a Trx_envelope_queue. m_channel/m_rli stay null;
/// read() still drains the queue, copies the dispatched Fetchable_transaction,
/// and builds a real Job_applier with a null Channel*.
class Queued_transaction_reader_test : public ::testing::Test {
 protected:
  void SetUp() override {
    // Statistics_monitor::get(0) / Resource_monitor::get(0) — used by the
    // reader's monitor reference members — need the instance-0 statistics maps
    // initialized first (mirrors rpl_applier_monitor-t.cc).
    std::ignore = scheduler::Statistics_map::init_statistics(0);
    std::ignore = csa::Statistics_map::init_statistics(0, 1, false);
  }

  // Builds a reader via the private queue-only constructor. Legal here because
  // this fixture is a friend of Queued_transaction_reader.
  std::unique_ptr<Queued_transaction_reader> make_reader(
      Trx_envelope_queue *queue) {
    return std::unique_ptr<Queued_transaction_reader>(
        new Queued_transaction_reader(queue));
  }

  // Enqueue one OPEN memory-path envelope. enqueue() itself creates the empty
  // single-batch destination and attaches the payload (reserving kTrxLength
  // bytes), so the returned envelope already owns a Fetchable_transaction
  // reachable via env->payload()->fetchable(). Returns the envelope
  // (non-owning).
  Transaction_envelope *enqueue_memory(Trx_envelope_queue *queue) {
    Transaction_envelope *env = queue->enqueue(kTrxLength, true, make_fde());
    EXPECT_NE(env, nullptr);
    return env;
  }

  // Commits the envelope and drains the queue so the queue destructor's "no
  // live payloads / bytes_used()==0" invariant holds when the local queue goes
  // out of scope. commit() releases the reserved bytes; reset() then drops the
  // entry. reset() (the production full-stop drain) is used rather than
  // sweep_committed() because some callers never dispatch the envelope (e.g. a
  // reader stopped before read()), and sweep_committed() refuses to advance the
  // commit mark past the dispatch cursor -- it would leave an undispatched
  // envelope in place. After this call env dangles - do not touch it.
  void drain(Trx_envelope_queue *queue, Transaction_envelope *env) {
    EXPECT_FALSE(env->commit());
    queue->reset();
  }
};

// read() builds a Job wired to the dispatched transaction, advances
// dispatch_seqno, and performs no decode.
TEST_F(Queued_transaction_reader_test, ReadBuildsJobAndAdvancesCursor) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);
  Transaction_envelope *env = enqueue_memory(&queue);
  ASSERT_NE(env, nullptr);
  // Use the enqueue-created Fetchable_transaction (a shared_ptr copy).
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  // Stream ONE event into the enqueue-created sink before read(). The
  // Job_binlog ctor peeks the first event via wait_next() (is_trx() is true);
  // without a buffered event that peek would block forever on the empty, open
  // stream. Do NOT seal: leaving the stream open keeps is_fetching_done() false
  // after the peek's reset_fetching(false).
  ASSERT_NE(env->current_sink(), nullptr);
  static_cast<Event_set_fetchable_memory *>(env->current_sink())
      ->append_reader_event(make_fake(make_event()));

  auto reader = make_reader(&queue);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);

  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr);
  // The dispatch cursor advanced past the single envelope.
  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  // read() must not decode: the transaction was never driven, so it is neither
  // done nor errored (no events were consumed).
  EXPECT_FALSE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  delete job;
  drain(&queue, env);
}

// After stop(), read() returns no job and leaves dispatch_seqno
// unchanged (it never reaches the queue hand-off).
TEST_F(Queued_transaction_reader_test, ReadAfterStopReturnsNullAndKeepsCursor) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);
  Transaction_envelope *env = enqueue_memory(&queue);
  ASSERT_NE(env, nullptr);

  auto reader = make_reader(&queue);
  reader->stop();
  EXPECT_TRUE(reader->is_stopped());

  Job_ptr job = reader->read();
  EXPECT_EQ(job, nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);

  drain(&queue, env);
}

// Payload lifetime under dispatch: the shared_ptr the reader hands to the
// Job_applier keeps the Fetchable_transaction alive while the job holds it,
// independent of the envelope's own payload reference.
TEST_F(Queued_transaction_reader_test, ReadKeepsFetchableAliveViaJob) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);
  Transaction_envelope *env = enqueue_memory(&queue);
  ASSERT_NE(env, nullptr);
  // Take an independent shared_ptr copy of the enqueue-created
  // Fetchable_transaction (payload()->fetchable() returns by value).
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  // Stream ONE event into the enqueue-created sink before read() so the
  // Job_binlog ctor's first-event peek (wait_next()) returns immediately
  // instead of blocking on the empty, open stream. Appending an event does not
  // change the Fetchable_transaction use_count, so the reference-count
  // assertions below still hold. Left unsealed on purpose.
  ASSERT_NE(env->current_sink(), nullptr);
  static_cast<Event_set_fetchable_memory *>(env->current_sink())
      ->append_reader_event(make_fake(make_event()));

  auto reader = make_reader(&queue);

  // Our own copy (1) + the envelope payload's copy (1).
  const long before = fetchable.use_count();
  EXPECT_EQ(before, 2);

  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr);
  // The job now holds an extra reference to the same Fetchable_transaction.
  EXPECT_EQ(fetchable.use_count(), before + 1);

  // Commit drops the envelope's payload reference; the job's copy must keep the
  // Fetchable_transaction alive on its own.
  EXPECT_FALSE(env->commit());
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_EQ(fetchable.use_count(), before);  // ours + job's copy

  // Deleting the job releases its reference; only our copy remains.
  delete job;
  EXPECT_EQ(fetchable.use_count(), 1);

  EXPECT_FALSE(queue.sweep_committed());
}

// Restart re-dispatch skip: read() skips an envelope that committed out of
// source order in a prior session (payload reset) and dispatches the next
// uncommitted one instead. The skip lives here on the reader side, off the
// queue lock, so the queue's dispatch primitive never takes a per-envelope lock
// while holding the queue lock.
TEST_F(Queued_transaction_reader_test, ReadSkipsCommittedOutOfOrderEnvelope) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Transaction_envelope *e1 = enqueue_memory(&queue);
  Transaction_envelope *e2 = enqueue_memory(&queue);
  Transaction_envelope *e3 = enqueue_memory(&queue);
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(e2, nullptr);
  ASSERT_NE(e3, nullptr);

  // e3 is the one that will be handed to a job, so give it a first event so the
  // Job_binlog ctor peek (wait_next()) returns immediately. e2 is skipped
  // before any job is built, so it needs no event.
  ASSERT_NE(e3->current_sink(), nullptr);
  static_cast<Event_set_fetchable_memory *>(e3->current_sink())
      ->append_reader_event(make_fake(make_event()));

  // Prior session: e1 dispatched (head, uncommitted); e2 committed out of order
  // behind the uncommitted head, its payload reset to null. dispatch_seqno now
  // points at e2.
  ASSERT_EQ(queue.dispatch_next(), e1);
  ASSERT_FALSE(e2->commit());
  ASSERT_EQ(e2->payload(), nullptr);
  ASSERT_EQ(queue.dispatch_seqno(), 1u);

  // Track e3's transaction to prove the job wraps e3, not the skipped e2.
  ASSERT_NE(e3->payload(), nullptr);
  auto f3 = e3->payload()->fetchable();
  ASSERT_NE(f3, nullptr);
  const long before = f3.use_count();  // ours + e3's payload == 2

  auto reader = make_reader(&queue);
  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr);

  // read() skipped the committed e2 and dispatched e3: the cursor advanced past
  // BOTH, and the job holds a reference to e3's transaction.
  EXPECT_EQ(queue.dispatch_seqno(), 3u);
  EXPECT_EQ(f3.use_count(), before + 1) << "the job must wrap e3, not e2";

  delete job;
  EXPECT_EQ(f3.use_count(), before);

  // Drain: commit the still-uncommitted e1 and e3 (e2 already committed), then
  // sweep the whole prefix so the queue dtor's empty/bytes==0 invariant holds.
  EXPECT_FALSE(e1->commit());
  EXPECT_FALSE(e3->commit());
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// The receiver truncates a transaction after sweep_and_dispatch() handed
// it out as live, while read() builds its job. The receiver thread waits until
// the dispatch cursor moved, which it can only see after the dispatch check
// released the queue lock, and then truncates the open group as the receiver
// does. The Job_binlog constructor's first-event peek either waits on the
// open, empty stream and is woken by the truncation, or sees the truncation at
// once. Either way read() must drop that job and return no job, and the next
// read() must sweep the truncated envelope and build the job for the next one.
// Without sticky truncation the constructor can instead wait forever (its
// reset erased a truncation that landed first), so read() runs on its own
// thread under a watchdog that fails the test and wakes the constructor.
TEST_F(Queued_transaction_reader_test, ReadDropsJobTruncatedBeforeBuilt) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  // e1 is held by the receiver and stays empty and open until truncated.
  Open_group group;
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength));
  Transaction_envelope *e1 = group.envelope;
  ASSERT_NE(e1, nullptr);
  auto f1 = e1->payload()->fetchable();
  ASSERT_NE(f1, nullptr);

  // e2 has a first event, so its job's peek returns at once.
  Transaction_envelope *e2 = enqueue_memory(&queue);
  ASSERT_NE(e2, nullptr);
  static_cast<Event_set_fetchable_memory *>(e2->current_sink())
      ->append_reader_event(make_fake(make_event()));
  auto f2 = e2->payload()->fetchable();
  ASSERT_NE(f2, nullptr);
  const long f2_before = f2.use_count();  // ours + e2's payload == 2

  std::thread receiver([&queue, &group] {
    while (queue.dispatch_seqno() < 1)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    truncate_transaction(group);
  });

  auto reader = make_reader(&queue);
  auto first_read =
      std::async(std::launch::async, [&reader] { return reader->read(); });
  receiver.join();
  if (first_read.wait_for(std::chrono::seconds(10)) !=
      std::future_status::ready) {
    ADD_FAILURE() << "read() is stuck in the job constructor: the truncation "
                     "was erased";
    f1->set_fetching_truncated();  // Wake it so the test can finish.
  }
  Job_ptr job = first_read.get();
  EXPECT_EQ(job, nullptr) << "the job of the truncated transaction is dropped";
  delete job;
  EXPECT_TRUE(f1->is_truncated()) << "building the job kept the truncation";
  f1.reset();

  // The next read() sweeps the truncated, released e1 and dispatches e2.
  job = reader->read();
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 2u);
  EXPECT_EQ(queue.queue_length(), 1u) << "e1 is swept";
  EXPECT_EQ(f2.use_count(), f2_before + 1) << "the job must wrap e2";

  delete job;
  f2.reset();
  EXPECT_FALSE(e2->commit());
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// The first event of a transaction does not decode.
// The Job_binlog constructor decodes it on the coordinator, which is where the
// null event used to be dereferenced. read() must build the job, the
// constructor's reset must clear the error, and the worker's read must fail
// again with the byte-source error so run_phase() reports it.
TEST_F(Queued_transaction_reader_test,
       UndecodableFirstEventBuildsJobAndFailsOnWorkerRead) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);
  Transaction_envelope *env = enqueue_memory(&queue);
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  // 5 bytes shorter than the header length, as the debug point makes it.
  const auto bytes = serialize_event();
  ASSERT_GT(bytes.size(), 5u);
  ASSERT_NE(env->current_sink(), nullptr);
  env->current_sink()->append_event(
      reinterpret_cast<const char *>(bytes.data()), bytes.size() - 5,
      /*seal_after=*/true);

  auto reader = make_reader(&queue);
  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr);
  EXPECT_FALSE(fetchable->is_fetching_error())
      << "the constructor's reset_fetching(false) clears the first-read error";

  // The worker's view: the event is there, but it does not decode.
  ASSERT_TRUE(fetchable->wait_next());
  EXPECT_FALSE(fetchable->fetch_next().has_value());
  EXPECT_TRUE(fetchable->is_fetching_error());
  EXPECT_FALSE(fetchable->is_truncated());
  EXPECT_NE(fetchable->get_fetch_error_msg().find("could not decode event #0"),
            std::string::npos)
      << fetchable->get_fetch_error_msg();

  delete job;
  drain(&queue, env);
}

// read() dispatches an envelope whose first event has not arrived,
// so the Job_binlog constructor's first-event peek parks in the batch wait.
// An applier stop must end the peek within 1 s, and read() must drop the job
// that the peek could not fill and return no job. Without the poll the peek
// never ends; the watchdog then truncates the stream to release read() and the
// test fails.
TEST_F(Queued_transaction_reader_test, ApplierStopEndsFirstEventPeek) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);
  Transaction_envelope *env = enqueue_memory(&queue);
  ASSERT_NE(env, nullptr);
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);

  auto reader = make_reader(&queue);
  auto read =
      std::async(std::launch::async, [&reader] { return reader->read(); });
  EXPECT_EQ(read.wait_for(std::chrono::milliseconds(50)),
            std::future_status::timeout)
      << "the consumer is parked";

  queue.stop(Trx_envelope_queue::Scope::APPLIER);
  if (read.wait_for(std::chrono::seconds(1)) != std::future_status::ready) {
    ADD_FAILURE() << "read() must end within 1 s of the applier stop";
    sink->set_stream_truncated();  // Wake it so the test can finish.
  }
  Job_ptr job = read.get();
  EXPECT_EQ(job, nullptr) << "the job built during the stop is dropped";
  delete job;
  EXPECT_EQ(queue.dispatch_seqno(), 1u);

  drain(&queue, env);
}

// A failed spill file delete stops the reader with an error before it
// dispatches anything, and the record is consumed. A record left while the
// applier is stopped is cleared when the applier resumes, as attach_role()
// does at START REPLICA SQL_THREAD, so the next reader runs.
TEST_F(Queued_transaction_reader_test, SpillDeleteFailureStopsReaderOnce) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);
  Transaction_envelope *env = enqueue_memory(&queue);
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->current_sink(), nullptr);
  static_cast<Event_set_fetchable_memory *>(env->current_sink())
      ->append_reader_event(make_fake(make_event()));

  queue.set_spill_delete_failed();
  auto reader = make_reader(&queue);
  EXPECT_EQ(reader->read(), nullptr);
  EXPECT_TRUE(reader->is_error());
  EXPECT_FALSE(queue.spill_delete_failed()) << "read() consumes the record";
  EXPECT_EQ(queue.dispatch_seqno(), 0u);

  queue.stop(Trx_envelope_queue::Scope::APPLIER);
  queue.set_spill_delete_failed();
  queue.resume(Trx_envelope_queue::Scope::APPLIER);
  EXPECT_FALSE(queue.spill_delete_failed())
      << "resume(APPLIER) clears the record";

  auto next_reader = make_reader(&queue);
  Job_ptr job = next_reader->read();
  ASSERT_NE(job, nullptr);
  EXPECT_FALSE(next_reader->is_error());
  EXPECT_EQ(queue.dispatch_seqno(), 1u);

  delete job;
  drain(&queue, env);
}

}  // namespace mysql::csa::unittests
