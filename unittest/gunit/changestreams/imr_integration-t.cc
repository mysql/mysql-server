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

/// @file
/// End-to-end in-memory harness test wiring the five core in-memory-relaylog
/// classes together: Trx_envelope_queue, Transaction_envelope, Trx_payload,
/// Event_set_fetchable_memory, and Queued_transaction_reader.
///
/// The flow mirrors the runtime memory path: admit + enqueue an envelope, build
/// a single-batch Event_set_fetchable_memory byte source owned by a
/// Fetchable_transaction, wrap that in a Trx_payload attached to the envelope,
/// stream a few events into the byte source and seal the byte stream (the
/// single authoritative "fully received" signal), dispatch the fully-received
/// transaction through the real Queued_transaction_reader::read() path (which
/// builds a Job_applier), drive the consumer surface to completion, then fire
/// the commit hook and sweep the committed head — asserting the reserved bytes
/// are fully released (bytes_used() == 0) and the queue drains.
///
/// The transaction is fully received (streamed + sealed) BEFORE it is
/// dispatched: building the Job_applier constructs a Job_binlog, whose ctor
/// (via restart_internal) peeks the transaction's first event with wait_next().
/// In this single-threaded harness, that peek would block forever if the stream
/// were still empty and unsealed, so reception must complete before read(). The
/// concurrent "dispatch while still streaming" case is a worker-thread scenario
/// exercised by the 7.2.x byte-source blocking tests.
///
/// As in imr_event_set_fetchable_memory-t.cc, the byte source stores *encoded*
/// events as IReader_event entries and decodes them lazily inside fetch_next().
/// The tests therefore feed a fake IReader_event whose decode() hands back a
/// known, non-TPLE Log_event (a Format_description_log_event), keeping the
/// transaction-payload decompression path out of the harness.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "my_dbug.h"  // DBUG_SET
#include "mysql/scheduler/statistics_map.h"
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_memory.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_reader.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // ensure_spill_directory
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/log_event.h"
#include "unittest/gunit/changestreams/imr_test_memory_resource.h"

namespace mysql::csa::unittests {

namespace {

// Generous per-channel bounds so enqueue() always takes the MEMORY path and
// never blocks in acquire_admission(). kTrxLength must be at or below the spill
// threshold so classify() picks MEMORY.
constexpr std::size_t kMemoryLimit = 1u << 20;     // 1 MiB
constexpr std::size_t kSpillThreshold = 1u << 16;  // 64 KiB
constexpr std::size_t kTrxLength = 4096;

/// A fake encoded event: it hands back a preset, already-decoded Log_event when
/// the byte source asks it to decode(). This lets the test control exactly
/// which Log_event object fetch_next() yields and assert identity/order.
class Fake_reader_event : public IReader_event {
 public:
  explicit Fake_reader_event(std::shared_ptr<Log_event> decoded)
      : m_decoded(std::move(decoded)) {}

  std::shared_ptr<Log_event> decode() override { return m_decoded; }

  // The byte source calls reset() only when re-reading with reset_events=true;
  // this harness never does, so a no-op is sufficient.
  void reset(const Format_description_log_event *) override {}

 private:
  std::shared_ptr<Log_event> m_decoded;
};

/// Build a Format_description_log_event as its precise type. enqueue() takes
/// the exact std::shared_ptr<Format_description_log_event> (matching the type
/// of Imr_runtime::m_fde_snapshot), so it is handed directly
/// with no upcast; passing it to the Event_set_fetchable_memory ctor, whose FDE
/// parameter is the base Log_event_ptr, is a well-formed implicit upcast.
std::shared_ptr<Format_description_log_event> make_fde() {
  return std::make_shared<Format_description_log_event>();
}

/// Arm both roles, as an attached receiver and applier do.
void resume_both_roles(Trx_envelope_queue &queue) {
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  queue.resume(Trx_envelope_queue::Scope::APPLIER);
}

/// A body event served by the stream: another FDE object (distinct instance),
/// so object identity is meaningful. Allocated with Log_event::operator new
/// (my_malloc), as a decoded event is: fetch_next() hands its PSI memory
/// ownership over, and my_claim() needs that header.
std::shared_ptr<Log_event> make_event() {
  return std::shared_ptr<Log_event>(new Format_description_log_event());
}

/// Wrap a decoded Log_event into a fake encoded IReader_event entry.
IReader_event_ptr make_fake(std::shared_ptr<Log_event> decoded) {
  return std::make_shared<Fake_reader_event>(std::move(decoded));
}

/// Serialize a real Format_description_log_event into a byte buffer, mimicking
/// the transient network bytes the receiver hands to
/// append_transaction_event(). A default server FDE serializes with checksum
/// OFF and is self-describing on decode, so the round-trip through the sink's
/// copy + Cached_event_memory:: decode() succeeds without a THD or checksum
/// plumbing (mirrors the helper in imr_queued_transaction_writer-t.cc).
std::vector<unsigned char> serialize_event() {
  Format_description_log_event ev;
  StringBuffer_ostream<1024> os;
  EXPECT_FALSE(ev.write(&os)) << "serializing the FDE must succeed";
  const auto *p = reinterpret_cast<const unsigned char *>(os.ptr());
  return std::vector<unsigned char>(p, p + os.length());
}

const char *as_char(const std::vector<unsigned char> &v) {
  return reinterpret_cast<const char *>(v.data());
}

/// A transaction length strictly above the spill threshold, so classify()
/// routes it to the SPILL path.
constexpr std::size_t kSpillTrxLength = kSpillThreshold + 1;

/// RAII private "relay log directory" for the spill-path integration tests;
/// removed on scope exit (with the spill files and temp-files subdir under it).
struct Scoped_temp_dir {
  std::string path;
  Scoped_temp_dir() {
    static std::atomic<unsigned> counter{0};
    path = (std::filesystem::temp_directory_path() /
            ("imr_integ_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1))))
               .string();
    std::filesystem::create_directories(path);
    // The spill-files subdirectory is provisioned once at server start by
    // ensure_spill_directory() (from init_replica); the queue no longer creates
    // it lazily on the first spill. This harness bypasses server startup, so
    // provision it here through the same production entry point.
    (void)ensure_spill_directory(path);
  }
  ~Scoped_temp_dir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

/// Short bounded wait that shows a consumer is still parked.
constexpr std::chrono::milliseconds kShortWait{50};
/// Bound on how long a parked wait may take to notice an applier stop.
constexpr std::chrono::seconds kStopWait{1};

/// A blocking wait run on its own thread under a watchdog. await() returns
/// the result if the wait ends in time; otherwise it calls @c release, which
/// must end the wait, and returns no value, so a regression fails instead of
/// hanging. The destructor releases and joins a wait that is still running.
class Watched_wait {
 public:
  Watched_wait(std::function<bool()> wait, std::function<void()> release)
      : m_release(std::move(release)),
        m_future(m_result.get_future()),
        m_thread(
            [this, wait = std::move(wait)] { m_result.set_value(wait()); }) {}
  ~Watched_wait() {
    if (!m_thread.joinable()) return;
    if (m_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      m_release();
    m_thread.join();
  }
  /// True while the wait has not returned after kShortWait.
  bool is_parked() {
    return m_future.wait_for(kShortWait) == std::future_status::timeout;
  }
  std::optional<bool> await(std::chrono::milliseconds limit) {
    const bool ended = m_future.wait_for(limit) == std::future_status::ready;
    if (!ended) m_release();
    m_thread.join();
    const bool value = m_future.get();
    if (!ended) return std::nullopt;
    return value;
  }

 private:
  std::function<void()> m_release;
  std::promise<bool> m_result;
  std::future<bool> m_future;
  std::thread m_thread;
};

}  // namespace

namespace fs = std::filesystem;

/// @brief End-to-end fixture wiring the five in-memory-relaylog classes.
///
/// Declared a friend of Queued_transaction_reader so make_reader() may build a
/// reader through the private queue-only test constructor (m_rli/m_channel stay
/// null; read() still runs its real code path). SetUp() initializes the
/// instance-0 statistics maps because constructing the reader binds its monitor
/// references via get(0).
class Imr_integration_test : public ::testing::Test {
 protected:
  void SetUp() override {
    // Statistics_monitor::get(0) / Resource_monitor::get(0) — bound by the
    // reader's monitor reference members — need the instance-0 statistics maps
    // initialized first (mirrors imr_queued_transaction_reader-t.cc).
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
};

// Wire all five classes into one end-to-end memory-path flow and assert
// the transaction is dispatched, committed via the byte-source commit hook, and
// swept, with the reserved bytes fully released.
TEST_F(Imr_integration_test, EndToEndMemoryPathDispatchCommitSweep) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  // 1) Admit + enqueue an uncommitted memory-path envelope. enqueue() now
  //    creates the EMPTY single-batch byte source, wraps it in a
  //    Fetchable_transaction (append_batch + set_fetching_complete), and
  //    attaches the payload — reserving exactly kTrxLength bytes — all before
  //    the envelope becomes observable. So the destination and its sink already
  //    exist on return.
  Transaction_envelope *env = queue.enqueue(kTrxLength, true, make_fde());
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->path(), Envelope_path::MEMORY);
  EXPECT_FALSE(env->is_committed());
  // The payload was attached at enqueue, so kTrxLength bytes are reserved.
  EXPECT_EQ(queue.bytes_used(), kTrxLength);

  // 2) The enqueue-created destination's sink is reachable via the envelope.
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);

  // 3) Stream a few events into the already-attached destination via the sink,
  //    then seal the byte stream. Reception is sealed on the SINK (byte source)
  //    — the single source of truth for "fully received". No
  //    set_fetching_complete() here: create_memory() already sealed the
  //    (single-batch) metadata stream.
  //
  //    This MUST happen before dispatch (step 4). Building the Job_applier in
  //    read() constructs a Job_binlog whose ctor (Job_binlog::restart_internal)
  //    peeks the transaction's first event via m_fetch_metadata->wait_next() +
  //    fetch_next() when is_trx() is true. In this single-threaded harness, if
  //    the stream were still empty and unsealed at that point, wait_next()
  //    would block forever on the stream CV and the Job ctor would deadlock.
  //    Streaming the events and sealing the stream first guarantees wait_next()
  //    has data to return. The concurrent "dispatch while still streaming" case
  //    is a worker-thread scenario covered by
  //    the 7.2.x byte-source blocking tests.
  constexpr int kEventCount = 3;
  std::vector<Log_event *> expected;
  expected.reserve(kEventCount);
  for (int i = 0; i < kEventCount; ++i) {
    auto decoded = make_event();
    expected.push_back(decoded.get());
    // Inject via the event-oriented seam so fetch_next() yields these exact
    // objects (the byte-oriented append_event decodes fresh events).
    static_cast<Event_set_fetchable_memory *>(sink)->append_reader_event(
        make_fake(decoded));
  }
  sink->seal_stream();

  // Grab a shared_ptr copy of the enqueue-created Fetchable_transaction to
  // drive the consumer surface and fire the commit hook. It is created and
  // sealed by create_memory(), so it is non-null and already trx-typed.
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  ASSERT_TRUE(fetchable->is_trx());

  // 4) Dispatch the fully-received transaction through the REAL reader path.
  //    read() copies the dispatched Fetchable_transaction and builds a
  //    Job_applier (whose Job_binlog ctor peeks the first event — safe now that
  //    the stream is sealed). read() performs no further decoding.
  auto reader = make_reader(&queue);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 1u);

  // 5) Drive the consumer surface to completion through the dispatched
  //    Fetchable_transaction: the events flow back in append order, by object
  //    identity, followed by a clean end-of-stream.
  std::vector<Log_event *> fetched;
  fetched.reserve(kEventCount);
  while (fetchable->wait_next()) {
    auto managed = fetchable->fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    fetched.push_back(managed->get_event().get());
  }
  EXPECT_EQ(fetched, expected);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  // 6) Fire the commit hook via the Fetchable_transaction, which fans out to
  //    the single byte source's set_success() and drives env->commit(): the
  //    envelope becomes committed, its payload is nulled, and the reserved
  //    bytes are released back to the queue counter.
  fetchable->set_success();
  EXPECT_TRUE(env->is_committed());
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);

  // The Job_applier still holds its own shared_ptr copy of the
  // Fetchable_transaction, independent of the now-dropped envelope payload.
  EXPECT_GE(fetchable.use_count(), 2);  // ours + the job's copy

  // 7) Coordinator sweep: the contiguous committed head is dequeued and
  //    commit_seqno advances. After the sweep, env dangles — do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);

  // The byte counter has returned to zero and the queue is drained, so the
  // queue destructor's "empty deque / bytes_used() == 0" invariant holds.
  EXPECT_EQ(queue.bytes_used(), 0u);

  // Releasing the job drops its reference; only our local copy remains.
  delete job;
  EXPECT_EQ(fetchable.use_count(), 1);
}

// Concurrency property test for out-of-order commit. This mirrors the real
// single-producer / single-consumer architecture: one producer thread streams
// envelopes into a shared Trx_envelope_queue, ONE coordinator thread is the
// sole consumer of the queue (it alone calls dispatch_next(), in FIFO order),
// and it hands each dispatched envelope to a pool of worker threads that COMMIT
// them out of source order. A concurrent sweeper thread advances the committed
// head prefix. The queue is never dispatched from more than one thread;
// out-of-order commit arises from the workers, exactly as in production (the
// coordinator dispatches in order, workers apply/commit in parallel).
//
// This deliberately operates at the queue/envelope/payload layer only: NO
// Queued_transaction_reader, NO Job_applier, and NO Event_set_fetchable_memory
// byte source is attached. Building a Job peeks the transaction's first event
// (which would block an empty stream), and the byte source is irrelevant to the
// memory-accounting / cursor-ordering properties under test. Plain Trx_payloads
// therefore stand in for the heavy byte source: each reserves its bytes at
// attach and releases them on commit() with no seal.
//
// The test is written to be ThreadSanitizer-clean: there is NO fixed sleep used
// for correctness. Threads coordinate exclusively through the queue's own
// blocking calls (dispatch_next() / enqueue()), the worker hand-off CV,
// atomics, and joins; std::this_thread::yield() is used only to avoid a busy
// spin and is never relied upon for ordering. All final assertions hold
// for ANY interleaving.
//
// Checks dispatch/commit ordering, the cursor invariant under concurrency, and
// that bytes are released at commit (the counter returns to 0 when drained).
TEST_F(Imr_integration_test, ConcurrentOutOfOrderCommitSweep) {
  constexpr int kEnvelopes = 200;
  constexpr int kWorkers = 4;
  // Huge memory limit and a large spill threshold so classify() always returns
  // MEMORY and enqueue() never blocks in admission — the property under test is
  // the dispatch/commit/sweep ordering, not admission back-pressure.
  constexpr std::size_t kHugeMemoryLimit = 1u << 30;      // 1 GiB
  constexpr std::size_t kLargeSpillThreshold = 1u << 20;  // 1 MiB

  // Fixed seed so any failure reproduces exactly; echoed once.
  constexpr std::uint32_t kSeed = 0xC0FFEE11u;
  std::cout << "[ConcurrentOutOfOrderCommitSweep] seed=" << kSeed
            << " kEnvelopes=" << kEnvelopes << " kWorkers=" << kWorkers
            << std::endl;

  // Precompute the per-envelope byte lengths from the fixed seed so the
  // producer thread does no shared RNG work (keeps the test race-free).
  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<std::size_t> len_dist(1, 4096);
  std::vector<std::size_t> lengths(kEnvelopes);
  for (int i = 0; i < kEnvelopes; ++i) lengths[i] = len_dist(rng);

  Trx_envelope_queue queue(kHugeMemoryLimit, kLargeSpillThreshold,
                           test_memory_resource(kHugeMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  std::atomic<int> committed_count{0};

  // Producer: enqueue every envelope in source order. enqueue() attaches the
  // MEMORY-path payload (reserving its bytes) itself, so no manual attach is
  // needed. This test never consumes events off the enqueue-created source; the
  // source is simply destroyed at commit. No seal is required; commit()
  // releases the bytes. Each enqueue wakes the coordinator
  // that is blocked in dispatch_next().
  std::thread producer([&]() {
    for (int i = 0; i < kEnvelopes; ++i) {
      const std::size_t len = lengths[i];
      Transaction_envelope *env = queue.enqueue(len, true, make_fde());
      ASSERT_NE(env, nullptr);
    }
  });

  // Worker hand-off: the sole consumer (coordinator) pushes FIFO-dispatched
  // envelopes here; the worker pool pops and commits them. A plain
  // mutex+CV+deque hand-off keeps the test race-free and TSan-clean.
  std::mutex work_mutex;
  std::condition_variable work_cv;
  std::deque<Transaction_envelope *> work;
  bool dispatch_done = false;

  // Coordinator: the ONLY consumer of the queue. It dispatches all envelopes in
  // FIFO order (single-consumer: no other thread calls dispatch_next()) and
  // enqueues each onto the worker hand-off.
  std::thread coordinator([&]() {
    for (int i = 0; i < kEnvelopes; ++i) {
      Transaction_envelope *env = queue.dispatch_next();
      ASSERT_NE(env, nullptr);
      {
        std::lock_guard<std::mutex> lock(work_mutex);
        work.push_back(env);
      }
      work_cv.notify_one();
    }
    {
      std::lock_guard<std::mutex> lock(work_mutex);
      dispatch_done = true;
    }
    work_cv.notify_all();
  });

  // Workers: pop dispatched envelopes and commit them. Because scheduling
  // decides which worker commits when, commit order is NOT the dispatch order —
  // out-of-order commit arises naturally. Each envelope is handed to exactly
  // one worker, so commit() must succeed (returns false).
  std::vector<std::thread> workers;
  workers.reserve(kWorkers);
  for (int w = 0; w < kWorkers; ++w) {
    workers.emplace_back([&]() {
      for (;;) {
        Transaction_envelope *env = nullptr;
        {
          std::unique_lock<std::mutex> lock(work_mutex);
          work_cv.wait(lock, [&] { return dispatch_done || !work.empty(); });
          if (work.empty()) break;  // dispatch_done and drained: exit.
          env = work.front();
          work.pop_front();
        }
        EXPECT_FALSE(env->commit());
        committed_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  // Sweeper: repeatedly sweep the contiguous committed head prefix until every
  // envelope has been swept (commit_seqno == kEnvelopes). The cursor reads are
  // atomic. The only cross-cursor checks are the <= orderings, which hold
  // regardless of the (non-atomic) skew between the three separate reads.
  // yield() avoids a busy spin; it is not relied on for correctness.
  std::thread sweeper([&]() {
    for (;;) {
      EXPECT_FALSE(queue.sweep_committed());

      const std::uint64_t commit = queue.commit_seqno();
      const std::uint64_t dispatch = queue.dispatch_seqno();
      const std::uint64_t insert = queue.insert_seqno();
      const std::size_t queue_length = queue.queue_length();
      // These orderings hold for ANY interleaving: a swept envelope was
      // dispatched, and a dispatched envelope was inserted. The independently
      // synchronized length snapshot must remain bounded and can never wrap.
      ASSERT_LE(commit, dispatch);
      ASSERT_LE(dispatch, insert);
      ASSERT_LE(queue_length, static_cast<std::size_t>(kEnvelopes));

      if (commit == static_cast<std::uint64_t>(kEnvelopes)) break;
      std::this_thread::yield();
    }
  });

  // Join in dependency order: producer feeds the coordinator, the coordinator
  // feeds the workers (and sets dispatch_done), the workers commit, and the
  // sweeper drains once every commit lands. No applier stop is needed: the
  // coordinator dispatches an exact count and returns on its own.
  producer.join();
  coordinator.join();
  for (auto &t : workers) t.join();
  sweeper.join();

  // Final assertions — hold for ANY interleaving now that all threads joined:
  //  - every envelope was inserted, dispatched, and swept exactly once, so all
  //    three cursors equal the total (commit_seqno advanced by exactly N means
  //    each envelope was swept exactly once);
  //  - the byte counter returned to 0: every payload's bytes were released at
  //    commit and the queue is fully drained (its destructor's empty-ring /
  //    bytes_used()==0 invariant therefore holds).
  EXPECT_EQ(queue.insert_seqno(), static_cast<std::uint64_t>(kEnvelopes));
  EXPECT_EQ(queue.dispatch_seqno(), static_cast<std::uint64_t>(kEnvelopes));
  EXPECT_EQ(queue.commit_seqno(), static_cast<std::uint64_t>(kEnvelopes));
  EXPECT_EQ(queue.queue_length(), 0u);
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(committed_count.load(), kEnvelopes);
}

// Concurrency property test for streaming into an OPEN envelope. One
// producer thread and one consumer thread share a single Trx_envelope_queue.
// The producer enqueues an envelope, attaches a Trx_payload wrapping an
// Event_set_fetchable_memory-backed Fetchable_transaction, then streams events
// over time (append_transaction_event) into the still-OPEN (unsealed) stream
// and finally seal_stream(). The consumer dispatch_next()s the envelope and
// drives wait_next()/fetch_next() DIRECTLY on the Fetchable_transaction/stream
// — so it blocks in wait_next() on the OPEN envelope until the producer appends
// events.
//
// This is the streaming case the single-threaded 9.1 harness cannot cover: 9.1
// must stream + seal BEFORE dispatch because building a Job_applier peeks the
// first event in Job_binlog::restart_internal() and would deadlock on an empty,
// unsealed stream. Here we deliberately stay at the queue + byte-source level
// (like 9.2): NO Queued_transaction_reader and NO Job_applier are built, so the
// consumer can begin waiting on the open stream while the producer is still
// streaming.
//
// ThreadSanitizer-clean: there is NO fixed sleep used for correctness. Ordering
// is enforced purely by the stream's own blocking wait_next() (which blocks
// until an append/seal/truncation), one acquire/release atomic that publishes
// the attached payload to the consumer, and the thread joins.
// std::this_thread::yield() appears only to avoid a busy spin while waiting for
// that publish and is never relied upon for ordering. The seed is fixed and
// echoed in every assertion so any failure reproduces exactly.
//
// Property — every appended event is consumed exactly once and in append order
// regardless of producer/consumer interleaving; after commit + sweep
// bytes_used() returns to 0.
TEST_F(Imr_integration_test, ConcurrentStreamingIntoOpenEnvelope) {
  // Generous bounds so enqueue() takes the MEMORY path and never blocks in
  // admission — the property under test is streaming/consumption ordering, not
  // back-pressure.
  constexpr std::size_t kHugeMemoryLimit = 1u << 30;      // 1 GiB
  constexpr std::size_t kLargeSpillThreshold = 1u << 20;  // 1 MiB

  // Fixed seed so any failure reproduces exactly; echoed once and in every
  // assertion message.
  constexpr std::uint32_t kSeed = 0x57EA311Du;
  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> count_dist(1, 128);
  const int kEventCount = count_dist(rng);
  std::cout << "[ConcurrentStreamingIntoOpenEnvelope] seed=" << kSeed
            << " kEventCount=" << kEventCount << std::endl;

  Trx_envelope_queue queue(kHugeMemoryLimit, kLargeSpillThreshold,
                           test_memory_resource(kHugeMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  // Pre-generate the events on the main thread and keep the shared_ptrs alive
  // for the whole test so the raw pointers stay valid. The expected consumption
  // order is exactly this generation order.
  std::vector<std::shared_ptr<Log_event>> owned_events;
  std::vector<Log_event *> expected;
  owned_events.reserve(kEventCount);
  expected.reserve(kEventCount);
  for (int i = 0; i < kEventCount; ++i) {
    auto e = make_event();
    expected.push_back(e.get());
    owned_events.push_back(std::move(e));
  }

  // Consumer results, read by the main thread only after join() (which
  // establishes happens-before).
  std::vector<Log_event *> fetched;
  std::atomic<bool> consumer_saw_committed{false};
  std::atomic<std::size_t> bytes_after_commit{~std::size_t{0}};
  std::atomic<bool> consumer_fetch_done{false};
  std::atomic<bool> consumer_fetch_error{true};

  // Producer: enqueue an OPEN envelope — enqueue() creates and attaches the
  // EMPTY single-batch Event_set_fetchable_memory destination (already
  // set_fetching_complete-sealed on the metadata side) and reserves kTrxLength
  // bytes, all under the queue mutex before the envelope is observable. Then
  // stream events one at a time into the still-open byte stream via the sink
  // and finally seal it. No sleeps between appends — the consumer wakes on each
  // append via the stream's own CV.
  std::thread producer([&]() {
    Transaction_envelope *env = queue.enqueue(kTrxLength, true, make_fde());
    ASSERT_NE(env, nullptr);

    // The enqueue-created destination's sink is reachable via the envelope.
    Streaming_event_sink *sink = env->current_sink();
    ASSERT_NE(sink, nullptr);

    // Stream events into the OPEN (unsealed) byte stream over time. The
    // consumer, blocked in wait_next() on this open stream, wakes on each
    // append.
    for (int i = 0; i < kEventCount; ++i) {
      // Inject via the event-oriented seam to preserve object identity through
      // fetch_next() (the byte-oriented append_event decodes fresh events).
      static_cast<Event_set_fetchable_memory *>(sink)->append_reader_event(
          make_fake(owned_events[i]));
    }
    // Seal the byte stream — the single authoritative "fully received" signal;
    // wait_next() reports end-of-stream after the last event. The
    // (single-batch) metadata stream was already sealed by create_memory(), so
    // no set_fetching_complete() is needed here for the consumer's wait_next()
    // loop to finish.
    sink->seal_stream();
  });

  // Consumer: dispatch the envelope, then drive wait_next()/fetch_next()
  // DIRECTLY on the Fetchable_transaction. It blocks in wait_next() on the OPEN
  // stream until the producer appends, drains events in order, and terminates
  // when wait_next() returns false after seal_stream()+set_fetching_complete().
  std::thread consumer([&]() {
    Transaction_envelope *env = queue.dispatch_next();
    ASSERT_NE(env, nullptr);

    // enqueue() attaches the payload under the queue mutex BEFORE the envelope
    // becomes observable, so by the time dispatch_next() returns it the payload
    // is guaranteed attached (happens-before via the queue mutex). No publish
    // gate is needed.
    Trx_payload *payload = env->payload();
    ASSERT_NE(payload, nullptr);
    // Hold an independent shared_ptr copy so the Fetchable_transaction (and its
    // byte source) survives the commit that nulls the envelope's payload.
    std::shared_ptr<Fetchable_transaction> fetchable = payload->fetchable();
    ASSERT_NE(fetchable, nullptr);

    // Drain: this is where the consumer blocks on the OPEN stream and wakes on
    // each producer append. Events flow back by object identity in append
    // order.
    while (fetchable->wait_next()) {
      auto managed = fetchable->fetch_next();
      ASSERT_TRUE(managed.has_value())
          << "wait_next() returned true so fetch_next() must yield an event";
      fetched.push_back(managed->get_event().get());
    }
    consumer_fetch_done.store(fetchable->is_fetching_done(),
                              std::memory_order_relaxed);
    consumer_fetch_error.store(fetchable->is_fetching_error(),
                               std::memory_order_relaxed);

    // Commit through the byte-source commit hook: set_success() fans out to the
    // single batch, driving env->commit() (mark committed + reset payload,
    // releasing the reserved bytes) under only the per-envelope mutex.
    fetchable->set_success();
    consumer_saw_committed.store(env->is_committed(),
                                 std::memory_order_relaxed);
    bytes_after_commit.store(queue.bytes_used(), std::memory_order_relaxed);
  });

  producer.join();
  consumer.join();

  // Property: every appended event was consumed exactly once and in append
  // order, for this interleaving.
  EXPECT_EQ(fetched, expected)
      << "seed=" << kSeed << " kEventCount=" << kEventCount;
  EXPECT_EQ(fetched.size(), static_cast<std::size_t>(kEventCount))
      << "seed=" << kSeed;
  EXPECT_TRUE(consumer_fetch_done.load())
      << "seed=" << kSeed << " (stream must reach clean end-of-stream)";
  EXPECT_FALSE(consumer_fetch_error.load()) << "seed=" << kSeed;

  // The commit hook committed the envelope and released the reserved bytes.
  EXPECT_TRUE(consumer_saw_committed.load()) << "seed=" << kSeed;
  EXPECT_EQ(bytes_after_commit.load(), 0u) << "seed=" << kSeed;

  // Coordinator sweep drains the committed head; bytes stay at 0 and the queue
  // empties (all three cursors reach 1). After the sweep env dangles — do not
  // touch it.
  EXPECT_FALSE(queue.sweep_committed()) << "seed=" << kSeed;
  EXPECT_EQ(queue.commit_seqno(), 1u) << "seed=" << kSeed;
  EXPECT_EQ(queue.dispatch_seqno(), 1u) << "seed=" << kSeed;
  EXPECT_EQ(queue.insert_seqno(), 1u) << "seed=" << kSeed;
  EXPECT_EQ(queue.bytes_used(), 0u) << "seed=" << kSeed;
}

// Wire the receiver -> queue -> coordinator path end-to-end through the
// actual receiver-hook mechanism the Master_info adapters wrap.
//
// FIDELITY / WHY A MIRROR: at runtime the receiver drives these steps through
// the file-static imr_on_gtid_event / imr_on_body_event / imr_on_truncate
// adapters in rpl_replica.cc, which are one-line wrappers that call exactly the
// open_transaction / append_transaction_event / truncate_transaction free
// functions below
// on the channel runtime's queue and open group. A real Master_info cannot be
// built in a gunit process (its constructor is private to Rpl_info_factory, and
// its destructor asserts the global channel_map write lock), and the provider's
// in-memory reader ctor dereferences rli->mi->get_channel() at construction. So
// this test mirrors the runtime's queue and open group with a local queue and a
// local Open_group group and drives the SAME writer functions the adapters
// wrap. The accessor is_in_memory_relaylog() is exactly
// (m_trx_queue != nullptr); that trivial getter and the full Master_info +
// Sync_transaction_provider path are exercised by MTR integration. The open
// group lifecycle asserted here (closed outside a group; the resolved sink and
// its held envelope between the GTID event and the terminal/truncate) is the
// runtime's open-group contract.
TEST_F(Imr_integration_test, ReceiverHooksToCoordinatorCommitSweep) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  // Mirror of the runtime's open group: closed outside a transaction group.
  Open_group group;
  ASSERT_TRUE(group.envelope == nullptr && group.sink == nullptr)
      << "the open group is closed outside a group";

  // --- GTID event -> open_transaction (imr_on_gtid_event) ---
  // Admits the transaction into the queue (reserving kTrxLength bytes) and
  // publishes the enqueue-created destination's sink through the open group.
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength,
                                /*is_trx=*/true));
  ASSERT_NE(group.sink, nullptr)
      << "open publishes the resolved sink (the runtime's receiver_sink())";
  EXPECT_EQ(queue.bytes_used(), kTrxLength);
  // The byte source lives in the envelope's payload and outlives the terminal
  // event that closes the group; keep a handle to fire the commit hook after
  // the coordinator dispatches (see below).
  Streaming_event_sink *byte_source = group.sink;

  // --- body + terminal events -> append_transaction_event (imr_on_body_event)
  // --- Real serialized FDE bytes so the sink's copy + lazy-decode path runs
  // for real. The terminal append seals the stream (the single authoritative
  // "fully received" signal) and closes the group.
  const std::vector<unsigned char> bytes = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(bytes), bytes.size(),
                                        /*is_terminal=*/false));
  ASSERT_NE(group.sink, nullptr)
      << "a non-terminal append keeps the group open";
  ASSERT_FALSE(append_transaction_event(group, as_char(bytes), bytes.size(),
                                        /*is_terminal=*/true));
  EXPECT_TRUE(group.envelope == nullptr && group.sink == nullptr)
      << "the terminal event seals the group and closes it";

  // --- coordinator iteration -> reader.read() (sweep_and_dispatch) ---
  // read() sweeps the (empty) committed head, dispatches the fully-received
  // transaction, and builds a Job_applier (whose Job_binlog ctor peeks the
  // first event -- safe now that the stream is sealed).
  auto reader = make_reader(&queue);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr) << "read() dispatches the fully-received transaction";
  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  EXPECT_EQ(queue.commit_seqno(), 0u)
      << "not committed until the worker succeeds";

  // --- worker set_success commit hook ---
  // In production a worker drives Job::set_success() once the job reaches the
  // 'done' phase. With a single memory batch this calls
  // Event_set_fetchable_memory::set_success(), which calls
  // Transaction_envelope::commit(). Running a job to the 'done' phase needs a
  // live worker/session, so the test fires the terminal link of that exact
  // chain, the byte source's commit hook, directly. The Job built above holds
  // a shared reference to the same Fetchable_transaction, so the byte source
  // is alive here.
  static_cast<Event_set_fetchable_memory *>(byte_source)->set_success();
  EXPECT_EQ(queue.bytes_used(), 0u)
      << "commit resets the payload, releasing the reserved bytes";

  // --- coordinator sweep ---
  // The contiguous committed head is dequeued and commit_seqno advances. After
  // the sweep the envelope dangles -- do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u) << "the committed head is swept";
  EXPECT_EQ(queue.bytes_used(), 0u);

  // Queue is empty with bytes_used()==0, so the destructor invariant holds.
  delete job;
}

// Truncation: a transaction cut short mid-stream is truncated (never
// sealed cleanly), so a worker rolls it back and never fires the success hook;
// the envelope is therefore never committed. Truncated is a terminal state, so
// the committed-head sweep reclaims it exactly like a committed one — advancing
// the commit low-water mark and releasing its reserved bytes at sweep — letting
// the applier keep progressing without a full-stop reset() (the same behavior
// asserted directly in imr_queue_lifecycle-t's
// SweepReclaimsDispatchedTruncatedHead). Drives the real truncate_transaction
// receiver hook.
TEST_F(Imr_integration_test, ReceiverHookTruncatedTransactionNeverCommits) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;

  // GTID event -> open_transaction.
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kTrxLength,
                                /*is_trx=*/true));
  ASSERT_NE(group.sink, nullptr);
  EXPECT_EQ(queue.bytes_used(), kTrxLength);

  // A partial body event arrives, then the group is cut short (rotate / error /
  // stop mid-transaction) -> truncate_transaction (imr_on_truncate): mark the
  // stream truncated and close the open group. No commit is performed.
  const std::vector<unsigned char> bytes = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(bytes), bytes.size(),
                                        /*is_terminal=*/false));
  truncate_transaction(group);
  EXPECT_TRUE(group.envelope == nullptr && group.sink == nullptr)
      << "truncation closes the open group";

  // The coordinator still dispatches the envelope. A worker applying
  // a truncated stream rolls back and never calls set_success(), so the
  // envelope is never committed.
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  EXPECT_TRUE(env->is_truncated())
      << "the receiver marked the open transaction truncated";
  EXPECT_FALSE(env->is_committed())
      << "a truncated transaction is rolled back, never committed";
  // Truncation alone does not release the payload; the bytes stay charged until
  // the sweep drops the envelope.
  EXPECT_EQ(queue.bytes_used(), kTrxLength);

  // The committed-head sweep reclaims the truncated head exactly like a
  // committed one (it is a terminal state): commit_seqno advances past it and
  // its reserved bytes are released at sweep, so the applier keeps progressing
  // without a full-stop reset(). After the sweep env dangles — do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u)
      << "the truncated head is reclaimed by the sweep";
  EXPECT_EQ(queue.bytes_used(), 0u)
      << "the reserved bytes are released when the truncated envelope is swept";
}

// Back-pressure property: the memory budget blocks ONLY the IO thread
// (the producer in acquire_admission), never the coordinator/workers. A budget
// far smaller than the workload forces the producer to block, and it can only
// advance as the consumer commits (releasing bytes) and sweeps. If the
// commit -> release_bytes -> wake path were broken, the producer would block
// forever and this test would hang, so completion itself proves the property.
//
// ThreadSanitizer-clean: threads coordinate exclusively through the queue's own
// blocking calls (enqueue()/dispatch_next()), env->commit(), sweep_committed(),
// and joins; the only shared reads are of the atomic byte counter. No fixed
// sleep is used for correctness.
TEST_F(Imr_integration_test, BackPressureBlocksProducerNotConsumer) {
  constexpr std::size_t kLen = 4096;
  constexpr std::size_t kCapacity = 4;  // live payloads that fit
  constexpr std::size_t kSmallLimit = kLen * kCapacity;  // tight memory budget
  // kLen <= threshold => MEMORY path
  constexpr std::size_t kSpill = kSmallLimit;
  constexpr int kTotal = 40;  // >> capacity: forces blocking

  Trx_envelope_queue queue(kSmallLimit, kSpill,
                           test_memory_resource(kSmallLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  std::atomic<int> committed{0};

  // Coordinator + worker: dispatch FIFO, commit (releasing kLen bytes and
  // waking the blocked IO thread), then sweep the committed head. Never blocks
  // in admission -- only the producer can.
  std::thread consumer([&] {
    for (int i = 0; i < kTotal; ++i) {
      Transaction_envelope *env = queue.dispatch_next();
      if (env == nullptr) {
        ADD_FAILURE() << "dispatch_next returned nullptr before draining";
        break;
      }
      // Back-pressure invariant: the counter is never above the limit at any
      // instant (admission enforces it before the producer reserves).
      EXPECT_LE(queue.bytes_used(), kSmallLimit);
      EXPECT_FALSE(env->commit());  // releases kLen bytes, wakes the producer
      EXPECT_FALSE(queue.sweep_committed());
      committed.fetch_add(1, std::memory_order_relaxed);
    }
  });

  // IO thread (receiver): enqueue far more than the budget holds at once, so it
  // MUST block in admission and advance only as the consumer frees space.
  std::thread producer([&] {
    for (int i = 0; i < kTotal; ++i) {
      if (queue.enqueue(kLen, /*is_trx=*/true, make_fde()) == nullptr) {
        ADD_FAILURE() << "enqueue returned nullptr (unexpected stop)";
        break;
      }
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(committed.load(), kTotal);
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_EQ(queue.commit_seqno(), static_cast<std::uint64_t>(kTotal));
  EXPECT_EQ(queue.insert_seqno(), static_cast<std::uint64_t>(kTotal));
  // Queue drained + empty: destructor invariant holds.
}

// A stop wakes an IO thread parked in admission: the blocked enqueue
// aborts and returns nullptr rather than reserving bytes.
TEST_F(Imr_integration_test, StopUnblocksProducerBlockedInAdmission) {
  constexpr std::chrono::milliseconds kShortWait{50};
  constexpr std::chrono::seconds kJoinWait{3};
  constexpr std::size_t kLen = 4096;
  constexpr std::size_t kCapacity = 2;
  constexpr std::size_t kSmallLimit = kLen * kCapacity;
  constexpr std::size_t kSpill = kSmallLimit;

  Trx_envelope_queue queue(kSmallLimit, kSpill,
                           test_memory_resource(kSmallLimit));
  // mi-owned queue defaults to stopped; arm it for the test.
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);

  // Fill the budget so the next admission must block.
  for (std::size_t i = 0; i < kCapacity; ++i) {
    ASSERT_NE(queue.enqueue(kLen, /*is_trx=*/true, make_fde()), nullptr);
  }
  ASSERT_EQ(queue.bytes_used(), kSmallLimit);

  // Park the IO thread in acquire_admission(), which blocks because the
  // budget is full.
  std::promise<Transaction_envelope *> parked;
  std::future<Transaction_envelope *> fut = parked.get_future();
  std::thread producer([&] {
    parked.set_value(queue.enqueue(kLen, /*is_trx=*/true, make_fde()));
  });

  ASSERT_EQ(fut.wait_for(kShortWait), std::future_status::timeout)
      << "the IO thread must block in admission while the budget is full";

  // stop(RECEIVER) wakes the blocked admission; the enqueue aborts and
  // returns nullptr.
  queue.stop(Trx_envelope_queue::Scope::RECEIVER);
  ASSERT_EQ(fut.wait_for(kJoinWait), std::future_status::ready)
      << "stop(RECEIVER) must wake the blocked IO thread";
  EXPECT_EQ(fut.get(), nullptr) << "a stopped admission fails the enqueue";
  producer.join();

  // The admitted envelopes are uncommitted; reset() drops them so the queue
  // dtor invariant holds.
  queue.reset();
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// A transaction larger than the whole budget is routed to SPILL, so an
// over-limit transaction never back-pressures the IO thread. classify() only
// routes (SPILL vs MEMORY); back-pressure is applied by the monitor inside
// memory-path admission, never by classify().
TEST_F(Imr_integration_test, OverLimitTransactionClassifiesSpillNotBlock) {
  constexpr std::size_t kLen = 4096;
  constexpr std::size_t kCapacity = 2;
  constexpr std::size_t kSmallLimit = kLen * kCapacity;
  // spill_threshold == memory_limit
  constexpr std::size_t kSpill = kSmallLimit;

  Trx_envelope_queue queue(kSmallLimit, kSpill,
                           test_memory_resource(kSmallLimit));

  // Larger than the whole budget -> SPILL.
  EXPECT_EQ(queue.classify(kSmallLimit + 1), Envelope_path::SPILL);
  // Within the threshold and room available -> MEMORY.
  EXPECT_EQ(queue.classify(kLen), Envelope_path::MEMORY);

  // Saturate the budget (without enqueuing). Routing ignores usage: an
  // in-threshold trx still classifies MEMORY (its admission is what would wait)
  // and an over-threshold trx still classifies SPILL.
  queue.add_bytes(kSmallLimit);
  EXPECT_EQ(queue.classify(kLen), Envelope_path::MEMORY);
  EXPECT_EQ(queue.classify(kSmallLimit + 1), Envelope_path::SPILL);

  queue.release_bytes(kSmallLimit);  // undo; no envelopes were created.
  EXPECT_EQ(queue.bytes_used(), 0u);
}

// ---------------------------------------------------------------------------
// End-to-end SPILL path through the real queue + reader.
// ---------------------------------------------------------------------------

// A transaction larger than the spill threshold flows end-to-end through the
// SPILL path: enqueue creates the on-disk destination, the receiver streams the
// body + terminal (sealing) into the spill file, the reader dispatches and
// builds a Job, the consumer drains the events back from the file, the commit
// hook commits the envelope (releasing ZERO bytes), the sweep reclaims it, and
// the spill file is deleted once the last holder of the byte source is dropped.
TEST_F(Imr_integration_test, EndToEndSpillPathDispatchCommitSweepAndCleanup) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  // 1) Admit + enqueue a SPILL-path envelope. enqueue() provisions the on-disk
  //    destination and reserves ZERO bytes, before the envelope is observable.
  Transaction_envelope *env =
      queue.enqueue(kSpillTrxLength, /*is_trx=*/true, make_fde());
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(env->path(), Envelope_path::SPILL);
  EXPECT_FALSE(env->is_committed());
  EXPECT_EQ(queue.bytes_used(), 0u) << "spill is outside the memory budget";

  // 2) The enqueue-created spill destination is reachable, healthy, over a real
  //    file on the channel's relay log directory.
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);
  auto *spill = dynamic_cast<Event_set_fetchable_spill *>(sink);
  ASSERT_NE(spill, nullptr);
  ASSERT_FALSE(spill->is_error()) << spill->get_error_str();
  const std::string spill_path = spill->spill_file_name();
  ASSERT_FALSE(spill_path.empty());
  EXPECT_TRUE(fs::exists(spill_path));

  // 3) Stream a few real serialized events into the spill file via the byte-
  //    oriented sink, sealing on the terminal event. Reception must complete
  //    before dispatch: read() builds a Job whose ctor peeks the first event.
  constexpr int kEventCount = 4;
  for (int i = 0; i < kEventCount; ++i) {
    const std::vector<unsigned char> bytes = serialize_event();
    const bool last = (i == kEventCount - 1);
    sink->append_event(as_char(bytes), bytes.size(), /*seal_after=*/last);
    ASSERT_FALSE(spill->is_error()) << spill->get_error_str();
  }

  // A shared handle to the enqueue-created Fetchable_transaction to drive the
  // consumer and fire the commit hook.
  ASSERT_NE(env->payload(), nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  ASSERT_TRUE(fetchable->is_trx());

  // 4) Dispatch through the REAL reader path (sweep_and_dispatch + Job build).
  auto reader = make_reader(&queue);
  EXPECT_EQ(queue.dispatch_seqno(), 0u);
  Job_ptr job = reader->read();
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 1u);

  // 5) Drive the consumer surface: the events decode back from the spill file
  //    in order, followed by a clean end-of-stream.
  int fetched = 0;
  while (fetchable->wait_next()) {
    auto managed = fetchable->fetch_next();
    ASSERT_TRUE(managed.has_value())
        << "wait_next() returned true so fetch_next() must yield an event";
    ++fetched;
  }
  EXPECT_EQ(fetched, kEventCount);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());

  // 6) Commit hook: the envelope commits, its payload is nulled, and ZERO bytes
  //    are released (spill never charged the counter).
  fetchable->set_success();
  EXPECT_TRUE(env->is_committed());
  EXPECT_EQ(env->payload(), nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);

  // The spill file is still present: the Job and our local handle keep the byte
  // source (hence the Spill_file_writer) alive past the envelope's payload.
  EXPECT_TRUE(fs::exists(spill_path));

  // 7) Coordinator sweep: the committed head is dequeued. After the sweep env
  //    dangles — do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.bytes_used(), 0u);

  // 8) Drop the remaining holders of the byte source. When the last shared_ptr
  //    to the Fetchable_transaction goes away, the Event_set_fetchable_spill
  //    (and its Spill_file_writer) is destroyed and the spill file is deleted.
  delete job;
  fetchable.reset();
  EXPECT_FALSE(fs::exists(spill_path))
      << "the spill file must be deleted once the byte source is released";
}

// A spill transaction cut short mid-stream is truncated (never sealed), so the
// coordinator dispatches it, a worker would roll it back (never committing),
// and the committed-head sweep reclaims the truncated envelope — releasing its
// (zero) bytes and deleting its spill file. Drives the real receiver hooks.
TEST_F(Imr_integration_test,
       EndToEndSpillPathTruncatedRollbackSweepAndCleanup) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  // mi-owned queue defaults to stopped; arm it for the test.
  resume_both_roles(queue);

  Open_group group;

  // GTID event -> open_transaction: admits the SPILL transaction and publishes
  // its spill sink. No memory is charged.
  ASSERT_FALSE(open_transaction(queue, group, make_fde(), kSpillTrxLength,
                                /*is_trx=*/true));
  ASSERT_NE(group.sink, nullptr);
  EXPECT_EQ(queue.bytes_used(), 0u);

  auto *spill = dynamic_cast<Event_set_fetchable_spill *>(group.sink);
  ASSERT_NE(spill, nullptr);
  const std::string spill_path = spill->spill_file_name();
  ASSERT_FALSE(spill_path.empty());
  EXPECT_TRUE(fs::exists(spill_path));

  // A partial body event arrives, then the group is cut short -> truncate.
  const std::vector<unsigned char> bytes = serialize_event();
  ASSERT_FALSE(append_transaction_event(group, as_char(bytes), bytes.size(),
                                        /*is_terminal=*/false));
  truncate_transaction(group);
  EXPECT_TRUE(group.envelope == nullptr && group.sink == nullptr)
      << "truncation closes the open group";

  // The coordinator still dispatches the envelope; a worker rolls back a
  // truncated stream and never commits.
  Transaction_envelope *env = queue.dispatch_next();
  ASSERT_NE(env, nullptr);
  EXPECT_EQ(queue.dispatch_seqno(), 1u);
  EXPECT_TRUE(env->is_truncated());
  EXPECT_FALSE(env->is_committed());
  EXPECT_EQ(queue.bytes_used(), 0u);
  // The spill file is still present until the envelope is swept.
  EXPECT_TRUE(fs::exists(spill_path));

  // The committed-head sweep reclaims the truncated head exactly like a
  // committed one: it destroys the envelope (and its payload/byte source), so
  // the spill file is deleted. After the sweep env dangles — do not touch it.
  EXPECT_FALSE(queue.sweep_committed());
  EXPECT_EQ(queue.commit_seqno(), 1u);
  EXPECT_EQ(queue.bytes_used(), 0u);
  EXPECT_FALSE(fs::exists(spill_path))
      << "a truncated spill transaction's file must be removed on sweep";
}

// ---------------------------------------------------------------------------
// An applier stop aborts a consumer parked in
// Fetchable_transaction::wait_next() without committing or truncating the
// envelope, and the next session reads the whole transaction.
// ---------------------------------------------------------------------------

namespace {

/// Parks a thread in @p fetchable's wait_next(), stops the applier and checks
/// the abort: false within kStopWait, aborted, not truncated, no fetch error,
/// and the commit hook leaves @p env neither committed nor truncated. Then
/// resumes the applier and resets the fetch, as the next START does.
void expect_applier_stop_aborts(Trx_envelope_queue &queue,
                                Transaction_envelope *env,
                                Fetchable_transaction &fetchable,
                                Streaming_event_sink *sink) {
  {
    Watched_wait wait([&fetchable] { return fetchable.wait_next(); },
                      [sink] { sink->set_stream_truncated(); });
    ASSERT_TRUE(wait.is_parked()) << "the consumer is parked";
    queue.stop(Trx_envelope_queue::Scope::APPLIER);
    const std::optional<bool> result = wait.await(kStopWait);
    ASSERT_TRUE(result.has_value())
        << "wait_next() must end within 1 s of the applier stop";
    EXPECT_FALSE(*result);
  }
  EXPECT_TRUE(fetchable.is_aborted());
  EXPECT_FALSE(fetchable.is_truncated());
  EXPECT_FALSE(fetchable.is_fetching_error());
  EXPECT_FALSE(fetchable.is_fetching_done());

  // The aborted job rolls back; the commit hook must not commit the envelope.
  fetchable.set_success();
  EXPECT_FALSE(env->is_committed());
  EXPECT_FALSE(env->is_truncated());

  queue.resume(Trx_envelope_queue::Scope::APPLIER);
  fetchable.reset_fetching(false);
  EXPECT_FALSE(fetchable.is_aborted()) << "reset_fetching() clears the abort";
}

}  // namespace

// Memory path: the consumer parks before the first event and again mid-
// transaction; both applier stops abort it, and after the restart the whole
// transaction is read in order and commits.
TEST_F(Imr_integration_test, ApplierStopAbortsParkedMemoryConsumer) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);
  Transaction_envelope *env = queue.enqueue(kTrxLength, true, make_fde());
  ASSERT_NE(env, nullptr);
  ASSERT_EQ(env->path(), Envelope_path::MEMORY);
  std::shared_ptr<void> drain(nullptr, [&queue](void *) { queue.reset(); });
  auto *sink = static_cast<Event_set_fetchable_memory *>(env->current_sink());
  ASSERT_NE(sink, nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  // Parked before the first event.
  ASSERT_NO_FATAL_FAILURE(
      expect_applier_stop_aborts(queue, env, *fetchable, sink));

  // Parked mid-transaction, after the first two events were fetched.
  std::vector<Log_event *> expected;
  for (int i = 0; i < 2; ++i) {
    auto decoded = make_event();
    expected.push_back(decoded.get());
    sink->append_reader_event(make_fake(decoded));
  }
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(fetchable->wait_next());
    auto managed = fetchable->fetch_next();
    ASSERT_TRUE(managed.has_value());
    EXPECT_EQ(managed->get_event().get(), expected[i]);
  }
  ASSERT_NO_FATAL_FAILURE(
      expect_applier_stop_aborts(queue, env, *fetchable, sink));

  // The receiver delivers the rest; the restarted consumer reads it all.
  for (int i = 0; i < 2; ++i) {
    auto decoded = make_event();
    expected.push_back(decoded.get());
    sink->append_reader_event(make_fake(decoded), /*seal_after=*/i == 1);
  }
  std::vector<Log_event *> fetched;
  while (fetchable->wait_next()) {
    auto managed = fetchable->fetch_next();
    ASSERT_TRUE(managed.has_value());
    fetched.push_back(managed->get_event().get());
  }
  EXPECT_EQ(fetched, expected);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_aborted());
  fetchable->set_success();
  EXPECT_TRUE(env->is_committed());
  fetchable.reset();
}

// Spill path: the same contract over a spill file.
TEST_F(Imr_integration_test, ApplierStopAbortsParkedSpillConsumer) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  resume_both_roles(queue);
  Transaction_envelope *env =
      queue.enqueue(kSpillTrxLength, /*is_trx=*/true, make_fde());
  ASSERT_NE(env, nullptr);
  ASSERT_EQ(env->path(), Envelope_path::SPILL);
  std::shared_ptr<void> drain(nullptr, [&queue](void *) { queue.reset(); });
  Streaming_event_sink *sink = env->current_sink();
  ASSERT_NE(sink, nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);
  const std::vector<unsigned char> bytes = serialize_event();

  // Parked before the first event.
  ASSERT_NO_FATAL_FAILURE(
      expect_applier_stop_aborts(queue, env, *fetchable, sink));

  // Parked mid-transaction, after the first two events were fetched.
  for (int i = 0; i < 2; ++i)
    ASSERT_FALSE(sink->append_event(as_char(bytes), bytes.size()));
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(fetchable->wait_next());
    ASSERT_TRUE(fetchable->fetch_next().has_value());
  }
  ASSERT_NO_FATAL_FAILURE(
      expect_applier_stop_aborts(queue, env, *fetchable, sink));

  // The receiver delivers the rest; the restarted consumer reads it all.
  for (int i = 0; i < 2; ++i)
    ASSERT_FALSE(sink->append_event(as_char(bytes), bytes.size(),
                                    /*seal_after=*/i == 1));
  int fetched = 0;
  while (fetchable->wait_next()) {
    ASSERT_TRUE(fetchable->fetch_next().has_value());
    ++fetched;
  }
  EXPECT_EQ(fetched, 4);
  EXPECT_TRUE(fetchable->is_fetching_done());
  EXPECT_FALSE(fetchable->is_fetching_error());
  fetchable->set_success();
  EXPECT_TRUE(env->is_committed());
  fetchable.reset();
}

// The imr_force_applier_queue_desync debug point needs a debug build.
#ifndef NDEBUG
// Structural failure: when the sweep finds the queue inconsistent,
// sweep_and_dispatch() sets the same applier stop flag that stop(APPLIER)
// sets, so a worker parked in a memory or a spill batch wait ends within one
// poll, as aborted. The flag stays set until resume(APPLIER).
TEST_F(Imr_integration_test, StructuralFailureEndsParkedConsumers) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  resume_both_roles(queue);
  Transaction_envelope *mem_env = queue.enqueue(kTrxLength, true, make_fde());
  ASSERT_NE(mem_env, nullptr);
  ASSERT_EQ(mem_env->path(), Envelope_path::MEMORY);
  Transaction_envelope *spill_env =
      queue.enqueue(kSpillTrxLength, /*is_trx=*/true, make_fde());
  ASSERT_NE(spill_env, nullptr);
  ASSERT_EQ(spill_env->path(), Envelope_path::SPILL);
  std::shared_ptr<void> drain(nullptr, [&queue](void *) { queue.reset(); });

  ASSERT_EQ(queue.sweep_and_dispatch(), mem_env);
  ASSERT_EQ(queue.sweep_and_dispatch(), spill_env);
  Streaming_event_sink *mem_sink = mem_env->current_sink();
  ASSERT_NE(mem_sink, nullptr);
  Streaming_event_sink *spill_sink = spill_env->current_sink();
  ASSERT_NE(spill_sink, nullptr);
  auto mem_fetchable = mem_env->payload()->fetchable();
  ASSERT_NE(mem_fetchable, nullptr);
  auto spill_fetchable = spill_env->payload()->fetchable();
  ASSERT_NE(spill_fetchable, nullptr);

  {
    Watched_wait mem_wait(
        [&mem_fetchable] { return mem_fetchable->wait_next(); },
        [mem_sink] { mem_sink->set_stream_truncated(); });
    Watched_wait spill_wait(
        [&spill_fetchable] { return spill_fetchable->wait_next(); },
        [spill_sink] { spill_sink->set_stream_truncated(); });
    ASSERT_TRUE(mem_wait.is_parked()) << "the memory consumer is parked";
    ASSERT_TRUE(spill_wait.is_parked()) << "the spill consumer is parked";

    DBUG_SET("+d,imr_force_applier_queue_desync");
    Transaction_envelope *after = queue.sweep_and_dispatch();
    DBUG_SET("-d,imr_force_applier_queue_desync");
    EXPECT_EQ(after, nullptr);
    EXPECT_TRUE(queue.applier_error());
    EXPECT_TRUE(queue.applier_stop_flag()->load())
        << "the structural failure sets the applier stop flag";

    const std::optional<bool> mem_result = mem_wait.await(kStopWait);
    ASSERT_TRUE(mem_result.has_value())
        << "the memory wait_next() must end within 1 s of the structural "
           "failure";
    EXPECT_FALSE(*mem_result);
    const std::optional<bool> spill_result = spill_wait.await(kStopWait);
    ASSERT_TRUE(spill_result.has_value())
        << "the spill wait_next() must end within 1 s of the structural "
           "failure";
    EXPECT_FALSE(*spill_result);
  }
  EXPECT_TRUE(mem_fetchable->is_aborted());
  EXPECT_FALSE(mem_fetchable->is_truncated());
  EXPECT_FALSE(mem_fetchable->is_fetching_error());
  EXPECT_TRUE(spill_fetchable->is_aborted());
  EXPECT_FALSE(spill_fetchable->is_truncated());
  EXPECT_FALSE(spill_fetchable->is_fetching_error());

  EXPECT_EQ(queue.sweep_and_dispatch(), nullptr)
      << "the flag stays set until resume";
  EXPECT_EQ(queue.dispatch_seqno(), 2u);
  queue.resume(Trx_envelope_queue::Scope::APPLIER);
  EXPECT_FALSE(queue.applier_error());
  EXPECT_FALSE(queue.applier_stop_flag()->load());
  mem_fetchable.reset();
  spill_fetchable.reset();
}
#endif  // NDEBUG

// Guard: a truncation survives reset_fetching(), so the next wait returns
// false at once, as truncated and not aborted.
TEST_F(Imr_integration_test, TruncationSurvivesResetFetchingNotAborted) {
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit));
  resume_both_roles(queue);
  Transaction_envelope *env = queue.enqueue(kTrxLength, true, make_fde());
  ASSERT_NE(env, nullptr);
  std::shared_ptr<void> drain(nullptr, [&queue](void *) { queue.reset(); });
  auto *sink = static_cast<Event_set_fetchable_memory *>(env->current_sink());
  ASSERT_NE(sink, nullptr);
  auto fetchable = env->payload()->fetchable();
  ASSERT_NE(fetchable, nullptr);

  sink->append_reader_event(make_fake(make_event()));
  sink->set_stream_truncated();
  fetchable->reset_fetching(false);
  {
    Watched_wait wait([&fetchable] { return fetchable->wait_next(); },
                      [&queue, &fetchable] {
                        queue.stop(Trx_envelope_queue::Scope::APPLIER);
                        fetchable->set_fetching_done();
                      });
    const std::optional<bool> result = wait.await(kStopWait);
    ASSERT_TRUE(result.has_value()) << "a truncated wait must end at once";
    EXPECT_FALSE(*result);
  }
  EXPECT_TRUE(fetchable->is_truncated());
  EXPECT_FALSE(fetchable->is_aborted());
  EXPECT_FALSE(env->is_committed());
  fetchable.reset();
}

// APPLIER_EVENT_MEMORY_LIMIT: appends through the sink that enqueue() wires
// raise the enqueue-created Fetchable_transaction's maximum event length, on
// both paths, and a consumer reset (re-dispatch, retry) keeps it. Nothing
// fetches these bytes, so only their lengths matter.
TEST_F(Imr_integration_test, SinkAppendsRaiseFetchableMaxEventLengthBothPaths) {
  Scoped_temp_dir relay_dir;
  Trx_envelope_queue queue(kMemoryLimit, kSpillThreshold,
                           test_memory_resource(kMemoryLimit), relay_dir.path);
  queue.resume(Trx_envelope_queue::Scope::RECEIVER);
  std::shared_ptr<void> drain(nullptr, [&queue](void *) { queue.reset(); });

  const std::vector<unsigned char> small(32, 0);
  const std::vector<unsigned char> large(300, 0);
  for (const auto &[trx_length, path] :
       {std::make_pair(kTrxLength, Envelope_path::MEMORY),
        std::make_pair(kSpillTrxLength, Envelope_path::SPILL)}) {
    Transaction_envelope *env =
        queue.enqueue(trx_length, /*is_trx=*/true, make_fde());
    ASSERT_NE(env, nullptr);
    ASSERT_EQ(env->path(), path);
    Streaming_event_sink *sink = env->current_sink();
    ASSERT_NE(sink, nullptr);
    auto fetchable = env->payload()->fetchable();
    ASSERT_NE(fetchable, nullptr);
    EXPECT_EQ(fetchable->get_max_event_length(), 0u);

    ASSERT_FALSE(sink->append_event(as_char(small), small.size()));
    EXPECT_EQ(fetchable->get_max_event_length(), small.size());
    ASSERT_FALSE(sink->append_event(as_char(large), large.size()));
    EXPECT_EQ(fetchable->get_max_event_length(), large.size());
    ASSERT_FALSE(sink->append_event(as_char(small), small.size(),
                                    /*seal_after=*/true));
    EXPECT_EQ(fetchable->get_max_event_length(), large.size());

    fetchable->reset_fetching(true);
    EXPECT_EQ(fetchable->get_max_event_length(), large.size())
        << "a consumer reset must keep the maximum event length";
  }
}

}  // namespace mysql::csa::unittests
